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

#include <google/profiler.h>

#include "../graph_interface/graph_interface.h"
#include "../graph_interface/node.h"
#include "../bw_slice.h"
#include "../maputil.h"

#include "input_set.h"
//#define DETAILS 1


using namespace std;
unordered_map<nodeid, struct input_info*> inputMap; 

static void addInput(nodeid id, 
		     struct input_info *inputBuf, u_long inputSize, 
		     IS::InputSet &is)
{
    auto element = inputMap.find(id);
    assert(element != inputMap.end());
	
    struct input_info *entry = element->second;
    nodeid adjusted_id = id - LOCAL_SOURCE_OFFSET;


    IS::Location l(entry->type, entry->record_pid, adjusted_id,
	       entry->imm_info.ip, entry->syscall_info.fileno);
    
    is.addLocation(l);    
}

static void addAddr(nodeid id, IS::InputSet &is) 
{
    IS::Location l(id); 
    is.addLocation(l);
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


int read_next_output(int fd, struct input_info *ii, u_long *buf, u_int *buf_size) 
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

    
    r3 = read(fd, buf_size, sizeof(buf_size)); 
    if (r3 != sizeof(buf_size)){ 
	assert(0);
    }   
    
    return r + r2 + r3;
}

static void bfs_walk(nodeid id, struct node *mergeLog, 
		     struct input_info *inputLog, u_long inputSize, 
		     IS::InputSet &is, IS::InputSet &addrs, 
		     unordered_set<nodeid> &walked)
{
    queue<nodeid> pq;
    pq.push(id);
    while ( !pq.empty() )
    {
	nodeid nxtId = pq.front();
	pq.pop();
	
	if (nxtId < LOCAL_SOURCE_OFFSET) 
	{
	    addAddr(nxtId, addrs); 
	}
	else if (nxtId < NODE_OFFSET) 
	{ 
	    addInput(nxtId, inputLog, inputSize, is);
	}
	else
	{
	    node_t *entry = mergeLog + (nxtId - NODE_OFFSET); //entry
	    if (entry->p1 && 
		walked.insert(entry->p1).second) 
	    {
		pq.push(entry->p1);
	    }
	    if (entry->p2 && 
		walked.insert(entry->p2).second) 
	    {
		pq.push(entry->p2);
	    }
	}
    }
}

int32_t
read_inputs (char * group_directory, 
	     struct input_info** token_log, struct input_info** output_log, node_t** merge_log,
	     u_long *idatasize, u_long *odatasize, u_long* mdatasize,
	     int* ifd, int* ofd, int* mfd)
{
    int rc;
    u_long mapsize; 
    char buf[256];

    sprintf(buf, "%s/inputs",group_directory);
    rc = map_file(buf, ifd, idatasize, &mapsize, (char **)token_log);
    assert (!rc);

    sprintf(buf, "%s/outputs",group_directory);
    rc = map_file(buf, ofd, odatasize, &mapsize, (char **)output_log);
    assert (!rc);

    sprintf(buf, "%s/node_nums",group_directory);
    rc = map_file(buf, mfd, mdatasize, &mapsize, (char **)merge_log);
    assert (!rc);

    return 0;
}


int read_merge (char* group_dir, char* pid, IS::InputSet &is, IS::InputSet &addrs)
{
    int rc;
    u_long inputSize, outputSize, mergeSize;
    int inFd, oFd, mFd; 
 
    //buffers
    struct input_info *inputBuf = NULL, *outputBuf = NULL;
    node_t *mergeBuf = NULL;

    rc = read_inputs(group_dir, &inputBuf, &outputBuf, &mergeBuf, &inputSize, &outputSize, &mergeSize, &inFd, &oFd, &mFd ); 
    preprocessInput(inputBuf, inputSize);
    assert(!rc);

    //iterate through outputs
    char * outputPtr = (char *) outputBuf; 
    unordered_set<nodeid> finished; 

    while (outputPtr < (char *)outputBuf + outputSize)
    { 
	struct input_info curr_ii = *outputBuf;
	outputPtr += sizeof(struct input_info);

	u_long bufaddr = *outputPtr;
	outputPtr += sizeof(u_long);

	u_int buf_size = *outputPtr; 
	outputPtr += sizeof(u_int);

	nodeid *buf = (nodeid *)outputPtr; 
	outputPtr += buf_size * sizeof(nodeid);  
	
	printf ("buf_size %u\n", buf_size);

	printf("rp %d, rgid %llu, syscall %d, ", curr_ii.record_pid, 
	       curr_ii.rg_id, curr_ii.syscall_cnt);
       	
	switch(curr_ii.type) {
	case IMM:
	    printf ("type: IMM: (0x%x)\n",curr_ii.imm_info.ip);
	    break;

	default:
	    printf ("type: SYSCALL: (%d)\n", curr_ii.syscall_info.fileno);
	    break;
	}

	ProfilerStart("postprocess_dift.out");
	for (u_int i = 0; i < buf_size; ++i) 
	{
	    cout << std::hex << "nodeid " << buf[i] << endl;
	    if (buf[i] > 0) 
	    {		
		IS::SliceCriteria sc(curr_ii.imm_info.ip, bufaddr +i);		
		is.addSliceCriteria(sc);	     
		bfs_walk(buf[i],  mergeBuf, inputBuf, inputSize,  is, addrs, finished);
	    }
	}
	ProfilerStop();
    }   

    return 0;
}


int main(int argc, char** argv)
{
    char *group_dir = NULL, *pid = NULL, opt;

    ofstream outstream;    

    while (1) 
    {
	opt = getopt(argc, argv, "m:o:");

	if (opt == -1) 
	{
	    break;
	}
	switch(opt) 
	{
	case 'm':
	    group_dir = optarg;
	    break;
	default:
	    fprintf(stderr, "Unrecognized option\n");
	    break;
	}
    }
    if(group_dir != NULL)
    {

	fprintf(stderr, "group %s\n", group_dir);
	IS::InputSet is;
	IS::InputSet addrs; 
	read_merge(group_dir, pid, is, addrs);
	
	//print out results
	char filename[256];
	sprintf (filename, "%s/dift", group_dir);
	outstream.open(filename); 
	outstream << is; 
	outstream.close(); 

	sprintf (filename, "%s/starting_as", group_dir);
	outstream.open(filename); 
	outstream << addrs;
	outstream.close(); 	
    }
    return 0;
}

