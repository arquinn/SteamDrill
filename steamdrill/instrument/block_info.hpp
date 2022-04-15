#ifndef BLOCK_INFO_H
#define BLOCK_INFO_H

#include <cstdint>

#define JMP_SIZE 5

struct blockInfo {
  uintptr_t start;
  uintptr_t end;

  bool jump;
  bool uncondJump;
  bool notForwardUncondJump;
  mutable bool unique;

  // there will be more fields eventually
  blockInfo(): start(0), end(0), unique(true) {}
  blockInfo(uintptr_t s, uintptr_t e, bool j, bool u, bool nfu):
      start(s), end(e), jump(j), uncondJump(u), notForwardUncondJump(nfu),
      unique(true) {};

  void notUnique() const {unique = false;}

  bool operator<(const struct blockInfo other) const { return start < other.start;}
};

#endif /*BLOCK_INFO_H*/
