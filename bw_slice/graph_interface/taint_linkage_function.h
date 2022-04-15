#ifndef LINKAGE_FUNCTION_H
#define LINKAGE_FUNCTION_H

#include "graph_interface.h"

#ifdef TAINT_STATS
extern struct taint_stats_profile tsp;
#endif

#ifdef VERBOSE
extern ofstream log_f;
#endif

extern uint64_t instcount; 


extern xray_monitor *open_fds;
/*
 * mix byte entries of registers (and mem) and 
 * return the mix. These *always* mix in the current instruction as well... 
 * may want to adjust that... may not
 */

template <uint_fast8_t size, uint_fast8_t offset> 
    static inline nodeid mixRegNodeid(int reg)
{
    nodeid *old = get_reg_nodes(reg, offset);
    nodeid newid = old[0];

    for (u_int i = 1; i < size; ++i) { 
	if (old[i] != NULL_NODE && old[i] != old[i-1]) 
	    newid = add_node(old[i], newid);

    }
    return newid;    
}


template <uint_fast8_t size> 
static inline nodeid mixMemNodeid(u_long memloc)
{
    nodeid newid = NULL_NODE; 
    uint_fast8_t offset = 0;

    while (offset < size) {
        nodeid* mem_taints = NULL;
        uint32_t count = get_mem_nodes(memloc + offset, size - offset, &mem_taints);
	offset += count; 
	
	if (mem_taints) { 
	    if (newid != NULL_NODE) 
		newid = add_node(mem_taints[0], newid);
	    else 
		newid = mem_taints[0];

	    for (int i = 1; i < count; ++i ) { 	    
		if (mem_taints[i] != NULL_NODE && mem_taints[i] != mem_taints[i-1]) 
		    newid = add_node(mem_taints[i], newid);
	    }
	}   
    }
    return newid;   
}

//can't template b/c of mem2mem copy
static inline void setMemNodeids(int memloc, nodeid *ids, uint_fast8_t size) 
{
    u_long offset = 0, count;
    while (offset < size) { 
	count = set_mem_nodes(memloc + offset, size - offset, ids + offset);	
	offset += count;
    }
}

static inline void setMemNodeid(int memloc, nodeid id, uint_fast8_t size)
{

#ifdef VERBOSE
    if (DEBUG_INSTCOUNT(instcount)) 
    {
	log_f << std::hex << "setMemNodeid " << instcount << " memloc " << memloc 
	      << " size " << size << " newid " << id << endl; 
    }
#endif

    u_long offset = 0,count;
    while (offset < size) { 
	count = set_mem_nodes_repeat(memloc + offset, size - offset, id);
	offset += count;

#ifdef VERBOSE
    if (DEBUG_INSTCOUNT(instcount)) 
    {
	nodeid* mem_taints; 
        get_mem_nodes( 0xbf94f81c, 1, &mem_taints);
	if (mem_taints && mem_taints[0] != 0xe3cc1383)
	    log_f << std::hex << "setMemNodeid " << instcount << " memloc " << memloc + offset 
		  << " size " << size - offset << " newid " << id << endl; 
    }
#endif

    }
}


///////////////////////
// linkage functions //
///////////////////////


template <uint_fast8_t size, uint_fast8_t doffset, uint_fast8_t soffset> 
    TAINTSIGN copy_reg2reg(int dst, int src)
{
    static_assert(!(doffset || soffset) || size==1, "unreasoanble params to copy_reg2reg\n");
    memcpy(get_reg_nodes(dst, doffset),  get_reg_nodes(src, soffset),
	   size * sizeof(nodeid));

#ifdef STATS
    tsp.copies+=size;
#endif    


}

template<uint_fast8_t size, uint_fast8_t soffset> 
    TAINTSIGN copy_reg2mem (u_long mem_loc, int reg)
{
    static_assert( !soffset || size == 1, "unreasoanble params to copy_reg2mem\n");
    nodeid *nodes = get_reg_nodes(reg, soffset);
    setMemNodeids(mem_loc, nodes, size);

#ifdef STATS
    tsp.copies+=size;
#endif    
}

template<uint_fast8_t size, uint_fast8_t doffset> 
    TAINTSIGN copy_mem2reg (int reg, u_long mem_loc)
{

    nodeid *nodes = get_reg_nodes(reg, doffset); 
    uint_fast8_t offset = 0, count;
    nodeid *mem_nodes = NULL;
    while (offset < size) { 
	mem_nodes = NULL; 
	count = get_mem_nodes(mem_loc + offset, size, &mem_nodes); 
	if (mem_nodes) { 
	    memcpy(nodes + offset, mem_nodes, count * sizeof(nodeid));
	}
	else 
	    memset(nodes + offset, NULL_NODE, count * sizeof(nodeid)); 
	offset += count;
    }
#ifdef STATS
	    tsp.copies+=size;
#endif
}

