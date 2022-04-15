// Shell program for running a sequential multi-stage DIFT
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/socket.h>

#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/mman.h>
#include <sys/socket.h>

#include <string.h>
#include <cassert>

#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>
#include <unordered_map>
#include <unordered_set>

#include "config.hpp"
#include "streamnw.hpp"
#include "../../../test/util.h"
#include "../../utils/utils.cpp"

#define SHOWPROGRESS 0
#define PTRACE_PATH "../tracing_phase/ptrace_wrapper"
#define RESUME_PATH "../../../test/resume"
#define EGLIBC_PATH "../../../eglibc-2.15/prefix/lib"

using std::string;

namespace Config
{

long ms_diff (struct timeval tv1, struct timeval tv2)
{
  return ((tv1.tv_sec - tv2.tv_sec) * 1000 + (tv1.tv_usec - tv2.tv_usec) / 1000);
}

ServerConfig::ServerConfig(const int devFd)
{
  _flags = 0;
  _devFd = devFd;
}

ServerConfig::ServerConfig(const char *replayName,
                           const char *ltpoint,
                           const char *analyzerName,
                           const char *localAnalyzer,
                           const char *streamAnalyzer,
                           const char *sharedObj,
                           const char *ipoints,
                           const char *tpoints,
                           const char logLevel,
                           const int logFd,
                           const u_long stackHint,
                           const u_long exitClock,
                           const uint32_t flags,
                           const int devFd)
{
  strncpy(_replayName, replayName, FILENAME_LEN);
  strncpy(_ltpoint, ltpoint, FILENAME_LEN);
  strncpy(_analyzerName, analyzerName, FILENAME_LEN);
  strncpy(_localAnalyzer, localAnalyzer, FILENAME_LEN);
  strncpy(_streamAnalyzer, streamAnalyzer, FILENAME_LEN);
  strncpy(_sharedObj, sharedObj, FILENAME_LEN);
  strncpy(_ipoints, ipoints, FILENAME_LEN);
  strncpy(_tpoints, tpoints, FILENAME_LEN);

  strncpy(_nextHost, "", FILENAME_LEN);

  _flags = flags;
  _logFd = logFd;
  _devFd = devFd;
  _stackHint = stackHint;
  _exitClock = exitClock;
  _logLevel = logLevel;
}

void ServerConfig::setTreeOutput(const char *outputName)
{
  strncpy(_treeOutputName, outputName, FILENAME_LEN);
}


void ServerConfig::addNextHost(const char* nextHost)
{
  strncpy(_nextHost, nextHost, FILENAME_LEN);
}

bool ServerConfig::useEpochAnalyzer() {
  return strnlen(_localAnalyzer, FILENAME_LEN) > 0 ||
      strnlen(_streamAnalyzer, FILENAME_LEN) > 0;
}

int ServerConfig::doAnalyzers() {
  if (useEpochAnalyzer()) {
    int rc;
    for (u_long i = 0; i < _epochs.size(); i++) {
      Epoch &e = _epochs[i];


      e.waitOnAnalyzer = fork();
      if (e.waitOnAnalyzer == 0)
      {
        e.analyzerPid = fork ();
        if (e.analyzerPid == 0) {


          const char* args[256];
          char port[80], output[128], local[128], stream[128];
          char next_host[128], next_port[80], prev_port[80];
          int argcnt = 0;

          sprintf(port, "%lu", ANALYZER_PORT_START + i);
          sprintf (output, ANALYZER_STATS_FORMAT, i);

          if (snprintf(local, 128, "--local=%s", _localAnalyzer) < 0) {
            std::cerr << "oops! problem copying usign snprintf " << errno << std::endl;
            return -1;
          }

          args[argcnt++] = "../analyzer/analyzer";
          args[argcnt++] = port;
          args[argcnt++] = _analyzerName;
          args[argcnt++] = LOG_FILE_FORMAT;
          args[argcnt++] = ANALYZER_FILE_FORMAT;
          args[argcnt++] = output;
          args[argcnt++] = local;
          if (strnlen(_streamAnalyzer, FILENAME_LEN) > 0)
          {
            if (snprintf(stream, 128, "--stream=%s", _streamAnalyzer) < 0) {
              std::cerr << "oops! problem copying usign snprintf " << errno << std::endl;
              return -1;
            }
            args[argcnt++] = stream;
            //std::cout << i << " added stream " << stream << std::endl;

            if (i < _epochs.size() - 1)
            {
              if (snprintf(next_host, 128, "--next_host=%s", _nextHost) < 0) {
                std::cerr << "oops! problem copying usign snprintf " << errno << std::endl;
                return -1;
              }
              args[argcnt++] = next_host;

              if (snprintf(next_port, 80, "--next_port=%lu", ANALYZER_STREAM_PORT_START + i + 1) < 0) {
                std::cerr << "oops! problem copying usign snprintf " << errno << std::endl;
                return -1;
              }
              args[argcnt++] = next_port;
            }
            if (i > 0)
            {
              sprintf(prev_port, "--prev_port=%lu", ANALYZER_STREAM_PORT_START + i);
              args[argcnt++] = prev_port;
            }
          }


          args[argcnt++] = NULL;
          rc = execv ("../analyzer/analyzer", (char **) args);
          std::cerr << "execl of ../analyzer/analyzer failed, rc=" << rc
                    << ", errno=" << errno << std::endl;
          _exit( -1);

        } else {
          //we're the listening process
          int status, pid;
          pid = waitpid(e.analyzerPid, &status, 0); 
          if (pid < 0)
          {
            char errstr[256];
            perror(errstr);
            std::cerr << "couldnt' wait on " << e.analyzerPid
                      << " " << errstr << std::endl;
          }
			
          else if (pid != e.analyzerPid)
          {
            std::cout << "waitpid on " << pid << " should " << e.analyzerPid
                      << " " << WIFEXITED(status) << " " << WIFSIGNALED(status)
                      << " " << WIFSTOPPED(status) << std::endl;
          }
          _exit(0);
        }
      }
		
      _waiters.insert(std::make_pair(e.waitOnAnalyzer, i));
		
#ifdef SHOWPROGRESS
      std::cout << e.analyzerPid <<  " analyzer " << e.waitOnAnalyzer << " waiter" << std::endl;
#endif

    }
  }

  return 0;
}

int ServerConfig::doCkptEpochs()
{
#ifdef SHOWPROGRESS
  std::cout << getpid() <<  "doCkptEpcohs: " << std::endl;
#endif
  int rc = -1;
  std::unordered_set<u_long> ckpts;
  for (u_long i = 0; i < _epochs.size(); i++) {
    Epoch &e = _epochs[i];

    if (ckpts.find(e.ckptClock) != ckpts.end())
    {
      std::cerr << "skipping " << e.ckptClock << std::endl;
      e.replayPid = -1;
      e.waitOnReplay = -1;
      continue;
    }
    ckpts.insert(e.ckptClock);

    if (e.ckptClock > 0) {
      e.replayPid = fork ();

      if (e.replayPid == 0) {
        const char* args[256];
        char ckpt[80];
        char exit_clock[80];
        int argcnt	= 0;

        //f- it... just adjust the stats later
        sleep(2);

        args[argcnt++] = "taskset";
        args[argcnt++] = "-c";
        args[argcnt++] = "0";
        args[argcnt++] = RESUME_PATH;
        //args[argcnt++] = "resume";
        args[argcnt++] = _replayName;
        args[argcnt++] = "--pthread";
        args[argcnt++] = "../../eglibc-2.15/prefix/lib";
        if (isCheckpointNOT())
        {
          std::cerr << "NOT doing checkpoint" << std::endl;
        }
        else
        {
          sprintf (ckpt, "--ckpt_at=%u", e.ckptClock);
          args[argcnt++] = ckpt;
        }

        if (_exitClock > 0) {
          sprintf(exit_clock, "--exit_clock=%lu\n", _exitClock);
          args[argcnt++] = exit_clock;
        }

        args[argcnt++] = NULL;

        rc = execv (RESUME_PATH, (char **) args);
        std::cerr << "execl of resume failed, rc=" << rc << ", errno=" << errno << std::endl;
        return -1;

      } else {
        e.status = STATUS_STARTING;
        gettimeofday (&e.tvStart, NULL);

        //create our listening process
        e.waitOnReplay = fork();
        if(e.waitOnReplay == 0){
          int tries = 0;
          while(rc < 0 && tries < 10000 ){
            rc = wait_for_replay_group(_devFd, e.replayPid);
            usleep(1000);
            tries ++;
          }
          if (tries >= 10000)
          {
            std::cerr << "huh? " << getpid() << " couldn't get through to " << e.replayPid << std::endl;
            std::cerr << std::dec << rc << " " << errno  << std::endl;
          }


          _exit(0);
        }
        _waiters.insert(std::make_pair(e.waitOnReplay, i));
#ifdef SHOWPROGRESS
        std::cout << std::dec << e.replayPid << " rp " << e.waitOnReplay << " waiter" << std::endl;
#endif
      }
    }
  }
  return 0;
}

int ServerConfig::doEpochs()
{
#ifdef SHOWPROGRESS
  std::cout << getpid() <<  "starting doEpcohs " << std::endl;
#endif
  int rc = -1;
  for (u_long i = 0; i < _epochs.size(); i++) {
    Epoch &e = _epochs[i];
    e.replayPid = fork ();
    if (e.replayPid == 0)
    {
      const char* args[256];
      char attach[80], ckpt[80];
      char exit_clock[80];
      int argcnt = 0;
      string eglibc = std::getPath(EGLIBC_PATH);
      string resume = std::getPath(RESUME_PATH);

      args[argcnt++] = "resume";
      args[argcnt++] = _replayName;
      args[argcnt++] = "-g";
      args[argcnt++] = "--pthread";
      args[argcnt++] = eglibc.c_str();

      if (_exitClock > 0) {
        sprintf(exit_clock, "--exit_clock=%lu\n", _exitClock);
        args[argcnt++] = exit_clock;
      }

      if (e.startClock > 0) {
        sprintf (attach, "--attach_offset=%d,%u", e.startPid, e.startClock);
        args[argcnt++] = attach;
      }

      if (e.ckptClock > 0) {
        sprintf (ckpt, "--from_ckpt=%u", e.ckptClock);
        args[argcnt++] = ckpt;
      }

      args[argcnt++] = NULL;
      rc = execv (resume.c_str(), (char **) args);
      std::cerr << "execl of resume failed, rc=" << rc << ", errno=" << errno << std::endl;
      return -1;

    } else {
      e.status = STATUS_STARTING;
      gettimeofday (&e.tvStart, NULL);

      //create our listening process
      e.waitOnReplay = fork();
      if(e.waitOnReplay == 0){
        while(rc < 0){
          rc = wait_for_replay_group(_devFd, e.replayPid);
        }
        //return 1;
        _exit(0);
      }
      _waiters.insert(std::make_pair(e.waitOnReplay, i));

#ifdef SHOWPROGRESS
      std::cout << e.replayPid << " rp " << e.waitOnReplay << " waiter" << std::endl;
#endif
    }
  }

  // Now attach pin to all of the epoch processes
  u_long executing = 0;
  do {
    for (u_long i = 0; i < _epochs.size(); i++) {
      Epoch &e = _epochs[i];
      rc = -1;
      if (e.status == STATUS_STARTING) {
        rc = get_attach_status (_devFd, e.replayPid);
        if (rc > 0) {
          //sometimes we attach pin to a different process.
          //When we do this, we need to save the rc in case we are getting stats
          e.attachPid = rc;
          e.ptracePid = fork();
          if (e.ptracePid == 0) {
            const char* args[256];
            char cpids[256];
            char stopClock[256],
                startClock[256],
                stack[256],
                level[2],
                logFd[256],
                outputFilename[256],
                statsFilename[256],
                port[256];

            string ptrace = std::getPath(PTRACE_PATH);
            u_int argcnt = 0;

            args[argcnt++] = "ptrace_wrapper";
            sprintf (cpids, "%d", e.attachPid);
            args[argcnt++] = cpids;
            args[argcnt++] = _ipoints;
            args[argcnt++] = _tpoints;
            args[argcnt++] = _ltpoint;
            args[argcnt++] = _sharedObj;
            sprintf (outputFilename, LOG_FORMAT, e.attachPid);
            args[argcnt++] = outputFilename;

            // tap that file:
            creat(outputFilename, 0644);

            sprintf (statsFilename, STATS_FORMAT, e.attachPid);
            args[argcnt++] = statsFilename;

            if (_logFd > 0)
            {
              sprintf (logFd, "%d", _logFd);
              args[argcnt++] = "-remap";
              args[argcnt++] = logFd;
            }
            if (isMemProtector())
            {
              std::cerr << "doing memProtectsion" << std::endl;
              args[argcnt++] = "-mp";
            }
            if (isStep())
              args[argcnt++] = "-s";
            if (isFork())
              args[argcnt++] = "-f";
            if (e.stopClock != 0)
            {
              sprintf (stopClock, "%d", e.stopClock);
              args[argcnt++] = "--stop";
              args[argcnt++] = stopClock;
            }

            if (e.startClock != 0)
            {
              sprintf (startClock, "%d", e.startClock);
              args[argcnt++] = "--start";
              args[argcnt++] = startClock;
            }

            if (_stackHint != 0)
            {
              sprintf (stack, "0x%lx", _stackHint);
              args[argcnt++] = "-stack";
              args[argcnt++] = stack;
            }

            if (_logLevel != 0)
            {
              sprintf (level, "%c", _logLevel);
              args[argcnt++] = "-l";
              args[argcnt++] = level;
            }

            if (useEpochAnalyzer())
            {
              sprintf( port, "%lu", ANALYZER_PORT_START + i);
              args[argcnt++] = "-port";
              args[argcnt++] = port;
            }

            args[argcnt++] = NULL;
            for (u_int i = 0; i < argcnt; ++i)
              std::cerr << args[i] << std::endl;

            rc = execv(ptrace.c_str(), (char **)args);
            std::cerr << "ptrace_wrapper can't execl "
                      << "rc=" << rc << " errno="
                      << errno << std::endl;
            return -1;
          } else {
            gettimeofday (&e.tvStartPtrace, NULL);
            e.status = STATUS_EXECUTING;
            executing++;
#ifdef SHOWPROGRESS
            std::cout << e.attachPid << " attach  " << e.ptracePid << " ptracer" << std::endl;
            std::cout << executing << "/" << _epochs.size() << "exec" << std::endl;
#endif
          }
        }
      }
    }
  } while (executing < _epochs.size());
  // Wait for all children to complete
  return 0;
}

