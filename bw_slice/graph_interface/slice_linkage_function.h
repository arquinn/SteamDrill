#ifndef LINKAGE_FUNCTION_H
#define LINKAGE_FUNCTION_H

#include "graph_interface.h"

#ifdef VERBOSE
extern ofstream log_f;
#endif

extern uint64_t instcount; 

u_long *current_mem_loc = NULL;  //can I do dis? 

/*
 * mix byte entries of registers (and mem) and 
 * return the mix. These *always* mix in the current instruction as well... 
 * may want to adjust that... may not
 */
template <uint_fast8_t size, uint_fast8_t offset> 
    static inline nodeid mixRegNodeid(int src_reg, long dest, u_long ip)
{
    nodeid *old = get_reg_nodes(src_reg, offset);
    nodeid newid = old[0];

    for (u_int i = 1; i < size; ++i) { 
	if (old[i] != NULL_NODE && old[i] != old[i-1]) 
	    newid = add_node(ip, dest, old[i], newid);
    }
    return newid;    
}

template <uint_fast8_t size> 
static inline nodeid mixMemNodeid(u_long mem_loc, long dest, u_long ip) 
{
    nodeid newid = NULL_NODE; 
    uint_fast8_t offset = 0;

    while (offset < size) {
        nodeid* mem_taints = NULL;
        uint32_t count = get_mem_nodes(mem_loc + offset, size - offset, &mem_taints);
	offset += count; 
	
	if (mem_taints) { 
	    if (newid != NULL_NODE) 
		newid = add_node(ip, dest, mem_taints[0], newid);
	    else 
		newid = mem_taints[0];

	    for (int i = 1; i < count; ++i ) { 	    
		if (mem_taints[i] != NULL_NODE && mem_taints[i] != mem_taints[i-1]) 
		    newid = add_node(ip, dest,  mem_taints[i], newid);
	    }
	}   
    }
    return newid;   
}

template<uint_fast8_t size, uint_fast8_t offset>
    static bool  getRegNodeids(int src_reg, long dest, u_long ip, nodeid *newids) 
{
    nodeid *old = get_reg_nodes(src_reg, offset);
    bool set = false;
    for (u_int i = 0; i < size; ++i) { 
	if (old[i] == NULL_NODE)
	    newids[i] = NULL_NODE;
	else {
	    newids[i] = add_node(ip, dest, old[i], NULL_NODE);
	    set = true;
	}

#ifdef VERBOSE
	if (VERBOSE(old[i]) || traceids.count(old[i]))
	{
	    traceids.insert(newids[i]);
	    log_f << "grni[" << ip << "] " << old[i] << " -> " << newids[i] << endl;
	}
#endif		
    }    
    return set;
}

static bool getMemNodeids(u_long mem_loc, long dest,  u_long ip, nodeid *newids, uint_fast8_t size) 
{
    uint32_t offset = 0;
    bool set = false;

    while (offset < size) {
        nodeid* mem_taints = NULL;
        uint32_t count = get_mem_nodes(mem_loc + offset, size - offset, &mem_taints);
	if (!mem_taints) { 
	    memset(newids, NULL_NODE, count * sizeof(nodeid)); 
	}
	else { 
	    for (uint_fast32_t i = 0; i < count; ++i ) { 
		if (mem_taints[i] == NULL_NODE)
		    newids[i] = NULL_NODE; 
		else { 
		    newids[i] = add_node(ip, dest, mem_taints[i],NULL_NODE);
		    set = true;
		}		
#ifdef VERBOSE
		if (VERBOSE(mem_taints[i]) || traceids.count(mem_taints[i]))
		{
		    traceids.insert(newids[i]);
		    log_f << "gmni[" << ip << "] " << mem_taints[i] << " -> " << newids[i] << endl;
		}
#endif		


	    }
	}
	offset += count;
    }
    return set; 
}


template<uint_fast8_t size>
static inline void setMemNodeid(int memloc, nodeid id)
{
    u_long offset = 0,count;
    while (offset < size) { 
	count = set_mem_nodes_repeat(memloc + offset, size - offset, id);
	offset += count;
    }
}

