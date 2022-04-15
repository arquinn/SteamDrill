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

#define TRACK(id) (no_track_ips.find(id) == no_track_ips.end())
//#define CHOP_OFF(id) (id == 0xe0250df1 || id == 0xe0250b65)
using namespace std;
vector<nodeid> outputs;



#define STACK_SIZE 1000000
typedef nodeid stacktype[STACK_SIZE];
stacktype stack;
MergeMapper *mergelog_mapper;
struct input_info  *outputBuf = NULL,  *inputBuf = NULL;
unordered_map<nodeid, struct input_info*> inputMap;
unordered_set<u_long> no_track_ips;


static void populate_no_track(char *fname)
{
    u_long ip;
    ifstream ifile(fname);
    while (ifile >> ip) 
	no_track_ips.insert(ip);
    cerr << "correct ips? " << no_track_ips.size() << endl;
}

static void print_local_node(nodeid id) 
{
    cout << std::hex << id << "," << id << "," << id << " starting state"
	 << "," << 0 << "," << 0
	 << "," << id << "," << 0 << endl;    
}

//is fileno a thing? I really want something meaningful from this... 
static void print_read_node(nodeid id, u_long dynid, u_long fileno, u_long syscall_cnt, input_info_type type) 
{
    switch(type) { 
    case input_info_type::READ:	
	cout << std::hex << id << "," << id << "," << " read from " 
	     << fileno << "@" << syscall_cnt 
	     << "," << 0 << "," << 0
	     << "," << id << "," << 0 << endl;    
	break;
    case input_info_type::MMAP:       
	cout << std::hex << id << "," << id << "," << " mmap from " 
	     << fileno << "@" << syscall_cnt 
	     << "," << 0 << "," << 0
	     << "," << id << "," << 0 << endl;    
	break;

    default:
	cout << std::hex << id << "," << id << "," << " input from " 
	     << fileno << "@" << syscall_cnt 
	     << "," << 0 << "," << 0
	     << "," << id << "," << 0 << endl;    
    }
}

static void print_node(nodeid id, u_long dynid, u_long ip, 
		nodeid p1, nodeid p2, 
		u_long location, u_long value)
{
    (void) dynid;
    cout << std::hex << id 
	 << "," << dynid 
	 << "," << ip
	 << "," << p1 << "," << p2
	 << "," << location << "," << value << endl;    
}


static void preprocessInput(struct input_info *inputBuf, u_long inputSize) 
{
    struct input_info *entry = inputBuf;   
    while ((char *)entry < (char *)inputBuf + inputSize) 
    {	
	if (entry->nodeid_start == 0xc0a9d594) 
	{
	    cerr << "found THE nodeid... why not catching it? " 
		 << entry->imm_info.ip << endl;
	}
	if (entry->type == input_info_type::IMM && 
	    entry->imm_info.ip == 0x8054e26) { 
	    
	    cerr << "found an input from the glob function " 
		 << entry->imm_info.ip << endl;
	}


	nodeid curr_nodeid = entry->nodeid_start; 
	while (curr_nodeid < entry->nodeid_start + entry->size)
	{
	    inputMap[curr_nodeid] = entry;
	    ++curr_nodeid;
	}	
	entry++;
    }       
}

static void print_input(nodeid i)
{
    struct input_info *entry = NULL; 

    auto element = inputMap.find(i);
    if (element == inputMap.end()) 
	cerr << "huh, can't find input " << i << endl;
    entry = element->second;

    switch(entry->type) { 
    case input_info_type::IMM:
	print_node(i, entry->imm_info.instcount, entry->imm_info.ip, 0,0,0,0); 
	break;
    default:
	print_read_node(i, i, entry->syscall_info.fileno, entry->syscall_cnt, entry->type);
	break;
    }
}


