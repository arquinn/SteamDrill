#include <sys/reg.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/ptrace.h>

#include <fstream>
#include <unordered_map>
#include <unordered_set>
#include <set>

#include "gadget.hpp"
#include "log.hpp"
#include "address.hpp"
#include "perf_counter.hpp"
#include "mem_region.hpp"

#ifndef __TASK_H_
#define __TASK_H_

// #define PARANOID // this model needs updating.

namespace ThreadGroup {

typedef std::unordered_set<MemRegion> RegionSet;
typedef std::unordered_set<std::string> SkipSet;

class Task {
 private:
  pid_t _pid;
  bool _isTgid;
  PerfCounter *_counter;
  uintptr_t _stackLow, _stackHigh;
  int _syscallNumber;

  std::unordered_map<Configuration::Address *, u_long> resolved_addrs;
  std::string _exe;

  bool _init;


  mutable int _memFd;
  mutable std::ifstream _regionFile;

  void initExe(void);
  void initMemFd(void) const;
  void initRegionFd(void);

  int setdataLarge(const unsigned long addr, const int len, const char *str) const;
  int getdataLarge(const unsigned long addr, const int len,  char **str) const;
  u_long getSyscall(void) const;

  static void resetFstream(std::ifstream &fs);

  void initStackBounds(bool mainPid);
 public:
  Task(pid_t pid, bool threadLeader = false) {
    _pid = pid;
    _isTgid = threadLeader;
    _memFd = -1;
    _stackLow = -1;
    _stackHigh = -1;
    _init = false;
    initRegionFd();
    initMemFd();
    initExe();
    initStackBounds(_isTgid);

    _syscallNumber = getSyscall();
    _counter = new PerfCounter(_pid);
  };


  void findFirstRegion(const std::string name,
                       const std::string flags,
                       u_long *start,
                       u_long *stop = NULL) const;

  void findAllRegions(const std::string match,
                      const SkipSet &skips,
                      RegionSet &regions) const;

  void findAllRegions(const MemRegionMatcher &matcher, RegionSet &regions) const;
  void findAllRegions(std::function<bool(MemRegion &&)> foo) const;

  void debugAllRegions(void) const;

  void getStackBounds(uintptr_t &stackLow, uintptr_t &stackHigh) const;

  int getdata(const long addr, const int len, char **str) const;
  int setdata(const long addr, const int len, const char *str) const;

  int getdata(const long addr, const int len, shared_ptr<char> *str) const;
  int setdata(const long addr, const int len, shared_ptr<char> str) const;

  void checkGets() const;

  bool getregs(struct user_regs_struct *regs) const;
  bool setregs(struct user_regs_struct *regs) const;

  bool getfpu(struct user_fpxregs_struct *regs) const;
  bool setfpu(struct user_fpxregs_struct *regs) const;


  inline pid_t getPid(void) const { return _pid;}

  inline bool getInit(void) const {return _init;}
  inline void setInit(void) {_init = true;}

  // ------------------------//
  // Manage mem Permissions  //
  // ------------------------//
  void changeProt(const u_long start,
                  const u_long length,
                  const u_long prot) const;

  u_long getFaultAddr(void) const;

  void getSiginfo(siginfo_t *input) const;


  // ------------------------//
  // Major major ptrace call //
  // ------------------------//
  void attach(void);
  void detach(void);

  //------------------------//
  // Control task execution //
  //------------------------//
  void cont(__ptrace_request ptraceType = PTRACE_CONT,
            int signalToSend = 0);

  int step(int signalToSend = 0);

  //may not be here..?
  void restartSyscall(void);

  long long getCounter(void) { return _counter->get();};
  void pauseCounter(void) { _counter->pause();};
  void startCounter(void) { _counter->start();};
  void addCounterSlack(int i) { _counter->addSlack(i);};

};
} /* Namespace ThreadGroup*/

#endif /* __TASK_H_*/
