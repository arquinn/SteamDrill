#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <linux/limits.h>

#include <cassert>
#include <cstring>
#include <ctime>
#include <cstdlib>

#include <unistd.h>
#include <fcntl.h>


#include <sstream>
#include <iterator>
#include <iostream>
#include <fstream>
#include <mutex>
#include <utility>

#include "util.h"
#include "sh_session.hpp"
#include "../utils/utils.hpp"


// paths relative to this .cpp file
// (this code assumes the binary will be in the same place)

#define PTRACE_PATH "../sledgehammer/tracing_phase/ptrace_wrapper"
#define RESUME_PATH "../../test/resume"
#define EGLIBC_PATH "../../eglibc-2.15/prefix/lib"
#define DEVSPEC_PATH "/dev/spec0"

//#define LOG_FORMAT "/dev/shm/log.%d"
//#define LOG_FORMAT "/tmp/fifo-log.%d.%d"
// #define CRTL_FORMAT "/tmp/done-fifo-log.%d"
// #define STATS_FORMAT "/dev/shm/log.%d.logging-stats"


using std::lock_guard;
using std::move;
using std::unique_lock;

namespace SHSession {

SHSession::SHSession(string   replay,
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
                     JumpCounter jc) {

  replaycnt = 0;
  replayargs = new const char*[256];

  ptracecnt = 0;
  ptraceargs = new const char*[256];
  char *attach = new char[80], *ckpt = new char[80], *exit = new char[80];
  char *level = new char[2], *stop = new char[80], *start = new char[80], *jumpCounter = new char[80];

  replayargs[replaycnt++] = "resume";
  replayargs[replaycnt++] = replay.c_str();
  replayargs[replaycnt++] = "-g";
  replayargs[replaycnt++] = "--pthread";
  replayargs[replaycnt++] = std::getPath(EGLIBC_PATH);

  if (exitClock > 0) {
    sprintf(exit, "--exit_clock=%u\n", exitClock);
    replayargs[replaycnt++] = exit;
  }

  if (startClock > 0) {
    sprintf (attach, "--attach_offset=%d,%u", startPid, startClock);
    replayargs[replaycnt++] = attach;
  }

  if (ckptClock > 0) {
    sprintf (ckpt, "--from_ckpt=%u", ckptClock);
    replayargs[replaycnt++] = ckpt;
  }
  replayargs[replaycnt++] = NULL;
  replayexec = std::getPath(RESUME_PATH);

  ptraceargs[ptracecnt++] = std::getPath(PTRACE_PATH);
  ptracecnt++; // one space for the cpid
  ptraceargs[ptracecnt++] = instpoints.c_str();
  ptraceargs[ptracecnt++] = tracepoints.c_str();
  ptraceargs[ptracecnt++] = lTracers.c_str();
  ptraceargs[ptracecnt++] = sharedObject.c_str();
  ptraceargs[ptracecnt++] = statsFile.c_str();

  ptraceargs[ptracecnt++] = "-l";
  sprintf(level, "%c", loglevel);
  ptraceargs[ptracecnt++] = level;

  if (stopClock != 0) {
    sprintf (stop, "%u", stopClock);
    ptraceargs[ptracecnt++] = "--stop";
    ptraceargs[ptracecnt++] = stop;
  }

  if (start != 0) {
    sprintf (start, "%u", startClock);
    ptraceargs[ptracecnt++] = "--start";
    ptraceargs[ptracecnt++] = start;
  }
	
  if (jc != DEFAULT) {
    sprintf(jumpCounter, "%s", jc == INST_ALL ? "all" : "opt");
    ptraceargs[ptracecnt++] = "--jumpCounter";
    ptraceargs[ptracecnt++] = jumpCounter;
  }

  for (auto output : outputs) {
    ptraceargs[ptracecnt++] = output.c_str(); // das wark?
  }
  ptraceargs[ptracecnt++] = NULL;
  ptraceexec = std::getPath(PTRACE_PATH);

  // for debugging
  _startPid = startPid;
  _startClock = startClock; 
  _stopClock = stopClock;
  _statsFile = statsFile;

  _devFd = open (DEVSPEC_PATH, O_RDWR);
  if (_devFd < 0) {
    SPDLOG_ERROR("cannot open devfd {}", errno);
    assert(0);
  }
  SPDLOG_DEBUG("sh ({}:{}) from ({},{}) -> ({})", replay, tracepoints, startPid, startClock, stopClock);

}


SHSession::~SHSession() {
  if (_devFd > 0)
    close(_devFd);
}


int SHSession::doEpoch() {
  SPDLOG_INFO("starting doEpochs\n");
  int rc = -1;
  pid_t ptracePid = 0;

  _replayPid = vfork ();
  if (_replayPid == 0) {
    // vfok doesn't allow us to do anything bug call execv
    //std::cerr << "whoopie calling resume!" << std::endl;
    //    for (int i = 0; i < replaycnt; ++i) { 
    //      std::cerr << (void *)replayargs[i] << std::endl;
    //      std::cerr << replayargs[i] << std::endl;
    //    }

    rc = execv (replayexec, (char **) replayargs);
    std::cerr << "execl of resume failed, rc=" << rc << ", errno=" << errno << std::endl;
    return -1;

  } else {
    _status = STARTING;
    _start = Timer::myClock::now();
    // std::cerr << "creating waiter!" << std::endl;
    //create our listening process
    // this one needs to be fork. 
    _waitOnReplay = fork();
    if(_waitOnReplay == 0){
      setpriority(PRIO_PROCESS, 0, 50);
      while(rc < 0){
        rc = wait_for_replay_group(_devFd, _replayPid);
      }
      //return 1;
      _exit(0);
    }
    SPDLOG_DEBUG("replay {0:d} waiter {0:d}\n",
                  _replayPid,
                  _waitOnReplay);
    std::cerr << "waiter boi!" << std::endl;
  }

  // Now attach to the epoch processes
  bool executing = false;
  do {
    // this should like, just work?
    rc = -1;
    // really early on in starting a replay you have to sometimes wait here.
    rc = wait_for_attach(_devFd, _replayPid); // this doesn't work! yet :)
    //rc = get_attach_status(_devFd, _replayPid);
    if (rc > 0) {
      char cpids[256];
      //sometimes we attach to a different process.
      //When we do this, we need to save the rc in case we are getting stats
      _attachPid = rc;
      sprintf (cpids, "%d", _attachPid);
      ptraceargs[1] = cpids;
      
      _ptracePid = vfork();
      if (_ptracePid == 0) {
        rc = execv(ptraceexec, (char **)ptraceargs);
        std::cerr << "ptrace_wrapper can't execl rc=" << rc
                  << " errno=" << errno << std::endl;
	_exit(-1);
      } else {
        _ptraceStart = Timer::myClock::now();
	setpriority(PRIO_PROCESS, 0, 50);
        _status = TRACING;
        executing = true;
        SPDLOG_INFO("{:d} runs ({:d},{:d}) -> ({:d})\n",
                    _attachPid, _startPid, _startClock, _stopClock);
      }
    }
  } while (!executing);
  return 0;
}

// this is going to go away!
void SHSession::wait() {
  SPDLOG_INFO("waiting for: {}", _waitOnReplay);
  int status;

  pid_t wpid = waitpid(_waitOnReplay, &status, 0);
  if (wpid < 0) {
    SPDLOG_ERROR("waitpid on {} failed! {}: {}", _waitOnReplay, errno, strerror(errno));;
  }
  wpid = waitpid(_replayPid, &status, 0);
  if (wpid < 0) {
    SPDLOG_ERROR("waitpid on {} failed! {}:{}", _replayPid, errno, strerror(errno));;
  }

  wpid = waitpid(_ptracePid, &status, 0);
  if (wpid < 0) {
    SPDLOG_ERROR("waitpid on {} failed! {}:{}", _ptracePid, errno, strerror(errno));;
  }

  SPDLOG_INFO("replay/ptrace finished!");
}



void SHSession::stats() {
  std::ofstream sfile(_statsFile, std::ios_base::app);
  sfile << "FForwardStart:" << _start << std::endl;
  sfile << "FForwardStop:" << _ptraceStart << std::endl;
}



  //std::ostream& operator<<(std::ostream &out, const SHSession &s) {
  //  return out << "(" << s._startPid << ", " << s._startClock  << ") - " << s._stopClock;
  //}
}; //namespace SHSession


