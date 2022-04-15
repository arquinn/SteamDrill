// Shell program for running a sequential multi-stage DIFT
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <string.h>
#include <cassert>

#include <iostream>
#include <vector>

#include "config.hpp"
#include "args.hxx"

#define SHOWPROGRESS 0

void doPtrace (Config::ServerConfig &c)
{
    c.doEpochs();
    c.waitForDone();
}

int main (int argc, char* argv[]) {
  char llevel = 'i';
  args::ArgumentParser parser("run sledgehammer sequentially.", "A.R.Q.");
  args::Group group(parser, "Required arguments:", args::Group::Validators::All);
  args::Flag
      verbose(parser, "verbose", "verbose output.", {'v', "verbose"});
  args::Flag step(parser, "step", "step through tracing.", {'s', "step"});
  args::Flag fork(parser, "fork", "Use fork isolation.", {'f',"fork"});
  args::ValueFlag<char>
      logLevel(parser, "logLevel", "level for logging", {'l',"level"});
  args::ValueFlag<u_long>
      startPid(parser, "startPid", "Pid to start replay after.", {"start-pid"});
  args::ValueFlag<u_long>
      startClock(parser, "startClock", "Clock to start replay after.", {"start"});
  args::ValueFlag<u_long>
      stopClock(parser, "stopClock", "Clock to stop replay before.", {"stop"});

  args::HelpFlag
      help(parser, "help", "Display help menu", {'h', "help"});

  args::Positional<std::string> replayName(group, "replay","The replay");
  args::Positional<std::string> ipoints(group, "ipoints", "The instpoint config");
  args::Positional<std::string> tpoints(group, "tracepoints", "The tracepoint config");
  args::Positional<std::string> ltpoint(group, "load point","Tracepoint for when program is loaded");
  args::Positional<std::string> tracers(group, "shared object", "Shared Obj");

  try {
    parser.ParseCLI(argc, argv);
  } catch (args::Help) {
    std::cout << parser;
    return 0;
  } catch (args::ParseError e) {
    std::cerr << e.what() << std::endl;
    std::cerr << parser;
    return -1;
  } catch (args::ValidationError e) {
    std::cerr << e.what() << std::endl;
    std::cerr << parser;
    return -2;
  }

  int devFd = open ("/dev/spec0", O_RDWR);
  if (devFd < 0) {
    std::cerr << "oops, cannot open spec?" << std::endl;
    return -2;
  }

  if (logLevel) {
    llevel = args::get(logLevel);
  }

  uint32_t flags = Config::ServerConfig::buildFlag(false,
                                                   false,
						   false,
                                                   false,
                                                   true,
                                                   false,
                                                   false,
                                                   false,
                                                   false,
                                                   args::get(step));

  Config::ServerConfig sc(args::get(replayName).c_str(),
                          args::get(ltpoint).c_str(),
                          "",
                          "",
                          "",
                          args::get(tracers).c_str(),
                          args::get(ipoints).c_str(),
                          args::get(tpoints).c_str(),
                          llevel,
                          0,
                          0,
                          0,
                          flags,
                          devFd);

  Config::Epoch e;
  e.startPid = args::get(startPid); //doesn't matter
  e.startClock = args::get(startClock);
  e.stopClock = args::get(stopClock);
  e.ckptClock = 0;
  sc.addEpoch(e);
  doPtrace (sc);
  printf ("done with epochs\n");
  return 0;
}
