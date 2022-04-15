// ptrace_wrapper.cpp -- driver for an SH query.
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <errno.h>

#include <sys/ptrace.h>
#include <sys/reg.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/user.h>
#include <sys/time.h>

#include <arpa/inet.h>

#include <cassert>
#include <sstream>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <vector>
#include <unordered_set>


// this gets us some NW stuffs.
#include "../server/streamnw.h"
#include "../../config.h"
#include "util.h"
#include "args.hxx"
#include "spdlog/spdlog.h"

//includes from this directory
#include "configuration.hpp"
#include "thread_group.hpp"
#include "log.hpp"
#include "globals.hpp"

#ifdef PARANOID
#include <mcheck.h>
#endif

#ifdef PARANOID
#define INTERESTING(x) ((x) == 97069 || (x) == 97573 || (x) == 97575 || (x) == 97587 )

//#define INTERESTING(x) (false)
#endif
#define BUFFER_SIZE 1024

//new system calls in order to setup ptrace output mappings when we don't use fork
#define NR_ptrace_outputs_before 137
#define NR_ptrace_outputs_after 167

static void signalDone() {
  auto c = Configuration::Configuration::getInstance();
  STATUS("we have {} logs\n", c->getTracerLogs().size());
  for (auto l : c->getTracerLogs()) {
    auto shared = shared_writer(l.c_str());
    STATUS("{} wrote {}", l.c_str(), shared->header->size);
    shared_finish(shared);
  }
}

static inline int checkClock(u_long pthreadLogClock, u_long end_clock) {
  if (pthreadLogClock > end_clock) {
    return 1;
  }
  return 0;
}


