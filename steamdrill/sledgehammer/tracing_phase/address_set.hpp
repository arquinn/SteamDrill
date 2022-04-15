#include <sys/mman.h>
#include <unistd.h>

#include <algorithm>
#include <stdlib.h>
#include <vector>

#include <unordered_map>
#include <iostream>

#include "log.hpp"

#ifndef __ADDRESS_SET_H_
#define __ADDRESS_SET_H_

#define WORD_ALIGN(addr) ((addr + 3) & 0xfffffffc)


namespace Tracer {

class AddressSetIter {
 private:
  int timestamp;
  std::unordered_map<u_long, u_long>::iterator iter, end;

 public:
  AddressSetIter(int to,
                 std::unordered_map<u_long, u_long>::iterator it,
                 std::unordered_map<u_long, u_long>::iterator en):
      
      timestamp(to), iter(it), end(en) {};

  u_long operator*() const { return iter->first;}

  AddressSetIter operator++() {
    while (++iter != end && (iter)->second != timestamp);
    return *this;
  }

  friend bool operator==  (const AddressSetIter &a, const AddressSetIter b) {
    return a.iter == b.iter;
  }

  friend bool operator!=  (const AddressSetIter &a, const AddressSetIter b) {
    return a.iter != b.iter;
  }
};

class AddressSet {
private:
  std::unordered_map<u_long, u_long> _addrs;
  u_long _timeStep;

public:
  AddressSet(): _timeStep(0) {};

  inline int size() { return _addrs.size(); }
  inline void incTimeStep() {_timeStep++;}

  bool addAddr(u_long addr) {
    u_long wordAddr = WORD_ALIGN(addr);
    auto elem = _addrs.find(wordAddr);

    // filter out NULL here:
    if (!wordAddr) return false;
    if (elem == _addrs.end() ) {
      TRACE("word was not in set {:x}", wordAddr);
      _addrs.emplace(wordAddr, _timeStep);
      return true;
    }
    bool updated = elem->second < _timeStep - 1;
    elem->second = _timeStep;

    TRACE("word may have been in set {:x} {}", wordAddr, updated);
    return updated;
  }

  bool contains(long addr, long timeStepOffset = 0) const {
    u_long check = _timeStep + timeStepOffset;
    auto a = _addrs.find(WORD_ALIGN(addr));
    return a != _addrs.end() ? a->second == check : false;
  }

  AddressSetIter begin() {return AddressSetIter(_timeStep, _addrs.begin(), _addrs.end());}
  AddressSetIter end() { return AddressSetIter(_timeStep, _addrs.end(), _addrs.end());}

  std::string debugString() {
    std::stringstream ss;
    ss << *this;
    return ss.str();
  }

friend std::ostream &operator<<(std::ostream &os, const AddressSet &as)  {
  std::vector<u_long> storedList;

  for (auto pr : as._addrs){
    storedList.push_back(pr.first);
  }

  std::sort (storedList.begin(), storedList.end());
  os << std::hex;
  os << "step=" << as._timeStep << " " << std::endl;

  for (auto pr : storedList) {
    //if (as.contains(pr))
    auto asIter = as._addrs.find(pr);
    os << "(addr=" << asIter->first << ", time=" << asIter->second << "); ";
  }
  return os;
}


};

} // namesapce ThreadGroup


#endif
