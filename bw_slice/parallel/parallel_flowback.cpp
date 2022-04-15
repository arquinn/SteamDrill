#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <assert.h>
#include <pthread.h>
#include <netdb.h>

#include <iostream>
#include <cstring>
#include <fstream>
#include <unordered_map>
#include <set>
#include <vector>

using namespace std;

#include "../graph_interface/node.h"
#include "../graph_interface/input_output.h"
#include "../maputil.h"

#include "merge_mapper.h"
#include "streamnw.h"
#include "queue.h"

//macros for stats, debugging and geting node from merge structure
#define STATS
#define TRACE
#define GET_NODE(ml,id) ml->getNode(id) 


#ifdef TRACE
ofstream trace_f;
#endif

//////////
//Globals -  here for performance/convenience
///////


//stacks for iterating mergelog
#define STACK_SIZE 1000000
typedef std::pair<nodeid,uint32_t> stacktype[STACK_SIZE];
stacktype *stacks;

//streaming communication structures
struct taintq_hdr*  outputq_hdr;
struct taintq_hdr*  inputq_hdr;
uint32_t*           outputq_buf;
uint32_t*           inputq_buf;
int                 oqfd = -1;
int                 iqfd = -1;

//input / ouput / mergelog
MergeMapper *mergelog_mapper;
struct addrstruct *addrspace;
struct input_info *inputBuf = NULL, *outputBuf = NULL;
unordered_map<nodeid, struct input_info*> inputMap; 
u_long mdatasize = 0;

//global constants for what this epoch is all about
u_char              parallelize = 1;
u_int32_t           depth = 10; //MAKE THIS A COMMAND LINE ARG
bool                start_flag = false;
bool                finish_flag = false;

//results of this thing
set<nodeid> *nodeids;


#ifdef STATS
struct timeval start_tv = {0,0}, end_tv = {0,0};
struct timeval recv_done_tv = {0,0}, preprocess_input_done_tv = {0,0};
struct timeval finish_outputs_tv = {0,0};

//counts 
int areceived = 0;
int aunchanged = 0;


static long ms_diff (struct timeval tv1, struct timeval tv2)
{
    return ((tv1.tv_sec - tv2.tv_sec) * 1000 + (tv1.tv_usec - tv2.tv_usec) / 1000);
}

void print_stats (const char* dirname, 
		  u_long mdatasize, u_long odatasize, 
		  u_long idatasize, u_long adatasize)
{
    char statsname[256];
    sprintf (statsname, "%s/stream-stats", dirname);
    ofstream stats(statsname);

    stats <<  "Start time:              " << start_tv.tv_sec 
	  << "." << start_tv.tv_usec << endl;
    stats <<  "Recv time:               " << recv_done_tv.tv_sec 
	  << "." << recv_done_tv.tv_usec << endl;
    stats <<  "End time:                " << end_tv.tv_sec 
	  << "." << end_tv.tv_usec << endl;
    stats <<  "Total time:              " << ms_diff(end_tv, start_tv) << "ms\n";
    stats <<  "Receive time:            " << ms_diff(recv_done_tv, start_tv) << "ms\n";
    stats <<  "Preprocess input time:   " 
	  << ms_diff(preprocess_input_done_tv,recv_done_tv) << "ms\n";
    stats <<  "Process outputs time:    " 
	  << ms_diff(finish_outputs_tv,preprocess_input_done_tv) << "ms\n";
    stats <<  "Finish time:             " 
	  << ms_diff(end_tv,finish_outputs_tv) << "ms\n";

    stats <<  "Received " << mdatasize << " bytes of merge data" << endl;
    stats <<  "Received " << odatasize << " bytes of output data" << endl;
    stats <<  "Received " << idatasize << " bytes of input data" << endl;
    stats <<  "Received " << adatasize << " bytes of address data" << endl;
    stats << endl;
    stats << areceived << " addrs from prev epoch "
	  << aunchanged << " were not tainted\n";

    stats.close();
}
#endif
 

static void
merge(set<nodeid> *addTo, set<nodeid> *addFrom) 
{
    for (auto a : *addFrom)
	addTo->insert(a);
}


