#ifndef _POSIX_SOURCE
#define _POSIX_SOURCE
#endif /* _POSIX_SOURCE */

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <dlfcn.h>
#include <errno.h>
#include <cstdio>
#include <cstdint>

#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/user.h>

#include <initializer_list>
#include <iostream>
#include <iomanip>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <memory>

extern "C" {
#include "../tracer_library/tracer_state.h"
}

#ifndef __PTRACE_FUNCTIONS__
#define __PTRACE_FUNCTIONS__

#include "address_set.hpp"
#include "gadget.hpp"
#include "globals.hpp"
#include "inst_point.hpp"
#include "log.hpp"
#include "mem_protector.hpp"
#include "mem_region.hpp"
#include "tracer_object_config.hpp"
#include "task.hpp"
#include "tracer.hpp"


#define DEBUG_TRACER(x) (true)
#define GADGET_REGION_LEN 4096
#define STACK_LEN 0xa09000

using std::function;
using std::string;
using std::unordered_map;
using std::unique_ptr;
using std::vector;


typedef struct user_regs_struct regs_struct;

namespace ThreadGroup {

extern bool debugTracer;

class MmapParameters {
 public:
  size_t    len;  // the length of the mmap
  uintptr_t addr; // this address where this is mapped
  u_long    prot;
};

class MprotectParameters {
 public:
  u_long prot;
  u_long len;
  u_long addr;
};

class BrkParameters {
 public:
  uintptr_t addr;
};

#define NR_MMAP2 192
#define NR_MPROTECT 125
#define NR_BRK 45
class Syscall {
 public:
  int syscallNum;
  union  {
    MmapParameters mmap;
    MprotectParameters mprotect;
    BrkParameters brk;
  };

  Syscall(int num)
  {
    syscallNum = num;
  }

  static std::unique_ptr<Syscall>  getSyscall(regs_struct &regs);
  void updateSyscall(regs_struct &regs);
};

class TracerObject {
 private:
  unordered_map<string, uintptr_t> _symbols;
  int _stackalign;
 public:
  std::unique_ptr<Tracer::Gadget<2>> taskInitGadget;
  std::unique_ptr<Tracer::Gadget<0>> destroyFxnGadget;

  struct sharedState *state;
  TracerObject(void): state(nullptr), taskInitGadget(nullptr), destroyFxnGadget(nullptr) {};

  TracerObject(TracerObject &to) {
    // this presupposes that the TO is already loaded into this address space.
    _symbols = to._symbols;
    _stackalign = to._stackalign;
    state = to.state; // just copy over that raw pointer.

    taskInitGadget.reset(Tracer::makeFxnCallGadget<2>(getSymbol(TASK_INIT_FXN), to.getAlign()));
    destroyFxnGadget.reset(Tracer::makeFxnCallGadget<0>(getSymbol(TRACER_DESTROY_FXN), to.getAlign()));
  };

  TracerObject(const TracerObject *to) {
    // this presupposes that the TO is already loaded into this address space.
    _symbols = to->_symbols;
    _stackalign = to->_stackalign;
    state = to->state;

    taskInitGadget.reset(Tracer::makeFxnCallGadget<2>(getSymbol(TASK_INIT_FXN), to->getAlign()));
    destroyFxnGadget.reset(Tracer::makeFxnCallGadget<0>(getSymbol(TRACER_DESTROY_FXN), to->getAlign()));
  };


  uintptr_t getSymbol(string s) {
    auto it = _symbols.find(s);
    assert (it != _symbols.end());
    return it->second;
  }
  void addSymbol(string s, uintptr_t addr) {_symbols.emplace(s, addr); }

  int getAlign(void) const {return _stackalign;}
  void setAlign(int a) {_stackalign = a;}

};

enum CTracerCause {CSTEP, CSEG, CSYS};
enum TGStatus {STEP, ALIVE, DYING, DEAD};
class ThreadGroup {
 private:
  std::unordered_map<pid_t, Task*> _tasks;
  unordered_map<Task*,unique_ptr<Syscall>> _syscallInProgress;

  // both of these structures are dumb and bad
  std::vector<Tracer::Tracer *> _ctracers;
  std::unordered_set<Tracer::Tracer *> _activeTracers; // don't wanna push the same thing twice.


  std::unordered_map<uintptr_t, Tracer::Tracer *> _tracerIndex;
  std::unordered_map<std::string, u_long> _strings;
  unordered_map<uintptr_t, Tracer::InstPoint*> _instrPoints;

  unique_ptr<TracerObject> _sharedObject;
  ///////////////////////////////////////////////////
  // Data and Fxns for managing memory protections //
  ///////////////////////////////////////////////////

  pid_t _tgid;
  pid_t _ppid;
  Task *_currTask;

  Tracer::Tracer *_exitTracer;
  Tracer::Tracer *_threadStartTracer;
  Tracer::Tracer *_forkExitTracer;

  Tracer::Gadget<6> *_regionsAllocator;