template<uint_fast8_t size, uint_fast8_t offset> 
    static inline void setRegNodeid(int reg, nodeid id)
{
    set_reg_nodes_repeat(reg, offset, size, id);
}


///////////////////////
// linkage functions //
///////////////////////


template <uint_fast8_t size, uint_fast8_t doffset, uint_fast8_t soffset> 
    TAINTSIGN copy_reg2reg(REG dst, REG src, u_long ip)
{
    static_assert((doffset && soffset && size == 1) ||
		  (!doffset || !soffset) , "unreasoanble params to copy_reg2reg\n");

    nodeid *newids = get_reg_nodes(dst, doffset);
    getRegNodeids<size, soffset>(src, dst, ip, newids); //FIXME

#ifdef VERBOSE
    if (V_INSTCOUNT(instcount)) { 
	log_f << hex << "cr2r[" << instcount << "," << ip<< "] " << REG_StringShort(dst) << "<-" << REG_StringShort(src) << endl;
    }
#endif
}

template<uint_fast8_t size, uint_fast8_t soffset> 
    TAINTSIGN copy_reg2mem (u_long mem_loc, REG reg, u_long ip)
{
    static_assert((soffset && size == 1) ||
		  (!soffset) , "unreasoanble params to copy_reg2reg\n");
    nodeid newids[size];
    u_long offset = 0, count;
    current_mem_loc = (u_long *)mem_loc;


    bool set = getRegNodeids<size, soffset>(reg, mem_loc, ip, newids);
    if (set) { 
	while (offset < size) { 
	    count = set_mem_nodes(mem_loc + offset, size - offset, newids + offset); 
	    offset += count;
	}
    }
#ifdef VERBOSE
    if (V_INSTCOUNT(instcount)) { 
	log_f << hex << "cr2m[" << instcount << "," << ip  << "] " << mem_loc << "<-" << REG_StringShort(reg) << endl;
    }
#endif
}

template<uint_fast8_t size, uint_fast8_t doffset> 
    TAINTSIGN copy_mem2reg (REG reg, u_long mem_loc, u_long ip)
{
    static_assert((doffset && size == 1) ||
		  (!doffset) , "unreasoanble params to copy_reg2reg\n");

    nodeid *newids = get_reg_nodes(reg, doffset);
    getMemNodeids(mem_loc, reg, ip, newids, size); //FIXME
#ifdef VERBOSE
    if (V_INSTCOUNT(instcount)) { 
	log_f << hex << "cm2r[" << instcount << "," << ip << "] "<< REG_StringShort(reg) << "<-" << mem_loc << endl;
    }
#endif

}

template<uint_fast8_t size>
TAINTSIGN copy_mem2mem (u_long dst, u_long src, u_long ip)
{
    nodeid newids[size];
    u_long offset = 0,count;
    current_mem_loc = (u_long *)dst;
    bool set = getMemNodeids(src, dst, ip, newids, size);

    if (set) { 
	while (offset < size) { 
	    count = set_mem_nodes(dst + offset, size - offset, newids + offset); 
	    offset += count;
	}
    }
#ifdef VERBOSE
    if (V_INSTCOUNT(instcount)) { 
	log_f << hex << "cm2m[" << instcount << "," << ip << "] "<< dst << "<-" << src << endl;
    }
#endif
}