    int ServerConfig::waitForDone()
    {
	std::cout << "wait for done " << _waiters.size() << std::endl;
	while (!_waiters.empty())
	{
	    int status;
	    pid_t wpid = waitpid(-1, &status, 0);
	    if (wpid < 0) {
		fprintf (stderr, "waitpid returns %d, errno %d\n", wpid, errno);
		return -1;
	    } else {
		//get the index
		auto it = _waiters.find(wpid);

		if( it != _waiters.end())
		{
#ifdef SHOWPROGRESS
		    std::cout << wpid << " finished" << std::endl;
#endif
		    Epoch &e = _epochs[it->second];
		    if (e.waitOnReplay == wpid)
		    {
			gettimeofday (&_epochs[it->second].tvLoggingEnd, NULL);
			_epochs[it->second].status = STATUS_LOGGING_DONE;
		    }
		    else if (e.waitOnAnalyzer == wpid)
		    {
			gettimeofday (&_epochs[it->second].tvAnalyzerEnd, NULL);
			_epochs[it->second].status = STATUS_DONE;
		    }
		    else 
		    {
			std::cerr << "huh? waited on " << wpid << std::endl;
		    }

		    _waiters.erase(it);
		}
	    }
	}
	fflush(stdout);
        return 0;
    }

    int ServerConfig::readConfig(int s)
    {
	int numEpochs = 0;
	safe_read(s, &_flags, sizeof(uint32_t));

	safe_read(s, &_replayName, sizeof(char) * FILENAME_LEN);
	safe_read(s, &_ltpoint, sizeof(char) * FILENAME_LEN);
	safe_read(s, &_analyzerName, sizeof(char) * FILENAME_LEN);
	safe_read(s, &_localAnalyzer, sizeof(char) * FILENAME_LEN);
	safe_read(s, &_streamAnalyzer, sizeof(char) * FILENAME_LEN);
	safe_read(s, &_sharedObj, sizeof(char) * FILENAME_LEN);
	safe_read(s, &_ipoints, sizeof(char) * FILENAME_LEN);
        safe_read(s, &_tpoints, sizeof(char) * FILENAME_LEN);
	safe_read(s, &_nextHost, sizeof(char) * FILENAME_LEN);
	safe_read(s, &_treeOutputName, sizeof(char) * FILENAME_LEN);

	safe_read(s, &_logFd, sizeof(uint32_t));
	safe_read(s, &_stackHint, sizeof(u_long));
	safe_read(s, &_exitClock, sizeof(u_long));
	safe_read(s, &numEpochs, sizeof(int));

	for (int i = 0; i < numEpochs; ++i)
	{
	    Epoch e;
	    safe_read(s,&e, sizeof(Epoch));
	    _epochs.push_back(e);
	}
        return 0;
    }