template<uint_fast8_t size>
TAINTSIGN copy_mem2mem (u_long dst, u_long src)
{
    uint_fast8_t offset = 0, count;
    nodeid *src_nodes;
    while (offset < size) { 
	count = get_mem_nodes(src + offset, size, &src_nodes);

	if (src_nodes) {
	    setMemNodeids(dst, src_nodes, count);
	}
	else {
	    setMemNodeid(dst, NULL_NODE, count);
	}
	offset += count;	

    }
#ifdef STATS
    tsp.copies+=size;
#endif
}



template <uint_fast8_t dsize, uint_fast8_t ssize, uint_fast8_t doffset, uint_fast8_t soffset1, uint_fast8_t soffset2> 
    TAINTSIGN merge_regreg2reg (int dst, int src1, int src2)
{
    nodeid newid1, newid2, newid; 
    newid1 = mixRegNodeid<ssize, soffset1>(src1);
    newid2 = mixRegNodeid<ssize, soffset2>(src2);

    if (newid1 == NULL_NODE) {
	set_reg_nodes_repeat(dst, doffset, dsize, newid2);
    }
    else if (newid2 == NULL_NODE) { 
	set_reg_nodes_repeat(dst, doffset, dsize, newid1);

    }
    else {
	/* (newid1 != NULL_NODE && newid2 != NULL_NODE) */
	newid = add_node(newid1, newid2);
	set_reg_nodes_repeat(dst, doffset, dsize, newid);
    }   
}

template <uint_fast8_t dsize, uint_fast8_t ssize, uint_fast8_t doffset, uint_fast8_t soffset>
    TAINTSIGN merge_regmem2reg (int dst, int src1, u_long mem_loc)
{
    nodeid newid1, newid2, newid; 
    newid1 = mixRegNodeid<ssize, soffset>(src1);
    newid2 = mixMemNodeid<ssize>(mem_loc);

    if (newid1 == NULL_NODE) {
	set_reg_nodes_repeat(dst, doffset, dsize, newid2);
    }
    else if (newid2 == NULL_NODE) { 
	set_reg_nodes_repeat(dst, doffset, dsize, newid1);
    }
    else { 

	newid = add_node(newid1, newid2);
	set_reg_nodes_repeat(dst, doffset, dsize, newid);
    }   
}

template <uint_fast8_t dsize, uint_fast8_t ssize, uint_fast8_t soffset>
TAINTSIGN merge_regmem2mem (u_long dst, int src1, u_long mem_loc)
{
    nodeid newid1, newid2, newid; 
    newid1 = mixRegNodeid<ssize, soffset>(src1);
    newid2 = mixMemNodeid<ssize>(mem_loc);
    
    if (newid1 == NULL_NODE) {
	setMemNodeid(dst, newid2, dsize);
    }
    else if (newid2 == NULL_NODE) { 
	setMemNodeid(dst, newid1, dsize);
    }
    else { 
	newid = add_node(newid1, newid2);
	setMemNodeid(dst, newid, dsize);
    }   
}



template <uint_fast8_t size>
TAINTSIGN merge_memimmval2mem (u_long dst, u_long src, u_long ip)
{
    nodeid newid;

    uint_fast8_t offset = 0, count, lcount;
    nodeid *src_nodes;

    newid = immval_input_none(ip); 

    while (offset < size) { 
	count = get_mem_nodes(src + offset, size, &src_nodes);

	if (src_nodes) {
	    //need to iterate
	    for (lcount = 0; lcount < count; ++lcount) 
	    {
		if (src_nodes[lcount] == NULL_NODE)
		    src_nodes[lcount] = newid;
	    }
	
	    setMemNodeids(dst, src_nodes, count);
	}
	else {
	    setMemNodeid(dst, newid, count);
	}
	offset += count;	
    }
}


template <uint_fast8_t size, uint_fast8_t doffset, uint_fast8_t soffset>
TAINTSIGN merge_regimmval2reg (int dst, int src, u_long ip)
{
    nodeid newids[size], newid;
    u_long offset;

    memcpy(newids, get_reg_nodes(src, soffset), size * sizeof(nodeid));
    newid = immval_input_none(ip); 

    for (offset = 0; offset < size; ++offset) 
    {
	if (newids[offset] == NULL_NODE) 
	    newids[offset] = newid;
    }
    set_reg_nodes(dst, doffset, size, newids); 
    
}


