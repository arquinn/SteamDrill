#ifndef _REPLAY_BININFO
#define _REPLAY_BININFO

// TODO: there's an approach to this that saves the value in shared memory to reduce overhead.

#include<unordered_map>
#include<unordered_set>

#include "mem_object.hpp"

#include "../binary_parser/binary_parser.hpp"

using namespace std;
class ReplayBinInfo {
public:
  string rpName;
  unordered_set<MemObject> files;
  unordered_map<string, uintptr_t> globals;

  ReplayBinInfo(string replay) {
    //TODO use the shared memory bin, make all of these happen on demand:
    getMappings(replay, [this](MemObject &&mo) {this->files.insert(std::move(mo));});
  }

  unordered_set<MemObject>::iterator getFile(uintptr_t addr) {
    return find_if(files.begin(), files.end(), [addr] (const MemObject &m) {
        return m.start <= addr && m.end > addr;
      });
  }

  unordered_set<MemObject>::iterator begin() {
    return files.begin();
  }

  unordered_set<MemObject>::iterator end() {
    return files.end();
  }

  uintptr_t getGlobal(string glblName) {
    if (globals.empty()) {
      // figure out globals\ I guess then:
      for (auto &mo : files) {
        if (mo.prot == 3) { // this means its read + write (HAZ HACKSY)
          for (auto &g : Parser::getGlobals(mo.name)) {
            globals.emplace(g.name, g.ptrLoc + mo.start);
          }
        }
      }
    }
    auto it = globals.find(glblName);
    return it != globals.end() ? it->second : 0;
  }
};

#endif /* _REPLAY_BININFO */

