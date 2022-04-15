#define _LARGEFILE64_SOURCE
#define _GNU_SOURCE 1

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <x86intrin.h>


#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/reg.h>
#include <linux/perf_event.h>


#include "tracer_library.h"
#include "tracer_state.h"
#include "tracer_perf_counter.h"

#include "util.h"


#define __NR_perf_event_open	336

#define likely(x) __builtin_expect((x),1)
#define unlikely(x) __builtin_expect((x),0)

#define PAGE_NO(x) (void*)((int)x & PAGE_MASK)

// use to debug the call and condBranch counters
// #define COUNTER_DEBUG

// pointers for undo log and access log

//struct TracerTimer TRACER_STATE endTimer;
//struct TracerTimer TRACER_STATE stackDepthTimer;

// per process state (it's easier to debug w/ this structure)
struct sharedState TRACER_STATE *sharedState = NULL;

// per thread state
struct threadState TRACER_STATE *threadState = NULL;

// there's a potential rollover issue here, but it seems unlikely
u_long condBranches = 0;
u_long calls = 0;
int hypervisor = 1;
//PerfCounter_get(threadState->condBranches) +
//PerfCounter_get(threadState->nearCalls);


// vars for assembly code to isolate tracers
extern TRACER_STATE u_long *stubStack asm("tracerStack");
extern TRACER_STATE uintptr_t stubTempEsp asm("tempEsp");
extern TRACER_STATE uintptr_t stubTempEip asm("origEip");
extern TRACER_STATE uintptr_t rtnTempEip asm("rtnEip");
extern TRACER_STATE uintptr_t stubECalls asm("extraCalls");
extern TRACER_STATE uintptr_t stubLraceoc asm("extraCallLoc");
extern TRACER_STATE uintptr_t stubJumpCounter asm("jmpCounter");

asm(
    ".global tracerStack\n"
    ".global tempEsp\n"
    ".global origEip\n"
    ".global rtnEip\n"
    ".global instEnd\n"
    ".global cfPtr\n"
    ".global extraCalls\n"
    ".global extraCallLoc\n"
    ".global jmpCounter\n"
    ".global tmpAH\n"
    ".data\n"
    "tracerStack:\n"
    ".long 0\n" /* variable declared to point to my stack*/
    "tempEsp:\n"
    ".long 0\n" /* for temporarily holding esp */
    "origEip:\n"
    ".long 0\n" /* for holding the eip location how do we get this again...? */
    "rtnEip:\n"
    ".long 0\n" /* for holding the eip location how do we get this again...? */
    "instEnd:\n"
    ".long 0\n"
    "cfPtr:\n"
    ".long 0\n"
    "extraCalls:\n"
    ".long 0\n"
    "extraCallLoc:\n"
    ".long 0\n"
    "jmpCounter:\n"
    ".long 0\n"
    "tmpAH:\n"
    ".byte 0\n"

    );

inline __attribute__((always_inline))
void TRACER_LIBRARY startTimer(struct TracerTimer *tt);

inline __attribute__((always_inline))
void TRACER_LIBRARY stopTimer(struct TracerTimer *tt);

inline __attribute__((always_inline))
void TRACER_LIBRARY resetTimer(struct TracerTimer *tt);


//
// main initialization fxn
//

int TRACER_LIBRARY tracerLibInit(char *myRegionName, enum JumpCounter jc) {

  fprintf(stderr, "initialize with region %s\n", myRegionName);
  sharedStateInit(myRegionName);
  //sharedState->undologStart = undologInit();
  /*
  if (sharedState->undologStart == MAP_FAILED) {
    fprintf(stderr, "caouldn't initialize undolog! %d\n", errno);
    assert (0);
  }

  sharedState->undologPtr = sharedState->undologStart;
  */

  // create accesslog:
  sharedState->acclogStart = (u_long*)((char*)sharedState + AL_OFFSET);
  sharedState->acclogPtr =  sharedState->acclogStart;

  sharedState->jc = jc;
  stubStack = sharedState->tracerStackHigh - 1; // I think??

  // check for hypervisor!
  {
    char *line; // seems longer than needed;
    size_t size = 0; // unused
    int broken = 0;
    FILE *cpuinfo = fopen("/proc/cpuinfo", "rb");
    while (getdelim(&line, &size, '\n', cpuinfo) != -1) {
      if (!strncmp(line, "flags", 5)) {
        hypervisor = (strstr(line, "hypervisor") != NULL);
        broken = 1;
        break;
      }
    }
    free(line);
    fclose(cpuinfo);
    assert (broken);
  }
  //sharedState->ptraceSyscall = 0;
  return 0;
}

