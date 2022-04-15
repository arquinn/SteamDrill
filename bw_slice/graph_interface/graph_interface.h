#ifndef GRAPH_INTERFACE_H
#define GRAPH_INTERFACE_H

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <assert.h>

#include <iostream>
#include <fstream>
#include <unordered_set> 

#include "pin.H"
#include "node.h"
#include "input_output.h"
#include "../bw_slice.h"

#define FAST_INLINE
#ifdef FAST_INLINE
#define TAINTSIGN void PIN_FAST_ANALYSIS_CALL inline
#else
#define TAINTSIGN void inline
#endif

#define TRACE_INFO
#ifdef TRACE_INFO
#define TPRINT(args...) fprintf(stderr,args);
#else 
#define TPRINT(x,...)
#endif

// filter types
#define FILTER_FILENAME     1
#define FILTER_PARTFILENAME 2
#define FILTER_SYSCALL      3
#define FILTER_REGEX        4
#define FILTER_BYTERANGE    5

//want to understand why this memory address NOT in slice? 
#define VERBOSE(a) (false)
#define V_INSTCOUNT(a) (a >= 0x5f2400 && a <= 0x5f2422)
#define V_ADDR(a) (false)// 0xebe38fed)

#define DEBUG_INSTCOUNT(a) (false)// (a == 0x6770D29)
#define DEBUG_VALUE(a) (false)
#define DEBUG_REG(a) (false)
#define DEBUG_MEM(a) (false)

#ifdef VERBOSE
extern unordered_set<nodeid> debug_set;
extern ofstream log_f;
extern unordered_set<nodeid> traceids;
#endif

extern int splice_output;
extern bool nodeids_initialized; 
extern unsigned long global_syscall_cnt;
extern u_long* ppthread_log_clock;
extern struct thread_data *current_thread;

extern struct graph g;
extern nodeid* mem_root[ROOT_TABLE_COUNT];  // Top-level table for memory taints
extern uint64_t instcount; 

#ifdef TAINT_STATS
struct taint_stats_profile {
    unsigned long num_second_tables;
    unsigned long merges;
    unsigned long merges_saved;
    unsigned long copies;
    unsigned long tokens;
    unsigned long immvals;
    unsigned long imm_filtered;
};
extern struct taint_stats_profile tsp;
#endif

//Init and destroy functions on the graph
void init_graph(char* group_dir);
void finish_and_print_graph_stats(FILE* fp);

void init_input_output(char *group_dir);
void finish_input_output();

void flush_graph_nodes(void);

void initialize_starting_nodeids(FILE *file);
void initialize_startingReg(int, struct thread_data *);
void initialize_endingAS(FILE *file);

//dump out the taints
void init_dump(char *group_dir);
void finish_dump(void);
void dump_reg_taints(nodeid *regs, int thread_ndx); 
void dump_mem_taints();
void dump_reg_taints_start(nodeid *regs, int thread_ndx); 
void dump_mem_taints_start();
void debug_dump_mem(); 

//create a new node in the graph for new data
nodeid get_next_nodeid(void); 
bool create_node (u_long mem_addr, u_int size);
bool create_node_reg (int reg, u_int size, u_int offs);

//helper function for getting static address
unsigned long find_static_address(ADDRINT ip);



/*
 * Translate a register from the Pin representation
 * E.g. translates AH to EAX
 */
inline int translate_reg(int reg)
{
    //these coorespond to the eax, ebx, ecx and edx registers so that we 
    //can have the low / high / word varients overlap in our register map. 
    
    if (reg == 25 || reg == 26 || reg == 27) {
        return 8;
    } else if (reg == 22 || reg == 23 || reg == 24) {
        return 9;
    } else if (reg == 28 || reg == 29 || reg == 30) {
        return 7;
    } else if (reg == 19 || reg == 20 || reg == 21) {
        return 10;
    }
    return reg;
}



inline void increment_instcount(void) 
{
    assert(instcount +1 > instcount); //assert no wrap-around
    instcount++;
#ifdef VERBOSE
    if (instcount >= 17217870)
	log_f << "inst " << instcount << endl;
#endif
}