template <uint_fast8_t dsize, uint_fast8_t ssize, uint_fast8_t doffset, uint_fast8_t soffset1, uint_fast8_t soffset2> 
    TAINTSIGN merge_regreg2reg (REG dst, REG src1, REG src2, u_long ip)
{
    nodeid newid1, newid2, newid; 

    newid1 = mixRegNodeid<ssize, soffset1>(src1, dst, ip);
    newid2 = mixRegNodeid<ssize, soffset2>(src2, dst, ip);

    if (newid1 != NULL_NODE || newid2 != NULL_NODE) { 
	newid = add_node(ip, dst, newid1, newid2); //FIXME
	setRegNodeid<dsize, doffset>(dst, newid);
#ifdef VERBOSE 
	if (VERBOSE(newid1) || traceids.count(newid1) || 
	    VERBOSE(newid2) || traceids.count(newid2) || 
	    VERBOSE(newid) || traceids.count(newid)) {
	    
	    traceids.insert(newid);	    
	    log_f <<std::hex  << "mergerr2r[" << find_static_address(ip) 
		  << "(" << unsigned(dsize) << "," << unsigned(ssize) <<"," << unsigned(doffset)
		  <<"," << unsigned(soffset1) << "," << unsigned(soffset2)  <<"," << unsigned(instcount) 
		  << ")]: (" << newid1 << "(" << src1 <<")," << newid2 << "(" << src2 << ")) -> "
		  << newid << "(" << dst << ")" << endl;
	}
#endif
    }   
#ifdef VERBOSE
    if (V_INSTCOUNT(instcount)) { 
	log_f << hex << "mrr2r[" << instcount << "," << ip<< "] " << REG_StringShort(dst) << "<-(" << REG_StringShort(src1)
	      << "," << REG_StringShort(src2) << ")" << endl;
    }
#endif

}

template <uint_fast8_t dsize, uint_fast8_t ssize, uint_fast8_t doffset, uint_fast8_t soffset>
TAINTSIGN merge_regmem2reg (REG dst, REG src1, u_long mem_loc, u_long ip)
{
    nodeid newid1, newid2, newid; 

    newid1 = mixRegNodeid<ssize, soffset>(src1, dst, ip);
    newid2 = mixMemNodeid<ssize>(mem_loc, dst, ip);

    if (newid1 != NULL_NODE || newid2 != NULL_NODE) { 
	newid = add_node(ip, dst, newid1, newid2); //FIXME
	setRegNodeid<dsize, doffset>(dst, newid);
#ifdef VERBOSE 
	if (VERBOSE(newid1) || traceids.count(newid1) || 
	    VERBOSE(newid2) || traceids.count(newid2) || 
	    VERBOSE(newid) || traceids.count(newid)) {
	    
	    traceids.insert(newid);	    
	    log_f <<std::hex  << "mergerm2r[" << find_static_address(ip) 
		  << "(" << unsigned(dsize) << "," << unsigned(ssize) <<"," << unsigned(instcount) 
		  << ")]: (" << newid1  << "(" << src1 << ")," 
		  << newid2 << "(" << mem_loc << ")) -> " 
		  << newid <<"(" << dst << ")" << endl;
	}
	if (debug_set.count(newid1) || debug_set.count(newid2))
	{
	    debug_set.insert(newid);
	}
#endif
    }   
#ifdef VERBOSE
    if (V_INSTCOUNT(instcount)) { 
	log_f << hex << "mrm2r[" << instcount << "," << ip<< "] " << REG_StringShort(dst) << "<-(" << REG_StringShort(src1)
	      << "," << mem_loc << ")" << endl;
    }
#endif
}

template <uint_fast8_t dsize, uint_fast8_t ssize, uint_fast8_t soffset>
    TAINTSIGN merge_regmem2mem (u_long dst, REG src1, u_long mem_loc, u_long ip)
{
    nodeid newid1, newid2, newid; 
    current_mem_loc = (u_long *)dst;

    newid1 = mixRegNodeid<ssize, soffset>(src1, dst, ip);
    newid2 = mixMemNodeid<ssize>(mem_loc, dst, ip);

    if (newid1 != NULL_NODE || newid2 != NULL_NODE) { 
	newid = add_node(ip, dst, newid1, newid2); 
	setMemNodeid<dsize>(dst, newid);

#ifdef VERBOSE 
	if (VERBOSE(newid1) || traceids.count(newid1) || 
	    VERBOSE(newid2) || traceids.count(newid2) || 
	    VERBOSE(newid) || traceids.count(newid)) {
	    
	    traceids.insert(newid);	   
	    log_f <<std::hex  << "mergerm2m[" << find_static_address(ip) 
		  << "(" << unsigned(dsize) << "," << unsigned(ssize) <<"," << unsigned(soffset) 	       
		  << ")]: (" << newid1 << "," << newid2  << ") -> " << newid << endl;
	}
	if (debug_set.count(newid1) || debug_set.count(newid2))
	{
	    debug_set.insert(newid);
	}
#endif
    }   
#ifdef VERBOSE
    if (V_INSTCOUNT(instcount)) { 
	log_f << hex << "mmr2m[" << instcount << "," << ip<< "] " << dst << "<-(" << REG_StringShort(src1)
	      << "," << mem_loc << ")" << endl;
    }
#endif

}

