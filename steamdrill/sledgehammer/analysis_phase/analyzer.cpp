#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <dirent.h>

#include <iostream>
#include <fstream>
#include <string>
#include <unordered_set>

#include "../../lib/args/args.hxx"
#include "spdlog/spdlog.h"
#include "../../../test/util.h"
#include "../server/streamnw.h"

//STATS
typedef int(*LocalAnalyzer)(void);
typedef int(*StreamAnalyzer)(int, int);

struct timeval tvStart, tvLocalDone, tvStreamDone, tvEnd;
int inputBytes = 0, outputBytes = 0;

int nextHost = 0, prevHost = 0, waitingHost = 0;

struct hostinfo
{
    char host[256];
    int port;
};


long ms_diff (struct timeval tv1, struct timeval tv2)
{
    return ((tv1.tv_sec - tv2.tv_sec) * 1000 + (tv1.tv_usec - tv2.tv_usec) / 1000);
}


long waitForReplay()
{
    //we're gonna open a socket for the other side to ping us
    int rc;
    pid_t attach_pid = 0;

    // This will be sent after processing is completed
    rc = safe_read (waitingHost, (void *)&attach_pid, sizeof(pid_t));
    if (rc != sizeof(pid_t)) {
	fprintf (stderr, "Read of group directory failed, rc=%d, errno=%d\n", rc, errno);
	return -1;
    }
    return attach_pid;
}


void dumpStats(const char *statsFile)
{
    std::ofstream lsStream(statsFile, std::ofstream::out);

    lsStream << "analyzer total time: "
	     << ms_diff(tvEnd, tvStart) << " ms\n";
    lsStream << "analyzer local time: "
	     << ms_diff(tvLocalDone, tvStart) << " ms\n";
   lsStream << "analyzer stream time: "
	    << ms_diff(tvStreamDone, tvLocalDone) << " ms\n";

    lsStream << "bytes in: " << inputBytes << std::endl;
    lsStream << "bytes out: " << outputBytes << std::endl;

    lsStream.close();

}

void initAnalyzer(std::string nextHostname,
                  int nextPort,
                  int prevPort,
                  int waitPort)
{
  waitingHost = init_socket (waitPort);
  if (waitingHost < 0) {
    spdlog::error("cannot create wait for replay socket, returns %d\n",
                  waitingHost);
  }

  if (nextPort > 0 && nextHostname.size()  > 0)
  {
    nextHost = connect_to_host(nextHostname.c_str(), nextPort);
    if (nextHost < 0) {
      spdlog::error("cannot create nextHost, returns %d\n", nextHost);
    }
  }
  if (prevPort > 0)
  {
    prevHost = init_socket (prevPort);
    if (prevHost < 0) {
      spdlog::error("cannot create prevHost, returns %d\n", prevHost);
    }
  }

  spdlog::info("{0} connect ({1}:{2}, {3}:{4})\n",
               getpid(),
               prevHost, prevPort,
               nextHost, nextPort);
}

void finishAnalyzer(void)
{
  // I think that if I don't do this than stuff gets swallowed?
  close(prevHost);
  close(nextHost);
  close(waitingHost);
}


void loadObject(std::string lAnalyzer,
                std::string sAnalyzer,
                std::string analyzer,
                LocalAnalyzer *lPointer,
                StreamAnalyzer *sPointer)
{
  char *dlhandle;
  *lPointer = NULL;
  *sPointer = NULL;


  dlhandle = (char *) dlopen(analyzer.c_str(), RTLD_NOW);
  if (dlhandle == NULL)
  {
    std::cerr << "dlopen of " << analyzer << " failed" << std::endl;
    std::cerr << dlerror();
  }

  if (lAnalyzer.size() > 0)
  {
    *lPointer = (LocalAnalyzer) dlsym(dlhandle, lAnalyzer.c_str());
  }
  if (sAnalyzer.size() > 0)
  {
    *sPointer = (StreamAnalyzer) dlsym(dlhandle, sAnalyzer.c_str());
  }
}

