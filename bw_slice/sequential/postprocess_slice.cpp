#define _LARGEFILE64_SOURCE
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <assert.h>
#include <fcntl.h>
#include <glib-2.0/glib.h>

#include <iostream>
#include <fstream>
#include <unordered_map> 
#include <unordered_set>
#include <queue> 

#include<google/profiler.h>

#include "../graph_interface/graph_interface.h"
#include "../graph_interface/node.h"
#include "../bw_slice.h" 
#include "../maputil.h"

#include "slice.h"
#include "input_set.h"
#include "merge_mapper.h"
#include "util.h"

//#define DEBUG(a) (a == 0x862a01e)
//#define DEBUG_NODEID(a) (a == 0xebe38fed || a == 0xebe38fee)


using namespace std;
unordered_map<nodeid, struct input_info*> inputMap; 
unordered_set<nodeid> diftNodes; 
unordered_set<nodeid> sliceInputs; 
IS::InputSet addrs; 
unordered_set<u_long> dynamicIds; 

//set of all nodeIds that are walked
unordered_set<nodeid> finished; 

static void addAddr(nodeid id) 
{
    IS::Location l(id);
    addrs.addLocation(l);
}

static void addInput(nodeid id, 
		     struct input_info *inputBuf, u_long inputSize, 
		     S::Slice  &s)
{

    if (diftNodes.size() != 0 && !diftNodes.count(id)) 
    {
	cerr << std::hex << id << " input for postprocess... not in dift \n";
	assert(0);
    }
    sliceInputs.insert(id); 

    auto element = inputMap.find(id);
    assert(element != inputMap.end());
	
    struct input_info *entry = element->second;    

//    cerr << "fist input is " << id << " ip " << entry->imm_info.ip << endl;
//    assert(0);
#ifdef DEBUG    
    if (DEBUG(entry->imm_info.ip))
	cerr << hex << "found the ip from input " << id << endl;

    if (DEBUG_NODEID(id))
	cerr << hex << "found the ip from input " << id << endl;

#endif
    S::Location l(entry->imm_info.ip); //doesn't work for syscall inputs...
    s.addLocation(l);
}