int TRACER_LIBRARY tracerLibDestruct() {
  sharedState->ptraceSyscall = 1;
  tracerlogDebug("destroing shared state at %ld after %d\n",
                 tracerReplayclock(), sharedState->tracerCount);

  tracerlogDebug("CLOSED FOR BUSINESS!\n");
  fsync(STDOUT_FILENO);
  close(STDOUT_FILENO);


  sharedState->ptraceSyscall = 0;
  return 0;
}



int TRACER_LIBRARY tracerLibThreadInit(uintptr_t threadStackLow, uintptr_t threadStackHigh) {
  // unclear what should live in this function...???
  // figure this out:
  int rc = 0, unused;
  sharedState->ptraceSyscall = 1; // set before anything in case this thread was crated DURING epoch

  // these initialize things that libc needs in order to work correctly...
  //uselocale(LC_GLOBAL_LOCALE);

  tracerlogDebug("%d: threadInit\n", tracerGettid());

  //tracerlogDebug("threadInit stack %x-%x shared %p thread %p (%p)\n",
  //threadStackLow,
  //threadStackHigh,
  //sharedState,
  //threadState,
  //&threadState);


  rc = set_ptrace_addr(sharedState->dev_fd, &(sharedState->ptraceSyscall));
  if (rc < 0) {
    fprintf(stderr, "cannot ptrace set addr!? errno %d\n", errno);
    return rc;
  }


  struct threadState *myThreadState = (struct threadState*) mmap(NULL,
                                                                 0x1000,
                                                                 PROT_READ | PROT_WRITE,
                                                                 MAP_ANON | MAP_PRIVATE,
                                                                 -1,
                                                                 0);
  if (myThreadState == MAP_FAILED) {
    tracerlogDebug("cannot map threadState, errno %d\n", errno);
    assert(0);
  }

  myThreadState->stackLow = threadStackLow;
  myThreadState->stackHigh = threadStackHigh;
  if (sharedState->jc != INST_ALL) {

    myThreadState->condBranches = PerfCounter_create(BR_RET_COND, DEFAULT_TYPE);
    myThreadState->nearCalls = PerfCounter_create(BR_RET_NCALL, DEFAULT_TYPE);

    // the stub is also a counter of sorts
    rc = add_counter(sharedState->dev_fd, (int*)&stubECalls);
    if (rc) {
      tracerlogDebug("add counter at %p failed %d errno %d\n", &(stubECalls), rc, errno);
    }
    assert (!rc);
  }

  // the stub JumpCounter is a counter
  rc = add_ticking_counter(sharedState->dev_fd, (int*)&stubJumpCounter);
  if (rc) {
    tracerlogDebug("add counter at %p failed %d errno %d\n", &(stubJumpCounter), rc, errno);
  }
  assert (!rc);

  // Existing fxn from jetstream that makes the kernel swap some user state for us:
  rc = set_pin_addr (sharedState->dev_fd, 0, myThreadState, (void**) &threadState, &unused);

  // these come from replay.h, but that's a pain to include :)
#define PIN_ATTACH_BLOCKED 2
#define PIN_NORMAL 0

  // If I'm not blocked, then I'm the captain now!
  if (!(rc & PIN_ATTACH_BLOCKED)) {
    threadState = myThreadState;
  }

  // one call to PerfCounter_start (w/ one ioctl) (inlined)
  // one call to PerfCounter_addSlack (inlined)

  if (sharedState->jc != INST_ALL) {
    PerfCounter_addSlack(myThreadState->nearCalls, -3); // is right??

    PerfCounter_start(myThreadState->nearCalls);
    PerfCounter_start(myThreadState->condBranches);
  }

  sharedState->ptraceSyscall = 0;

  return 0;
}


//
// tracer library I/O support
//
/*
int TRACER_LIBRARY tracerWrite(const int fd, const void *msg, const int len) {
  int rc = write(fd, msg, len);
  if (rc != len) {
    tracerlogDebug("hmm, write only returns %d (of %d) bytes", rc, len);
  }
  if (rc <= 0) {
    // there are many reasons this might happen:
    try_to_exit(sharedState->dev_fd, 0); // not sure this is the right argument?
    // I think maybe just die??
    // tracerlogDebug("cannot write to tracerlog? %d %d", rc, errno);
    assert (0);
  }
  return rc;
  }*/

