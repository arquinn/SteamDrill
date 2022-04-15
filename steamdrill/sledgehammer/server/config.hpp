#ifndef __PTRACE_TOOL_H__
#define __PTRACE_TOOL_H__

#include <sys/socket.h>
#include <sys/types.h>
#include <linux/limits.h>


#include <unistd.h>
#include <netdb.h>
#include <pthread.h>

#include <cstdint>
#include <vector>
#include <unordered_map>
#include "streamserver.h"

//port stat stuff (meh)
#define ANALYZER_PORT_START 11111

#define ANALYZER_STREAM_PORT_START 22221


//Status for tracking progress
#define STATUS_STARTING 0
#define STATUS_EXECUTING 1
#define STATUS_LOGGING_DONE 2
#define STATUS_DONE 3

//flags that can be set in Config
#define DO_PTRACE 1
#define SEND_ACK 4
#define SYNC_REPLAY 16
#define BUILT_IN_LOGGER 32
#define FORK 64
#define MEM_PROTECTOR 128
#define ALTRACE 256
#define DO_CKPT 512
#define DO_TREE_MERGE 1024
#define DO_CKPT_NOT 2048
#define STEP 4096

#define LOG_FORMAT "/dev/shm/log.%d"
#define STATS_FORMAT "/dev/shm/stats.%d"
#define ANALYZER_PREFIX_FORMAT "/dev/shm/%d"

#define LOG_FILE_FORMAT "/dev/shm/log.%d.output"
#define LOG_STATS_FORMAT "/dev/shm/log.%d.logging-stats"
#define ALTRACE_FORMAT "/dev/shm/log.%d.altrace"

#define ANALYZER_FILE_FORMAT "/dev/shm/%d.output"
#define ANALYZER_STATS_FORMAT "/dev/shm/%lu.analyzer-stats"


#define STREAMCTL_TMP "/dev/shm/"
#define STREAMCTL_TMP_ENDING ".streamctl.%d"
#define STREAMCTL_TMP_PREFIX "/dev/shm/output.streamctl.%d"


// Info from description file
namespace Config
{

long ms_diff (struct timeval tv1, struct timeval tv2);
class Epoch {
 public:
  pid_t    startPid;
  uint32_t startClock;
  uint32_t stopClock;
  uint32_t ckptClock;

  pid_t replayPid;
  pid_t ptracePid;
  pid_t attachPid;
  pid_t waitOnReplay;

  pid_t analyzerPid;
  pid_t waitOnAnalyzer;

  int status;

  struct timeval tvStart;
  struct timeval tvStartPtrace;
  struct timeval tvLoggingEnd;
  struct timeval tvAnalyzerEnd;

  bool shouldCkpt() {return ckptClock > 0;}
};

class ServerConfig {
 private:
  uint32_t _flags;

  char _nextHost[FILENAME_LEN];
  char _replayName[FILENAME_LEN];
  char _ltpoint[FILENAME_LEN];
  char _analyzerName[FILENAME_LEN];
  char _localAnalyzer[FILENAME_LEN];
  char _streamAnalyzer[FILENAME_LEN];
  char _sharedObj[FILENAME_LEN];
  char _ipoints[FILENAME_LEN];
  char _tpoints[FILENAME_LEN];
  char _treeOutputName[FILENAME_LEN];

  char _logLevel;
  int _devFd;
  int _logFd;
  u_long _stackHint;
  u_long _exitClock;

  std::vector<Epoch> _epochs;

 public:
  std::unordered_map<pid_t, int> _waiters;

  ServerConfig(const int devFd);
  ServerConfig(const char *replayName,
               const char *soName,
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
               const int devFd);


  ServerConfig(const Config::ServerConfig &other)
  {
    _flags = other._flags;
    _devFd = other._devFd;
    _logFd = other._logFd;
    _stackHint = other._stackHint;
    _exitClock = other._exitClock;
    _logLevel = other._logLevel;

    strcpy(_nextHost, other._nextHost);
    strcpy(_replayName, other._replayName);
    strcpy(_ltpoint, other._ltpoint);
    strcpy(_analyzerName, other._analyzerName);
    strcpy(_localAnalyzer, other._localAnalyzer);
    strcpy(_streamAnalyzer, other._streamAnalyzer);
    strcpy(_sharedObj, other._sharedObj);
    strcpy(_ipoints, other._ipoints);
    strcpy(_tpoints, other._tpoints);
    strcpy(_treeOutputName, "");
  }

