#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>
#include <linux/perf_event.h>

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <x86intrin.h>

#include "tracer_library.h"

#ifndef __PERFCOUNTER_H_
#define __PERFCOUNTER_H_


// raw perf_events events:
// conditional branches retired in user-space
#define BR_RET_COND  0x5101c4

// near calls retired in user-space
#define BR_RET_NCALL 0x5102c4


// tried and failed :/
//#define BR_RET_ALL 0x5100c4
//#define HW_INTERRUPTS 0x5100c8 //is right?3
//#define BR_EXEC_IJUMP 0x51c488
//#define BR_EXEC_ALL 0x51ff88
//#define BR_RET_NRET  0x5108c4
//#define BR_RET_NOT   0x5110c4
//
//#define BR_RET_FAR   0x5140c4


#define DEFAULT_TYPE PERF_TYPE_RAW

struct PerfCounter {
  int fd;
  int slack;
  struct perf_event_mmap_page *buf;
};

extern int hypervisor;

#define mem_barrier asm volatile("" ::: "memory")

inline __attribute__((always_inline)) long long rdpmc(unsigned index) {
  unsigned high, low;

  __asm__ volatile("rdpmc" : "=a" (low), "=d" (high) : "c" (index));
  return ((long long)low | ((long long)high) << 32);
}

inline __attribute__((always_inline)) long long PerfCounter_get(struct PerfCounter *pc, struct PerfCounter *condPC, struct PerfCounter *callPC, int end) {
  uint64_t rdpmc_count, offset, retVal;
  uint32_t seq_lock, low = 0, high = 0, cnt = 0;
  int32_t callLeftovers = 0, condLeftovers = 0;
  struct perf_event_mmap_page *buf = pc->buf;

  if (!hypervisor) {
    do {
      seq_lock = buf->lock;
      cnt ++;
      mem_barrier;
      offset = buf->offset;
      __asm__ volatile("rdpmc" : "=a" (low), "=d" (high) : "c" (buf->index - 1));
      mem_barrier;
    } while (buf->lock != seq_lock);
    rdpmc_count = (offset + low + (((long long)high) << 32)) & 0xffffffffffff;

    // should adjust slack by count??
    // this slack doesn't work if you have multiple iterations.
    condPC->slack -= (cnt + 1); // one extra for the hypervisor check
  }
  else {
    int rc = read(pc->fd, &rdpmc_count, sizeof(rdpmc_count));
    (void)rc; //get the compiler off my back
    //#define PARANOID
#ifndef PARANOID
        if (!end) {
          // we're in the beginning!
          // if we're the cond, then I know there's three extra calls through read.
          // if we're the call, then I experimentally learned that xxx of those calls comes before the read
          callPC->slack -= (3 * (pc == condPC)); // cond happens before calls, so these are all extras.
          callPC->slack -= (pc == callPC); // Any calls that I already made are extras for the start

          // before we get the value, we definitely check hypervisor--
          condPC->slack -= (pc == condPC);
        }
        else {
          // we're in the end!
          // if we're the cond, then there are no extra calls after the read.
          // if we're the call, then I know experimentally that xxx of those calls come afterwards:

          callPC->slack -= 3 * (pc == condPC); //cond happens after the calls, all 3 are extras
          callLeftovers -= 2 * (pc == callPC);  // two calls in read after call gets the value

          condLeftovers -= 2 * (pc == condPC); // after we get the value, we check end, let's say read checks for errno?

        }
#endif
        //assert(rc == sizeof(rdpmc_count)); playing on the wild side
    //if (rc != sizeof(rdpmc_count)) {
    //tracerlogDebug("cannot read the counter %d\n", errno);
    //assert (0);
    //}
    //tracerlogDebug("raw counter %llx\n", rdpmc_count);
  }
  retVal = rdpmc_count + pc->slack;
  callPC->slack += callLeftovers;
  condPC->slack += condLeftovers;
  return retVal;
}

inline __attribute__((always_inline)) void PerfCounter_addSlack(struct PerfCounter *pc, int i) {
  pc->slack += i;\
}

inline __attribute__((always_inline)) void PerfCounter_start(struct PerfCounter *pc) {
  int rc = ioctl(pc->fd, PERF_EVENT_IOC_ENABLE, 0);
  if (rc) {
    tracerlogDebug("Can't start counter, errno %d", errno);
    assert (0);
  }
}

inline __attribute__((always_inline)) void PerfCounter_pause(struct PerfCounter *pc) {
  int rc = ioctl(pc->fd, PERF_EVENT_IOC_DISABLE, 0);
  if (rc) {
    tracerlogDebug("Can't start counter, errno %d", errno);
    assert (0);
  }
}



struct PerfCounter* PerfCounter_create(int event, int type);
#endif /* __PERFCOUNTER_H_ */
