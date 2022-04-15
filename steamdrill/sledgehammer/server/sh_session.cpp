#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <iostream>
#include "sh_session.hpp"

int SHSession::SHSession::doEpochs()
{
#ifdef SHOWPROGRESS
  std::cout << getpid() <<  "starting doEpcohs " << std::endl;
#endif
  int rc = -1;
  for (u_long i = 0; i < _epochs.size(); i++) {
    Epoch &e = _epochs[i];
    e.replayPid = fork ();
    if (e.replayPid == 0) {

      const char* args[256];
      char attach[80], ckpt[80];
      char exit_clock[80];
      int argcnt	= 0;

      args[argcnt++] = "resume";
      args[argcnt++] = _replayName;
      args[argcnt++] = "-g";
      args[argcnt++] = "--pthread";
      args[argcnt++] = "../../eglibc-2.15/prefix/lib";

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
      rc = execv ("../../test/resume", (char **) args);
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
            char cpids[256],
                stopClock[256],
                startClock[256],
                outputFilename[256];
            u_int argcnt = 0;

            args[argcnt++] = "../ptrace_tool/ptrace_wrapper";
            sprintf (cpids, "%d", e.attachPid);
            args[argcnt++] = cpids;
            args[argcnt++] = _soName;
            args[argcnt++] = _configName;
            args[argcnt++] = _libcName;
            sprintf (outputFilename, LOG_PREFIX_FORMAT, e.attachPid);
            args[argcnt++] = outputFilename;

            if (logLevel())
            {
              args[argcnt++] = "-l";
              args[argcnt++] = logLevel().c_str();
            }
            if (isStep())
              args[argcnt++] = "-s";
            if (e.stopClock != 0)
            {
              sprintf (stopClock, "%d", e.stopClock);
              args[argcnt++] = "-stopAt";
              args[argcnt++] = stopClock;
            }

            if (e.startClock != 0)
            {
              sprintf (startClock, "%d", e.startClock);
              args[argcnt++] = "-startAt";
              args[argcnt++] = startClock;
            }
            args[argcnt++] = NULL;
            //                            for (u_int i = 0; i < argcnt; ++i)
            //                              std::cerr << args[i] << std::endl;
            rc = execv(PTRACE_WRAPPER, (char **) args);
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
            printf ("%lu/%ld epochs executing\n", executing, _epochs.size());
#endif

          }
        }
      }
    }
  } while (executing < _epochs.size());
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
    }
