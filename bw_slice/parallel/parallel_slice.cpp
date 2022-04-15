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
#include <unordered_set> 
#include <set> 

using namespace std;

#include "../graph_interface/node.h"
#include "../graph_interface/input_output.h"
#include "merge_mapper.h"
#include "../maputil.h"

#include "streamnw.h"
// Globals -  here for performance/convenience

#define STATS
#define TRACE

#ifdef TRACE
ofstream trace_f;
#endif

//#define DEBUG_ID(a) (a == 0xe01349dd)
//#define DEBUG_INSTCOUNT(a) (a == 0xe1481)
//#define DEBUG_ADDR(a) (a == 0xa1dd71c)

#define STACK_SIZE 1000000
typedef nodeid stacktype[STACK_SIZE];
stacktype *stacks;

//buffers
MergeMapper *mergelog_mapper; 
unordered_set<nodeid> *nodeids;
unordered_set<nodeid> *outputs; 
//unordered_set<nodeid> *dynamicids;
nodeid max_dynamicid = 0;

struct addrstruct *addrspace;
struct input_info  *outputBuf = NULL,  *inputBuf = NULL;


u_char              parallelize = 1;
bool                start_flag = false;
bool                finish_flag = false;


#define GET_NODE(ml,id) ml->getNode(id)    

static void mergelog_iter_depth(nodeid id, stacktype stack, set<nodeid> *nodeids, int depth);


#ifdef STATS
struct timeval start_tv = {0,0}, end_tv = {0,0};
struct timeval recv_done_tv = {0,0}, preprocess_input_done_tv = {0,0};
struct timeval finish_outputs_tv = {0,0};

static long ms_diff (struct timeval tv1, struct timeval tv2)
{
    return ((tv1.tv_sec - tv2.tv_sec) * 1000 + (tv1.tv_usec - tv2.tv_usec) / 1000);
}

void print_stats (const char* dirname)
{
    char statsname[256];
    sprintf (statsname, "%s/slice-stats", dirname);
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

    stats.close();
}
#endif
 
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
read_inputs (int port, u_long &idatasize, u_long &odatasize, u_long& mdatasize, u_long& adatasize,
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
    trace_f << dec << getpid() << hex << " merge: " << mergelog_mapper->numNodes << endl;
    trace_f << dec << getpid() << hex << " addrs: " << adatasize << endl;
#endif

    return 0;
}

static long 
setup_aggregation (const char *dirname)
{
    stacks = new stacktype[parallelize];
    nodeids = new unordered_set<nodeid>(); 
    outputs = new unordered_set<nodeid>(); 
//    dynamicids = new unordered_set<nodeid>(); 
    long rc = mkdir(dirname, 0755);
    if (rc < 0 && errno != EEXIST) {
	fprintf (stderr, "Cannot create output dir %s, errno=%d\n", dirname, errno);
	return rc;
    }

#ifdef TRACE
    char filename[256];
    sprintf(filename, "%s/slice.trace",dirname);
    trace_f.open(filename);
#endif

    return 0;
}


static long
finish_aggregation (const char *dirname)
{
    cerr << "nodeids size " << nodeids->size() << endl;
    cerr << "percent " 
	 << (double) nodeids->size() / (double) mergelog_mapper->numNodes
	 << endl;

    ofstream ostream("%sips",dirname);
    //calculate the ips of 'em all
    for (auto nid : nodeids) 
    {
	node_t *entry = GET_NODE(mergelog_mapper, nid); 
	ostream << entry->ip << endl;
    }
    ostream.close();


    return 0;
}