template <uint_fast8_t size>
TAINTSIGN merge_memimmval2mem (u_long dst, u_long src, u_long ip)
{
    nodeid newids[size], newid;
    u_long offset = 0, count;

    getMemNodeids(src, dst,ip, newids, size); //fixme...? 
    newid = immval_input_none(ip); 

    for (offset = 0; offset < size; ++offset) 
    {
	if (newids[offset] == NULL_NODE) 
	    newids[offset] = newid;
    }

    offset = 0;
    while (offset < size) { 
	count = set_mem_nodes(dst + offset, size - offset, newids + offset); 
	offset += count;
    }
}


template <uint_fast8_t size, uint_fast8_t doffset, uint_fast8_t soffset>
TAINTSIGN merge_regimmval2reg (int dst, int src, u_long ip)
{
    nodeid newids[size], newid;
    u_long offset;

    getRegNodeids<size,soffset>(src, dst, ip, newids);
    newid = immval_input_none(ip); 

    for (offset = 0; offset < size; ++offset) 
    {
	if (newids[offset] == NULL_NODE) 
	    newids[offset] = newid;
    }
    set_reg_nodes(dst, doffset, size, newids); 
    
}

TAINTSIGN get_mem_value()
{
    ADDRINT dvalue = *current_mem_loc;
    set_value(dvalue); 

}

TAINTSIGN get_reg_value(CONTEXT *ctxt, REG reg)
{
    ADDRINT dvalue = 0;
    if (!REG_is_xmm(reg)) { 
	dvalue= PIN_GetContextReg(ctxt, reg);	
    }
    set_value(dvalue);
}




#define COPY_REG2REG(size,od,os,dst,src)    INS_InsertCall(ins, IPOINT_BEFORE, \
							   (AFUNPTR)copy_reg2reg<size,od,os>, \
							   IARG_FAST_ANALYSIS_CALL, \
							   IARG_UINT32, dst, \
							   IARG_UINT32, src, \
							   IARG_INST_PTR, \
							   IARG_END);\
    INS_InsertCall(ins, IPOINT_AFTER,					\
		   AFUNPTR(get_reg_value),				\
		   IARG_FAST_ANALYSIS_CALL,				\
		   IARG_CONST_CONTEXT,					\
		   IARG_UINT32, dst,					\
		   IARG_END);

#define COPY_MEM2REG(size,od,dst)	INS_InsertCall(ins, IPOINT_BEFORE, \
						       AFUNPTR(copy_mem2reg<size, od>),	\
						       IARG_FAST_ANALYSIS_CALL,	\
						       IARG_UINT32, dst, \
						       IARG_MEMORYREAD_EA, \
						       IARG_INST_PTR,	\
						       IARG_END);	\
    INS_InsertCall(ins, IPOINT_AFTER,					\
		   AFUNPTR(get_reg_value),				\
		   IARG_FAST_ANALYSIS_CALL,				\
		   IARG_CONST_CONTEXT,					\
		   IARG_UINT32, dst,					\
		   IARG_END);

