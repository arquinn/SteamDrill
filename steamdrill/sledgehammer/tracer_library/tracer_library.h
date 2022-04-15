#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>


#include <sys/types.h>
#include <sys/user.h>


#ifndef __PTRACE_LIBRARY__
#define __PTRACE_LIBRARY__

#include "tracer_timer.h"
#include "shared_state.h"
#include "tracer_state.h"
#include "config.h" // configures the type of jumpCounter that we use

#define TRACER_LIBRARY __attribute__((annotate("tracer_library")))
#define TRACER_STATE   __attribute__((annotate("tracer_state")))
#define __rdtscp __builtin_ia32_rdtscp
//
// main initialization fxn
//

//extern struct TracerTimer stackDepthTimer;

extern struct sharedState TRACER_STATE *sharedState;
extern struct threadState TRACER_STATE *threadState;

extern u_long condBranches;
extern u_long calls;
// extern struct threadState TRACER_STATE *threadState;

// vars for assembly code to isolate tracers
extern TRACER_STATE u_long *stubStack asm("tracerStack");
extern TRACER_STATE uintptr_t stubTempEsp asm("tempEsp");
extern TRACER_STATE uintptr_t stubTempEip asm("origEip");
extern TRACER_STATE uintptr_t rtnTempEip asm("rtnEip");
extern TRACER_STATE uintptr_t stubECalls asm("extraCalls");
extern TRACER_STATE uintptr_t stubLoc asm("extraCallLoc");
extern TRACER_STATE uintptr_t stubJumpCounter asm("jmpCounter");


int TRACER_LIBRARY tracerLibInit(char *myRegion, enum JumpCounter type);
void TRACER_LIBRARY tracerLibDestroy();
int TRACER_LIBRARY tracerlogDebug(const char *fmt, ...);
void TRACER_LIBRARY tracerAddLog(const char *log);
int TRACER_LIBRARY tracerlogReset();
void TRACER_LIBRARY accesslogClear();
void TRACER_LIBRARY accesslogAdd(u_long *addr, u_int count);
void TRACER_LIBRARY accesslogMemcpy(u_long *addr, int size);
void TRACER_LIBRARY sharedStateInit(char *regionName);
pid_t TRACER_LIBRARY tracerGetpid();
pid_t TRACER_LIBRARY tracerGettid();

#include "tracer_perf_counter.h"

//int TRACER_LIBRARY tracerlogPrintf(int idx, const char *fmt, ...);
//int TRACER_LIBRARY tracerlogWrite( const void* start, int len);




inline __attribute__((always_inline)) uint64_t TRACER_LIBRARY ttime(void) {
#define USE_RDTSCP
#ifdef USE_RDTSCP
  unsigned int low;
  return __rdtscp(&low);// * HZ_TO_MS;
#else
  struct timespec ts;
  uint64_t rtnVal;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0) {
    tracerlogDebug("failed to get monotonic clock? %d\n", errno);
    return 0;
  }
  rtnVal = ts.tv_nsec / 1000 + ((uint64_t)ts.tv_sec * 1000000);
  return rtnVal;
#endif
};


inline __attribute__((always_inline))
void TRACER_LIBRARY resetTimer(struct TracerTimer *tt) {
  tt->total_us = tt->start = 0;
};

inline __attribute__((always_inline))
void TRACER_LIBRARY startTimer(struct TracerTimer *tt) {
  tt->start = ttime();
};

inline __attribute__((always_inline))
void TRACER_LIBRARY stopTimer(struct TracerTimer *tt) {
  uint64_t stop = ttime();
  tt->total_us += stop - tt->start;

  //tracerlogDebug("timer: %llu - %llu : %llu\n", tt->start, stop, tt->total_us);
  tt->start = 0;
};



//static int TRACER_STATE old_status = 0;
void TRACER_LIBRARY tracerBegin();
void TRACER_LIBRARY tracerEnd();

//
// compiler isolation support
//

//u_long* TRACER_LIBRARY undologInit();
//void TRACER_LIBRARY undologAdd(u_long *addr);
//void TRACER_LIBRARY undologMemcpy(u_long *start, int size);
//void TRACER_LIBRARY undologResetUpdates();
//void TRACER_LIBRARY undologClear();

//
// input logging support (for continuous tracing functions)
//

// the order of the pusha instruction:
// 0 :return address (from calling the tracerISO)
// 0:eax, 1:ecx, 2:edx, 3:ebx, 4:esp, 5:ebp, 6:esi, 7:edi

static inline uintptr_t TRACER_LIBRARY tracerGetEax() {
  return *(stubStack - 2);
}
static inline uintptr_t TRACER_LIBRARY tracerGetEcx() {
  return *(stubStack - 3);
}
static inline uintptr_t TRACER_LIBRARY tracerGetEdx() {
  return *(stubStack - 4);
}
static inline uintptr_t TRACER_LIBRARY tracerGetEbx() {
  return *(stubStack - 5);
}
static inline uintptr_t TRACER_LIBRARY tracerGetEbp() {
  return *(stubStack - 7);
}
static inline uintptr_t TRACER_LIBRARY tracerGetEsi() {
  return *(stubStack - 8);
}
static inline uintptr_t TRACER_LIBRARY tracerGetEdi() {
  return *(stubStack - 9);
}

//special cases:
static inline uintptr_t TRACER_LIBRARY tracerGetEsp() {
  return stubTempEsp;
}
static inline uintptr_t TRACER_LIBRARY tracerGetEip() {
  return stubTempEip;
}

static inline u_long TRACER_LIBRARY tracerReplayclock() {
  return *(sharedState->replayClock);
}

static inline u_long TRACER_LIBRARY tracerLogicalClock() {
  return stubJumpCounter + condBranches + calls;
}

//static inline u_long TRACER_LIBRARY tracerUpdatedMem();
//static inline u_long TRACER_LIBRARY tracerRetrotime();


//void TRACER_LIBRARY tracerReportMemory(void *start, int size);

uintptr_t TRACER_LIBRARY tracerGetAddress(const char *regionName,
                                          const char *regionFlags,
                                          uintptr_t offset);

static inline void TRACER_LIBRARY startRow(const int idx) {
  shared_startMods(sharedState->output[idx]);
}

static inline void TRACER_LIBRARY endRow(const int idx) {
  shared_syncMods(sharedState->output[idx]);
}

static inline void TRACER_LIBRARY tracerlogAddInt(const int idx, int val) {
  shared_addInt(sharedState->output[idx], val);
}

static inline void TRACER_LIBRARY tracerlogAddLongLong(const int idx, long long val) {
  shared_addLongLong(sharedState->output[idx], val);
}

static inline void TRACER_LIBRARY tracerlogAddShort(const int idx, short val) {
  // todo: consider a debugging output here?
  //  tracerlogDebug("tracerlogAddShot, %x, %x, %d\n", tracerGetEip(), idx, val);
  shared_addShort(sharedState->output[idx], val);
}

static inline void TRACER_LIBRARY tracerlogAddByte(const int idx, char val) {
  shared_addByte(sharedState->output[idx], val);
}

static inline void TRACER_LIBRARY tracerlogAddFmtString(const int idx,
                                                        const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  shared_addFmtString(sharedState->output[idx], fmt, ap);
  va_end(ap);
}

#endif

