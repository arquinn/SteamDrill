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

#include "../graph_interface/graph_interface.h"
#include "../bw_slice.h"
#include "../maputil.h"

#include "flowback.h"
#include "merge_mapper.h"

//#define DEBUGTRACE_OUTPUT 0x47df9c 
#ifdef DEBUGTRACE_OUTPUT 
int debug_me = 0;
#endif

#define DETAILS 1

long nxt_out = -1 ;

using namespace std;

static struct token* find_input(nodeid value, 
				struct token *tokenlog, u_long tokensize) 
{
    struct token *entry = tokenlog;
    

    while ((char *)entry < (char *)tokenlog + tokensize) 
    {	
	if (entry->token_num <= value && 
			       (entry->token_num + entry->size) > value)

	{	    
	    return entry;
	}
	entry++;
    }
    
    cerr << std::hex << "\tcouldn't find input " << value << endl;
    cerr << (entry - 1)->token_num <<  " " << (entry - 1)->token_num + (entry-1)->size << endl;
    return NULL;
}
static Location addInputNode (u_long ip, Flowback &fb)
{
    Location l(ip, nxt_out--);
//    cerr << std::hex << l._dynamic_id << "is an input\n";
    l = fb.addLocation(l);
    return l;

}

static Location addNode (u_long ip, u_long dynamicid, Flowback &fb)
{       
    Location l(ip, dynamicid);
//    cerr << std::hex << l._dynamic_id << "is a node\n";
    l = fb.addLocation(l);
    return l;
}

static SliceCriteria addSliceCriteria(u_long ip, 
			     u_long sliceLoc, 
			     u_long lastModified, 
			     Flowback &fb)
{
    SliceCriteria sc(ip, nxt_out--, sliceLoc, lastModified);
    sc = fb.addSliceCriteria(sc);
    return sc;
}

static void addLink(Location parent, Location child, Flowback &fb)
{
    fb.addLink(parent,child);
    cout << "adding link from  " << child << " to " << parent << endl;
}


static void dfs_walk(nodeid id, Location parent, MergeMapper &mm, 
		     struct token *tokenlog, u_long tokensize, 
		     Flowback &fb, unordered_set<nodeid> &walked,
		     int currDepth, int maxDepth)
{

#ifdef  DETAILS
    cout << "dfs_walk "<< id; 
#endif
    if (id == 0) { 
	cout << endl;
	return;
    }
    if (currDepth >= maxDepth) 
    {
	cout << endl;
	return;
    }
    if (walked.count(id)) {
	cout << endl;
	return; //already finished
   }	
    else if (id < NODE_OFFSET) { 
	struct token *e = find_input(id, tokenlog, tokensize);

//	cerr << "input" << e->ip << e->record_pid << "," << e->token_num <<  "," << 
//	    e->token_num + e->size << endl;

	cout << " leads to input " << e->ip << endl;
	assert(e);

	Location in = addInputNode (e->ip, fb);
	if (!parent.isNull()) { 
	    addLink (parent, in, fb); 
	}
	walked.insert(id);
    }

    else {
	//add the current entry's ip
	node_t *entry = mm.getNode(id);
	nodeid p1 = entry->p1, p2 = entry->p2; 
	int newDepth = currDepth;

	Location l = addNode (entry->ip, entry->instcount, fb);
	walked.insert(id);

	//if we point to ourself, still continue traversing, but ignore the link
	if (!(l == parent) && !parent.isNull()) { 
	    addLink (parent, l, fb);
	    newDepth += 1; 
	}
	
#ifdef DETAILS
	cout << " leads to (" << entry->p1 << ", " << entry->p2 << "), ip " << entry->ip << endl;
#endif
	
	//recursive step
	if (p1) {
	    dfs_walk(p1, l, mm, tokenlog, tokensize, fb, walked, newDepth, maxDepth);
	}
	if (p2) {
	    dfs_walk(p2, l, mm, tokenlog, tokensize, fb, walked, newDepth, maxDepth);
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


int read_merge (char* group_dir, char* pid, int max_depth, Flowback &fb)
{
    char mergelog_filename[256], input_filename[256], results_filename[256];
    
    int output_fd, rc, bytes_read = 0;
    u_long inputSize = 0, inputMapSize = 0;
    int inFd; 
    
    struct stat output_stat;
    struct token *inputBuf = NULL;

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

    MergeMapper mm(mergelog_filename);
    rc = map_file(input_filename, &inFd, &inputSize, &inputMapSize, (char **) &inputBuf);
    assert(!rc);

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

    unordered_set<nodeid> finished; 

    while (bytes_read < output_stat.st_size) 
    { 
	int buf_size;
	nodeid buf[1024]; //should be way bigger than buf_size... sometimes
	u_long bufaddr;
	int count = read_next_output(output_fd, &curr_ii, &bufaddr, &buf_size); 
	bytes_read += count;
	
	printf ("count %d, sizeof ii %d, buf_size %d, sizeof nodeid %d\n",count, 
		sizeof(struct input_info), buf_size, sizeof(nodeid));

	printf("rp %d, rgid %llu, syscall %d, ", curr_ii.record_pid, 
	       curr_ii.rg_id, curr_ii.syscall_cnt);
       	
	switch(curr_ii.type) {
	case input_info::SYSCALL:

	    printf ("type: SYSCALL: (%d,%d)\n",curr_ii.syscall_info.offset, 
		    curr_ii.syscall_info.fileno);
	    break;
	case input_info::IMM:
	    printf ("type: IMM: (0x%lx)\n",curr_ii.imm_info.ip);
	    break;
	}

	count = read(output_fd, buf, buf_size * sizeof(nodeid));

	for (int i = 0; i < buf_size; ++i) 
	{
	    cout << std::hex << "nodeid " << buf[i] << endl;
	    u_long lastModified = 0;
	    Location l(0,0);
	    if (buf[i] > 0) 
	    {
		if (buf[i] < NODE_OFFSET)
		{
		    struct token *e = find_input(buf[i], inputBuf, inputSize);
		    assert(e);
		    lastModified = (e->ip);
		    l = addInputNode(e->ip, fb); 
		}
		else 
		{
		    node_t *e = mm.getNode(buf[i]);
		    lastModified = e->ip;
		    
		    l = addNode(e->ip, e->instcount,fb); 
		}
		
		SliceCriteria sc = addSliceCriteria(curr_ii.imm_info.ip,
						    bufaddr + i,
						    lastModified, fb);

		Location parent(sc._ip, sc._dynamic_id);
		addLink(parent,l,fb);
		
		Location n(0,0);
		dfs_walk(buf[i], n, mm, inputBuf, inputSize,  fb, finished, 0, max_depth);
	    }
	}
	bytes_read += count;      
    }   
    return 0;
}

int main(int argc, char** argv)
{
    char *group_dir = NULL, *pid = NULL, *slice_loc = NULL, opt;
    int depth; 
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
	case 'o':
	    slice_loc = optarg;
	    break;
	case 'd': 
	    depth = atoi(optarg);
	    break;
	default:
	    fprintf(stderr, "Unrecognized option\n");
	    break;
	}
    }
    if(group_dir != NULL)
    {

	fprintf(stderr, "group %s, slice %s, depth %d\n", group_dir, slice_loc, depth);
	Flowback fb; 
	read_merge(group_dir, pid, depth, fb);

	if (slice_loc) { 
	    string st(slice_loc);
	    outstream.open(st);
	    outstream << fb; 
	    outstream.close();
	}
    }
    return -1;
}