  void operator=(const Config::ServerConfig &other)
  {
    _flags = other._flags;
    _devFd = other._devFd;
    _logFd = other._logFd;
    _stackHint = other._stackHint;
    _exitClock = other._exitClock;

    strcpy(_nextHost, other._nextHost);
    strcpy(_replayName, other._replayName);
    strcpy(_ltpoint, other._ltpoint);
    strcpy(_analyzerName, other._analyzerName);
    strcpy(_localAnalyzer, other._localAnalyzer);
    strcpy(_streamAnalyzer, other._streamAnalyzer);
    strcpy(_sharedObj, other._sharedObj);
    strcpy(_ipoints, other._ipoints);
    strcpy(_tpoints, other._tpoints);
  }

  void setTreeOutput(const char *outputName);
  bool hasTreeOutput() { return strlen(_treeOutputName) > 0;}
  void addNextHost(const char* nextHost);

  void addEpoch (Epoch e) { _epochs.push_back(e);};
  u_int numEpochs() {return _epochs.size();};

  int readConfig(int s);
  int sendConfig(int s);
  int sendAck(int s);
  int sendOutput(int s);
  int sendTreeOutput(int s);
  int sendStats(int s);
  int sendCkptStats(int s);
  int sendAltrace(int s);
  int recvReplayFiles(int s);

  int doEpochs();
  int doCkptEpochs();
  int doAnalyzers();
  int waitForDone();
  int printTimes();

  bool useEpochAnalyzer();
  bool isStep()     {return (_flags & STEP);}
  bool isAck()     { return (_flags & SEND_ACK);}
  bool isSync() {return (_flags & SYNC_REPLAY);}
  bool isBuiltInLogger() {return (_flags & BUILT_IN_LOGGER);}

  bool getEpochStats(int i)
  {
    return !isCheckpoint() || _epochs[i].shouldCkpt();
  }

  void dump();

  static uint32_t buildFlag(const bool ckpt,
                            const bool tree,
                            const bool wait,
                            const bool sync,
                            const bool builtInLogger,
                            const bool fork,
                            const bool memProtector,
                            const bool altrace,
                            const bool ckptNOT,
                            const bool step = false);

  inline bool isPtrace()
  {
    return (_flags & DO_PTRACE);
  }

  inline bool isCheckpoint()
  {
    return (_flags & DO_CKPT);
  }

  inline bool isCheckpointNOT()
  {
    return (_flags & DO_CKPT_NOT);
  }

  inline bool isTreeMerge()
  {
    return (_flags & DO_TREE_MERGE);
  }


  inline bool isFork()
  {
    return (_flags & FORK);
  }

  inline bool isMemProtector()
  {
    return (_flags & MEM_PROTECTOR);
  }

  inline bool isAltrace()
  {
    return (_flags & ALTRACE);
  }
};

class Host {
 public:
  char _name[FILENAME_LEN];
  char _outputDir[FILENAME_LEN];
  int _epochCapacity;
  int _port;
  int _s;

  struct sockaddr_in _addr;

  struct timeval tvStart;
  struct timeval tvSyncStop;
  struct timeval tvWaitStop;
  struct timeval tvEnd;

  ServerConfig _serverConfig;
  Host(): _serverConfig(-1) {};
  Host(const char *hostname,
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
       const uint32_t flags);


  Host(const Config::Host &other): _serverConfig(other._serverConfig)
  {
    strcpy(_name, other._name);
    strcpy(_outputDir, other._outputDir);
    _epochCapacity = other._epochCapacity;
    _port = other._port;
    _addr = other._addr;
    _s = other._s;
  }

  void operator=(const Config::Host &other)
  {
    strncpy(_name, other._name, FILENAME_LEN);
    strncpy(_outputDir, other._outputDir, FILENAME_LEN);
    _epochCapacity = other._epochCapacity;
    _serverConfig = other._serverConfig;
    _port = other._port;
    _s = other._s;
    _addr = other._addr;
  }


  void dump();
  void setTreeOutput(const char *outputName);
  void addNextHost(const char* nextHost);
  int  addEpoch(Epoch &epoch);
  bool epochSlotsLeft();
  int capacity();
  int numEpochs();

  int connectToHost();
  int sendServerConfig();
  int sendReplayFiles(std::vector<struct replay_path> &log_files,
                      std::vector<struct cache_info> &cache_files);

  int waitForResponse();
  int getOutput(int offset);
  int getStats(int offset);
  int getAltrace(int offset);

  friend std::ostream &operator<<(std::ostream &os, const Host &h)
  {
    os << h._name << " " << h._outputDir;
    return os;
  }
};


class ServerThread {
 public:
  int offset;
  Host *h;
  pthread_t tid;
};
}
#endif
