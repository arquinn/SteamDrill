#include <iostream>
#include "timer.hpp"

namespace std {
#define MS_CAST  std::chrono::duration_cast<std::chrono::milliseconds>

std::ostream& operator<<(ostream &out, const Timer::timePoint &time) {
  auto d = MS_CAST(time.time_since_epoch());
  return out << d.count();
}
}