    int ServerConfig::sendConfig(int s)
    {
	safe_write(s, &_flags, sizeof(uint32_t));
	safe_write(s, &_replayName, sizeof(char) * FILENAME_LEN);
	safe_write(s, &_ltpoint, sizeof(char) * FILENAME_LEN);
	safe_write(s, &_analyzerName, sizeof(char) * FILENAME_LEN);
	safe_write(s, &_localAnalyzer, sizeof(char) * FILENAME_LEN);
	safe_write(s, &_streamAnalyzer, sizeof(char) * FILENAME_LEN);
	safe_write(s, &_sharedObj, sizeof(char) * FILENAME_LEN);
	safe_write(s, &_ipoints, sizeof(char) * FILENAME_LEN);
        safe_write(s, &_tpoints, sizeof(char) * FILENAME_LEN);
	safe_write(s, &_nextHost, sizeof(char) * FILENAME_LEN);
	safe_write(s, &_treeOutputName, sizeof(char) * FILENAME_LEN);
	safe_write(s, &_logFd, sizeof(uint32_t));
	safe_write(s, &_stackHint, sizeof(u_long));
	safe_write(s, &_exitClock, sizeof(u_long));

	int numEpochs = _epochs.size();
	safe_write(s, &numEpochs, sizeof(int));
	for (auto e : _epochs)
	{
	    safe_write(s,&e, sizeof(Epoch));
	}
        return 0;
    }