int main(int argc, char *argv[])
{
#ifdef PARANOID
  mtrace();
#endif
  int status, rc, signalToSend, devFd;
  u_long stopClock = 0;
  pid_t tpid, cpid;
  bool finished = false;

  ThreadGroup::ThreadGroup *tg = nullptr;
  Globals::timePoint start, end;
  Globals::Globals *g = nullptr;
  std::ofstream statsStream;

  Globals::Globals::initialize();
  g = Globals::Globals::getInstance();

  args::ArgumentParser parser("Trace a replay process.", "A.R.Q.");
  args::Group group(parser, "Required arguments:", args::Group::Validators::All);
  args::Flag step(parser, "step", "Step through tracing", {'s', "step"});
  args::ValueFlag<char> logLevel(parser, "logLevel", "level of logging", {'l', "level"});

  args::ValueFlag<u_long> startClock(parser, "startClock", "Clock to start replay after.", {"start"});
  args::ValueFlag<u_long> aStopClock(parser, "stopClock", "Clock to stop replay before.", {"stop"});
  args::ValueFlag<string> jumpCounter(parser, "jumpCounter", "Type of JumpCounter to use.", {"jumpCounter"});

  args::Positional<int> tpidArg(group, "tracee", "process to trace");
  args::Positional<std::string> ipoints(group, "instpoints",  "File containing instr configs");
  args::Positional<std::string> tpoints(group, "tracepoints", "File containing continuous configs");
  args::Positional<std::string> lpoints(group, "keypoints", "File containing key points in program");
  args::Positional<std::string> tobject(group, "tracerobject", "SO for tracer boject");

  // the loggingFilename needs some additional coordination.
  args::Positional<std::string> statsFilename(group, "STATS FILENAME", "where to store the stats for this trace");
  args::PositionalList<string> logfiles(group, "LOGGING FILENAME", "the log files for this query");

  args::HelpFlag help(parser, "help", "Display help", {'h', "help"});

  start = Globals::myClock::now();
  try {
    parser.ParseCLI(argc, argv);
  }
  catch (args::Help) {
    std::cout << parser << std::endl;
    return 0;
  }
  catch (args::ParseError e) {
    ERROR("{}", e.what());
    std::cout << parser << std::endl;
    return -1;
  }
  catch (args::ValidationError e) {
    ERROR("{}", e.what());
    std::cout << parser << std::endl;
    return -2;
  }

  tpid = args::get(tpidArg);

  if (logLevel) {
    switch (args::get(logLevel)) {
      case 'i':
        Log::initLogger(spdlog::level::info);
        break;
      case 'd':
        Log::initLogger(spdlog::level::debug);
        break;
      case 'e':
        Log::initLogger(spdlog::level::err);
        break;
      case 't':
        Log::initLogger(spdlog::level::trace);
        break;
      default:
        ERROR("I don't understand log level {}", args::get(logLevel));
        Log::initLogger(spdlog::level::info);
    }
    INFO("log level is {}", args::get(logLevel));
  }
  else
    Log::initLogger(spdlog::level::info);


  if (args::get(step))
    Log::doSteps = true;
  else
    Log::doSteps = false;


  statsStream.open(args::get(statsFilename), std::ios_base::app);
  if (statsStream.bad())  {
    ERROR("can't open {}, errno {}", args::get(statsFilename), errno);
    return 1;
  }
  INFO("opened {}", args::get(statsFilename));
  if (aStopClock) {
    stopClock = args::get(aStopClock);
  }

  rc = devspec_init (&devFd);
  assert(!rc);
  g->setDevFd(devFd);


  rc = ignore_segfaults(g->getDevFd(), 1, tpid);
  // assert(!rc);

  STEP();
  JumpCounter jc = INST_OPT;
  if (jumpCounter && args::get(jumpCounter) == "all") jc = INST_ALL;
  Configuration::Configuration::load(args::get(ipoints),
                                     args::get(tpoints),
                                     args::get(lpoints),
                                     args::get(tobject),
                                     args::get(logfiles),
                                     jc);

  g->setPthreadClock(map_other_clock(g->getDevFd(), tpid));
  Tracer::initStandardGadgets();

  // attach to the thread group
  pid_t tgid, ppid;
  Globals::Globals::getPpidTgid(tpid, ppid, tgid);
  tg = new ThreadGroup::ThreadGroup(tpid, tgid);
  g->addThreadGroup(tg);
  STEP();
  STATUS("created and added the main jane thread group");

  if (args::get(startClock)) {
    tg->restart(tpid);
    INFO("start clock set for {} shared says: {}", args::get(startClock), g->getPthreadLogClock());
    STATUS("restarted the thread group (what exactly does this do?)");
  }
  g->setStartClock(args::get(startClock));


  //there CAN be more logic to this.. but this is the simplest:
  //  get a ping when we want to run the next trace:
  std::vector<uintptr_t> pingList = Configuration::Configuration::getInstance()->getPingList();
  pingList.push_back(stopClock > 0 ? stopClock : LONG_MAX);
  auto pingListIt = pingList.begin();
  rc = set_report_syscall(g->getDevFd(), tpid, *pingListIt);

  end = Globals::myClock::now();
  // kick all of the replay_threads at the same time
  STEP();
  STATUS("kicking the thread group..leader?");
  tg->cont();
  STATUS("kicked!");

  start = Globals::myClock::now();

  do {
    cpid = g->wait(&status);
    if (cpid <= 0) {
      try_to_exit (g->getDevFd(), cpid);
      STATUS("tired exit");
      finished = true;
      break;
    }
    else if (stopClock > 0 && checkClock(g->getPthreadLogClock(), stopClock)) {
      try_to_exit(g->getDevFd(), cpid);
      finished = true;
      break;
    }
    else if (checkClock(g->getPthreadLogClock(), *pingListIt)) {
      pingListIt++;
      WARN("updating ping list notification to {}", *pingListIt);
      rc = set_report_syscall(g->getDevFd(), tpid, *pingListIt);
    }
    // this is, absurd?
    tg = g->getThreadGroup(cpid);
    ThreadGroup::TGStatus tgstatus = tg->dispatch(status, cpid, signalToSend);

    end = Globals::myClock::now();
    if (tgstatus == ThreadGroup::TGStatus::ALIVE) {
      set_active_tracers(g->getDevFd(), cpid,  0); // no active tracers
        tg->cont(cpid, signalToSend);
    }
    else if (tgstatus == ThreadGroup::TGStatus::STEP) {
      set_active_tracers(g->getDevFd(), cpid, 1);
      tg->cont(cpid, signalToSend);
    }

    else if (tgstatus == ThreadGroup::TGStatus::DYING) {
      if (g->setDead()) {
        INFO("globals says we are dead dead.");
        finished = true;
      }
      else {
        INFO("One dead... more to go?");
      }
    }
    else if (tgstatus == ThreadGroup::TGStatus::DEAD) {
      INFO("tgstatus says we are dead dead.");
      finished = true;
    }
  } while (!finished);

    // todo: flush and whatever the shared_state (don't need it for seq tests)
  STATUS("we out. telling the whole wide world that we out.");
  signalDone();
  g->finish();
  g->dumpStats(statsStream);

  if ((cpid != -1 || errno != 10) && !finished) {
    std::cout << "wait returned " << cpid << " " << errno << std::endl;
    return errno;
  }

  STATUS("ptrace_wrapper done @ {}", g->getPthreadLogClock());

  return 0;
}
