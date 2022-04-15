#pragma once

#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>

#include <cassert>

#include <iostream>
#include <sstream>
#include <unordered_set>
#include <map>
#include <vector>

#include <boost/serialization/access.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/vector.hpp>

#include "address.hpp"

#define MAX_FUNCTION_NAME 128

using std::make_unique;
using std::shared_ptr;
using std::vector;

namespace Configuration {
typedef std::map<Address*, Address*> RegionMap;
typedef std::pair<Address*, Address*> RegionIter;
typedef std::map<uintptr_t, uintptr_t> ClockMap;
typedef ClockMap::const_iterator ClockIter;
class TracerConfiguration {
 private:
  friend class boost::serialization::access;
  // For managing a software breakpooint
  RegionMap _breakpointRegions;

  ClockMap _clockRegions;
  std::string  _outputFunction;
  shared_ptr<TracerConfiguration> _filter;
  bool _regFree; // we can optimize the filter function (maybe the output function too?)

 public:
  TracerConfiguration(): _outputFunction(""), _filter(nullptr), _regFree(false) {};
  TracerConfiguration(std::string f):
      _outputFunction(f), _filter(nullptr), _regFree(false) {};

  ~TracerConfiguration() {
    for (auto p : _breakpointRegions) {
      delete p.first;
      delete p.second;
    }
  }

  // getters:
  inline std::string getOutputFunction(void) const {return _outputFunction;}
  inline const shared_ptr<TracerConfiguration>& getFilter(void) const {return _filter;}
  inline void
  forEachBreakpoint(std::function<void(const RegionIter, const TracerConfiguration&)> func) const {
    for(auto bp : _breakpointRegions)
      func(bp, *this);
  }

  ClockIter clocksBegin() const {return _clockRegions.begin();}
  ClockIter clocksEnd() const {return _clockRegions.end();}

  // tracer is continuous if it has no breakpoints:
  inline bool isContinuous(void) {return _breakpointRegions.size() == 0;}
  inline int isRegFree(void) { return _regFree;}

  // setters:
  inline void setOutputFunction(std::string f) {_outputFunction = f;}
  inline void setFilter(shared_ptr<TracerConfiguration> f) {_filter = f;}
  inline void setRegFree(bool in) { _regFree = in;}

  inline void addBreakpoint(Address *start, Address *end) {
    assert (start && end);
    _breakpointRegions.emplace(std::make_pair(start, end));
  }

  inline void addClockRange(uintptr_t start, uintptr_t end) {
    assert (start && end);
    _clockRegions.emplace(std::make_pair(start, end));
  }

  std::string toString() {
    std::stringstream ss;
    ss << *this;
    return ss.str();
  };

  friend std::ostream &operator<<(std::ostream &os, const TracerConfiguration &t) {
    os << t.getOutputFunction() << " bps ";
    t.forEachBreakpoint([&os](const RegionIter it, const TracerConfiguration &unused) {
        (void) unused;
        os << std::hex << "(" << *(it.first) << ", " << *(it.second) <<") "; });
    if (t._filter)
      os << " filter " << *t._filter;
    else
      os << " no filter";
    os << " clocks ";
    for (auto it = t.clocksBegin(), en = t.clocksEnd(); it != en; ++it) {
      os << "[" << it->first << ", " << it->second <<") ";
    }

    return os;
  };

  template<class Archive>
  void save(Archive &ar, const unsigned int version) const {
    bool hasFilter = _filter != nullptr;

    ar & _breakpointRegions;
    ar & _clockRegions;
    ar & _outputFunction;
    ar & _regFree;
    ar & hasFilter;

    if (hasFilter) {

      // boost won't let me do this with the pointer object directly
      ar & (*_filter);
    }
  }

  template<class Archive>
  void load(Archive &ar, const unsigned int version) {
    bool hasFil = false;

    ar & _breakpointRegions;
    ar & _clockRegions;
    ar & _outputFunction;
    ar & _regFree;

    ar & hasFil;
    if (hasFil) {
      TracerConfiguration tc;
      ar & tc;
      _filter = make_unique<TracerConfiguration>(tc.getOutputFunction());
      // filters aren't allowed to have breakpointregions
    }
    else {
      _filter = nullptr;
    }
  }
  BOOST_SERIALIZATION_SPLIT_MEMBER()
}; /* end class TracerConfiguration */

std::vector<TracerConfiguration*> parseTPoints(std::string);
void writeTPoints(std::vector<TracerConfiguration*> tracers, std::string tracepoints);


class InstConfiguration {
 private:
  friend class boost::serialization::access;
  // For managing a software breakpooint
  RegionIter _bp;
  std::string  _link;
  bool _jump;

 public:
  InstConfiguration(): _link("") {};
  InstConfiguration(std::string l): _link(l) {};

  // getters:
  inline bool getJump(void) const {return _jump;}
  inline std::string getLink(void) const {return _link;}
  inline RegionIter  getBP(void) const {return _bp;}

  // setters:
  inline void setJump(bool j) {_jump = j;}
  inline void setLink(std::string f) {_link = f;}
  inline void setBreakpoint(Address *start, Address *end = nullptr) {
    if (end == nullptr) {
      end = new Address(start->getLibrary(),
                        start->getFlags(),
                        start->getOffset() + 1,
                        start->getPid());
    }
    _bp = std::make_pair(start, end);
  }

  std::string toString() {
    std::stringstream ss;
    ss << *this;
    return ss.str();
  };


  friend std::ostream &operator<<(std::ostream &os, const InstConfiguration &t) {
    return os << std::hex
              << "instConfig=" << t.getLink()
              << " location=" << t._bp.first->getOffset()
              << " isJump=" << t._jump;
  };

  template<class Archive>
  void serialize(Archive &ar, const unsigned int version) {
    ar & _bp;
    ar & _link;
    ar & _jump;
  }

}; /* end class InstConfiguration */

vector<InstConfiguration*> parseInstPoints(std::string);
void writeInstPoints(vector<InstConfiguration*> tracers, std::string tracepoints);


} /* end namespace Configuration */