    int ServerConfig::sendAck(int s)
    {
	int rc = 0;

	if (isAck()) {
	    std::cout << "sending ack" << std::endl;
	    long retval = 0;
	    rc = send (s, &retval, sizeof(retval), 0);
	    if (rc != sizeof(retval)) {
		fprintf (stderr, "Cannot send ack,rc=%d, errno %d\n", rc, errno);
	    }
	}
        return 0;
    }

    int ServerConfig::sendOutput(int s)
    {
	int rc = 0;
	for (auto e : _epochs)
	{
	    char pathname[FILENAME_LEN];
	    if (useEpochAnalyzer())
	    {
		sprintf (pathname, ANALYZER_FILE_FORMAT, e.attachPid);
	    }
	    else
	    {
		sprintf (pathname, LOG_FILE_FORMAT, e.attachPid);
	    }
	    std::cout << "sending " << pathname << std::endl;;
	    rc = send_file (s, pathname, "output");
	    if (rc < 0) {
		return rc;
	    }

	}
	return 0;
    }

    int ServerConfig::sendTreeOutput(int s)
    {
	int rc = 0;
	std::cout << "sending " << _treeOutputName << std::endl;;
	rc = send_file (s, _treeOutputName, "output");
	    
	if (rc < 0) {
	    return rc; 
	}
	return 0;
    }