static void 
merge_set(unordered_set<nodeid> *s, 
	  unordered_set<nodeid> *base)
{
    for (auto &l : *s) 
    {
	base->insert(l);
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
mergelog_iter(nodeid id, stacktype stack, 
	      unordered_set<nodeid> *nodeids)
{

    cerr << hex << "mergelog_iter called with " << id << endl;
    nodeids->insert(id); 


    if (id >= NODE_OFFSET) {
	node_t *entry; 
	uint_fast32_t stack_depth = 0;
	stack[stack_depth++] = id; 

	do { 
	    assert(stack_depth < STACK_SIZE); //assert we haven't overflowed 
	    id = stack[--stack_depth];	   	    
	    entry = GET_NODE(mergelog_mapper,id); 
	    
	    if (entry->p1 >= NODE_OFFSET && 		
		nodeids->insert(entry->p1).second)

		stack[stack_depth++] = entry->p1; 
	    
	    if (entry->p2 >= NODE_OFFSET && 
		nodeids->insert(entry->p2).second)
		
		stack[stack_depth++] = entry->p2;
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
    unordered_set<nodeid> *  nodeids;
    unordered_set<nodeid> *  dynamicids;   
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
    unordered_set<nodeid> *nodeids = opdata->nodeids; 
//    unordered_set<nodeid> *dynamicids = opdata->dynamicids; 

    while (outputPtr < outStop)
    { 
	struct input_info *curr_ii = outputPtr; 
	outputPtr += sizeof(struct input_info);
	outputPtr += sizeof(u_long);

	u_int buf_size = *outputPtr; 
	outputPtr += sizeof(u_int);

	nodeid *buf = (nodeid *)outputPtr; 
	outputPtr += buf_size * sizeof(nodeid);  
	
	for (u_int i = to_skip; i < buf_size; i += stripes)
	{
#ifdef TRACE	    
	    trace_f << std::hex << "nodeid " << buf[i] << endl;
#endif
	    if (buf[i] == 0xe160753b) 
	    {		
		outputs->insert(buf[i]);
		mergelog_iter(buf[i], *stack, nodeids);//dynamicids);
	    }
	}
    }   

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
	    output_data[i].nodeids = new unordered_set<nodeid>(); 
	    output_data[i].dynamicids = new unordered_set<nodeid>(); 
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
	merge_set(output_data[ocnt-1].nodeids, nodeids);
//	merge_set(output_data[ocnt-1].dynamicids, dynamicids);
	delete output_data[ocnt-1].nodeids;
	delete output_data[ocnt-1].dynamicids;

	for (int i = 0; i < ocnt-1; i++) {
	    long rc = pthread_join(output_data[i].tid, NULL);
	    if (rc < 0) fprintf (stderr, "Cannot join output thread, rc=%ld\n", rc); 
	    merge_set(output_data[i].nodeids, nodeids);
//	    merge_set(output_data[i].dynamicids,dynamicids);
	    delete output_data[i].nodeids;
	    delete output_data[i].dynamicids;
	}
    }   
}
/*
struct address_par_data {
    pthread_t                        tid;
    unordered_map<uint32_t,nodeid>*  paddress_map;
    stacktype*                       stack;
};


static void* 
do_addresses (void* pdata)
{
    unordered_set<nodeid> *seen = new unordered_set<nodeid>;    
    
    // Unpack arguments
    struct address_par_data* apdata = (struct address_par_data *) pdata;
    unordered_map<uint32_t,nodeid>* paddress_map = apdata->paddress_map;
    stacktype* stack = apdata->stack;
    //iterate through the paddresses
    for (auto addr: *paddress_map)
    {
	mergelog_iter(addr.second,  *stack,seen);
    }
    delete seen;
    return NULL;
}



void process_addresses (unordered_map<uint32_t,nodeid>& address_map)
{    
    struct address_par_data address_data; 

    address_data.paddress_map = &address_map;
    address_data.stack = &stacks[0];   
    address_data.flowback = new Flowback();

    do_addresses (&address_data);    

    flowback->merge(*(address_data.flowback));
    delete address_data.flowback;
}
*/

// Process one epoch 
long bw_pass (const char* dirname, int port)
{
    int rc;
    u_long inputSize, outputSize, mergeSize, addrSize;
    unordered_map<uint32_t,nodeid> address_map;
    pthread_t build_map_tid; 
    int inFd, oFd, mFd, aFd; 
 
#ifdef STATS
    gettimeofday(&start_tv, NULL);
#endif

#ifdef TRACE
    trace_f << "pid " << getpid() << " dirname " << dirname
	 << " finish " << finish_flag << " start " << start_flag << endl;
#endif

    rc = setup_aggregation (dirname);
    if (rc < 0) return rc;
#ifdef TRACE
    trace_f << "pid " << getpid() << " reading inputs" << endl;
#endif
    rc = read_inputs(port, inputSize, outputSize, mergeSize, addrSize, inFd, oFd, mFd, aFd); 
    if (rc < 0) return rc;

#ifdef STATS
    gettimeofday(&recv_done_tv, NULL);
#endif

    //not needed if finish_flag
    if (!finish_flag) build_map_tid = spawn_map_thread (&address_map, addrSize);


#ifdef TRACE
    trace_f << "pid " << getpid() << " preprocessed inputs" << endl;
#endif
#ifdef STATS
    gettimeofday(&preprocess_input_done_tv, NULL);
#endif

    process_outputs ((char *)outputBuf, (char *)outputBuf + outputSize,  do_outputs_stream);
#ifdef TRACE
    trace_f << "pid " << getpid() << " processed output" << endl;
#endif

    if (!finish_flag) {    	
	rc = pthread_join(build_map_tid, NULL);
	if (rc < 0) return rc;
#ifdef TRACE
	trace_f << "pid " << getpid() << " build map finished " << address_map.size() << endl;
#endif
//	process_addresses(address_map);
#ifdef TRACE
    trace_f << "pid " << getpid() << " processed addresses" << endl;
#endif
    } 

#ifdef STATS
    gettimeofday(&finish_outputs_tv, NULL);
#endif

    finish_aggregation (dirname);
#ifdef TRACE
    trace_f << "pid " << getpid() << "finished aggregation" << endl;
#endif

#ifdef STATS
    gettimeofday (&end_tv, NULL);
    print_stats (dirname);
#endif

    return 0;
}

void format ()
{
    fprintf (stderr, "format: stream <dir> <taint port> [-par # of threads]\n");
    exit (0);
}

int main (int argc, char* argv[]) 
{
    if (argc < 3) format();
    
    cerr << "starting parallel_slice " << getpid() << endl;
    for (int i = 3; i < argc; i++) {
	if (!strcmp (argv[i], "-par")) {
	    i++;
	    if (i < argc) {
		parallelize = atoi(argv[i]);
	    } else {
		format();
	    }
	} else if (!strcmp (argv[i], "-s")) {
	    start_flag = true;
	} else if (!strcmp (argv[i], "-f")) {
	    finish_flag = true;
	} else {
	    format();
	}
    }


    bw_pass (argv[1], atoi(argv[2]));
    return 0;
}