#define COPY_MEM2MEM(size) INS_InsertCall(ins, IPOINT_BEFORE, \
					  AFUNPTR(copy_mem2mem<size>),	\
					  IARG_FAST_ANALYSIS_CALL,	\
					  IARG_MEMORYWRITE_EA,		\
					  IARG_MEMORYREAD_EA,		\
					  IARG_INST_PTR,		\
					  IARG_END);			\
    INS_InsertCall(ins, IPOINT_AFTER,					\
		   AFUNPTR(get_mem_value),				\
		   IARG_FAST_ANALYSIS_CALL,				\
		   IARG_END);
	
#define COPY_REG2MEM(size,os,src)	INS_InsertCall(ins, IPOINT_BEFORE, \
						       AFUNPTR(copy_reg2mem<size, os>),	\
						       IARG_FAST_ANALYSIS_CALL,	\
						       IARG_MEMORYWRITE_EA, \
						       IARG_UINT32, src, \
						       IARG_INST_PTR,	\
						       IARG_END); \
    INS_InsertCall(ins, IPOINT_AFTER,				  \
		   AFUNPTR(get_mem_value),			  \
		   IARG_FAST_ANALYSIS_CALL,			  \
		   IARG_END);
	

#define COPY_IMMVAL2REG(size,od,dst) INS_InsertCall(ins, IPOINT_BEFORE, \
							  AFUNPTR(copy_immval2reg<size, od>),	\
							  IARG_FAST_ANALYSIS_CALL,\
							  IARG_UINT32, dst, \
							  IARG_INST_PTR, \
							  IARG_END);     

#define COPY_IMMVAL2MEM(size) INS_InsertCall(ins, IPOINT_BEFORE, \
						   AFUNPTR(copy_immval2mem<size>), \
						   IARG_FAST_ANALYSIS_CALL, \
						   IARG_MEMORYWRITE_EA,\
						   IARG_INST_PTR,	\
						   IARG_END);     


#define MERGE_REGREG2REG(sd,ss,od,os1,os2,dst,s1,s2) INS_InsertCall(ins, IPOINT_BEFORE, \
								     (AFUNPTR)merge_regreg2reg<sd,ss,od,os1,os2>, \
								     IARG_FAST_ANALYSIS_CALL, \
								     IARG_UINT32, dst, \
								     IARG_UINT32, s1, \
								     IARG_UINT32, s2, \
								     IARG_INST_PTR, \
								    IARG_END);\
    INS_InsertCall(ins, IPOINT_AFTER,					\
		   AFUNPTR(get_reg_value),				\
		   IARG_FAST_ANALYSIS_CALL,				\
		   IARG_CONST_CONTEXT,					\
		   IARG_UINT32, dst,					\
		   IARG_END);


#define MERGE_REGMEM2REG(sd,ss,od,os1,dst,s1) INS_InsertCall(ins, IPOINT_BEFORE, \
							     (AFUNPTR)merge_regmem2reg<sd,ss,od,os1>, \
							     IARG_FAST_ANALYSIS_CALL, \
							     IARG_UINT32, dst, \
							     IARG_UINT32, s1, \
							     IARG_MEMORYREAD_EA, \
							     IARG_INST_PTR, \
							     IARG_END);	\
    INS_InsertCall(ins, IPOINT_AFTER,					\
		   AFUNPTR(get_reg_value),				\
		   IARG_FAST_ANALYSIS_CALL,				\
		   IARG_CONST_CONTEXT,					\
		   IARG_UINT32, dst,					\
		   IARG_END);

#define MERGE_REGMEM2MEM(sd,ss,os1,s1) INS_InsertCall(ins, IPOINT_BEFORE, \
						      (AFUNPTR)merge_regmem2mem<sd,ss,os1>, \
						      IARG_FAST_ANALYSIS_CALL, \
						      IARG_MEMORYWRITE_EA, \
						      IARG_UINT32, s1,	\
						      IARG_MEMORYWRITE_EA, \
						      IARG_INST_PTR,	\
						      IARG_END);	\
    INS_InsertCall(ins, IPOINT_AFTER,					\
		   AFUNPTR(get_mem_value),				\
		   IARG_FAST_ANALYSIS_CALL,				\
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
