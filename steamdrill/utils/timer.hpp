#include <chrono>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#ifndef __TIMER_HPP__
#define __TIMER_HPP__

#define MS_CAST  std::chrono::duration_cast<std::chrono::milliseconds>

namespace Timer {
typedef std::chrono::high_resolution_clock myClock;
typedef std::chrono::time_point<myClock> timePoint;
}

namespace std {
inline std::ostream& operator<<(ostream &out, const Timer::timePoint &time) {
  auto d = MS_CAST(time.time_since_epoch());
  return out << d.count();
}
}

namespace Timer {
class StampManager {
  class Stamp {
   public:
    std::string name;
    Timer::timePoint tp;
    Stamp(std::string n, Timer::timePoint t): name(n), tp(t) {};
  };
  std::vector<Stamp> timestamps;
  std::vector<std::pair<std::string, u_long>> counts;

 public:
  inline void addStamp(std::string s) {
    timestamps.emplace_back(s, myClock::now());
  }
  inline void addCount(std::string s, uint64_t c) {
    counts.emplace_back(s, c);
  }

  inline void writeStamps(std::string f) {
    std::cerr << "writing stamps to " << f << std::endl;
    std::ofstream outfile (f, std::ios_base::app);
    for (auto ts : timestamps) {
      outfile << ts.name << ":" << ts.tp << std::endl;
    }
    for (auto c : counts) {
      outfile << c.first << ":" << c.second << std::endl;
    }
  }
};
}


#endif /*__TIMER_HPP__*/
