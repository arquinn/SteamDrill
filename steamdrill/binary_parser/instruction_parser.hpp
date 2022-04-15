#include <algorithm>
#include <functional>
#include <string>
#include <utility>
#include <unordered_set>
#include <unordered_map>

#include <capstone/capstone.h>

extern "C"
{
#include <dwarf.h>
#include <elf.h>

//why are these at different spots..>? Ubuntu you confuse me!
#include <elfutils/libdw.h>
#include <libelf.h>
}

#ifndef __INSTRUCTION_PARSER__
#define __INSTRUCTION_PARSER__

using std::function;
using std::swap;
using std::move;


// this is stupid:
namespace std {

template <> struct hash<x86_insn> {
    std::size_t operator()(const x86_insn& i) const {
      return i;
    }
  };
}

namespace Parser {

class SeqInstIter {
  cs_insn* insts;
  size_t  index;
  size_t  num_insts;
  uint32_t high_bound;
 public:

  SeqInstIter(): insts(nullptr), index(0), num_insts(0) {
    //std::cerr << (void*)this << " constructed with " << insts << std::endl;
  }

  SeqInstIter(cs_insn *i, size_t t) :insts(i), index(0), num_insts(t) {
    //std::cerr << (void*)this << " constructed with " << insts << std::endl;
    high_bound = insts[num_insts - 1].address;
  }
  SeqInstIter(SeqInstIter &&ssi) {
    swap(insts, ssi.insts);
    index = move(ssi.index);
    swap(num_insts, ssi.num_insts);
    high_bound = move(ssi.high_bound);

    //std::cerr << (void*)this << " mcosnt with " << ssi.insts << " sw " << insts << std::endl;
  }

  SeqInstIter(SeqInstIter &ssi) = delete;

  SeqInstIter& operator=(SeqInstIter &&ssi) {
    swap(insts, ssi.insts);
    index = move(ssi.index);
    swap(num_insts, ssi.num_insts);
    high_bound = move(ssi.high_bound);

    // maybe these should be swaps??? (o/w I think things are destructed correctly)
    //std::cerr << (void*)this << " move= " << insts << ", " << ssi.insts << " from " << &ssi << std::endl;

    return *this;
  }


  ~SeqInstIter() {
    //std::cerr  << (void*)this << " destruct " << (void*)insts << std::endl;
    if (insts) cs_free(insts, num_insts);
  }

  bool finished() {return index == num_insts;}
  bool contains(uint32_t bound) {return bound <= high_bound;}

  void getInstructions(uint32_t stop, function<void(cs_insn)> f);

  u_long getLastInstAddr(void);
};

class InstructionInfo {
 private:
  csh _handle;
  uint8_t *_fileData;
  uint32_t _size;

 public:
  InstructionInfo(const std::string &binaryName);
  ~InstructionInfo();

  InstructionInfo(InstructionInfo const &) = delete;
  void operator=(InstructionInfo const &) = delete;

  std::unordered_set<u_long>
  getInstructions(uint32_t start, uint32_t stop, uint32_t secStart, uint32_t secOffset,
                  x86_insn match = X86_INS_ENDING);

  uintptr_t
  getInstructions(uintptr_t offset, uintptr_t size, uintptr_t load, cs_insn **in);

  // hard to use interfaces, but that's OK
  uint8_t* getFileData(){return _fileData;}
  uint32_t getSize() {return _size;}

  SeqInstIter getSeqInstIter(uint32_t secStart, uint32_t size, uint32_t loadAddr);

};
}

#endif /* INSTRUCTION_PARSER */
