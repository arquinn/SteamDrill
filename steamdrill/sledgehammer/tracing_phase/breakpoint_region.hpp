// A DynamicRegion is a dynamic region of the program (start,end) that
// stores the memory values of the program from [start, len)

// In some cases, we might want redundent pointers to the same BreakpointRegion,
// so we use shared_ptrs over unique_ptrs

#pragma once

#include <cstdint>
#include <memory>

using std::shared_ptr;
namespace Tracer {
class BreakpointRegion
{
 private:
  uintptr_t _start;
  uintptr_t _end;
  shared_ptr<char> _region;
  shared_ptr<char> _breakpoint;
  long _regionLen;

 public:
  BreakpointRegion(uintptr_t s, uintptr_t e, shared_ptr<char> r, shared_ptr<char> bp, long l):
      _start(s), _end(e), _region(r), _breakpoint(bp),  _regionLen(l)  {};

  BreakpointRegion(BreakpointRegion &&other) {
    _start = std::move(other._start);
    _end = std::move(other._end);
    _region = std::move(other._region);
    _breakpoint = std::move(other._breakpoint);
    _regionLen = std::move(other._regionLen);
  };

  inline uintptr_t getStart() const {return _start;}
  inline uintptr_t getEnd() const {return _end;}
  inline shared_ptr<char>& getRegion(void) {return _region;}
  inline shared_ptr<char>& getBreakpoint(void) {return _breakpoint;}
  inline long getRegionLen() const {return _regionLen;}
};
}
