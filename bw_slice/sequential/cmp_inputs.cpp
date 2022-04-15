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

#include "input_set.h"
//#define DETAILS 1


using namespace std;


static void preprocessInput(struct input_info *inputBuf, u_long inputSize,
			    unordered_map<nodeid, struct input_info*> &inputMap) 
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




int cmp_input (char* group_dir1, char* group_dir2)
{
    char input_filename[256], input_filename2[256];
    
    int rc;
    u_long inputSize = 0, inputMapSize = 0;
    u_long inputSize2 = 0, inputMapSize2 = 0;
    int inFd, inFd2; 
    
    struct input_info *inputBuf = NULL, *inputBuf2 = NULL;

    unordered_map<nodeid, struct input_info*> inputMap; 
    unordered_map<nodeid, struct input_info*> inputMap2; 
    
    snprintf(input_filename, 256, "%s/inputs", group_dir1);
    snprintf(input_filename2, 256, "%s/inputs", group_dir2);



    rc = map_file(input_filename, &inFd, &inputSize, &inputMapSize, (char **) &inputBuf);
    assert(!rc);
    rc = map_file(input_filename2, &inFd2, &inputSize2, &inputMapSize2, (char **) &inputBuf2);
    assert(!rc);

    
    preprocessInput(inputBuf, inputSize, inputMap);
    preprocessInput(inputBuf2, inputSize2, inputMap2);


    for (auto in : inputMap) 
    {
	auto elem = inputMap2.find(in.first);
	if (!elem->second) { 
	    cerr << "taint_num " << in.first << " not in second input\n";
	}
	else { 
	    struct input_info *e1 = in.second, *e2 = inputMap2[in.first];
	    
	    if (e1->record_pid != e2->record_pid || 
		e1->rg_id != e2->rg_id || 
		e1->syscall_cnt != e2->syscall_cnt ||
		e1->nodeid_start != e2->nodeid_start || 
		e1->size != e2->size) 
	    {
		cerr << "taint_num " << in.first << " different input_info?? \n";

		cerr << std::hex << "e1 " << e1->type << ","<< e1->syscall_cnt << "," << e1->nodeid_start << ","
		     << e1->size << "," << e1->imm_info.ip << endl;

		cerr << std::hex << "e2 " << e2->type <<  "," << e2->syscall_cnt << "," << e2->nodeid_start << ","
		     << e2->size << "," << e2->imm_info.ip << endl;

		
		assert(0);
	    }
	    
	}
    }
    return 0;
}


int main(int argc, char** argv)
{
    char *firstdir = NULL, *seconddir = NULL, opt; 

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
	    firstdir = optarg;
	    break;
	case 's':
	    seconddir = optarg;
	    break;
	default:
	    fprintf(stderr, "Unrecognized option\n");
	    break;
	}
    }
    cmp_input(firstdir, seconddir);
    return -1;
}