//
// tracerlogsupport
//
void TRACER_LIBRARY tracerAddLog(const char *log) {
  assert (log);

  char debugFile[128];
  int debuggingFd;

// this is 64 MB:
  fprintf(stderr, "opening writer! for %s\n", log);
  assert (sharedState->outputCount < 10);
  sharedState->output[sharedState->outputCount] = shared_writer(log);
  sharedState->outputCount ++;
  //sharedState->output = shared_writer(log);
  sprintf(debugFile, "%s.debug", log);
  debuggingFd = open(debugFile, O_RDWR | O_CREAT | O_LARGEFILE, 0644);

  dup2(debuggingFd, STDOUT_FILENO);
  dup2(debuggingFd, STDERR_FILENO);

  tracerlogDebug("tracerlogAdded %s into %x\n", log, sharedState->outputCount - 1);
}


/*int TRACER_LIBRARY tracerlogPrintf(int idx, const char *fmt, ...) {
  va_list ap;

  // before you do any of this fancy stuff, dump the damn thing:
  //va_start(ap, fmt);
  //vfprintf(stderr, fmt, ap);
  //va_end(ap);
  //fprintf(stderr, "\n");
  va_start(ap, fmt);
  int rtn = shared_addMsg(sharedState->output[idx], fmt, ap);
  va_end(ap);
  return rtn;
  }*/


int TRACER_LIBRARY tracerlogDebug(const char *fmt, ...) {
  va_list ap;
  int rtn;

  va_start(ap, fmt);
  rtn = vfprintf(stderr, fmt, ap);
  va_end(ap);
  fflush(stderr); //will this force it to go to ouput? (I hope?)
  return rtn;
}

/*
u_long* TRACER_LIBRARY undologInit() {
  return (u_long *)mmap(NULL, UNDO_LOG_SIZE,
                        PROT_EXEC | PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
}

void TRACER_LIBRARY undologClear() {
  if (likely(sharedState->undologPtr != NULL))
    sharedState->undologPtr = sharedState->undologStart;
}

void TRACER_LIBRARY undologResetUpdates() {
  u_long *addr, *curr, val;
  curr = sharedState->undologPtr;
  sharedState->ulogCount += ((curr - sharedState->undologStart) / 2);

  while (curr != sharedState->undologStart) {
    addr = (u_long*)*(curr - 2);
    val = *(curr - 1);

    //tracerlogDebug("ulReset %p <- %lx (%p, %p)\n", (void*)addr, val,
    //(void*)(curr - 2), (void*)(curr - 1));
    *addr = val;
    curr -=2;
  }
}

void TRACER_LIBRARY undologAdd(u_long *addr) {
  if (likely(addr < sharedState->tracerStackLow || addr >= sharedState->tracerStackHigh ||
             addr < sharedState->tracerDataLow || addr >= sharedState->tracerDataHigh)) {

    // if we assert that tracers are memory safe, then this ignore is always false(!)
#define MEM_SAFE
#ifndef MEM_SAFE
    int ignore =  ((u_long*)sharedState <= addr &&
                   addr < ((u_long*)sharedState) + sizeof(sharedState));

    if (likely(!ignore)) {
#endif
      //tracerlogDebug("ulAdd %p %lx (%p, %p)\n", (void*)addr, *addr,
      //               sharedState->undologPtr, sharedState->undologPtr + 1);

      *(sharedState->undologPtr) = (u_long)addr;
      (sharedState->undologPtr)++;
      *(sharedState->undologPtr) = *addr;
      (sharedState->undologPtr)++;
#ifndef MEM_SAFE
    }
#endif
  }
}

void TRACER_LIBRARY undologMemcpy(u_long *addr, int size) {
  u_long *curr = addr;
  while (curr < (addr + size)) {
    undologAdd(curr);
    curr += 4;
  }
  } */

//
// input logging support (for continuous tracing functions)
//

// was there some way to ignore this?
void inline TRACER_LIBRARY accesslogClear() {
  sharedState->acclogPtr = sharedState->acclogStart;
}