//node getters and setters
static inline nodeid* new_leaf_table(u_long memloc)
{
    nodeid* leaf_table = (nodeid*) malloc(LEAF_TABLE_COUNT * sizeof(nodeid)); 

    if (!leaf_table) {
	fprintf(stderr,"sycall_cnt %ld clock %ld\n", global_syscall_cnt, *ppthread_log_clock);
	assert (0);
    }

    if (!nodeids_initialized && splice_output) { 
	memloc &= ROOT_INDEX_MASK;
	for (int i = 0; i < LEAF_TABLE_COUNT; i++) {
	    leaf_table[i] = memloc++;
	}
    } else {
	memset(leaf_table, 0, LEAF_TABLE_COUNT * sizeof(nodeid));
    }
#ifdef TAINT_STATS
    tsp.num_second_tables++;
#endif
    return leaf_table;
}

// Returns smaller of size or bytes left in leaf table
static inline int get_mem_split(u_long mem_loc, uint32_t size)
{
    uint32_t bytes_left = LEAF_TABLE_COUNT-(mem_loc&LEAF_INDEX_MASK);
    return (bytes_left < size) ? bytes_left : size;
}

inline uint32_t get_mem_nodes(u_long mem_loc, uint32_t size, nodeid** mem_nodes)
{
    unsigned bytes_left = get_mem_split(mem_loc, size);
    unsigned index = mem_loc >> LEAF_TABLE_BITS;
    nodeid* leaf_t = mem_root[index];    
    assert(ROOT_TABLE_COUNT > index);    
    
    if(!leaf_t) {
	if (!nodeids_initialized && splice_output) { 
	    // Uninitialized - create table with correct values
	    leaf_t = mem_root[index] = new_leaf_table(mem_loc);
	} else {
	    *mem_nodes = NULL;
	    return bytes_left;
	}
    }

    *mem_nodes = &leaf_t[mem_loc & LEAF_INDEX_MASK];
    return bytes_left;
}

/* Returns the number of bytes set in a memory location.
 *  This can be less than size if it requires walking over to another
 *   page table structure.
 *   This is a performance optimization.
 * */
inline uint32_t set_mem_nodes_repeat(u_long mem_loc, uint32_t size, nodeid value)
{
    uint32_t set_size = get_mem_split(mem_loc, size);
    nodeid* leaf_t;
    unsigned index = mem_loc >> LEAF_TABLE_BITS;
    if(!mem_root[index]) mem_root[index] = new_leaf_table(mem_loc);
    leaf_t = mem_root[index];

    unsigned low_index = mem_loc & LEAF_INDEX_MASK;
    for (uint32_t i = 0; i < size; ++i) 
    {   
	leaf_t[low_index + i] = value; 
    }

    return set_size;
}

inline uint32_t set_mem_nodes(u_long mem_loc, uint32_t size, nodeid* values)
{
    uint32_t set_size = get_mem_split(mem_loc, size);
    nodeid* leaf_t;
    unsigned index = mem_loc >> LEAF_TABLE_BITS;
    if(!mem_root[index]) mem_root[index] = new_leaf_table(mem_loc);
    leaf_t = mem_root[index];

    unsigned low_index = mem_loc & LEAF_INDEX_MASK;
    memcpy(leaf_t + low_index, values, set_size * sizeof(nodeid));

    return set_size;
}

inline nodeid* get_reg_nodes(int reg, uint32_t offset)
{
    return current_thread->shadow_reg_table + (reg * REG_SIZE + offset);
}


inline void set_reg_nodes_repeat(int reg, int offset, int size, nodeid value)
{
    nodeid* shadow_reg_table = current_thread->shadow_reg_table;
    //unrolled... not sure if good or bab
    for (int i = 0; i < size; ++i)
    {	
	shadow_reg_table[reg * REG_SIZE + offset + i] = value;	
    }
}

inline void set_reg_nodes(int reg, int offset, int size, nodeid* values)
{
    nodeid* shadow_reg_table = current_thread->shadow_reg_table;

    memcpy(&shadow_reg_table[reg * REG_SIZE + offset], values,
            size * sizeof(nodeid));
}


#ifdef USE_MERGE_HASH

// simple hash for holding merged indices
#define SIMPLE_HASH_SIZE 0x1000000
struct simple_bucket {
    nodeid p1, p2, n;
#ifndef DIFT
    uint64_t instcount;
#endif    
};
static struct simple_bucket simple_hash[SIMPLE_HASH_SIZE];