static void mergelog_iter_depth(nodeid id, set<nodeid> *nodeids, int depth)
{
    nodeids->insert(id); 
    unordered_map<nodeid, int> depth_map; 

    if (id >= NODE_OFFSET) {
	node_t *entry; 
	uint_fast32_t stack_depth = 0;
	stack[stack_depth++] = id; 
	depth_map[id] = depth;
	do { 
	    assert(stack_depth < STACK_SIZE); //assert we haven't overflowed 
	    id = stack[--stack_depth];	   	    
	    if (id < NODE_OFFSET) {
		continue;		
	    }

	    entry = GET_NODE(mergelog_mapper,id); 


	    if (depth_map[id] <= 0) 
		continue;

	    if (entry->p1 &&
//		!CHOP_OFF(entry->p1) &&
		nodeids->insert(entry->p1).second)	    
	    {
		depth_map[entry->p1] = depth_map[id] -1;
		stack[stack_depth++] = entry->p1; 
	    }
	    
	    if (entry->p2 && 
//		!CHOP_OFF(entry->p2) &&
		nodeids->insert(entry->p2).second)
	    {
		depth_map[entry->p2] = depth_map[id] -1;	
		stack[stack_depth++] = entry->p2;
	    }
	} while(stack_depth);
    }

//    for (auto n : depth_map) 
//    { 
//	if (n.first >= NODE_OFFSET && n.second < 0) 
//	    nodeids->erase(n.first);
	//how do i patch up the links...?
//    }


}

int main(int argc, char** argv)
{
    assert(argc >= 4);

    u_long idatasize, odatasize;
    int ifd, ofd; 
    char *outputPtr, *outStop;
    set<nodeid> nodeids; 	
    int32_t newnodeid = -1;
    char merge_file[256];
    u_long desired_output = 0;
    if (argc >4) 
	desired_output = strtoul(argv[4], &outputPtr, 16);


    cerr << hex << "desired_output " << desired_output << endl;
    populate_no_track(argv[3]);


   
    snprintf(merge_file, 256, "/node_nums_shm%s", argv[1]);
    for (u_int i = 1; i < strlen(merge_file); i++) {
	if (merge_file[i] == '/') merge_file[i] = '.';
    }

    mergelog_mapper = new MergeMapper(merge_file);
    inputBuf = (struct input_info *) map_buffer ("inputs", argv[1], idatasize, ifd);    
    outputBuf = (struct input_info *) map_buffer ("outputs", argv[1], odatasize, ofd);


    //preprocess inputs
    preprocessInput(inputBuf, idatasize);

    //deal with outputs
    outputPtr = (char *) outputBuf; 
    outStop = (char *) outputBuf + odatasize; 

    nodeid *buf; 
    u_long ip = 0;
    u_long instcount = 0;
    while (outputPtr < outStop)
    { 
	struct input_info curr_ii = *(struct input_info *)outputPtr;
	ip = curr_ii.imm_info.ip; 
	instcount = curr_ii.imm_info.instcount;
	outputPtr += sizeof(struct input_info);

//	u_long bufaddr = *outputPtr;
	outputPtr += sizeof(u_long);

	u_int buf_size = *outputPtr; 
	outputPtr += sizeof(u_int);

	buf = (nodeid *)outputPtr; 
	outputPtr += buf_size * sizeof(nodeid);  
	cerr << "next buf " << buf[0] << endl;
	if (desired_output && buf[0] == desired_output) 
	{
	    
	    print_node(newnodeid, instcount, ip, buf[0], 0, 0, 0);
	    newnodeid--;
	    outputs.push_back(buf[0]);    
	}
    }   

    for (auto &i : outputs)   
	mergelog_iter_depth(i, &nodeids, atoi(argv[2]));

//    cerr << hex << nodeids.size() << endl;
/*
    int count = 0;
    for (auto &i : nodeids) 
    {      
	if (i < NODE_OFFSET)
	    continue;
	else {
	    node_t *entry = GET_NODE(mergelog_mapper, i);
	    if (TRACK(entry->ip))
		count++;
	}	
    }
*/
//    cerr << hex << count << " " << hex << mergelog_mapper->numNodes; 
    u_long maxicnt = 0, minicnt = 0xffffffff;

    for (auto &i : nodeids) 
    {      
	if (i < LOCAL_SOURCE_OFFSET) { 
	    print_local_node(i);	    
	}
	else if (i < NODE_OFFSET) { 
	    print_input(i);
	}
	else { 	
	    node_t *entry = GET_NODE(mergelog_mapper, i);
	    if (TRACK(entry->ip)) {
		if (entry->instcount > maxicnt)
		    maxicnt = entry->instcount;
		if (entry->instcount < minicnt)
		    minicnt = entry->instcount;

		print_node(i, entry->instcount, entry->ip,
			   entry->p1, entry->p2, entry->location, entry->value);
	    }
	}	
    }  
    cerr << hex << "icnt [" << minicnt << ", " << maxicnt << "]: " << dec << maxicnt - minicnt << endl;

    return 0;
}

