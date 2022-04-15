#include <unordered_set>

#include <fcntl.h>
#include <dirent.h>

#include "util.h"

#include "thread_group.hpp"
#include "log.hpp"
#include "globals.hpp"

namespace std
{
ostream& operator<<(ostream &out, const Globals::timePoint &time) {
  auto d = MS_CAST(time.time_since_epoch());
  return out << d;
}
ostream& operator<<(ostream &out, const Globals::duration &d) {
  return out << d.count();
}
}

// init this to null...
namespace Globals {
Globals* Globals::instance = NULL;

pid_t Globals::basicWait(int *status) {
  pid_t rtn = waitpid(-1, status, __WALL);
  addSignal();
  return rtn;
}

pid_t Globals::waitOne(int *status) {
  int wpid = -1;
  ThreadMap::iterator it;

  do {
    if ((wpid = pullFromQueue(status)) == 0) {
      wpid = basicWait(status);
    }

    if (wpid == sockPid)
      return wpid;
    if (wpid < 0)
      return wpid;

    it = _threadGroups.find(wpid);
    if (it == _threadGroups.end()) {
      pid_t tgid, ppid;
      getPpidTgid(wpid, ppid, tgid);
      INFO("getppidtgid: {} {} {}", wpid, tgid, ppid);
      auto tgiter = _threadGroups.find(tgid);

      if (tgiter == _threadGroups.end()) {
        INFO("{}: new process!", wpid);
        numProcs++;
        auto piter = _threadGroups.find(ppid);
        auto tg = new ThreadGroup::ThreadGroup(wpid, tgid, piter->second);
        INFO("new tg {}", (void*)tg);
        tgiter = addThreadGroup(tg);
      }
      else {
        INFO ("{}: new thread ", wpid);
        tgiter->second->addThread(wpid);
        _threadGroups.insert(std::make_pair(wpid, tgiter->second));
      }
      INFO("continuing on {}", wpid);
      tgiter->second->cont(wpid);
    } //if
  } while (it == _threadGroups.end());

  return wpid;
}
pid_t Globals::pullFromQueue(int *status) {
  if (waitQueue.size() > 0) {
    auto rtn = waitQueue.front();
    waitQueue.pop_front();
    if (status) *status = rtn.status;
    return rtn.pid;
  }
  return 0;
}



void Globals::waitFor(int *status, pid_t pid) {
  int tempStatus;
  pid_t wpid;
  while ((wpid = basicWait(&tempStatus)) != pid) {
    INFO("waited on {} {}", wpid, Log::parseStatus(tempStatus));
    waitQueue.emplace_back(wpid, tempStatus);
  }
  *status = tempStatus;
}

void Globals::getPpidTgid(pid_t pid, pid_t &ppid, pid_t &tgid) {
  char filename[256];
  std::ifstream statFile;
  std::string line;

  sprintf(filename, "/proc/%d/status", pid);
  statFile.open(filename);
  if (!statFile.good()) {
    ERROR("can't open {}.. errno {}", filename, errno);
  }
  assert (statFile.good());

  std::getline(statFile, line);
  std::getline(statFile, line);
  std::getline(statFile, line);

  // magic #5 is sizeof tgid:
  tgid = stoul(line.substr(5), NULL, 0);
  std::getline(statFile, line);
  std::getline(statFile, line);
  ppid = stoul(line.substr(6), NULL, 0);

  statFile.close();
}

ThreadGroup::ThreadGroup *Globals::getThreadGroup(pid_t pid) {
  auto it = _threadGroups.find(pid);
  assert (it != _threadGroups.end());
  return it->second;
}

ThreadMapIter Globals::addThreadGroup(ThreadGroup::ThreadGroup *tg) {
  ThreadMapIter rtn;
  for (auto it = tg->begin(), end = tg->end(); it != end; ++it) {
    INFO("add thread group {}->{}", it->first, (void*)tg);
    rtn = _threadGroups.insert(std::make_pair(it->first, tg)).first;
  }
  return rtn;
}


pid_t Globals::wait(int *status, unordered_set<pid_t> pids) {

  if (pids.empty()) return waitOne(status);

  for (auto p : pids) {
    waitFor(status, p);

    if (!WIFSTOPPED(status) || WSTOPSIG(status) != (0x80 | SIGTRAP)) {
      ERROR("waited for weird reason {}", Log::parseStatus(*status));
      return 0;
    }
  }
  return 0;
}

void Globals::cleanupOnError() {
  std::unordered_set<ThreadGroup::ThreadGroup*> detached;
  for (auto it : _threadGroups) {
    if (detached.find(it.second) != detached.end()) {
      it.second->detach();
      detached.insert(it.second);
    }
  }
};

uint64_t Globals::getTracerCount() {
  uint64_t count = 0;
  std::unordered_set<ThreadGroup::ThreadGroup *>seen;

  for (auto groups : _threadGroups) {
    auto found = seen.find(groups.second);
    if (found == seen.end()) {
      count += groups.second->getTracerState()->tracerCount;
      seen.insert(groups.second);
    }
  }
  return count;
}


void Globals::dumpStats(std::ostream &out) {
  INFO("dumping stats");
  out << "Epoch:" << _startClock << std::endl;
  out << "SledgeStart:" << startTime << std::endl;
  out << "SledgeStop:"  << stopTime << std::endl;
  out << "LoadStart:"  << loadStartTime << std::endl;
  out << "LoadStop:"  << loadStopTime << std::endl;
  out << "LoadTracerStart:"  << loadTracerStartTime << std::endl;
  out << "LoadTracerStop:"  << loadTracerStopTime << std::endl;

  uint64_t tracerTime = 0;
  uint64_t tracerCount = 0;
  uint64_t ulogCount = 0;
  uint64_t tickCount = 0;

  std::unordered_set<ThreadGroup::ThreadGroup *>seen;

  for (auto groups : _threadGroups) {
    // add all of the thread stuffs:
    int64_t tempTicks = 0;
    int rc = get_counter(_devFd, groups.first, &tempTicks);
    assert (!rc);
    tickCount += tempTicks;
    auto found = seen.find(groups.second);
    if (found == seen.end()) {
      // state stuff:
      auto state = groups.second->getTracerState();
      tracerTime += state->tracerTime.total_us;
      tracerCount += state->tracerCount;
      ulogCount += state->ulogCount;
      seen.insert(groups.second);
    }
  }

#define RDTSC_KHZ 3100000

  out << "\n\n";
  out << "TracerTime: "     << tracerTime / RDTSC_KHZ << std::endl;
  out << "numTracer: "  << tracerCount << std::endl;
  out << "undoEntries: "     << ulogCount << std::endl;
  out << "NumBPs: "     << breakpointCount << std::endl;
  out << "signals: "     << signalCount - breakpointCount << std::endl;
  out << "clockTicks: " << tickCount << std::endl;
}
} // namespace Globals
