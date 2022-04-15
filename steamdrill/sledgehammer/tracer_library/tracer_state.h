#include <sys/user.h>
#include <sys/types.h>
#include <stdint.h>


#ifndef _TRACER_STATE_H
#define _TRACER_STATE_H

#include <shared_state.h>

#include "tracer_timer.h"
#include "config.h"

// 1 MB for shared state, 1 MB for undolog,
// 10 MB for tracer_log (this should be circular buffer
#define SHARED_STATE_SIZE 0x1000000
#define UNDO_LOG_SIZE     0x1000000
#define TRACER_LOG_SIZE   0x10000000
#define AL_OFFSET sizeof(struct sharedState)

// forward decl
struct PerfCounter;
struct sharedState {
  u_long *replayClock;     // pointer to the replay clock
  u_long *undologStart;    // pointer to the start of the undolog
  u_long *undologPtr;      // pointer to current element in undolog
  int     dev_fd;          // opened fd for devspec

  // for ignoring syscalls and libc ops during a tracer's execution
  int     ptraceSyscall;        // pointer to the ignore pointer

  u_long *tracerStackLow;  // low value of the stack for this .so
  u_long *tracerStackHigh; // high value of the stack for this .so

  u_long *tracerDataLow; // low value of the data section of this .so
  u_long *tracerDataHigh; // low value of the data section of this .so

  // stuffs for the access log
  u_long *acclogStart;     // pointer to the start of the accesslog
  u_long *acclogPtr;       // pointer to the current element in accessLog

  // stuff for output:
  int outputCount; // number
  // only allows 10 states.
  struct shared_state *output[10]; // pointer to the control buffers..
  //  struct shared_state *output;   // pointer to the control buffer

  // timing vars
  struct TracerTimer tracerTime;
  struct TracerTimer outputTime;
  u_long tracerCount;
  u_long ulogCount;
  enum JumpCounter jc;
};

struct threadState {
  uintptr_t stackLow;
  uintptr_t stackHigh;
  struct PerfCounter *condBranches;
  struct PerfCounter *nearCalls;

};



#endif /* _TRACER_STATE_H*/


