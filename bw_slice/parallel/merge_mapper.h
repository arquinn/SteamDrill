#ifndef MERGE_MAPPER_H_
#define MERGE_MAPPER_H_

#include "../graph_interface/node.h"


//#define MAP_SIZE (1024 * 1024 * 1024)
#define MAP_SIZE (4 * 1024 * sizeof(node_t)) //a number of pages

class MergeMapper
{
private:
    void mapRegion(nodeid id);

public:
    int fd;
    nodeid numNodes;

    node_t *currNodes;
    size_t nodeSize;
    nodeid lowMap;
    nodeid highMap;
    
    u_long numSearches;
    u_long numMaps;

    MergeMapper(const char *filename);
    MergeMapper();
    node_t* getNode(nodeid id);
    void  updateFile(const char *filename);
    

};


#endif /* MERGE_MAPPER_H_ */
