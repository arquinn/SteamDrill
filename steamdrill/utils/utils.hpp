#ifndef __SD_UTILS_H
#define __SD_UTILS_H

#define SEP '\u0002'
#define ITEM_SEP ','
#define FIELD_SEP ':'

#include <fcntl.h>
#include <gelf.h>
#include <libgen.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>


#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>


std::unique_ptr<Elf32_Ehdr>& getElf(std::string pathname);

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

inline const char* getPath(const char *str) {
  // get the relative directory of this executable based on proc:
  char dest[PATH_MAX * 2], path[PATH_MAX] = "/proc/self/exe";
  memset(dest, 0, sizeof(dest));

  if (readlink(path, dest, PATH_MAX) == -1)
    std::cerr << "problem with readlink " << errno << std::endl;
  else
  {
    // convert to just the directory,
    char *dir = dirname(dest);
    // strncpy(dest, dir, PATH_MAX * 2);
    strncat(dir, "/", PATH_MAX * 2);
    strncat(dir, str, PATH_MAX * 2);
    char *rpath = realpath(dir, NULL);
    return rpath;
  }

  return "";
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

#endif /*__SD_UTILS_H */
