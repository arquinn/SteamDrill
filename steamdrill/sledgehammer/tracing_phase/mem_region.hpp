#ifndef __MEM_REGION_H_
#define __MEM_REGION_H_

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/user.h>
#include <unistd.h>

#include <string>
#include <iostream>
#include <sstream>
#include <unordered_map>

#include "log.hpp"

using std::string;

namespace ThreadGroup {

class Task; // whill be defined in task.hpp
class MemRegion
{
 private:
  u_long _start;
  u_long _length;
  u_long _prot;
  std::string _name;

  // mutable b/c adjusting _region doesn't change anything else
  mutable char *_region;

 public:
  MemRegion(const u_long start,
            const u_long end,
            const u_long prot)
  {
    _start = start;
    _length = end - start;
    _prot = prot;
  };

  MemRegion(const u_long start,
            const u_long end,
            const std::string prot)
  {
    _start = start;
    _length = end - start;
    _prot = convertProt(prot);
  };

  MemRegion(const u_long start,
            const u_long end,
            const std::string prot,
            const std::string name) {
    _start = start;
    _length = end - start;
    _prot = convertProt(prot);
    _name = name;
  };


  static inline int convertProt(const std::string protStr)
  {
    return (protStr[0] == 'r' ? PROT_READ : 0) |
        (protStr[1] == 'w' ? PROT_WRITE : 0) |
        (protStr[2] == 'x' ? PROT_EXEC : 0);
  }

  inline char** getRegionLoc(void) const {return &_region;}
  inline u_long getStart(void) const {return _start;}
  inline u_long getEnd(void) const {return _start + _length;}
  inline u_long getLen(void) const {return _length;}
  inline u_long getProt(void) const {return _prot;}
  inline string getName(void) const {return _name;}

  inline void setProt(u_long prot) {_prot = prot;}

  inline std::string toString() const
  {
    std::stringstream ss;
    ss << *this;
    return ss.str();
  } // end toString()

  bool operator==(const MemRegion &other) const
  {
    return (_start == other._start) && (_length == other._length);
  } //end operator==

  bool operator<(const MemRegion &other) const
  {
    return (_start < other._start);
  } //end operator<

  void dumpRaw(int fd) {
    size_t curr = 0;
    size_t rc = write(fd, (void*) &_length, sizeof(_length));
    if (rc <= 0) {
      ERROR("coudn't write length!? {} {}", rc, errno);
      return;
    }

    while (curr < _length) {
      rc = write(fd, _region + curr, _length - curr);
      if (rc <= 0) {
        ERROR("couldn't dump bytes to file descriptor? {} {}", rc, errno);
        return;
      }
      curr += rc;
    }
  }

  friend std::ostream &operator<<(std::ostream &os,
                                  const MemRegion &mr)
  {
    std::ios state(nullptr);
    state.copyfmt(os);

    os << std::hex << mr._start << "-" << mr._start + mr._length
       << ": " << mr._prot << " " << mr._name;

    os.copyfmt(state);
    return os;
  } //end operator<<
}; /* class MemRegion */


class MemRegionMatcher {
 public:
  std::string filename;
  u_long prot;
  u_long addr;
  MemRegionMatcher(string f = "", u_long p = 0, u_long a = 0):
      filename(f), prot(p), addr(a) {};
  MemRegionMatcher(string f = "", string p = 0):
      filename(f), prot(MemRegion::convertProt(p)), addr(0) {};


  bool matches(const MemRegion &m) const {
    if (!filename.empty() && m.getName().find(filename) != string::npos)
      return true;
    if (prot && m.getProt() & prot) // matches whenever p set in memRegion
      return true;
    if (addr && m.getStart() == addr)
      return true;

    return false;
  }

}; /* class MemRegionMatchSpec */

/*
class MemTracker
{

 public:
  enum Status {PROTECTED, CLEARED, UN_INIT};
  class Tracker : public MemRegion
  {
   public:
    Status _status;
    Tracker(const u_long start,
            const u_long end,
            const std::string prot)
        : MemRegion(start, end, prot) {}

    Tracker(const u_long start,
            const u_long end,
            const u_long prot)
        : MemRegion(start, end, prot) {}
  };
  typedef std::unordered_map<u_long, Tracker> MemStateMap;
 private:

  MemStateMap _memStates;

  static inline u_long Page(const u_long addr) {
    return addr & PAGE_MASK;
  }

  void protect(MemStateMap::iterator ps, const Task *t);
  void unprotect(MemStateMap::iterator ps, const Task *t);
 public:

  //void changePageProt(const u_long page, const u_long prot);
  void unprotectPage(const u_long page, const Task *t);
  void protectPage(const u_long page,  const Task *t);
  void updateTracking(const u_long start,
                      const u_long end,
                      const u_long prot);

  void protectAll(const Task *t);
  void unprotectAll(const Task *t);
};*/ /* class Page */

} /* end namespace ThreadGroup */

namespace std {
template<>
struct hash<ThreadGroup::MemRegion>
{
  size_t operator()(const ThreadGroup::MemRegion &mr) const
  {
    return hash<u_long>()(mr.getStart());
  } // end operator()
}; /* end struct hash<ThreadGroup::MemRegion*/
} /* end namespace std */
#endif /* __TRACER_H_ */
