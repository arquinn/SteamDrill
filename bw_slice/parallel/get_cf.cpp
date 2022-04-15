#define _LARGEFILE64_SOURCE
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <assert.h>
#include <fcntl.h>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <vector>
#include <cstring>
#include <fstream>

#include "../graph_interface/node.h"
#include "../graph_interface/input_output.h"
#include "merge_mapper.h"
#include "../maputil.h"

//#define DETAILS 1
#define GET_NODE(ml,id) ml->getNode(id)

using namespace std;
vector<nodeid> outputs;
MergeMapper *mergelog_mapper;


static void mergelog_back_walk(nodeid id, uint32_t branches_until) 
{
    assert (id >= NODE_OFFSET); 
    uint32_t branch_cnt = 1;

    while (branch_cnt < branches_until) 
    {
	node_t *entry = GET_NODE(mergelog_mapper, id); 


	if (entry->p1 == 2 && entry->p2 == 2)
	{
	    branch_cnt ++;
	    cerr << "found ret  " << entry->ip << "[" << id 
		 << "] stack " << entry->value << endl;
	}    
	else if (entry->p1 == 1 && entry->p2 == 1) 
	{
	    branch_cnt --; 
	    cerr << "found call " << entry->ip << "[" << id  << "] spot " << entry->location
		 << " stack " << entry->value << endl;
	}
	if (!branch_cnt) 
	    return;
//	else { 
//	    cerr << "found inst " << entry->ip << " " << id << endl;
//	}
	--id;
    }
}

int main(int argc, char** argv)
{
    assert(argc >= 2);

    char *outputPtr; 
    char merge_file[256];
    u_long desired_output = 0;
    if (argc > 2) 
	desired_output = strtoul(argv[2], &outputPtr, 16);


    cerr << hex << "desired_output " << desired_output << endl;

   
    snprintf(merge_file, 256, "/node_nums_shm%s", argv[1]);
    for (u_int i = 1; i < strlen(merge_file); i++) {
	if (merge_file[i] == '/') merge_file[i] = '.';
    }
    mergelog_mapper = new MergeMapper(merge_file);

    mergelog_back_walk(desired_output, 10); 

    return 0;
}