static void populate_dift(char *dift_loc)
{
    
    ifstream ifs(dift_loc);
    IS::InputSet is; 
    
    is.deserialize(ifs);
    for (auto l : is._locations) 
    {
	diftNodes.insert(l._token_num);
	
    }
    assert(diftNodes.size() > 0);
    cerr << "checking against dift results\n";
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

static void addNode (u_long ip, S::Slice &s)
{       
    S::Location l(ip);   
    s.addLocation(l);
}

static void addDynamicId (u_long dyn_id) 
{
    dynamicIds.insert(dyn_id);
}

static void addSliceCriteria(u_long ip, 
			     u_long sliceLoc, 
			     u_long lastModified, 
			     S::Slice &s)
{
    S::SliceCriteria sc(ip, sliceLoc, lastModified);
    s.addSliceCriteria(sc);
}



static void bfs_walk(nodeid id, MergeMapper &mm, 
		     struct input_info *inputLog, u_long inputSize, 
		     S::Slice &slice, 
		     unordered_set<nodeid> &walked)
{

    priority_queue<nodeid> pq; 
    pq.push(id);
    while (! pq.empty() ) 
    {
	nodeid nxtId = pq.top();
	pq.pop();
	if (nxtId < LOCAL_SOURCE_OFFSET) { 
	    addAddr(nxtId);
	}
	else if (nxtId < NODE_OFFSET) { 
	    addInput(nxtId, inputLog, inputSize, slice); 
	}
	else {
	    node_t *entry = mm.getNode(nxtId);
#ifdef DEBUG	    
	    if (DEBUG(entry->ip))
	    {
		cerr << hex << "found " << entry->ip << " from node " << nxtId
		     << " dynamicid " << entry->instcount << endl;
	    }
	    if (DEBUG_NODEID(nxtId))
		cerr << hex << "walking " << nxtId
		     << " dynamicid " << entry->instcount << endl;
#endif

	    addNode (entry->ip, slice);
	    addDynamicId(entry->instcount);

//	    cerr << std::hex << nxtId << ":" << *entry << endl;

	    if (entry->p1 && entry->p1 < NODE_OFFSET) 
		pq.push(entry->p1);
	    else if (entry->p1 && 
		walked.insert(entry->p1).second) 
	    {
#ifdef DEBUG	    
		if (DEBUG_NODEID(entry->p1))
		    cerr << hex << "walking " << nxtId
			 << " added nodeid " << entry->p1 << endl;
#endif

		pq.push(entry->p1);
	    }

	    if (entry->p2 && entry->p2 < NODE_OFFSET) 
		pq.push(entry->p2);

	    else if (entry->p2 && 
		     walked.insert(entry->p2).second)
	    {
#ifdef DEBUG	    
		if (DEBUG_NODEID(entry->p2))
		    cerr << hex << "walking " << nxtId
			 << " added nodeid " << entry->p2 << endl;
#endif

		pq.push(entry->p2);
	    }
	}
    }
}

int read_next_output(int fd, struct input_info *ii, u_long *buf, int *buf_size) 
{ 
    int r, r2, r3;
    r = read(fd, ii, sizeof(struct input_info));
    if (r != sizeof(struct input_info)) { 
	assert(0);
	return -1;
    }   
    
    r2 = read(fd, buf, sizeof(u_long)); 
    if (r2 != sizeof(u_long)){ 
	assert(0);
    }   


    r3 = read(fd, buf_size, sizeof(int)); 
    if (r3 != sizeof(int)){ 
	assert(0);
    }   
    
    return r + r2 + r3;
}


int read_merge (char* group_dir, char* pid, S::Slice &s)
{
    char mergelog_filename[256], input_filename[256], results_filename[256];
    
    int output_fd, rc, bytes_read = 0;
    u_long inputSize = 0, inputMapSize = 0;
    int inFd; 
    
    struct stat output_stat;
    struct input_info *inputBuf = NULL;

    /*
     * If we are running the code for a specific pid than the output and the 
     * dataflow.results filenames are different. However, the shared memory 
     * needs to be handled the same regardless as to whether a pid is
     * specified.
     */

    if(pid == NULL) 
    {
	snprintf(results_filename, 256, "%s/outputs", group_dir);
	snprintf(mergelog_filename, 256, "%s/node_nums", group_dir);
	snprintf(input_filename, 256, "%s/inputs", group_dir);
    }

    MergeMapper m(mergelog_filename);
    rc = map_file(input_filename, &inFd, &inputSize, &inputMapSize, (char **) &inputBuf);
    assert(!rc);
    preprocessInput(inputBuf, inputSize);


    //first things first, iterate through outputs
    struct input_info curr_ii;
    output_fd  = open(results_filename, O_RDONLY);
    if (output_fd < 0) { 
	fprintf(stderr, "could not open results file: %s, errno %d\n",
		results_filename, errno);
	return errno;
    }

    rc = fstat(output_fd, &output_stat);
    if (rc) {
	fprintf(stderr, "could not stat results file, errno %d\n", errno);
	return errno;
    }

    while (bytes_read < output_stat.st_size) 
    { 
	int buf_size;
	nodeid *buf; 
	u_long bufaddr;
	int count = read_next_output(output_fd, &curr_ii, &bufaddr, &buf_size); 
	bytes_read += count;
	
	printf ("count %d, sizeof ii %d, buf_size %d, sizeof nodeid %d\n",count, 
		sizeof(struct input_info), buf_size, sizeof(nodeid));

	printf("rp %d, rgid %llu, syscall %d, ", curr_ii.record_pid, 
	       curr_ii.rg_id, curr_ii.syscall_cnt);
       	
	printf ("type: IMM: (0x%x)\n",curr_ii.imm_info.ip);

	buf = new nodeid[buf_size];
	count = read(output_fd, buf, buf_size * sizeof(nodeid));

	ProfilerStart("postprocess_slice.out");
	for (int i = 0; i < buf_size; ++i) 
	{
	    cout << std::hex << "nodeid " << buf[i] << endl;
	    u_long lastModified = 0;
	    if (buf[i] > 0) 
	    {
		assert( (buf[i] >= NODE_OFFSET));
/*		{
		    struct token *e = find_input(buf[i], inputBuf, inputSize);
		    assert(e);
		    lastModified = (e->ip);
		}
		else 
		{
*/
		    lastModified = m.getNode(buf[i])->ip;
//		}
		
		addSliceCriteria(curr_ii.imm_info.ip,
				 bufaddr + i,
				 lastModified,
				 s);
	    
		bfs_walk(buf[i],  m, inputBuf, inputSize,  s, finished);
	    }
	}
	ProfilerStop();
	cerr << "numSearches " << m.numSearches << " " << m.numMaps << endl;
	delete[] buf;
	bytes_read += count;      
    }   

    

    return 0;
}

void print_slice_stats(S::Slice s, 
		       char *group_dir,
		       ofstream &outstream)
{


    char mergelog_filename[256];
    snprintf(mergelog_filename, 256, "%s/node_nums", group_dir);   
    MergeMapper m(mergelog_filename);

    uint64_t dynamic_instructions = 0;
    unordered_set<u_long> static_ips;
    nodeid id = NODE_OFFSET;

    while (id < m.numNodes + NODE_OFFSET)
    {	
	node_t *n = m.getNode(id);
	if (n->instcount > dynamic_instructions) 
	{
	    dynamic_instructions = n->instcount;
	}
	static_ips.insert(n->ip);

	id += 1;
    }


    outstream << std::dec << "walked " << finished.size() << " out of " << m.numNodes << endl;    
    outstream << "contains " << s._nodes.size() << " out of " << static_ips.size() << " static instructions" << endl;
    outstream << " out of " << dynamic_instructions<< " dynamic instructions" << endl;   
}


int main(int argc, char** argv)
{
    char *group_dir = NULL, *pid = NULL,
	*dift_loc = NULL, opt;

    ofstream outstream;    

    while (1) 
    {
	opt = getopt(argc, argv, "m:o:d:");

	if (opt == -1) 
	{
	    break;
	}
	switch(opt) 
	{
	case 'm':
	    group_dir = optarg;
	    break;
	case 'd': 
	    dift_loc = optarg; 
	    break;
	default:
	    fprintf(stderr, "Unrecognized option\n");
	    break;
	}
    }
    if (dift_loc != NULL) 
    {
	populate_dift(dift_loc);
    }
    if(group_dir != NULL)
    {

	fprintf(stderr, "group %s\n", group_dir);
	S::Slice s;
	read_merge(group_dir, pid, s);


	char filename[256];
	sprintf (filename, "%s/slice", group_dir);
	outstream.open(filename); 
	outstream << s; 
	outstream.close(); 

	sprintf (filename, "%s/starting_as", group_dir);
	outstream.open(filename); 
	outstream << addrs;
	outstream.close(); 	


	sprintf (filename, "%s/slice_stats", group_dir);
	outstream.open(filename); 
	print_slice_stats(s, group_dir, outstream);
	outstream.close(); 	

	
	if (diftNodes.size() > 0) 
	{
	    for (auto &l : diftNodes) 
	    {
		if (! sliceInputs.count(l))
		{
		    cerr << std::hex << l << " input for dift... not in slice \n";
		}
	    }
	}

    }

    return 0;
}

