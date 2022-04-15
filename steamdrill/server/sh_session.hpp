#ifndef __SH_SESSION_H_
#define __SH_SESSION_H_

#include <sys/socket.h>
#include <netdb.h>
#include <sys/types.h>
#include <pthread.h>

#include <algorithm>
#include <cstdint>
#include <condition_variable>
#include <fstream>
#include <iterator>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
#include <unordered_map>

#ifdef SPDLOG_ACTIVE_LEVEL
#undef SPDLOG_ACTIVE_LEVEL
#endif
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_DEBUG
#include "spdlog/spdlog.h"

#include "../utils/utils.hpp"
#include "../config.h"

//Status for tracking progress
#define STATUS_STARTING 0
#define STATUS_EXECUTING 1
#define STATUS_LOGGING_DONE 2
#define STATUS_DONE 3

#define LOG_PREFIX_FORMAT "/dev/shm/log.%d"
#define ANALYZER_PREFIX_FORMAT "/dev/shm/%d"

#define TRACER_STOP '\u0001'

using std::condition_variable;
using std::mutex;
using std::shared_ptr;
using std::string;
using std::thread;
using std::unordered_map;
using std::vector;


// Info from description file
namespace SHSession {

class Line : public std::string {
  friend std::istream& operator>>(std::istream& is, Line& l)  {
    std::getline(is, l, TRACER_STOP);
    return is.ignore(1,'\n');
  }
};


class SHSession {
 private:
  std::string _statsFile;
  int _devFd;
  //int _tag;
  uintptr_t _startPid, _startClock, _stopClock; 
  // arguments: 
  const char **replayargs, **ptraceargs;
  const char *replayexec, *ptraceexec;
  int replaycnt, ptracecnt;

  // housekeeping for what's going on
  pid_t _replayPid, _waitOnReplay, _attachPid, _ptracePid;
  enum SessionStatus {STARTING, TRACING, DONE, ERROR};
  SessionStatus _status;

  // timers for statistics
  //struct timeval _tvStart, _tvPtraceStart, _tvPtraceEnd;
  Timer::timePoint _start, _ptraceStart, _end;

  // helper fxns
  const char* getPath(const char *);
  std::string getStatsFilename();


 public:
  //  SHSession(): _replay(0), _lTracer(""), _instpoints(""), _tracepoints(""), _sharedObject(""),
  //_statsFile(""), _startClock(0), _stopClock(0), _startPid(0), _ckptClock(0),
  //               _exitClock(0), _devFd(0), _createdLog(false), _loglevel('i'), _jc(INST_OPT) {
  //    spdlog::info("{} is blankerino", (void*)this);
  //  };

  SHSession(string   replay,
            string   lTracers,
            string   instpoints,
            string   tracepoints,
            string   sharedObject,
            vector<string> outputs,
            string   statsFile,
            uint32_t startClock,
            uint32_t startPid,
            uint32_t stopClock,
            uint32_t exitClock,
            uint32_t ckptClock,
            char     loglevel,
            JumpCounter jc);

  SHSession(const SHSession &ssh) = delete; //get rid of copy constructor
  SHSession(SHSession &&ssh) = delete; //noexcept;

  ~SHSession();
  int doEpoch();
  void wait();
  void cleanup();
  void stats();

  friend std::ostream& operator<<(std::ostream &, const SHSession&);

};

} /* namespace SHSession */


#endif