static int
init_socket (int port)
{
   int c = socket (AF_INET, SOCK_STREAM, 0);
    if (c < 0) {
	fprintf (stderr, "Cannot create socket, errno=%d\n", errno);
	return c;
    }

    int on = 1;
    long rc = setsockopt (c, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    if (rc < 0) {
	fprintf (stderr, "Cannot set socket option, errno=%d\n", errno);
	return rc;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    rc = ::bind (c, (struct sockaddr *) &addr, sizeof(addr));
    if (rc < 0) {
	fprintf (stderr, "Cannot bind socket, errno=%d\n", errno);
	return rc;
    }

    rc = listen (c, 5);
    if (rc < 0) {
	fprintf (stderr, "Cannot listen on socket, errno=%d\n", errno);
	return rc;
    }
    
    int s = accept (c, NULL, NULL);
    if (s < 0) {
	fprintf (stderr, "Cannot accept connection, errno=%d\n", errno);
	return s;
    }

    close (c);
    return s;
}


static long
setup_shmem (int port, char* group_directory)
{
    // Initialize a socket to receive a little bit of input data
    int s = init_socket (port);
    if (s < 0) {
	fprintf (stderr, "init socket reutrns %d\n", s);
	return s;
    }
    // This will be sent after processing is completed 
    int rc = safe_read (s, group_directory, 256);
    if (rc != 256) {
	fprintf (stderr, "Read of group directory failed, rc=%d, errno=%d\n", rc, errno);
	return -1;
    }

    close (s);
    return 0;
}


static int32_t
read_inputs (int port, u_long &idatasize, u_long &odatasize, u_long& adatasize,
	     int& ifd, int& ofd, int& mfd, int &afd)
{
    char group_directory[256];
    char merge_file[256];

    if (setup_shmem(port, group_directory) < 0) return -1;

    inputBuf = (struct input_info *) map_buffer ("inputs", group_directory, idatasize, ifd);
    outputBuf = (struct input_info *) map_buffer ("outputs", group_directory, odatasize, ofd);
    addrspace = (struct addrstruct *) map_buffer ("ending_as", group_directory, adatasize, afd);

    
    snprintf(merge_file, 256, "/node_nums_shm%s", group_directory);
    for (u_int i = 1; i < strlen(merge_file); i++) {
	if (merge_file[i] == '/') merge_file[i] = '.';
    }
    mergelog_mapper = new MergeMapper(merge_file);

#ifdef TRACE
    trace_f << dec << getpid() << hex << " inputs: " << idatasize << endl;
    trace_f << dec << getpid() << hex << " outputs: " << odatasize << endl;
    trace_f << dec << getpid() << hex << " merge: " << mdatasize << endl;
    trace_f << dec << getpid() << hex << " addrs: " << adatasize << endl;
#endif

    return 0;
}

static long 
setup_aggregation (const char *dirname)
{
    stacks = new stacktype[parallelize];
    nodeids = new set<nodeid>(); 


    long rc = mkdir(dirname, 0755);
    if (rc < 0 && errno != EEXIST) {
	fprintf (stderr, "Cannot create output dir %s, errno=%d\n", dirname, errno);
	return rc;
    }

#ifdef TRACE
    char filename[256];
    sprintf(filename, "%s/dift.trace",dirname);
    trace_f.open(filename);
#endif

    return 0;
}

static void print_node(nodeid id, u_long dynid, char *ip,//u_long ip, 
		       nodeid p1, nodeid p2, 
		       u_long location, u_long value, ofstream &outf)
{
    outf << std::hex << id 
	 << "," << dynid 
	 << "," << ip
	 << "," << p1 << "," << p2
	 << "," << location << "," << value << endl;    
}


static void print_local_node(nodeid id, ofstream &outf) 
{
    char input[256] = "starting state";
    print_node(id, id, input, 0,0,id,0, outf);
}


static void print_input(nodeid i, ofstream &outf)
{
    struct input_info *entry = NULL; 

    auto element = inputMap.find(i);
    if (element == inputMap.end()) 
	cerr << "huh, can't find input " << i << endl;
    entry = element->second;
    u_long dynid = 0;
    char input[1024];

    switch(entry->type) { 
    case input_info_type::IMM:
	sprintf(input, "%x", entry->imm_info.ip);
	dynid = entry->imm_info.instcount;

    case input_info_type::READ:	
	
	sprintf(input, "read from %d@%u",entry->syscall_info.fileno, 
		entry->syscall_cnt);
	break;
    case input_info_type::MMAP:       
	sprintf(input, "mmap from %d@%u",entry->syscall_info.fileno, entry->syscall_cnt);
	break;
    default:
	sprintf(input, "input from %d@%u",entry->syscall_info.fileno, entry->syscall_cnt);
	break;
    }

    print_node(i, dynid, input, 0,0,0,0, outf); 
}


static long
finish_aggregation (const char *dirname, char *outputPtr, char *outputStop)
{
    char statsname[256];

    sprintf (statsname, "%s/nodeids", dirname);
    ofstream file(statsname);
    u_long maxicnt = 0, minicnt = 0xffffffff;

    for (auto &i : *nodeids) 
    {      
	if (i < LOCAL_SOURCE_OFFSET) { 
	    print_local_node(i, file);	    
	}
	else if (i < NODE_OFFSET) { 
	    print_input(i,file);
	}
	else { 	
	    node_t *entry = GET_NODE(mergelog_mapper, i);

	    if (entry->instcount > maxicnt)
		maxicnt = entry->instcount;
	    if (entry->instcount < minicnt)
		minicnt = entry->instcount;

	    char input[128];
	    sprintf(input, "%x", entry->ip);
	    print_node(i, entry->instcount, input,
		       entry->p1, entry->p2, entry->location, entry->value, file);
	}	
    }  


    cerr << hex << "icnt [" << minicnt << ", " << maxicnt << "]: " << dec << maxicnt - minicnt << endl;
/*
  long next_id = -1;
    u_long instcount = 0;
    char ip[128];
    //print out info for the outputs 
    while (outputPtr < outputStop)
    {
	struct input_info *curr_ii = (struct input_info *)outputPtr; 
	instcount = curr_ii->imm_info.instcount;
	sprintf(ip, "%x",curr_ii->imm_info.ip); 

	outputPtr += sizeof(struct input_info);
	outputPtr += sizeof(u_long);
	u_int buf_size = *outputPtr; 
	outputPtr += sizeof(u_int);

	nodeid *buf = (nodeid *)outputPtr; 
	outputPtr += buf_size * sizeof(nodeid);  
	
	for (u_int i = 0; i < buf_size; ++i)
	{
	    if ((i > 0 && buf[i-1] != buf[i]) || 
		(i == 0 && buf[i] > 0))
	    {		
		print_node(next_id, instcount, ip, buf[0], 0,0,0,file);
		next_id--;
	    }
	}
    }   
*/

    file.close();
    return 0;
}

static void preprocessInput(struct input_info *inputBuf, u_long inputSize) 
{
    struct input_info *entry = inputBuf;   
    while ((char *)entry < (char *)inputBuf + inputSize) 
    {	
	nodeid curr_nodeid = entry->nodeid_start; 
	while (curr_nodeid < entry->nodeid_start + entry->size)
	{
	    inputMap[curr_nodeid] = entry;
	    ++curr_nodeid;
	}	
	entry++;
    }       
}

/* The address map is constructed in a low-priority thread since it is not needed for a while */
struct build_map_data {
    unordered_map<uint32_t,nodeid>* paddress_map;
    u_long                          adatasize;
};

void*
build_address_map_entry (void* data)
{
    //unpack args
    struct build_map_data* pbmd = (struct build_map_data *) data;
    unordered_map<uint32_t, nodeid> *address_map = pbmd->paddress_map;   
    u_long adatasize = pbmd->adatasize;
    struct addrstruct *asEnd = (struct addrstruct*)((char *)addrspace + adatasize);
    struct addrstruct *asPtr = addrspace;

    while (asPtr < asEnd) 
    {
	(*address_map)[asPtr->addr] = asPtr->nid;
	++asPtr; 
    }

    return NULL;
}

static pthread_t
spawn_map_thread (unordered_map<uint32_t,nodeid>* paddress_map, u_long adatasize)
{
    pthread_t build_map_tid = 0;

    // Thread data
    build_map_data* bmd = new build_map_data;
    bmd->paddress_map = paddress_map;
    bmd->adatasize = adatasize;
    
    // Make low priority
    pthread_attr_t attr;
    pthread_attr_init (&attr);
    struct sched_param sp;
    sp.sched_priority = 19;
    if (pthread_attr_setschedparam(&attr, &sp) < 0) {
	fprintf (stderr, "pthread_attr_setschedparam failed, errno=%d\n", errno);
    }
    
    assert (pthread_create (&build_map_tid, NULL, build_address_map_entry, bmd) == 0);
    return build_map_tid;
}


static void 
mergelog_iter_depth(nodeid id,  set<nodeid> *nodeids, stacktype stack,
	      u_int32_t &bucket_cnt, uint32_t &bucket_stop)
{
    nodeids->insert(id);

    if (id < LOCAL_SOURCE_OFFSET) {
	//send local_source to previous
	if (!start_flag) 	    
	    PUT_QVALUE(id, outputq_hdr, outputq_buf, bucket_cnt, bucket_stop);
    }
    else if (id >= NODE_OFFSET){ 

	uint_fast32_t stack_depth = 0;
	stack[stack_depth++] = std::make_pair(id, depth); 
	uint32_t depth; 
	pair<nodeid, uint32_t> p; 
	node_t *entry;
	do { 
	    assert(stack_depth < STACK_SIZE); //assert we haven't overflowed 
	    p = stack[--stack_depth];
	    id = p.first;
	    depth = p.second;

	    entry = GET_NODE(mergelog_mapper, id);

	    if (entry->p1 && nodeids->insert(entry->p1).second) { 
		if (entry->p1 < LOCAL_SOURCE_OFFSET) { 
		    if (!start_flag)
			PUT_QVALUE(entry->p1, outputq_hdr, outputq_buf, bucket_cnt, bucket_stop);
		}
		else if (entry->p1 >= NODE_OFFSET && depth > 0)
		    stack[stack_depth++] = make_pair(entry->p1, depth - 1);
	    }
	    
	    if (entry->p2 && nodeids->insert(entry->p2).second) { 
		if (entry->p2 < LOCAL_SOURCE_OFFSET) { 
		    if (!start_flag)
			PUT_QVALUE(entry->p2, outputq_hdr, outputq_buf, bucket_cnt, bucket_stop);
		}
		else if (entry->p2 >= NODE_OFFSET && depth > 0 )
		    stack[stack_depth++] = make_pair(entry->p2, depth - 1);
	    }
	} while(stack_depth);
    }
}


struct output_par_data {
    pthread_t                tid;
    char*                    outputPtr;
    char*                    outStop;
    u_int                    stripes;
    u_int                    offset;
    stacktype*               stack;
    set<nodeid>*             nodeids;
};



static void*
do_outputs_stream (void* pdata) 
{
    // Unpack arguments
    struct output_par_data* opdata = (struct output_par_data *) pdata;
    char * outputPtr = (char *) opdata->outputPtr; 
    char * outStop = (char *) opdata->outStop; 
    u_int to_skip = opdata->offset; 
    int stripes = opdata->stripes;
    stacktype *stack = opdata->stack;
    set<nodeid> *nodeids = opdata->nodeids; 
    uint32_t wbucket_cnt = 0, wbucket_stop = 0;

    while (outputPtr < outStop)
    {
//	struct input_info *curr_ii = (struct input_info *)outputPtr; 
	outputPtr += sizeof(struct input_info);
	outputPtr += sizeof(u_long);
	u_int buf_size = *outputPtr; 
	outputPtr += sizeof(u_int);

	nodeid *buf = (nodeid *)outputPtr; 
	outputPtr += buf_size * sizeof(nodeid);  
	
	for (u_int i = to_skip; i < buf_size; i += stripes)
	{
	    if (buf[i] > 0)
	    {		
//		cerr << std::hex << "nodeid " << buf[i] << " " 
//		     << curr_ii->imm_info.instcount << endl; 

		mergelog_iter_depth(buf[i], nodeids, *stack, wbucket_cnt, wbucket_stop);  
	    }
	}
    }   

    bucket_term(outputq_hdr,outputq_buf,wbucket_cnt,wbucket_stop);
    return NULL;
}

static void
process_outputs (char* outputPtr, char* outputStop, void *(*do_outputs)(void *))
{
    struct output_par_data output_data[parallelize];
    int ocnt = 0;

    if (outputPtr != outputStop) {
	for (int i = 0; i < parallelize; i++) {
	    output_data[i].outputPtr = outputPtr;
	    output_data[i].outStop = outputStop;
	    output_data[i].stack = &stacks[i];
	    output_data[i].stripes = parallelize;
	    output_data[i].offset = i;
	    output_data[i].nodeids = new set<nodeid>();
	}
	ocnt = parallelize;
	
	for (int i = 0; i < ocnt-1; i++) {
	    long rc = pthread_create (&output_data[i].tid, NULL, do_outputs, &output_data[i]);
	    if (rc < 0) {
		fprintf (stderr, "Cannot create output thread, rc=%ld\n", rc);
		assert (0);
	    }
	}
	
	(*do_outputs)(&output_data[ocnt-1]);
	merge(nodeids, output_data[ocnt-1].nodeids); 
	delete output_data[ocnt-1].nodeids;
	
	for (int i = 0; i < ocnt-1; i++) {
	    long rc = pthread_join(output_data[i].tid, NULL);
	    if (rc < 0) fprintf (stderr, "Cannot join output thread, rc=%ld\n", rc); 

	    //merge the thread's is with mega is
	    merge(nodeids, output_data[i].nodeids);
	    delete output_data[i].nodeids;
	}
    }   
}
struct address_par_data {
    pthread_t                        tid;
    unordered_map<uint32_t,nodeid>*  paddress_map;
    stacktype*                       stack;
    set<nodeid>*                     nodeids;

#ifdef STATS
    int received;
    int unchanged;
#endif    
};

static void* 
do_addresses (void* pdata)
{
    uint32_t rbucket_cnt = 0, rbucket_stop = 0, wbucket_cnt = 0, wbucket_stop = 0;
    uint32_t input;
    
    // Unpack arguments
    struct address_par_data* apdata = (struct address_par_data *) pdata;
    unordered_map<uint32_t,nodeid>* paddress_map = apdata->paddress_map;
    stacktype* stack = apdata->stack;
    set<nodeid> *nodeids = apdata->nodeids;


    //statistics arguments
#ifdef STATS
    int received = 0;
    int unchanged = 0;
#endif
    // Now, process input queue of later epoch outputs
    while (1) {
	GET_QVALUE(input, inputq_hdr, inputq_buf, rbucket_cnt, rbucket_stop);

#ifdef STATS
	received++; 
#endif
	if (input == TERM_VAL) {
	    break;
	}

	auto iter = paddress_map->find(input);
	if (iter == paddress_map->end()) {
#ifdef STATS
	    unchanged++;
#endif
	    if (!start_flag) {
		// Not in this epoch - so pass through to next
		PUT_QVALUE(input,outputq_hdr,outputq_buf,wbucket_cnt,wbucket_stop);
	    }
	    else if (input != NULL_NODE){ 		
		nodeids->insert(input);
	    }
	} else {
	    //this is an adress changed by this epoch, we have to treat as endpoint
	    mergelog_iter_depth(iter->second, nodeids, *stack, wbucket_cnt, wbucket_stop);
	}
    }    
    bucket_term(outputq_hdr,outputq_buf,wbucket_cnt,wbucket_stop); // Flush partial bucket    

#ifdef STATS
    apdata->received = received;
    apdata->unchanged = unchanged;
#endif
    return NULL;
}



void process_addresses (unordered_map<uint32_t,nodeid>& address_map )
{
    struct address_par_data address_data[parallelize];
    int i;

    for (i = 0; i < parallelize; i++) {
	address_data[i].paddress_map = &address_map;
	address_data[i].stack = &stacks[i];
	address_data[i].nodeids = new set<nodeid>();
    }
    for (i = 0; i < parallelize - 1; ++i) { 
    	long rc = pthread_create (&address_data[i].tid, NULL, do_addresses, &address_data[i]);
	if (rc < 0) {
	    fprintf (stderr, "Cannot create address thread, rc=%ld\n", rc);
	    assert (0);
	}
    }

    do_addresses (&address_data[parallelize-1]);
    merge(nodeids, address_data[parallelize-1].nodeids);
    delete address_data[parallelize-1].nodeids;

#ifdef STATS
    areceived += address_data[parallelize-1].received;
    aunchanged += address_data[parallelize-1].unchanged;
#endif



    for (i = 0; i < parallelize-1; i++) {
	long rc = pthread_join(address_data[i].tid, NULL);
	if (rc < 0) fprintf (stderr, "Cannot join address thread, rc=%ld\n", rc); 
	merge(nodeids, address_data[i].nodeids);
	delete address_data[i].nodeids;

#ifdef STATS
	areceived += address_data[i].received;
	aunchanged += address_data[i].unchanged;
#endif

    }

    if (!start_flag) {
	// Put end-of-data sentinel in queue
	uint32_t wbucket_cnt = 0, wbucket_stop = 0;
	//why is this in here...? 
	PUT_QVALUE(TERM_VAL,outputq_hdr,outputq_buf,wbucket_cnt,wbucket_stop);
	bucket_term(outputq_hdr,outputq_buf,wbucket_cnt,wbucket_stop);
    }
}




// Process one epoch 
long bw_pass (const char* dirname, int port)
{
    int rc;
    u_long inputSize, outputSize, addrSize;
    unordered_map<uint32_t,nodeid> address_map;
    pthread_t build_map_tid; 
    int inFd, oFd, mFd, aFd; 
 
#ifdef STATS
    gettimeofday(&start_tv, NULL);
#endif

#ifdef TRACE
    trace_f << getpid() << ": dirname " << dirname<< " finish " << finish_flag 
	 << " start " << start_flag << endl;
#endif


    rc = setup_aggregation (dirname);
    if (rc < 0) return rc;
#ifdef TRACE
    trace_f << getpid() << ": reading inputs" << endl;
#endif
    rc = read_inputs(port, inputSize, outputSize,  addrSize, inFd, oFd, mFd, aFd); 
    if (rc < 0) return rc;

#ifdef STATS
    gettimeofday(&recv_done_tv, NULL);
#endif
    if (!finish_flag) build_map_tid = spawn_map_thread (&address_map, addrSize);

#ifdef TRACE
    trace_f << getpid() << ": preprocessed inputs" << endl;
#endif
    preprocessInput(inputBuf, inputSize); //could be in parallel thread

#ifdef STATS
    gettimeofday(&preprocess_input_done_tv, NULL);
#endif

    bucket_init();

    process_outputs ((char *)outputBuf, (char *)outputBuf + outputSize,  do_outputs_stream);
#ifdef TRACE
    trace_f << getpid() << ": processed outputs" << endl;
#endif


    if (!finish_flag) {    
	
	rc = pthread_join(build_map_tid, NULL);
	if (rc < 0) return rc;
#ifdef TRACE
	trace_f << getpid() << ": build_map finished " << address_map.size() << endl;
#endif
	process_addresses(address_map );
#ifdef TRACE
    trace_f << getpid() << ": processed addresses" << endl;
#endif
    } else if (!start_flag) {
	uint32_t write_cnt = 0, write_stop = 0;
	PUT_QVALUE(TERM_VAL,outputq_hdr,outputq_buf, write_cnt, write_stop);
	bucket_term (outputq_hdr,outputq_buf,write_cnt, write_stop);
    } 

#ifdef STATS
    gettimeofday(&finish_outputs_tv, NULL);
#endif
    
    finish_aggregation (dirname, (char*)outputBuf, (char *)outputBuf + outputSize);
#ifdef TRACE
    trace_f  << getpid() << ": finished aggregation" << endl;
#endif

#ifdef STATS
    gettimeofday (&end_tv, NULL);
    print_stats (dirname, mdatasize, outputSize, inputSize, addrSize);
#endif

    return 0;
}

void format ()
{
    fprintf (stderr, "format: parallel_flowback <dir> <taint port> [-iq input_queue_hdr input_queue] [-oq output_queue_hdr output_queue] [-par # of threads]\n");
    exit (0);
}

int main (int argc, char* argv[]) 
{
    char* input_queue_hdr = NULL;
    char* output_queue_hdr = NULL;
    char* input_queue = NULL;
    char* output_queue = NULL;

    if (argc < 3) format();

    for (int i = 3; i < argc; i++) {
	if (!strcmp (argv[i], "-par")) {
	    i++;
	    if (i < argc) {
		parallelize = atoi(argv[i]);
	    } else {
		format();
	    }
	} else if (!strcmp (argv[i], "-iq")) {
	    i++;
	    if (i < argc) {
		input_queue_hdr = argv[i];
	    } else {
		format();
	    }
	    i++;
	    if (i < argc) {
		input_queue = argv[i];
	    } else {
		format();
	    }
	} else if (!strcmp (argv[i], "-oq")) {
	    i++;
	    if (i < argc) {
		output_queue_hdr = argv[i];
	    } else {
		format();
	    }
	    i++;
	    if (i < argc) {
		output_queue = argv[i];
	    } else {
		format();
	    }
	} else {
	    format();
	}
    }


    if (input_queue) {
	int iqhdrfd = shm_open (input_queue_hdr, O_RDWR, 0);
	if (iqhdrfd < 0) {
	    fprintf (stderr, "Cannot open input queue header %s, errno=%d\n", input_queue_hdr, errno);
	    return -1;
	}
	iqfd = shm_open (input_queue, O_RDWR, 0);
	if (iqfd < 0) {
	    fprintf (stderr, "Cannot open input queue %s, errno=%d\n", input_queue, errno);
	    return -1;
	}
	inputq_hdr = (struct taintq_hdr *) mmap (NULL, TAINTQHDRSIZE, PROT_READ|PROT_WRITE, MAP_SHARED, iqhdrfd, 0);
	if (inputq_hdr == MAP_FAILED) {
	    fprintf (stderr, "Cannot map input queue header, errno=%d\n", errno);
	    return -1;
	}
	inputq_buf = (uint32_t *) mmap (NULL, TAINTQSIZE, PROT_READ|PROT_WRITE, MAP_SHARED, iqfd, 0);
	if (inputq_buf == MAP_FAILED) {
	    fprintf (stderr, "Cannot map input queue, errno=%d\n", errno);
	    return -1;
	}
	finish_flag = false;
    } else {
	inputq_hdr = NULL;
	inputq_buf = NULL;
	finish_flag = true;
    }

    if (output_queue) {
	int oqhdrfd = shm_open (output_queue_hdr, O_RDWR, 0);
	if (oqhdrfd < 0) {
	    fprintf (stderr, "Cannot open output queue header %s, errno=%d\n", output_queue_hdr, errno);
	    return -1;
	}
	oqfd = shm_open (output_queue, O_RDWR, 0);
	if (oqfd < 0) {
	    fprintf (stderr, "Cannot open output queue %s, errno=%d\n", output_queue, errno);
	    return -1;
	}
	outputq_hdr = (struct taintq_hdr *) mmap (NULL, TAINTQHDRSIZE, PROT_READ|PROT_WRITE, MAP_SHARED, oqhdrfd, 0);
	if (outputq_hdr == MAP_FAILED) {
	    fprintf (stderr, "Cannot map output queue header, errno=%d\n", errno);
	    return -1;
	}
	outputq_buf = (uint32_t *) mmap (NULL, TAINTQSIZE, PROT_READ|PROT_WRITE, MAP_SHARED, oqfd, 0);
	if (outputq_buf == MAP_FAILED) {
	    fprintf (stderr, "Cannot map input queue, errno=%d\n", errno);
	    return -1;
	}
	start_flag = false;
    } else {
	outputq_hdr = NULL;
	outputq_buf = NULL;
	start_flag = true;
    }

    bw_pass (argv[1], atoi(argv[2]));
    return 0;
}