#endif

#ifdef DIFT
inline nodeid add_node(nodeid p1, nodeid p2)
#else 
    inline nodeid add_node(u_long ip, long location,
		       nodeid p1, nodeid p2)
#endif
{
#ifdef DIFT
    if (p1 == NULL_NODE)
	return p2;
    if (p2 == NULL_NODE)
	return p1;
    if (p1 == p2)
	return p2;
#endif
#ifdef USE_MERGE_HASH
    if (p2 < p1) {
	nodeid tmp = p2;
	p2 = p1;
	p1 = tmp;
    }

    nodeid h = p1 + (p2 << 2) + (p2 << 3);
    struct simple_bucket& bucket = simple_hash[h%SIMPLE_HASH_SIZE];

#ifdef DIFT
    if (bucket.p1 == p1 && bucket.p2 == p2) {
#else
    if (bucket.p1 == p1 && bucket.p2 == p2 && bucket.instcount == instcount) {
#endif

#ifdef TAINT_STATS
	tsp.merges_saved++;
#endif       
	return bucket.n;
    }
    else {
	bucket.p1 = p1;
	bucket.p2 = p2;
	bucket.n = g.total_count; //this is the nodeid
#ifndef DIFT
	bucket.instcount = instcount; 
#endif	
    }
#endif

    if (g.buffer_count == MERGE_FILE_ENTRIES) {
	flush_graph_nodes();
    } 

#ifndef DIFT    
	
    g.nodes[g.buffer_count].ip = ip;
    g.nodes[g.buffer_count].instcount = (uint32_t)instcount; //cast necessary? 
    g.nodes[g.buffer_count].location = location; 
//    g.nodes[g.buffer_count].reg_offset = reg_offset; 
#endif

/*
#ifdef VERBOSE    
    if ((g.total_count - NODE_OFFSET) >= 0x6db6000) 
    {
	log_f << "an tc " << (g.total_count - NODE_OFFSET)
	      << " ptr " << (g.nodes + g.buffer_count) 
	      << " size " << (g.total_count - NODE_OFFSET +1) * sizeof(node_t) 
	      << endl;
    }
#endif
*/
    g.nodes[g.buffer_count].p1 = p1;
    g.nodes[g.buffer_count].p2 = p2; 
    
    //update graph buffer index, return graph total count (increased of course) 
    g.buffer_count++; 
    assert(g.total_count + 1 > NODE_OFFSET); //assert no wrap around
    
#ifdef TAINT_STATS
    tsp.merges++;
#endif
    return g.total_count++;
}

#ifndef DIFT
inline void set_value(ADDRINT dvalue) { 
	g.nodes[g.buffer_count - 1].value = dvalue;
    }
#endif

// information tracking functions that are not related to linkage_function
template <uint32_t size, uint32_t soffset>
    TAINTSIGN copy_immval2reg(uint32_t reg, u_long ip)
{
    immval_input_reg(reg, size, soffset, ip);
}

template <uint32_t size>
TAINTSIGN copy_immval2mem(u_long mem_loc, u_long ip)
{
    immval_input_mem((void *)mem_loc, size, ip);
}

#ifndef DIFT
TAINTSIGN log_jump(uint32_t branch_target_addr, 
		   uint32_t fallthrough_addr, 
		   bool branch_taken,
		   u_long ip) 
{   
    if (branch_taken)
	add_node(ip, branch_target_addr, 0,0);
    else 
	add_node(ip, fallthrough_addr, 0,0); 

}

TAINTSIGN log_call(uint32_t ip, 
		   uint32_t branch_target_addr, 
		   uint32_t stack_ptr)		   
{   
    add_node(ip, branch_target_addr, 1,1);
    g.nodes[g.buffer_count -1].value = stack_ptr;


//    cerr << hex << g.total_count - 1 << " " << ip << " " << stack_ptr << endl;
}

TAINTSIGN log_ret(uint32_t stack_ptr,
		  u_long ip) 
{   

    add_node(ip, 0, 2,2);
    g.nodes[g.buffer_count -1].value = stack_ptr;
}


#endif

#endif
