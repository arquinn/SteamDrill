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

#define STACK_SIZE 1000000
typedef nodeid stacktype[STACK_SIZE];
stacktype stack;
MergeMapper *mergelog_mapper;
struct input_info  *inputBuf = NULL;
unordered_map<nodeid, struct input_info*> inputMap;

/*
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

static void find_input(nodeid i, struct input_info *entry)
{	    
    auto element = inputMap.find(i);
    assert(element != inputMap.end());
    entry = element->second;
}


static void mergelog_iter(nodeid id, set<nodeid> *nodeids)
{
    nodeids->insert(id); 

    if (id >= NODE_OFFSET) {
	node_t *entry; 
	uint_fast32_t stack_depth = 0;
	stack[stack_depth++] = id; 
	do { 
	    assert(stack_depth < STACK_SIZE); //assert we haven't overflowed 
	    id = stack[--stack_depth];	   	    
	    if (id < NODE_OFFSET) {
		continue;		
	    }

	    entry = GET_NODE(mergelog_mapper,id); 
	    if (entry->p1 &&
		nodeids->insert(entry->p1).second)	    
	    {
		stack[stack_depth++] = entry->p1; 
	    }
	    
	    if (entry->p2 && 
		nodeids->insert(entry->p2).second)
	    {
		stack[stack_depth++] = entry->p2;
	    }
	} while(stack_depth);
    }
}
*/
static void search_ip(u_long ip, set<nodeid> *nodeids, 
		      set<u_long> *ips) 
{
    nodeid id;
    for (id = NODE_OFFSET; 
	 id < NODE_OFFSET + mergelog_mapper->numNodes; 
	 ++id) 
    {
	node_t *entry = GET_NODE(mergelog_mapper, id); 
	ips->insert(entry->ip); 
/*
	if (nodeids->find(id) == nodeids->end()) 
	{ 
	    node_t *entry = GET_NODE(mergelog_mapper, id);
//	    if (entry->ip == ip) 
//		mergelog_iter(id, nodeids);
	}
*/
    }
}


int main(int argc, char** argv)
{
    assert(argc == 3);

    u_long idatasize;
    int ifd;
    set<nodeid> nodeids; 	
    set<u_long> ips; 
    char merge_file[256];
    char *pos;
    u_long criteria = strtoul(argv[2], &pos, 16);

   
    snprintf(merge_file, 256, "/node_nums_shm%s", argv[1]);
    for (u_int i = 1; i < strlen(merge_file); i++) {
	if (merge_file[i] == '/') merge_file[i] = '.';
    }

    mergelog_mapper = new MergeMapper(merge_file);
    inputBuf = (struct input_info *) map_buffer ("inputs", argv[1], idatasize, ifd);    


    //preprocess inputs
    cerr << "finding all ips" << endl;
//    preprocessInput(inputBuf, idatasize);
    search_ip(criteria, &nodeids, &ips);
    cerr << hex << ips.size() << endl;

    cerr << "writing out" << endl;
    for (auto &i : ips) 
	cout << i << endl;

    int count = 0;
/*
  for (auto &i : nodeids) 
    {      
	if (i < LOCAL_SOURCE_OFFSET)  
	    continue;
	else if (i < NODE_OFFSET) { 
	    struct input_info entry;
	    find_input(i, &entry);
	    if (entry.type != input_info_type::IMM)
		cerr << "can't handle nodeid " << i 
		     << "type >" << entry.type << endl;
	    }
	else {
	    node_t *entry = GET_NODE(mergelog_mapper, i);
	    if (TRACK(entry->ip))
		count++;
	}	
    }
*/
    cerr << hex << count << endl;
    cerr << hex << mergelog_mapper->numNodes; 

    return 0;
}

