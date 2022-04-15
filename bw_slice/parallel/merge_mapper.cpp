#include <errno.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <iostream>
#include <fstream>
#include <assert.h>

#include "merge_mapper.h"
//#include "../graph_interface/graph_interface.h"
#include "../graph_interface/node.h"

using namespace std;
MergeMapper::MergeMapper() 
{
    numNodes = 0;
    numMaps = 0;
    numSearches = 0;
    currNodes = NULL;   
    nodeSize = 0;
}

MergeMapper::MergeMapper(const char *filename)
{
    updateFile(filename);
}

void MergeMapper::updateFile(const char* filename)
{
    struct stat64 st;
    int rc;

    fd = shm_open (filename, O_RDONLY | O_LARGEFILE, 0644);
//    fd = open(filename, O_RDONLY | O_LARGEFILE);
    if (fd < 0) {
	fprintf (stderr, "map_file: unable to open %s, rc=%d, errno=%d\n", 
		 filename, fd, errno);
    }
/*    rc = shm_unlink (filename);
    if (rc < 0) {
	fprintf (stderr, "shm_unlink of %s failed, rc=%d, errno=%d\n", filename, rc, errno);
    }
*/


    rc = fstat64(fd, &st);
    if (rc < 0) {
	fprintf (stderr, "Unable to stat %s, rc=%d, errno=%d\n", 
		 filename, rc, errno);
    }
    
    numNodes = (st.st_size / sizeof(node_t)); 
    numMaps = 0;
    numSearches = 0;
    currNodes = NULL;
    nodeSize = 0;
}

void MergeMapper::mapRegion(nodeid id)
{   

    numMaps++;
    uint64_t off = ((uint64_t)(id - NODE_OFFSET) * sizeof(node_t));
    off -= (off % MAP_SIZE);

    uint64_t size = MAP_SIZE;
    uint64_t file_size = (uint64_t)numNodes * sizeof(node_t);


    lowMap = (off / sizeof(node_t)) + NODE_OFFSET;
 
   //if the endpoint is past the numNodes then adjust it!
    if (off + size > file_size) { 
	          
       //remaining nodes adjusted to be divisible by 4096
	size = (file_size - off);
	size = size + 4096-size%4096; 

	highMap = (off + (file_size - off)) / sizeof(node_t) + NODE_OFFSET; 
    }
    else 
	highMap = (off + size) / sizeof(node_t) + NODE_OFFSET; 

    if (currNodes != NULL) { 
	munmap(currNodes, nodeSize);
    }

    currNodes = (node_t *) mmap64(NULL, size, PROT_READ | PROT_WRITE, 
				  MAP_PRIVATE, fd, off);
    nodeSize = size; 
    if (currNodes == MAP_FAILED) { 
	cerr << std::dec << "map problem, errno" << errno << endl;
	cerr << std::dec << "size " << size
	     << " fd " << fd
	     << " id " << id 
	     << " off " << off << endl;; 

	assert(0);
    }

//    cerr << hex << "mapRegion id " <<id << " map (" << lowMap << ","<<highMap<<") off "
//	 << off << " sizeof " << sizeof(node_t) << endl;

    assert(lowMap <= id && highMap > id); 

}

node_t* MergeMapper::getNode(nodeid id)
{
    numSearches++;
    if (id > numNodes + NODE_OFFSET) {
	cerr << hex << id << " is a problem?!?\n"; 
	cerr << hex << "num nodes: " << numNodes
	     << " (plus off: " << numNodes + NODE_OFFSET << ")\n";
	
    }
    
    assert (id <= (numNodes + NODE_OFFSET));

    if (!currNodes || id < lowMap || id >= highMap)
    {
	mapRegion(id);
    }

    assert (id >= lowMap && id < highMap);    
    u_long n = (u_long)(currNodes + (id - lowMap));    
    assert (n >= (u_long)currNodes && n < ((u_long)currNodes) + MAP_SIZE);

    return currNodes + (id - lowMap); 
}
