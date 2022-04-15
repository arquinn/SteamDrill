#ifndef __NODE_H__
#define __NODE_H__

#include <sys/types.h>
#include <iostream>
#include <cstdint>


//pointer into our graph. 
typedef uint64_t nodeid;
//typedef uint32_t nodeid; //ugh... couldn't get everything to fit otherwise

#define NULL_NODE 0
#define NODE_OFFSET 0xe0000001 //offset for node index
#define LOCAL_SOURCE_OFFSET 0xc0000001 //offset for local source


//graph related constants
#define MERGE_FILE_ENTRIES 0x1000
#define MERGE_FILE_CHUNK (MERGE_FILE_ENTRIES*sizeof(node_t))
#define MAX_MERGE_SIZE 0x400000000 /*   16 GB -- not sure if this is okay */
#define MAX_MERGES (MAX_MERGE_SIZE/sizeof(node_t))


//related to mem_table
#define LEAF_TABLE_COUNT 1024
#define ROOT_TABLE_COUNT 4194304
#define ROOT_TABLE_BITS 22
#define LEAF_TABLE_BITS 10
#define ROOT_INDEX_MASK 0xfffffc00
#define LEAF_INDEX_MASK 0x000003ff

#define LOWEST_MEM_ADDR 0x8000000

//related to dumping address space
#define MAX_DUMP_SIZE  0xc0000000 /*   3 GB - streaming so large=OK */

#define USE_MERGE_HASH

#define TAINT_STATS

#ifdef DIFT
struct node { 
    nodeid p1; //parent1
    nodeid p2; //parent2
};

#else
struct node { 
    uint32_t ip;
    uint32_t instcount;  //really should be uint64_t...? 
    u_long   value;
    long     location; //the place where this is being added
//    long     reg_offset; //the register that matters for location calculation (idk why I want this)
    nodeid   p1; //parent1
    nodeid   p2; //parent2
};
#endif

typedef struct node node_t; 

std::ostream& operator<<(std::ostream &os, const node_t nt);

struct graph { 
    nodeid  buffer_count;
    nodeid  total_count;  
    bool    buffer_overflow;
    node_t* nodes;
};

struct addrstruct { 
    u_long addr; 
    nodeid nid;
};

#endif