void TRACER_LIBRARY accesslogAdd(u_long *addr, u_int count) {
  /*tracerlogDebug("accesslogAdd w/ %p %p %p %p\n", addr, sharedState->acclogPtr,
    sharedState->tracerStackHigh,
    sharedState->tracerStackLow);
  */
  if (likely(sharedState->acclogPtr != NULL)) {
    if (addr >= sharedState->tracerStackHigh || addr < sharedState->tracerStackLow ||
        addr >= sharedState->tracerDataHigh || addr < sharedState->tracerDataLow) {
      /*
        int ignore =
        ((u_long*)sharedState <= addr && addr < ((u_long*)sharedState) + sizeof(sharedState)) ||
        ((u_long*)threadState <= addr && addr < ((u_long*)threadState) + sizeof(threadState));

        if (likely(!ignore)) {
      */
      *(sharedState->acclogPtr) = (u_long)addr;
      (sharedState->acclogPtr)++;

      //}
    }
  }
}

void TRACER_LIBRARY accesslogMemcpy(u_long *addr, int size) {
  u_long *curr = addr;
  while (curr < (addr + size)) {
    accesslogAdd((u_long *)curr, 0);
    curr += 4;
  }
}

void TRACER_LIBRARY sharedStateInit(char *regionName) {
  int fd = open(regionName, O_RDWR);
  if (fd < 0) {
    fprintf(stderr, "Cannot open %s, errno %s (%d)\n", regionName, strerror(errno), errno);
    return;
  }

  assert (sizeof(struct sharedState) < 0x1000);
  // maybe this isn't mmap??
  sharedState = (struct sharedState*) mmap(NULL,
                                           SHARED_STATE_SIZE,
                                           PROT_READ | PROT_WRITE,
                                           MAP_SHARED,
                                           fd,
                                           0);
  if (sharedState == MAP_FAILED) {
    fprintf(stderr, "Cannot map sharedState, %s (%d)\n", strerror(errno), errno);

    assert(0); //because I haven't bothered to fix the eh_frame things, these give weird results
  }

  int rc = devspec_init(&(sharedState->dev_fd));
  if (rc < 0) {
    fprintf(stderr, "Cannot open devspec! errno %d\n", errno);
    assert (0);
  }

  // setup the replay clock
  sharedState->replayClock = map_shared_clock(sharedState->dev_fd);
  sharedState->outputCount = 0;
  close(fd);
}

pid_t TRACER_LIBRARY tracerGetpid() {
  return get_log_tgid(sharedState->dev_fd);
}

pid_t TRACER_LIBRARY tracerGettid() {
  return get_log_id(sharedState->dev_fd, 0);
}

  // this didn't like being inlined for some reason :(
struct PerfCounter* PerfCounter_create(int event, int type) {

  struct perf_event_attr pe;
  struct PerfCounter *pc = NULL;
  int fd;

  memset(&pe, 0, sizeof(struct perf_event_attr));
  pe.size = sizeof(struct perf_event_attr);
  pe.type = type;
  pe.config = event;
  pe.disabled = 1;
  pe.exclude_kernel = 1;
  pe.exclude_hv = 1;
  if (!hypervisor)
    pe.sample_type = PERF_SAMPLE_READ;

  fd = syscall(__NR_perf_event_open, &pe, 0, -1, -1, 0);

  if (fd < 0) {
    tracerlogDebug("Can't build perfcounter, errno %d", errno);
    assert (0);
  }

  pc = (struct PerfCounter*) malloc(sizeof(struct PerfCounter));
  pc->fd = fd;
  pc->slack = 0;

  if (!hypervisor)
    pc->buf = mmap(NULL, PAGESIZE, PROT_READ, MAP_SHARED, pc->fd, 0);
  else
    pc->buf = NULL;

  // I'm not sure that I *actually* need to do this reset. But, whatever *shrug*
  int rc = ioctl(pc->fd, PERF_EVENT_IOC_RESET, 0);
  if (rc) {
    tracerlogDebug("Can't reset counter, errno %d", errno);
    assert (0);
  }

  // tell the kernel about this guy:
  rc = add_perf_fd(sharedState->dev_fd, fd);
  assert (!rc);

  rc = add_counter(sharedState->dev_fd, &(pc->slack));
  if (rc) {
    tracerlogDebug("add counter at %p failed %d errno %d\n", &(pc->slack), rc, errno);
  }
  assert (!rc);

  return pc;
}


