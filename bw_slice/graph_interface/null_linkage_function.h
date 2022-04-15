#ifndef LINKAGE_FUNCTION_H
#define LINKAGE_FUNCTION_H

#include "graph_interface.h"

#ifdef TAINT_STATS
extern struct taint_stats_profile tsp;
#endif


///////////////////////
// linkage functions //
///////////////////////


template <uint_fast8_t size, uint_fast8_t doffset, uint_fast8_t soffset> 
    TAINTSIGN copy_reg2reg(int dst, int src)
{
}

template<uint_fast8_t size, uint_fast8_t soffset> 
    TAINTSIGN copy_reg2mem (u_long mem_loc, int reg)
{
}

template<uint_fast8_t size, uint_fast8_t doffset> 
    TAINTSIGN copy_mem2reg (int reg, u_long mem_loc)
{
}

template<uint_fast8_t size>
TAINTSIGN copy_mem2mem (u_long dst, u_long src)
{
}



template <uint_fast8_t dsize, uint_fast8_t ssize, uint_fast8_t doffset, uint_fast8_t soffset1, uint_fast8_t soffset2> 
    TAINTSIGN merge_regreg2reg (int dst, int src1, int src2)
{
}

template <uint_fast8_t dsize, uint_fast8_t ssize, uint_fast8_t doffset, uint_fast8_t soffset>
    TAINTSIGN merge_regmem2reg (int dst, int src1, u_long mem_loc)
{
}

template <uint_fast8_t dsize, uint_fast8_t ssize, uint_fast8_t soffset>
TAINTSIGN merge_regmem2mem (u_long dst, int src1, u_long mem_loc)
{
}



template <uint_fast8_t size>
TAINTSIGN merge_memimmval2mem (u_long dst, u_long src, u_long ip)
{
}


template <uint_fast8_t size, uint_fast8_t doffset, uint_fast8_t soffset>
TAINTSIGN merge_regimmval2reg (int dst, int src, u_long ip)
{
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
