// type for storing a list of different ranges. Needed in order to track bounds
// on non-filtery tracing config information (eip, replay_clock)

#include <algorithm>
#include <cassert>
#include <climits>
#include <cstdint>
#include <iostream>
#include <vector>



#ifndef RANGE_LIST
#define RANGE_LIST

using std::vector;
typedef  std::pair<uintptr_t, uintptr_t> range;

class RangeList{
 public:
  vector<range> ranges;

  RangeList() {
    ranges.push_back({0, UINT_MAX});
  }
  RangeList(uintptr_t low, uintptr_t high) {
    ranges.push_back({low, high});
  }

  bool empty() {
    auto it = ranges.begin();
    return ranges.size() == 1 && it->first == 0 && it->second == 0;
  }

  void reset(uintptr_t low, uintptr_t high) {
    vector<range> nVec;
    nVec.push_back({low, high});
    std::swap(ranges, nVec);
  }

  RangeList intersectRanges(RangeList o) {
    //std::cerr << "intersect " << *this << " and " << o << std::endl;
    vector<range> newRanges;

    auto it = ranges.begin(), end = ranges.end(), oit = o.ranges.begin(), oend = o.ranges.end();
    while (it != end && oit != oend) {
      // is there an overlap?
      // suppose that it before oit: .. it->second] [oit->first ..
      // suppose that oit before it: .. oit->second] [it->first ..
      // if neither is true, there's an overlap.
      // So, check:      !(it->second < oit->first || oit->second < it->first)

      if (it->second >= oit->first && oit->second >= it->first) {
        newRanges.push_back({std::max(it->first, oit->first), std::min(it->second, oit->second)});

        if (it->second < oit->second) ++it;
        else ++oit; // else in case oit->second = it->second;
      }

      // is it entirely before oit?
      else if (it->second < oit->first) ++ it;

      // else, is oit entirely before it?
      else if (oit->second < it->first) ++ oit;
    }

    // in theory, we're done intersecting. let's try it out:
    RangeList rl;
    std::swap(rl.ranges, newRanges);
    //std::cerr << "intersected ranges " << rl << std::endl;
    return rl;
  }


  RangeList unionRanges(RangeList o) {
    // this will make ranges a sorted (potentially overlapping) mega list

    // save ourselves from dealling with this manually:
    if (empty()) {
      return o;
    }
    else if (o.empty()) {
      return *this;
    }
    //std::cerr << "merging " << *this << " " << o << std::endl;
    vector<range> nVec;
    std::merge(ranges.begin(), ranges.end(), o.ranges.begin(), o.ranges.end(), std::back_inserter(nVec));

    if (nVec.size() > 0) {
      auto it = ++nVec.begin();
      while (it != nVec.end()) {
        auto pit = it - 1;
        if (it->first < pit->second) {
          pit->second = std::max(pit->second, it->second);
          it = nVec.erase(it);
        }
        else {
          ++it;
        }
      }
    }
    RangeList rl;
    std::swap(rl.ranges, nVec);
    return rl;
  }

  friend std::ostream &operator<<(std::ostream &os, const RangeList rl) {
    os << std::hex << "{";
    for (auto r : rl.ranges) {
      os << "[" << r.first << ", " << r.second << ") ";
    }
    return os << "}";
  };
}; // end class RangeList


#endif /* RANGE_LIST*/
