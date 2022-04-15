#include <chrono>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#ifndef __TIMER_HPP__
#define __TIMER_HPP__

namespace Timer {
typedef std::chrono::high_resolution_clock myClock;
typedef std::chrono::time_point<myClock> timePoint;
}

namespace std{
std::ostream& operator<<(std::ostream &out, const Timer::timePoint &time);
}

namespace Timer {
class StampManager {
  class Stamp {
   public:
    std::string name;
    Timer::timePoint tp;
    Stamp(std::string n, Timer::timePoint t): name(n), tp(t) {};
    Stamp(Stamp &&s): name(std::move(s.name)), tp(s.tp) {};
  };
  std::vector<Stamp> timestamps;
 public:
  StampManager() {}
  StampManager(StampManager &&t):  timestamps(std::move(t.timestamps)) {}

  inline void addStamp(std::string s) {
    timestamps.emplace_back(s, myClock::now());
  }

  inline void writeStamps(std::string f) {
    std::ofstream outfile (f, std::ios_base::app);
    for (auto &ts : timestamps) {
      outfile << ts.name << ":" << ts.tp << std::endl;
    }
  }
};
}


#endif /*__TIMER_HPP__*/