  Tracer::Gadget<0> *_segfaultGadget;

  u_long _stackLow;
  u_long _stackHigh;
  u_long _nextRegion;
  u_long _endRegionBlock;

  ///////////////////////////////////////////////////
  // Stuff for managing memory protections         //
  ///////////////////////////////////////////////////

  MemProtector mp;


  uintptr_t resolveAddress(Configuration::Address *addr);
  void getNewRegion(u_long *start,
                    u_long *end,
                    u_long len = GADGET_REGION_LEN,
                    u_long prot = PROT_READ | PROT_EXEC);

  u_long getStringLocation(std::string s);

  void checkpointRegion(RegionSet &regions,
                        const std::string regionId);
  void checkpointRegions(RegionSet &regions, const MemRegionMatcher &matcher);
  void restoreRegions(RegionSet &regions);

  void unsetTracers(void);
  void setTracers(void);
  void finishSetTracers();
  void initStack(int align);

  void setInstPoint(Tracer::InstPoint *ip);
  void unsetInstPoint(Tracer::InstPoint *ip);

  void setTracer(Tracer::Tracer *t);
  void unsetTracer(Tracer::Tracer *t);

  Tracer::Tracer* buildTracer(Configuration::TracerConfiguration*);
  void rebuildTracer(Tracer::Tracer *tracer);
  void populateInstPoint(Tracer::InstPoint *instP);
  Tracer::InstPoint* buildInstPoint(Configuration::InstConfiguration*, uintptr_t);

  void bootstrapDlopen(void);
  bool mapTracers(void);
  void mapPoints(void);
  void mapInstPoints(void);
  void mapCTPoints(void);
  void dlerror(void);

  void loadTracerObject();

  void addThreadsInGroup(pid_t pid, bool attached = false);
  void getStatus(pid_t pid);

  bool callCTracer(Tracer::Tracer *tracer, struct user_regs_struct &regs, CTracerCause cause=CSEG);

  void instPointJmp(Tracer::InstPoint *instp, struct user_regs_struct &regs);
  void updateAddresses(struct sharedState *state, Tracer::AddressSet &as);


  void inline enableSingleStep(struct user_regs_struct &regs) {
    regs.eflags = regs.eflags | (1 << 8);
  }
  void inline disableSingleStep(struct user_regs_struct &regs) {
    regs.eflags = regs.eflags & ~(1 << 8);
  }

    // want to protect this with a template
  template<unsigned int N>
  uintptr_t getGadgetLocation(Tracer::Gadget<N> *g) {
    if (_nextRegion + g->getLen() > _endRegionBlock)
      getNewRegion(&_nextRegion, &_endRegionBlock);

    // determine where to place gadget (and update for next placement)
    uintptr_t returnLoc = _nextRegion;
    _nextRegion += g->getLen();

    // and insert the gadget
    _currTask->setdata(returnLoc, g->getLen(), g->getBuffer());
    g->setLocation(returnLoc);
    return returnLoc;
  }

  template <unsigned int N, typename... Args,
            std::enable_if_t<std::conjunction_v<std::is_integral<Args>...>, bool> = true>
  u_long executeGadget(Tracer::Gadget<N> *gadget, Args... args) {
    return executeGadget(false, gadget, args...);
  }

