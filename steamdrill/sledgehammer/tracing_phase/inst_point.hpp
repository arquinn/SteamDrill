#ifndef __INST_POINT_H_
#define __INST_POINT_H_
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <errno.h>

#include <cstdint>
#include <cassert>
#include <iostream>
#include <memory>
#include <sstream>

#include <boost/optional.hpp>

#include "breakpoint_region.hpp"
#include "address.hpp"
#include "tracer_configuration.hpp"


using Configuration::InstConfiguration;
using boost::optional;
using std::unique_ptr;

namespace Tracer {
enum InstPointState {IP_RESOLVED, IP_SET};
class InstPoint {
 private:
  InstConfiguration* _ic;
  uintptr_t _resolvedLink;
  uintptr_t _gadgetLen;
  uintptr_t _bpLoc;
  unique_ptr<BreakpointRegion> _breakpoint;
  uintptr_t _count;

 public:
  InstPoint(InstConfiguration *ic, uintptr_t bp) : _count(0) {
    _ic = ic;
    _resolvedLink = 0;
    _bpLoc = bp;
  };

  // Getters
  inline InstConfiguration* getConfig() const  {return _ic;}
  inline InstPointState getState(void) {
    return !_resolvedLink ? IP_RESOLVED : IP_SET;
  }
  inline uintptr_t getResolvedLink(void) {return _resolvedLink;}
  inline uintptr_t getBPLoc(void) {return _bpLoc;}
  inline unique_ptr<BreakpointRegion>& getBPRegion(void) {return _breakpoint;}
  inline uintptr_t getCount(void) { return _count;}

  inline bool isJump() {return _ic->getJump();}
  inline uintptr_t getGadgetLen(void) {return _gadgetLen;}

  // Setters
  inline void addCount() { _count++;}
  inline void setResolvedLink(uintptr_t l) {_resolvedLink = l;}

  // Needed sometimes for initialization:
  inline void setBreakpoint(shared_ptr<char> r, shared_ptr<char> bp, long l, long gadgetLen) {
    _breakpoint = std::make_unique<BreakpointRegion>(0, 0, r, bp, l);
    _gadgetLen = gadgetLen;
  }

  // Printers forp debugging
  inline std::string toString() {
    std::stringstream ss;
    ss << *this;
    return ss.str();
  }

  friend std::ostream &operator<<(std::ostream &os, const InstPoint &t) {
    return os << std::hex
              << "instPoint=" <<  *(t.getConfig())
              << " resolved=" << t._resolvedLink;
  };
}; /* end class InstPoint */
} /* end namespace Configuration */
#endif /* __INST_POINT_H_ */
