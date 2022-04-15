#include <iostream>
#include <fstream>
#include <sstream>
#include <cassert>
#include <cstring>
#include <boost/regex.hpp>
#include <boost/lexical_cast.hpp>
#include <unordered_map>

#include "configuration.hpp"
#include "tracer.hpp"
#include "log.hpp"


namespace Configuration {

// init this to null...
Configuration* Configuration::instance = NULL;

Configuration::Configuration(string ipoints,
                             string tpoints,
                             string lpoints,
                             string obj,
                             vector<string> logs,
                             JumpCounter jc) {
  NDEBUG("parse instPoints");
  _instrumentation = parseInstPoints(ipoints);

  std::sort(_instrumentation.begin(), _instrumentation.end(),
            [](const InstConfiguration* x, const InstConfiguration* y) {
              return x->getBP().first->getOffset() > y->getBP().first->getOffset(); });

  unordered_map<uintptr_t, string> usedBytes;
  for (auto ic : _instrumentation) {
    if (ic->getJump()) {
      // assuming that a JMP is 5 byties:
      uintptr_t start = ic->getBP().first->getOffset();
      for (uintptr_t i = 0; i < 5; ++i) {
        auto val = usedBytes.emplace(start + i, ic->getLink());
        if (!val.second) {
          ERROR("{} is using {:x}! trying to use it for {}",
                val.first->second, start + i, ic->getLink());
          THROW("problem in instrumentation!");
        }
      }
    }
  }

  auto allTPs = parseTPoints(tpoints);
  for (auto &tp : allTPs) {
    if (tp->isContinuous()) {
      _ctracers.push_back(tp);
    }
  }

  NDEBUG("parse config tracers, found {} and {}", _instrumentation.size(), _ctracers.size());
  auto temp = parseTPoints(lpoints);
  assert (temp.size() > 0);
  auto it = temp.begin();
  _loadedTracerConfig = (*it);

  it++;
  assert (it != temp.end());
  _exitTracerConfig = *it;

  it++;
  assert (it != temp.end());
  _threadStartTracerConfig = *it;

  it++;
  assert (it != temp.end());
  _forkExitTracerConfig = *it;

  _tracerObj = obj;
  _tracerLogs = logs;

  _jc = jc;
}


std::vector<uintptr_t> Configuration::getPingList() {
  std::vector<uintptr_t> pingList;
  for (auto &tp : _ctracers) {
    TRACE("look at the clocks on that guy! {}", tp->toString());
    for (auto it = tp->clocksBegin(), en = tp->clocksEnd(); it != en; ++it) {
      if (it->first > 0 && it->second > 0) {
        // I'm assuming that 0 is the default (meaning there is nothing to see here0
        pingList.push_back(it->first);
        pingList.push_back(it->second);
      }
    }
  }
  sort(pingList.begin(), pingList.end());
  return pingList;
}
} // namespace Configuration