  template <unsigned int N, typename... Args,
            std::enable_if_t<std::conjunction_v<std::is_integral<Args>...>, bool> = true>
  u_long executeGadget(bool forFilter, Tracer::Gadget<N> *gadget, Args... args) {
    static_assert(sizeof ...(Args) == N, "incorrect # of arguments");

    pid_t wpid;
    struct user_regs_struct regs;
    int status;
    u_long returnCode = 0;
    Globals::Globals *g = Globals::Globals::getInstance();
    uintptr_t location = gadget->getLocation() ? gadget->getLocation() : getGadgetLocation(gadget);
    assert(_currTask);

    TRACE("executeGadget @ {:x}", location);
    // update the registers
    _currTask->getregs(&regs);
    if (N > 0)
      gadget->fillRegs(&regs, args...);

    // assign the stack correctly if possible
    if (_stackHigh) {
      regs.esp = _stackHigh - gadget->getStackOffset();
      TRACE("{:x} - {:x} sets stack to {:x}",
            _stackHigh, gadget->getStackOffset(),
            (unsigned)regs.esp);
    }

    regs.eip = location;
    _currTask->setregs(&regs);

#define SUPER_DEBUG 1
#if SUPER_DEBUG
    if (debugTracer) {
      NDEBUG("entering super debug on task {}", _currTask->getPid());
      STEP();
      _currTask->debugAllRegions();
      uintptr_t endLoc = gadget->getLen() + location;
      // dump stack:
      do {
        int rtn = Log::debugAll(_currTask, endLoc);
        if(rtn && forFilter && _segfaultGadget) {
          _currTask->getregs(&regs);
          INFO("segfault in tracer {} {:x}", Log::parseStatus(rtn), _currTask->getFaultAddr());
          auto segLoc = getGadgetLocation(_segfaultGadget);
          endLoc = segLoc + _segfaultGadget->getLen();
          regs.eip = segLoc;  // can you really just change eip? Seem like you'd need more?
          regs.eax = 0;       // just pretend like the filter returned 0 (das wark?)
          _currTask->setregs(&regs);
        }

        else if (rtn) {
          if (WSTOPSIG(status) == SIGBUS) {
            ERROR("did you truncate (or otherwise) break the shared memory output??");
          }
          else {
            _currTask->getregs(&regs);
            ERROR("status {}", Log::parseStatus(rtn));
            ERROR("should reach {:x}, regs {}",  gadget->getLen() + location, Log::parseRegs(regs));
          }
          siginfo_t inf;
          _currTask->getSiginfo(&inf);
          ERROR("siginf: {}", Log::parseSiginfo(inf));
          _currTask->debugAllRegions();
          _currTask->step(WSTOPSIG(status));
          THROW("segfault I think??");
        }
        else {
          _currTask->getregs(&regs);
          return regs.eax;
        }
      } while (true);
      
    }
#endif

    // run until we're finished with the tracer
    bool done = false;
    uintptr_t endLoc = gadget->getLen() + location;
    do {
      // run until we hit something
      _currTask->cont(PTRACE_CONT);
      g->waitFor(&status, _currTask->getPid());
      _currTask->getregs(&regs);
      returnCode = regs.eax;

      if (WIFSTOPPED(status) && WSTOPSIG(status) == SIGSEGV) {
        u_long faultAddr = _currTask->getFaultAddr();
        auto g = Globals::Globals::getInstance();
        RegionSet regions;

        // if we stopped because we're protecting this page,
        // unprotect and try again (don't worry about reprotecting)
        // there's a chance that this is actually gobblygook.
        if (mp.protecting(faultAddr)) {
          INFO("unprotecting the fault address {}", faultAddr);
          mp.unprotectOne(_currTask, faultAddr);
        }
        else if (forFilter && _segfaultGadget) {
          //          INFO("segfault on {} @ {:x}. save it!", _currTask->getFaultAddr(), regs.eip);
          auto segLoc = getGadgetLocation(_segfaultGadget);
          endLoc = segLoc + _segfaultGadget->getLen();
          regs.eip = segLoc;  // can you really just change eip? Seem like you'd need more?
          _currTask->setregs(&regs);
          returnCode = 0; // designed to indicate that we should filter the output.
        }
        else {
          ERROR("gadget segfaulted on {:x}", _currTask->getFaultAddr());
          ERROR("should reach {:x}, regs {}", gadget->getLen() + location, Log::parseRegs(regs));
          _currTask->debugAllRegions();
          THROW ("segfault in tracer!");
        }
      }
      else if (WIFSTOPPED(status) && WSTOPSIG(status) == SIGSTOP && regs.eip != endLoc) {
        INFO("err. Not stopping. keep trying at {:x}", regs.eip);
        STEP();
        done = false;
      }
      else if (regs.eip != endLoc) {
        ERROR("gadget ended prematurely! reg {} stat {}", Log::parseRegs(regs), Log::parseStatus(status));
        _currTask->debugAllRegions();
        done = true;
        returnCode = -1;
        THROW("premature tracer end!");
      }
      else {
        done = true;
      }
    } while(!done);

    TRACE("Gadget finished at {:x}", (unsigned)regs.eip);
    // return 0 if we see that the endpoint was swapped
    return endLoc == gadget->getLen() + location ? returnCode : 0;
  }


 public:
  ThreadGroup(pid_t pid, pid_t tgid);
  ThreadGroup(pid_t npid, pid_t tgid, const ThreadGroup *parent); //can you make this work??

  void addThread(pid_t pid);
  void initTaskStructure(Task *t = nullptr); // this maybe could be moved into addThread...?
  void closeTracerLibs(pid_t pid = 0);

  Task *getTask(pid_t pid) {
    assert (_tasks.find(pid) != _tasks.end());
    return _tasks.find(pid)->second;
  }
  void detach();
  void restart(pid_t attachPid);
  void cont(pid_t pid = 0, int signalRecd = 0);
  void step(pid_t pid, int signalRecd = 0);

  pid_t getPpid(void) { return _ppid;}
  std::vector<pid_t> getPids();

  void syscallLoop();
  void watchLoop(u_int endclock);
  TGStatus dispatch(int status, pid_t caughtPid, int &signalToSend);

  struct sharedState* getTracerState(void) {return _sharedObject->state;}

  std::unordered_map<pid_t, Task*>::iterator begin(void) { return _tasks.begin();}
  std::unordered_map<pid_t, Task*>::iterator end(void) { return _tasks.end();}

}; /* end ThreadGroup */
} /*end namespace ThreadGroup */

#endif /*__PTRACE_FUNCTIONS__*/
