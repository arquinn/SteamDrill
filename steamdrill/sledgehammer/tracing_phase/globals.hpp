#include<unistd.h>

#include<chrono>
#include<deque>
#include<iostream>
#include<unordered_map>
#include<unordered_set>

#ifndef __GLOBALS_H_
#define __GLOBALS_H_


// stuff for statistics

using std::string;
using std::unordered_set;

namespace ThreadGroup {
class ThreadGroup;
}

namespace Globals {
typedef std::unordered_map<pid_t, ThreadGroup::ThreadGroup *> ThreadMap;
typedef ThreadMap::iterator ThreadMapIter;
typedef std::chrono::high_resolution_clock myClock;
typedef std::chrono::time_point<myClock> timePoint;
typedef std::chrono::milliseconds duration;

#define MS_CAST  std::chrono::duration_cast<std::chrono::milliseconds>

class signalled {
 public:
  pid_t pid;
  int status;

  signalled(pid_t p, int s): pid(p), status(s) {};
};

class Globals {
 private:
  std::unordered_map<pid_t, int> _expectingSigs;
  std::deque<signalled> waitQueue;

  ThreadMap  _threadGroups;
  u_long* _ppthreadLC;
  int _devFd;
  u_long _startClock; // here for stats file easyness

  pid_t basicWait(int * status = NULL);
  pid_t waitOne(int *status = NULL);
  pid_t pullFromQueue(int *status = NULL);
  ThreadMap::iterator getParent(pid_t pid);
  int numProcs;

  static class Globals *instance;
  timePoint startTime, stopTime;
  timePoint loadStartTime, loadStopTime;
  timePoint loadTracerStartTime, loadTracerStopTime;
  duration loopWork;

  int signalCount;
  int breakpointCount;

  pid_t sockPid;

 public:

  inline void addBreakpointCount() {breakpointCount++;}
  inline int getBreakpointCount() {return breakpointCount;}
  inline void startLoadTime() {loadStartTime = myClock::now();}
  inline void stopLoadTime() {loadStopTime = myClock::now();}
  inline void startLoadTracerTime() {loadTracerStartTime = myClock::now();}
  inline void stopLoadTracerTime() {loadTracerStopTime = myClock::now();}

  inline void setStartClock(u_long sc) { _startClock = sc;}

  static inline void initialize() {
    assert (!instance);
    instance = new Globals();

    instance->startTime = myClock::now();
    instance->breakpointCount = 0;
    instance->numProcs = 1; // always starts at uno
    instance->signalCount = 0;
  }
  static inline Globals *getInstance() {
    assert (instance);
    return instance;
  }
  static void getPpidTgid(pid_t pid, pid_t &ppid, pid_t &tgid);
  
  inline void addSignal() { signalCount++;};

  template<class T> inline void addLoopWork(T d) { loopWork += MS_CAST(d);}
  inline void setPthreadClock(u_long *clock) { _ppthreadLC = clock; }
  inline u_long getPthreadLogClock() { return *_ppthreadLC;}

  inline void setDevFd(int devFd) {_devFd = devFd;}
  inline int getDevFd(void) { return _devFd;};

  inline void setSockPid(pid_t pid) { sockPid = pid;}
  inline pid_t getSockPid() {return sockPid;}

  ThreadGroup::ThreadGroup* getThreadGroup(pid_t pid);
  //void addIgnorePid(pid_t, int expectSig);
  ThreadMapIter addThreadGroup(ThreadGroup::ThreadGroup *tg);

  pid_t wait(int *status = NULL, unordered_set<pid_t> waitFor = {});
  void waitFor(int *status, pid_t pid);

  void cleanupOnError();

  inline void finish() {stopTime = myClock::now();}
  void dumpStats(std::ostream &out);

  bool setDead() {
    return --numProcs <= 0;
  }

  pid_t getPid() { return _threadGroups.begin()->first;}
  uint64_t getTracerCount();
};
} /* end namespace Globals */

namespace std {
ostream& operator<<(ostream &out, const Globals::timePoint &time);
ostream& operator<<(ostream &out, const Globals::duration &time);
}

#endif