    int ServerConfig::sendCkptStats(int s)
    {
	int rc = 0, i = 0;

	for (auto e : _epochs)
	{
	    char pathname[FILENAME_LEN]; 
	    sprintf (pathname, LOG_STATS_FORMAT, e.ckptClock); //just doing something independent
	    std::ofstream lsStream(pathname, std::ofstream::out);

	    if (e.ckptClock > 0)
	    {
		lsStream << "ckpt time: " 
			 << Config::ms_diff(e.tvLoggingEnd,e.tvStart) << " us\n";

		lsStream.close();
		std::cout << "sending " << pathname << std::endl;
		rc = send_file (s, pathname, "ckpt-stats");
		if (rc < 0) return rc; 
	    }
	    i++;
	}
	return 0;
    }


int ServerConfig::sendStats(int s)
{
  int rc = 0;
  u_long i = 0;
  /*
   * This is dumb. But, we wake up AS SOON AS POSSIBLE from the finished
   * epoch, but the epoch dumps stats after we wake up!
   *
   *  so sleep for an arbirtrary amout of time for the epoch to catch-up
   */
  sleep(2);

  for (auto e : _epochs)
  {
    char pathname[FILENAME_LEN];
    sprintf (pathname, LOG_STATS_FORMAT, e.attachPid);
    std::ofstream lsStream(pathname,
                           std::ofstream::out | std::ofstream::app);

    lsStream << "start: " << e.tvStart.tv_sec << "." << e.tvStart.tv_usec << std::endl;
    lsStream << "pt_start: " << e.tvStartPtrace.tv_sec << "." << e.tvStartPtrace.tv_usec << std::endl;	    
    lsStream << "log_end: " << e.tvLoggingEnd.tv_sec << "." << e.tvLoggingEnd.tv_usec << std::endl;
    lsStream << "analyzer_end: " << e.tvAnalyzerEnd.tv_sec << "." << e.tvAnalyzerEnd.tv_usec << std::endl;

    lsStream << "fast forward time: "
             << Config::ms_diff(e.tvStartPtrace,e.tvStart) << " us\n";
    lsStream << "ptrace time: "
             << Config::ms_diff(e.tvLoggingEnd,e.tvStartPtrace) << " us\n";
    lsStream << "analyzer time: "
             << Config::ms_diff(e.tvAnalyzerEnd,e.tvStartPtrace) << " us\n";


    lsStream.close();
    std::cout << "sending " << pathname << std::endl;
    rc = send_file (s, pathname, "logging-stats");
    if (rc < 0) return rc;

    if (useEpochAnalyzer())
    {
      sprintf (pathname, ANALYZER_STATS_FORMAT, i);
      std::cout << "sending " << pathname << std::endl;
      rc = send_file (s, pathname, "analyzer-stats");
      if (rc < 0) return rc;
    }
    i++;
  }
  return 0;
}

int ServerConfig::sendAltrace(int s)
{
  if (isAltrace())
  {
    int rc = 0;
    for (auto e : _epochs)
    {
      char pathname[FILENAME_LEN];
      sprintf (pathname, ALTRACE_FORMAT, e.attachPid);
      std::cout << "sending " << pathname << std::endl;
      rc = send_file (s, pathname, "altrace");
      if (rc < 0) return rc;
    }
  }
  return 0;
}


int ServerConfig::recvReplayFiles(int s)
{
  if (!isSync())
  {
    std::cerr << "not syncing files" << std::endl;
    return 0;
  }

  u_long fcnt, ccnt;
  bool* freply = NULL;
  bool* creply = NULL;
  std::vector<struct replay_path> dirs;

  std::cerr << "receiving replay files" << std::endl;
  int rc = safe_read (s, &fcnt, sizeof(fcnt));
  if (rc != sizeof(fcnt)) {
    fprintf (stderr, "Cannot recieve file count,rc=%d\n", rc);
    return -1;
  }

  if (fcnt) {
    freply = (bool *) malloc(sizeof(bool)*fcnt);
    if (freply == NULL) {
      fprintf (stderr, "Cannot allocate file reply array of size %lu\n", fcnt);
      return -1;
    }

    for (u_long i = 0; i < fcnt; i++) {
      replay_path fpath;
      rc = safe_read (s, &fpath, sizeof(fpath));
      if (rc != sizeof(fpath)) {
        fprintf (stderr, "Cannot recieve file path,rc=%d\n", rc);
        return -1;
      }

      // Does this file exist?
      struct stat st;
      rc = stat (fpath.path, &st);
      if (rc == 0) {
        freply[i] = false;
      } else {
        freply[i] = true;

        // Make sure directory exists
        for (int i = strlen(fpath.path); i >= 0; i--) {
          if (fpath.path[i] == '/') {
            fpath.path[i] = '\0';
            rc = mkdir (fpath.path, 0777);
            if (rc < 0 && errno != EEXIST) {
              printf ("mkdir of %s returns %d\n", fpath.path, rc);
            }
            break;
          }
        }
        dirs.push_back(fpath);
      }
    }

    // Send back response
    rc = safe_write (s, freply, sizeof(bool)*fcnt);
    if (rc != (int) (sizeof(bool)*fcnt)) {
      fprintf (stderr, "Cannot send file check reply,rc=%d\n", rc);
      return -1;
    }
  }

  rc = safe_read (s, &ccnt, sizeof(ccnt));
  if (rc != sizeof(fcnt)) {
    fprintf (stderr, "Cannot recieve cache count,rc=%d\n", rc);
    return -1;
  }

  struct cache_info* ci = new cache_info[ccnt];
  if (ccnt) {
    creply = (bool *) malloc(sizeof(bool)*ccnt);
    if (creply == NULL) {
      fprintf (stderr, "Cannot allocate cache reply array of size %lu\n", ccnt);
      return -1;
    }

    rc = safe_read (s, ci, sizeof(struct cache_info)*ccnt);
    if (rc != (long) (sizeof(struct cache_info)*ccnt)) {
      fprintf (stderr, "Cannot recieve cache info,rc=%d\n", rc);
      return -1;
    }

    for (u_long i = 0; i < ccnt; i++) {
      // Does this file exist?
      char cname[FILENAME_LEN], cmname[FILENAME_LEN];
      struct stat64 st;

      sprintf (cname, "/replay_cache/%x_%x", ci[i].dev, ci[i].ino);
      rc = stat64 (cname, &st);
      if (rc == 0) {
        // Is this the right version?
        if (st.st_mtim.tv_sec == ci[i].mtime.tv_sec && st.st_mtim.tv_nsec == ci[i].mtime.tv_nsec) {
          creply[i] = false;
        } else {
          // Nope - but maybe we have it?
          sprintf (cmname, "/replay_cache/%x_%x_%lu_%lu", ci[i].dev, ci[i].ino, ci[i].mtime.tv_sec, ci[i].mtime.tv_nsec);
          rc = stat64 (cmname, &st);
          if (rc == 0) {
            creply[i] = false;
          } else {
            creply[i] = true;
          }
        }
      } else {
        // No versions at all
        creply[i] = true;
      }
    }

    // Send back response
    rc = safe_write (s, creply, sizeof(bool)*ccnt);
    if (rc != (int) (sizeof(bool)*ccnt)) {
      fprintf (stderr, "Cannot send cache info check reply,rc=%d\n", rc);
      return -1;
    }
  }

  // Now receive the files we requested
  u_long dcnt = 0;
  for (u_long i = 0; i < fcnt; i++) {
    if (freply[i]) {
      rc = fetch_file (s, dirs[dcnt++].path,NULL);
      if (rc < 0) return rc;
    }
  }

  u_long ffcnt = 0;
  for (u_long i = 0; i < ccnt; i++) {
    if (creply[i]) {
      rc = fetch_file (s, "/replay_cache",NULL);
      if (rc < 0) return rc;

      // Now rename the file to the correct version - must check to see where to put it
      char cname[FILENAME_LEN], crname[FILENAME_LEN], newname[FILENAME_LEN];
      struct stat64 st;
      sprintf (cname, "/replay_cache/%x_%x", ci[i].dev, ci[i].ino);
      rc = stat64 (cname, &st);
      if (rc == 0) {
        if (st.st_mtim.tv_sec > ci[i].mtime.tv_sec ||
            (st.st_mtim.tv_sec == ci[i].mtime.tv_sec && st.st_mtim.tv_nsec > ci[i].mtime.tv_nsec)) {
          // Exists and new file is past version
          sprintf (newname, "/replay_cache/%x_%x_%lu_%lu", ci[i].dev, ci[i].ino, ci[i].mtime.tv_sec, ci[i].mtime.tv_nsec);
          rc = rename ("/replay_cache/rename_me", newname);
          if (rc < 0) {
            fprintf (stderr, "Cannot rename temp cache file to %s, rc=%d\n", newname, rc);
            return rc;
          }
        } else {
          // Exists and new file is more recent version
          sprintf (crname, "/replay_cache/%x_%x_%lu_%lu", ci[i].dev, ci[i].ino, st.st_mtim.tv_sec, st.st_mtim.tv_nsec);
          rc = rename (cname, crname);
          if (rc < 0) {
            fprintf (stderr, "Cannot rename cache file %s to %s, rc=%d\n", cname, crname, rc);
            return rc;
          }
          rc = rename ("/replay_cache/rename_me", cname);
          if (rc < 0) {
            fprintf (stderr, "Cannot rename temp cache file to %s, rc=%d\n", cname, rc);
            return rc;
          }
        }
      } else {
        // Does not exist
        rc = rename ("/replay_cache/rename_me", cname);
        if (rc < 0) {
          fprintf (stderr, "Cannot rename temp cache file to %s, rc=%d\n", cname, rc);
          return rc;
        }
      }
      ffcnt++;
    }
  }

  free (freply);
  free (creply);
  delete [] ci;
  return 0;
}



void ServerConfig::dump()
{
  std::cout << std::dec << "flags " << _flags << std::endl;
  std::cout << "replay " << _replayName << std::endl;
  std::cout << "ltpoint " << _ltpoint << std::endl;
  std::cout << "analyzer " << _analyzerName << std::endl;
  std::cout << "local  " << _localAnalyzer << std::endl;
  std::cout << "stream  " << _streamAnalyzer << std::endl;
  std::cout << "ipoints " << _ipoints << std::endl;
  std::cout << "tpoints " << _tpoints << std::endl;
  std::cout << "logFd " << _logFd << std::endl;
  std::cout << "nextHost " << _nextHost << std::endl;

  std::cout << std::hex << "stackHint: " << _stackHint << std::endl;
  std::cout << std::hex << "exitClock: " << _exitClock << std::endl;
  std::cout << "epochs:" << std::endl;

  for (auto e : _epochs)
  {
    std::cout << "\t(" << e.startClock << ", " << e.stopClock << ")" << std::endl;
  }
}

uint32_t ServerConfig::buildFlag(bool ckpt,
                                 bool tree,
                                 bool wait,
                                 bool sync,
                                 bool builtInLogger,
                                 bool fork,
                                 bool memProtector,
                                 bool altrace,
                                 bool ckptNOT,
                                 bool step)
{
  uint32_t rtn = 0;
  if (ckpt) rtn |= DO_CKPT;
  else if (tree)
    rtn |= DO_TREE_MERGE;
  else
    rtn |= DO_PTRACE;

  if (wait) rtn |= SEND_ACK;
  if (sync) rtn |= SYNC_REPLAY;
  if (builtInLogger) rtn |= BUILT_IN_LOGGER;
  if (fork) rtn |= FORK;
  if (memProtector) rtn |= MEM_PROTECTOR;
  if (altrace) rtn |= ALTRACE;
  if (ckptNOT) rtn |= DO_CKPT_NOT;
  if (step) rtn |= STEP;

  return rtn;
}

Host::Host(const char *hostname,
           const char *outputDir,
           const int numEpochs,
           const int port,
           const char *replayName,
           const char *soName,
           const char *analyzerName,
           const char *localAnalyzer,
           const char *streamAnalyzer,
           const char *sharedObj,
           const char *configName,
           const char logLevel,
           const int logFd,
           const u_long stackHint,
           const u_long exitClock,
           const uint32_t flags) : _serverConfig(replayName, soName, analyzerName, localAnalyzer, streamAnalyzer, sharedObj, configName, "", logLevel, logFd, stackHint, exitClock, flags, -1)
{
  assert (false);
  strncpy(_name, hostname, FILENAME_LEN);
  strncpy(_outputDir, outputDir, FILENAME_LEN);
  _epochCapacity = numEpochs;
  _port = port;
  //setup our socket:
  // Connect to streamserver
  struct hostent* hp = gethostbyname (_name);
  if (hp == NULL) {
    fprintf (stderr, "Invalid host %s, errno=%d\n", _name, h_errno);
  }
  _addr.sin_family = AF_INET;
  _addr.sin_port = htons(_port);
  memcpy (&_addr.sin_addr, hp->h_addr, hp->h_length);
}

int Host::addEpoch(Epoch &epoch)
{
  _serverConfig.addEpoch(epoch);
  return 0;
}

bool Host::epochSlotsLeft()
{
  return  _serverConfig.numEpochs() < capacity();
}

int Host::capacity()
{
  return  _epochCapacity;
}

int Host::numEpochs()
{
  return this->_serverConfig.numEpochs();
}

int Host::connectToHost()
{
  _s = socket (AF_INET, SOCK_STREAM, 0);
  if (_s < 0) {
    fprintf (stderr, "Cannot create socket, errno=%d\n", errno);
    return _s;
  }

  long rc = connect (_s, (struct sockaddr *) &_addr, sizeof(_addr));
  if (rc < 0) {
    fprintf (stderr, "Cannot connect to %s:%d, errno=%d\n", _name, _port, errno);
    return rc;
  }
  //std::cerr << "connected to " << _s << " " << *this << std::endl;
  return 0;
}

void Host::setTreeOutput(const char *outputName)
{
  _serverConfig.setTreeOutput(outputName);
}

int Host::sendServerConfig()
{
  return _serverConfig.sendConfig(_s);
};

void Host::addNextHost(const char* nextHost)
{
  _serverConfig.addNextHost(nextHost);
}

int Host::waitForResponse()
{
  long ack;
  int rc;

  if (this->_serverConfig.isAck())
  {
    rc = safe_read (_s, &ack, sizeof(ack));
    if (rc != sizeof(ack)) {
      fprintf (stderr, "Cannot recv ack,rc=%d,errno=%d\n", rc, errno);
      return -1;
    }
  }
  return 0;
}

int Host::getOutput(int offset)
{
  int rc = 0;

  if (_serverConfig.isPtrace())
  {
    for (int i = 0; i < _serverConfig.numEpochs(); ++i)
    {
      char str[128];
      sprintf(str, STREAMCTL_TMP_ENDING, offset + i);

      rc = fetch_file(_s, STREAMCTL_TMP, str);
      if ( rc < 0)
      {
        std::cerr << "couldn't fetch file for ptrace " << rc << " " << errno << std::endl;
      }
    }
  }
  else if (_serverConfig.isTreeMerge() &&
           _serverConfig.hasTreeOutput())
  {
    char str[128];
    sprintf(str, STREAMCTL_TMP_ENDING, offset);
    rc = fetch_file(_s, STREAMCTL_TMP, str);
    if ( rc < 0)
    {
      std::cerr << "couldn't fetch file for tree merge " << rc << " " << errno << std::endl;
    }
  }
  return 0;
}

int Host::getStats(int offset)
{
  int rc = 0;
  for (int i = 0; i < _serverConfig.numEpochs(); ++i)
  {
    //skip if there aren't epoch stats
    if (!_serverConfig.getEpochStats(i))
    {
      continue;
    }

    char str[128];
    sprintf(str, ".%d",offset + i);
    //fetch the stats
    rc = fetch_file(_s, _outputDir, str);
    if ( rc < 0)
    {
      std::cerr << "couldn't fetch file " << rc << " " << errno << std::endl;
    }

    //fetch the analyzer-stats
    if (_serverConfig.useEpochAnalyzer())
    {
      rc = fetch_file(_s, _outputDir, str);
      if ( rc < 0)
      {
        std::cerr << "couldn't fetch analyzer-stats " << rc << " " << errno << std::endl;
      }
    }
  }
  return 0;
}

int Host::getAltrace(int offset)
{
  if (_serverConfig.isAltrace())
  {
    int rc = 0;
    for (u_int i = 0; i < _serverConfig.numEpochs(); ++i)
    {
      char str[128];
      sprintf(str, ".%d",offset + i);
      //fetch the stats
      rc = fetch_file(_s, _outputDir, str);
      if ( rc < 0)
      {
        std::cerr << "couldn't fetch file " << rc << " " << errno << std::endl;
      }
    }
  }
  return 0;
}


void Host::dump()
{
  _serverConfig.dump();
}
int Host::sendReplayFiles(std::vector<struct replay_path> &logFiles, std::vector<struct cache_info> &cacheFiles)
{
  int rc;
  if (_serverConfig.isSync())
  {

    // First send count of log files
    uint32_t cnt = logFiles.size();
    rc = safe_write (_s, &cnt, sizeof(cnt));
    if (rc != sizeof(cnt)) {
      std::cerr << "Cannot send log file count, rc=" << rc << " "<< errno << std::endl;
      return rc;
    }

    // Next send log files
    for (auto iter = logFiles.begin(); iter != logFiles.end(); iter++) {
      struct replay_path p = *iter;
      rc = safe_write (_s, &p, sizeof(struct replay_path));
      if (rc != sizeof(struct replay_path)) {
        std::cerr << "Cannot send log file name, rc=" << rc <<  " "<< errno << std::endl;
        return rc;
      }
    }

    // Next send count of cache files
    cnt = cacheFiles.size();
    rc = safe_write (_s, &cnt, sizeof(cnt));
    if (rc != sizeof(cnt)) {
      std::cerr << "Cannot send cache file count, rc=" << rc <<  " " << errno << std::endl;
      return rc;
    }

    // And finally the cache files
    for (auto iter = cacheFiles.begin(); iter != cacheFiles.end(); iter++) {
      struct cache_info c = *iter;
      rc = safe_write (_s, &c, sizeof(struct cache_info));
      if (rc != sizeof(struct cache_info)) {
        std::cerr << "Cannot send cache file name, rc=" << rc << " " << errno << std::endl;
        return rc;
      }
    }

    // Get back response
    bool response[logFiles.size()+cacheFiles.size()];
    rc = safe_read (_s, response, sizeof(bool)*(logFiles.size()+cacheFiles.size()));
    if (rc != (long) (sizeof(bool)*(logFiles.size()+cacheFiles.size()))) {
      std::cerr << "Cannot read sync results, rc=" << rc << " " << errno << std::endl;
      return rc;
    }

    // Send requested files
    u_long l, j;
    for (l = 0; l < logFiles.size(); l++) {
      if (response[l]) {
        std::cout << l << " of " << logFiles.size() << " requested" << std::endl;
        char* filename = NULL;
        for (int j = strlen(logFiles[l].path); j >= 0; j--) {
          if (logFiles[l].path[j] == '/') {
            filename = &logFiles[l].path[j+1];
            break;
          }
        }
        if (filename == NULL) {
          std::cerr << "Bad path name " << logFiles[l].path << std::endl;
          return -1;
        }
        rc = send_file (_s, logFiles[l].path, filename);
        if (rc < 0) {
          std::cerr << "Canot send log file " << logFiles[l].path << " " << rc << " " << errno << std::endl;
          return rc;
        }
      }
    }

    for (j = 0; j < cacheFiles.size(); j++) {
      if (response[l+j]) {
        char cname[FILENAME_LEN];
        struct stat64 st;

        // Find the cache file locally
        sprintf (cname, "/replay_cache/%x_%x", cacheFiles[j].dev, cacheFiles[j].ino);
        rc = stat64 (cname, &st);
        if (rc < 0) {
          std::cerr << "Cannot stat cache file " << cname << " " << rc << " " << errno << std::endl;
          return rc;
        }

        if (st.st_mtim.tv_sec != cacheFiles[j].mtime.tv_sec || st.st_mtim.tv_nsec != cacheFiles[j].mtime.tv_nsec) {
          // if times do not match, open a past version
          sprintf (cname, "/replay_cache/%x_%x_%lu_%lu", cacheFiles[j].dev, cacheFiles[j].ino,
                   cacheFiles[j].mtime.tv_sec, cacheFiles[j].mtime.tv_nsec);
        }

        // Send the file to streamserver
        rc = send_file (_s, cname, "rename_me");
        if (rc < 0) {
          std::cerr << "Canot send cache file " << cname << " " << rc << " " << errno << std::endl;
          return rc;
        }
      }
    }
  }
  return 0;
}

} /*End Namespace Config */
