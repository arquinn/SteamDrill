#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <vector>

#ifndef __SH_SESSION_
#define __SH_SESSION_
namespace SHSession {
class SHSession {

  std::vector<pid_t> _waiters;

  std::string _replayName;
  u_long _exitClock;

  int _devFd;
 public:
  int doEpochs();
  int wait();
};
}

#endif /*__SH_SESSION_ */