//static int TRACER_STATE old_status = 0;
void TRACER_LIBRARY tracerBegin() {
  // at the beginning of time:
  sharedState->ptraceSyscall = 1;

  if (sharedState->jc != INST_ALL) {
    //#define PARANOID
#ifdef PARANOID
    PerfCounter_pause(threadState->condBranches);
    PerfCounter_pause(threadState->nearCalls);
    PerfCounter_addSlack(threadState->nearCalls, -3 + stubECalls); // tracer begin and two ioctl?

    condBranches = (u_long)PerfCounter_get(threadState->condBranches, threadState->condBranches, threadState->nearCalls, 0);
    calls = (u_long)PerfCounter_get(threadState->nearCalls, threadState->condBranches, threadState->nearCalls, 0);

#else
    // needs to happen before the calls to _get (!)
    PerfCounter_addSlack(threadState->nearCalls, -2 + stubECalls); //tracerBegin and the gadget itself
    PerfCounter_addSlack(threadState->condBranches, -1); // check of counter type

    condBranches = (u_long)PerfCounter_get(threadState->condBranches, threadState->condBranches, threadState->nearCalls, 0); 
    calls = (u_long)PerfCounter_get(threadState->nearCalls,  threadState->condBranches, threadState->nearCalls, 0);
#endif


    // need to update this slack value for our changes below..
    // two call to PerfCounter_pause (w/ one ioctl w/ an internal __kernel_vsyscall) (inliend)
    // one call to tracerBegin()
    // one for a thunk (?)

  }
  accesslogClear(); // not sure what this does?


  startTimer(&(sharedState->tracerTime));
  sharedState->tracerCount++;

  //undologClear();

  //  tracerlogDebug("starting tracer, eip=0x%lx clock=%lx\n", tracerGetEip(), tracerReplayclock());
  // dump all regs:
  //  tracerlogDebug("stack %p eax %lx ecx %lx edx %lx ebx %lx ebp %lx esi %lx edi %lx esp %lx eip %lx\n",
  //stubStack,
  //tracerGetEax(), tracerGetEcx(), tracerGetEdx(), tracerGetEbx(),
  //tracerGetEbp(), tracerGetEsi(), tracerGetEdi(), tracerGetEsp(), tracerGetEip());

  // #define EIP_CHECK 0x8060214
#ifdef EIP_CHECK
  if (tracerGetEip() == EIP_CHECK) {
    tracerlogDebug("tid=%x, clock=%lx, eip=%lx\n", tracerGettid(), tracerReplayclock(), tracerGetEip());
  }
#endif

  // #define COUNTER_DEBUG
#ifdef COUNTER_DEBUG
  if (sharedState->jc != INST_ALL) {

    tracerlogDebug("tid=%d, eip=%lx, clock=%lx, stubbly=%x, cond=(%lx, %lx), calls=(%lx, %x), stub=%x\n",
                   tracerGettid(), tracerGetEip(), tracerReplayclock(),
                   stubJumpCounter,
                   condBranches,
                   threadState->condBranches->slack,
                   calls,
                   threadState->nearCalls->slack,
                   stubECalls);
    stubLoc = 0;
  }
  else {
    tracerlogDebug("tid=%d, eip=%lx, clock=%lx, tubbly=%x\n", tracerGettid(), tracerGetEip(),
                   tracerReplayclock(), stubJumpCounter);
  }
#endif
  stubECalls = 0;

  //#define PROGRESS
#ifdef PROGRESS
  tracerlogDebug("eip=%lx\n", tracerGetEip());
#endif /* PROGRESS */
  //  tracerlogDebug("oh no! (%lx, %lx): calls=%llx (%llx, %x)\n",

}

void TRACER_LIBRARY tracerEnd() {

  //undologResetUpdates();
  stopTimer(&(sharedState->tracerTime));

  if (sharedState->jc != INST_ALL) {
#ifdef PARANOID
    PerfCounter_start(threadState->nearCalls);
    PerfCounter_start(threadState->condBranches);
#else
    uint64_t endCalls = PerfCounter_get(threadState->nearCalls,  threadState->condBranches, threadState->nearCalls, 1); //calls gets to go first. It's much easier to count them
    uint64_t endCond = PerfCounter_get(threadState->condBranches, threadState->condBranches,  threadState->nearCalls, 1);

    PerfCounter_addSlack(threadState->condBranches, condBranches - endCond);
    PerfCounter_addSlack(threadState->nearCalls, calls - endCalls);
#endif

  }
  sharedState->ptraceSyscall = 0;
}