int analyze(LocalAnalyzer localFunc,
            StreamAnalyzer streamFunc,
            std::string inFilename,
            std::string outFilename)
{
  int rc = 0;
  std::stringstream tempLog;
  tempLog << "/dev/shm/analyzerPipe." << getpid();

  FILE *unused = freopen(inFilename.c_str(), "r", stdin);
  if (streamFunc) {
    unused = freopen(tempLog.str().c_str(), "w+", stdout);
  }
  else {
    unused = freopen(outFilename.c_str(), "w+", stdout);
  }

  rc = (localFunc)();
  gettimeofday(&tvLocalDone, NULL);


  if (streamFunc)
  {
    fclose(stdout);
    fclose(stdin);
    unused = freopen(tempLog.str().c_str(), "r", stdin);
    unused = freopen(outFilename.c_str(), "w+", stdout);

    rc = streamFunc(prevHost, nextHost);
    gettimeofday(&tvStreamDone, NULL);
  }
  spdlog::info("{0} stream finished\n", getpid());
  fclose(stdout);
  fclose(stdin);

  return rc;
}



int main (int argc, char* argv[])
{
  long rc, replay_pid;
  char inLog[256], outLog[256];
  LocalAnalyzer localFunc;
  StreamAnalyzer streamFunc;

  args::ArgumentParser parser("Analyzes log output.", "A.R.Q.");
  args::Group group(parser, "Required arguments:", args::Group::Validators::All);
  args::Positional<long> port(group, "port", "The port for the socket");
  args::Positional<std::string> analyzer(group, "analyzer", "The path to the analyzer shared object");
  args::Positional<std::string> start_log(group, "start_log_format", "format for input path");
  args::Positional<std::string> end_log(group, "end_log", "Format for output path");
  args::Positional<std::string> stats(group, "stats", "Where to shove the stats file");

  args::ValueFlag<std::string> lAnalyzer(parser, "local analyer", "local analyzer function", {"l", "local"},"");
  args::ValueFlag<std::string> sAnalyzer(parser, "stream analyer", "stream analyzer function", {"s", "stream"},"");

  args::ValueFlag<std::string> next_host(parser, "hostname", "next hostname", {"h", "next_host"},"");
  args::ValueFlag<int> next_port(parser, "NEXT_PORT", "next server port", {"n", "next_port"}, 0);
  args::ValueFlag<int> prev_port(parser, "PREV_PORT", "prev server port", {"p", "prev_port"},0);
  args::Flag
      verboseFlag(parser, "verbose", "verbose log", {'v', "verbose"});

  args::HelpFlag help(parser, "help", "Display this help menu", {'h', "help"});
  try
  {
    parser.ParseCLI(argc, argv);
  }
  catch (args::Help)
  {
    std::cout << parser;
    return 0;
  }
  catch (args::ParseError e)
  {
    std::cerr << e.what() << std::endl;
    std::cerr << parser;
    return -1;
  }
  catch (args::ValidationError e)
  {
    std::cerr << e.what() << std::endl;
    std::cerr << parser;
    return -2;
  }
  if (args::get(verboseFlag))
  {

    std::cerr << "analyzer args: " << std::endl
              << "port: " << args::get(port) << std::endl
              << "analyzer: " << args::get(analyzer) << std::endl
              << "local : " << args::get(lAnalyzer) << std::endl
              << "stream : " << args::get(sAnalyzer) << std::endl
              << "next_host: " << args::get(next_host) << std::endl
              << "next_port: " <<  args::get(next_port)  << std::endl
              << "prev_port: " <<  args::get(prev_port)  << std::endl
              << "stats: " << args::get(stats) << std::endl;
  }

  // Initialize analyzer log files, functions and sockets
  initAnalyzer(args::get(next_host),
               args::get(next_port),
               args::get(prev_port),
               args::get(port));

  sprintf(inLog, args::get(start_log).c_str(), replay_pid);
  sprintf(outLog, args::get(end_log).c_str(), replay_pid);

  loadObject(args::get(lAnalyzer).c_str(),
             args::get(sAnalyzer).c_str(),
             args::get(analyzer).c_str(),
             &localFunc,
             &streamFunc);

  replay_pid = waitForReplay();
  close (waitingHost);
  if (replay_pid < 0)
  {
    std::cerr << "couldn't wait? " << replay_pid << std::endl;
    assert(0);
  }

  gettimeofday (&tvStart, NULL);
  rc = analyze(localFunc,
               streamFunc,
               inLog,
               outLog);


  finishAnalyzer();

  gettimeofday (&tvEnd, NULL);
  dumpStats(args::get(stats).c_str());

  std::cout << "analyzer returned " << rc << std::endl;

  return 0;
}
