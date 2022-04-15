#ifndef __TRACER_H_
#define __TRACER_H_
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <errno.h>

#include <cstdint>
#include <cassert>
#include <iostream>
#include <sstream>

#include <boost/optional.hpp>

#include "address.hpp"
#include "tracer_configuration.hpp"

#include "gadget.hpp"
#include "breakpoint_region.hpp"
#include "address_set.hpp"

typedef Configuration::TracerConfiguration TracerConfig;
using boost::optional;

namespace Tracer {
enum TracerState {ACTIVE, SET};
class Tracer {
 private:
  TracerConfig* _tc;
  TracerState _state;
  Gadget<0> *_gadget;

  Gadget<0> *_filterGadget;

  Configuration::ClockIter _clockIter, _clockEnd;
  bool always; // am I always supposed to do this?
  bool inRegionAlready; // did I just start this clock region?
  bool active;
  std::vector<shared_ptr<BreakpointRegion>> _breakpoints;
  std::unordered_map<uintptr_t, shared_ptr<BreakpointRegion>> _bpIndex;

 public:
  Tracer(TracerConfig *tc) {
    _tc = tc;
    _gadget = NULL;
    _filterGadget = NULL;
    _state = SET;
    _clockIter = tc->clocksBegin(), _clockEnd = tc->clocksEnd();
    always = _clockIter == _clockEnd;
    inRegionAlready = false;
    active = false;
  };
  //public memba
  AddressSet as;

  // Getters
  inline TracerConfig* getConfig() const  {return _tc;}
  inline Gadget<0>* getGadget() const {return _gadget;}

  inline Gadget<0>* getFilterGadget() const {return _filterGadget;}

  inline bool hasRegion(uintptr_t addr) {
    auto iter = _bpIndex.find(addr);
    return iter != _bpIndex.end();
  };

  inline TracerState getState(void) {return _state;}
  inline bool getInRegion(void) const { return inRegionAlready;}
  inline bool getActive(){return active;}

  // Setter
  inline void setActive(bool b) {active = b;}
  inline void setGadget(Gadget<0> *g) {_gadget = g;}
  inline void setFilterGadget(Gadget<0> *g) {_filterGadget = g;}

  inline void addBreakpointRegion(BreakpointRegion &&dr) {
    shared_ptr<BreakpointRegion> br = std::make_shared<BreakpointRegion>(std::move(dr));

    _breakpoints.push_back(br);
    for (uintptr_t addr = br->getStart(); addr <= br->getEnd(); ++addr) {
      _bpIndex.emplace(addr, br);
    }
  }

  inline void forEachBreakpoint(std::function<void(BreakpointRegion&)> func) {
    for(auto &bp : _breakpoints) {
      func(*bp);
    }
  }

  inline void forEachBreakpoint(std::function<void(const BreakpointRegion&)> func) const {
    for(auto &bp : _breakpoints) {
      func(*bp);
    }
  }

  typedef std::vector<shared_ptr<BreakpointRegion>>::iterator iter;
  iter begin() { return _breakpoints.begin(); }
  iter end() { return _breakpoints.end(); }

  bool isInRegion(u_long clock) {
    if (always) return true;
    // this requires clock regions to be non-overlapping
    while (_clockIter != _clockEnd && _clockIter->second < clock) {
      _clockIter++;
    }
    bool rtn = (_clockIter != _clockEnd && clock > _clockIter->first && clock < _clockIter->second);
    if (rtn && !inRegionAlready) {
      inRegionAlready = true;
    }
    else if (!rtn)
      inRegionAlready = false;
    // do I still have an iter? it the bound in that iter?
    return rtn;
  }
  // Printers for debugging
  inline std::string toString()  {
    std::stringstream ss;
    ss << *this;
    return ss.str();
  }

  friend std::ostream &operator<<(std::ostream &os, const Tracer &t) {
    auto config = t.getConfig();
    os << std::hex << *(config) << " @ ";
    t.forEachBreakpoint( [&os](const BreakpointRegion &r ) {
        os << "[" << r.getStart() << ", " << r.getEnd() << ") ";});
    os << std::dec << " nxt/curr clock region [" << t._clockIter->first << ", " << t._clockIter->second << ")";
    return os;
  };
}; /* end class TracerConfiguration */
} /* end namespace Configuration */
#endif /* __TRACER_H_ */
