#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "address.hpp"
#include "tracer_configuration.hpp"

#include "log.hpp"
#include "task.hpp"

#include "../../config.h"


#ifndef __CONFIGURATION_H__
#define __CONFIGURATION_H__

using std::string;
using std::unordered_map;
using std::vector;

namespace Configuration {
class Configuration
{
 private:
  vector<InstConfiguration *> _instrumentation;
  vector<TracerConfiguration *> _ctracers;

  TracerConfiguration *_loadedTracerConfig;
  TracerConfiguration *_exitTracerConfig;
  TracerConfiguration *_threadStartTracerConfig;
  TracerConfiguration *_forkExitTracerConfig;


  // state for getting helper functions
  unordered_map<pid_t, Address*> _dlopens, _dlsyms, _syscalls, _waitpids,
    _mmaps, _opens, _mprotects, _mallocHooks, _freeHooks, _reallocHooks;


  std::string _tracerObj;

  JumpCounter _jc;

  // tracerlog files:
  vector<string> _tracerLogs;

  static Configuration *instance;

  void parseFunctions(std::string functions);

  Configuration(std::string instPoints,
                std::string tracePoints,
                std::string loadedTracer,
                std::string functions,
                vector<string> logFiles,
                JumpCounter jc);

 public:
  vector<string> getTracerLogs() const {return _tracerLogs;}
  std::string getTracerObj() const {return _tracerObj;}
  static void load(std::string instpoints,
                   std::string tracepoints,
                   std::string loadedTracer,
                   std::string functions,
                   vector<string> logs,
                   JumpCounter jc) {
    instance = new Configuration(instpoints, tracepoints, loadedTracer, functions, logs, jc);
  };

  static Configuration *getInstance() {
    if (!instance) {
      ERROR("Configuration wasn't loaded yet!");
      THROW("Config load Exception!");
    }

    return instance;
  }

  TracerConfiguration* getLoadedTracerConfig() { return _loadedTracerConfig;}
  TracerConfiguration* getExitTracerConfig() { return _exitTracerConfig;}
  TracerConfiguration* getThreadStartTracerConfig() { return _threadStartTracerConfig;}
  TracerConfiguration* getForkExitTracerConfig() {return _forkExitTracerConfig;}

  JumpCounter getJC(void) {return _jc;}
  std::vector<uintptr_t> getPingList();

  typedef std::vector<InstConfiguration *>::iterator iter;
  iter begin() {return _instrumentation.begin();}
  iter end()  {return _instrumentation.end();};

  typedef std::vector<TracerConfiguration *>::iterator titer;
  titer tracerBegin() {return _ctracers.begin();}
  titer tracerEnd() {return _ctracers.end();}


  string debugString() {
    std::stringstream ss;
    ss << *this;
    return ss.str();
  }

  friend std::ostream &operator<<(std::ostream &os, const Configuration &c) {
    std::stringstream logs;
    for (auto l : c._tracerLogs) logs << " " << l;
    os << "config obj=" << c._tracerObj
       << " logs= (" << logs.str()
       << ") ips=" << c._instrumentation.size();
    return os;
  };
}; /* end Class Configuration*/
} /* end namespace Configuration*/
#endif /*__CONFIGURATION_H__*/