#define COPY_REG2REG(size,od,os,dst,src)    INS_InsertCall(ins, IPOINT_BEFORE, \
								 (AFUNPTR)copy_reg2reg<size,od,os>, \
								 IARG_FAST_ANALYSIS_CALL, \
								 IARG_UINT32, dst, \
								 IARG_UINT32, src, \
								 IARG_END);

#define COPY_MEM2REG(size,od,dst)	INS_InsertCall(ins, IPOINT_BEFORE, \
						       AFUNPTR(copy_mem2reg<size, od>),	\
						       IARG_FAST_ANALYSIS_CALL,	\
						       IARG_UINT32, dst, \
						       IARG_MEMORYREAD_EA, \
						       IARG_END);     

#define COPY_MEM2MEM(size) INS_InsertCall(ins, IPOINT_BEFORE, \
						AFUNPTR(copy_mem2mem<size>), \
						IARG_FAST_ANALYSIS_CALL,\
						IARG_MEMORYWRITE_EA,\
						IARG_MEMORYREAD_EA,\
						IARG_END);     
	
#define COPY_REG2MEM(size,os,src)	INS_InsertCall(ins, IPOINT_BEFORE, \
						       AFUNPTR(copy_reg2mem<size, os>),	\
						       IARG_FAST_ANALYSIS_CALL,	\
						       IARG_MEMORYWRITE_EA, \
						       IARG_UINT32, src, \
						       IARG_END);     

#define COPY_IMMVAL2REG(size,od,dst) INS_InsertCall(ins, IPOINT_BEFORE, \
						    AFUNPTR(copy_immval2reg<size, od>),	\
						    IARG_FAST_ANALYSIS_CALL, \
						    IARG_UINT32, dst,	\
						    IARG_INST_PTR,	\
						    IARG_END);     

#define COPY_IMMVAL2MEM(size) INS_InsertCall(ins, IPOINT_BEFORE, \
					     AFUNPTR(copy_immval2mem<size>), \
					     IARG_FAST_ANALYSIS_CALL,	\
					     IARG_MEMORYWRITE_EA,	\
					     IARG_INST_PTR,	       \
					     IARG_END);     


#define MERGE_REGREG2REG(sd,ss,od,os1,os2,dst,s1,s2) INS_InsertCall(ins, IPOINT_BEFORE, \
								     (AFUNPTR)merge_regreg2reg<sd,ss,od,os1,os2>, \
								     IARG_FAST_ANALYSIS_CALL, \
								     IARG_UINT32, dst, \
								     IARG_UINT32, s1, \
								     IARG_UINT32, s2, \
								     IARG_END);


#define MERGE_REGMEM2REG(sd,ss,od,os1,dst,s1) INS_InsertCall(ins, IPOINT_BEFORE, \
							     (AFUNPTR)merge_regmem2reg<sd,ss,od,os1>, \
							     IARG_FAST_ANALYSIS_CALL, \
							     IARG_UINT32, dst, \
							     IARG_UINT32, s1, \
							     IARG_MEMORYREAD_EA, \
							     IARG_END);

#define MERGE_REGMEM2MEM(sd,ss,os1,s1) INS_InsertCall(ins, IPOINT_BEFORE, \
						      (AFUNPTR)merge_regmem2mem<sd,ss,os1>, \
						      IARG_FAST_ANALYSIS_CALL, \
						      IARG_MEMORYWRITE_EA, \
						      IARG_UINT32, s1,	\
						      IARG_MEMORYWRITE_EA, \
						      IARG_END);

#define MERGE_MEMIMMVAL2MEM(size) INS_InsertCall(ins, IPOINT_BEFORE, \
						 AFUNPTR(merge_memimmval2mem<size>), \
						 IARG_MEMORYWRITE_EA,	\
						 IARG_MEMORYREAD_EA,	\
						 IARG_INST_PTR,		\
						 IARG_END);     

#define MERGE_REGIMMVAL2REG(size,od,os,dst,src) INS_InsertCall(ins, IPOINT_BEFORE, \
							       AFUNPTR(merge_regimmval2reg<size,od,os>), \
							       IARG_FAST_ANALYSIS_CALL, \
							       IARG_UINT32,dst, \
							       IARG_UINT32,src, \
							       IARG_INST_PTR, \
							       IARG_END);     




#define INC_COUNT(ins) INS_InsertCall(ins, IPOINT_BEFORE, \
				   (AFUNPTR)increment_instcount,	\
				   IARG_FAST_ANALYSIS_CALL,		\
				   IARG_END);

#endif
