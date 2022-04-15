#include <iostream>
#include <string>
#include <vector>

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>

#include <unordered_map>


#include <capstone/capstone.h>
#include "instruction_parser.hpp"

//#define DEBUG(...) fprintf(stderr, __VA_ARGS__)
#define DEBUG(x,...)


using std::function;

namespace Parser {
InstructionInfo::InstructionInfo(const std::string &binaryName) {

  struct stat sbuf;
  int fd = open(binaryName.c_str(), O_RDONLY);

  if (stat(binaryName.c_str(), &sbuf) > 0) {
    std::cerr << "Can't stat " << binaryName << " " << errno << std::endl;
    assert(false);
  }
  _size = sbuf.st_size;
  // what if I read this in at once?

  _fileData = (uint8_t*) mmap(0, _size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (_fileData == MAP_FAILED) {
    std::cerr << "Can't map " << binaryName << " " << errno << std::endl;
    assert(false);
    }

  close(fd);

  assert (cs_open(CS_ARCH_X86, CS_MODE_32, &_handle) == CS_ERR_OK);
  cs_option(_handle, CS_OPT_DETAIL, CS_OPT_ON); // get 'dem details (!)
  cs_option(_handle, CS_OPT_SYNTAX, CS_OPT_SYNTAX_ATT);
}


InstructionInfo::~InstructionInfo() {
  cs_close(&_handle);
}


std::unordered_set<u_long>
InstructionInfo::getInstructions(uint32_t start,
                                 uint32_t stop,
                                 uint32_t secStart,
                                 uint32_t secOffset,
                                 x86_insn match) {

  cs_insn *insn;
  size_t  count;
  std::unordered_set<u_long> rtns;
  uint32_t offset = secStart + (start - secOffset);
  uint32_t size = stop - start;


  assert (stop > start);
  count = cs_disasm(_handle, _fileData + offset, size, start, 0, &insn);
  DEBUG("Get insts from %x %x %x to %x\n", start, stop, offset, size);
  DEBUG("Found %d instructions\n", count);

  if (count > 0) {
    size_t j;
    for (j = 0; j < count; j++) {
      if (match == X86_INS_ENDING || insn[j].id == match) {
        // fprintf(stderr, "found return at %x\n", insn[j].address);
        rtns.insert(insn[j].address);
      }
    }
    cs_free(insn, count);
  } else
    printf("ERROR: Failed to disassemble given code!\n");

  return rtns;
}

uintptr_t
InstructionInfo::getInstructions(uintptr_t offset, uintptr_t size, uintptr_t load,  cs_insn **in) {
  return cs_disasm(_handle, _fileData + offset, size, load, 0, in);
}


SeqInstIter InstructionInfo::getSeqInstIter(uint32_t section,
                                            uint32_t sectionSize,
                                            uint32_t loadAddr) {

  assert (sectionSize > 0);
  cs_insn *insn = nullptr;
  size_t count = cs_disasm(_handle, _fileData + section, sectionSize, loadAddr, 0, &insn);
  DEBUG("getseqInstIter (%x - %x) returns %p [%llx - %llx]\n",
        section, section + sectionSize  - 1, insn, insn[0].address, insn[count - 1].address);
  /*  for (int i = 0; i < count; ++i) {
    DEBUG("address: %llx\n", insn[i].address);
    }*/

  assert (insn != nullptr);
  return SeqInstIter(insn, count);
}

void SeqInstIter::getInstructions(uint32_t stop, function<void(cs_insn)> f) {

  while (index < num_insts && insts[index].address <= stop) {
    f(insts[index]);
    index++;
  }
}

u_long SeqInstIter::getLastInstAddr() {
  return insts[index-1].address;
}

} /*namespace Parser*/
