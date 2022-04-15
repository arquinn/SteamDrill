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

#include "../graph_interface/node.h"
#include "../graph_interface/graph_interface.h"
#include "merge_mapper.h"

//#define DEBUGTRACE_OUTPUT 0x47df9c 
#ifdef DEBUGTRACE_OUTPUT 
int debug_me = 0;
#endif

//#define DETAILS 1
using namespace std;

unordered_set<nodeid> trace_ids; //I actually don't think this can work.. we'll see

void add_nodeids(FILE *file)
{   
    while (!feof(file)) {
	char line[256];	
	if (fgets (line, 255, file)) {
	    trace_ids.insert(strtol(line, NULL, 16));
	}
    }    
}

int read_merge (char* group_dir)
{
    char mergelog_filename[256];
    
    int merge_fd; 
    uint64_t id = NODE_OFFSET; 
    node_t node, node2; 

    snprintf(mergelog_filename, 256, "%s/node_nums", group_dir);   
    merge_fd = open(mergelog_filename, O_RDONLY | O_LARGEFILE);

    assert (merge_fd > 0 && merge_fd2 > 0);


    while (read(merge_fd, (void *)&node, sizeof(node_t))) 
    {

//	read(merge_fd2, (void *)&node2, sizeof(node_t))) 
	

	if (node.p1 != node2.p1 || node.p2 != node2.p2) 
	{
	    cout << std::hex << id << ":" << node << "," << node2 << endl;
	}

	assert(node.p1 == node2.p1 && node.p2 == node2.p2);

	id += 1;	       
	assert(id > NODE_OFFSET);
    }
    return 0;
}


int main(int argc, char** argv)
{
    char *group_dir = NULL, *group_dir2, opt;

    ofstream outstream;    

    while (1) 
    {
	opt = getopt(argc, argv, "f:s:");

	if (opt == -1) 
	{
	    break;
	}
	switch(opt) 
	{
	case 'f':
	    group_dir = optarg;
	    break;
	case 's':
	    group_dir2 = optarg;
	    break;
	default:
	    fprintf(stderr, "Unrecognized option\n");
	    break;
	}
    }
    if(group_dir != NULL)
    {

	fprintf(stderr, "group %s group2 %s\n", group_dir, group_dir2);
	read_merge(group_dir, group_dir2);
    }
    return -1;
}

