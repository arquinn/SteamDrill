// executable for getting all of the blocks contained in an executable
// author: ARQ

#include <cassert>
#include <cstring>
#include <deque>
#include <iostream>
#include <set>
#include <unordered_map>
#include <unordered_set>

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

extern "C" {
#include <fcntl.h>
#include <unistd.h>
}


#include "args.hxx"


#include "../binary_parser/elf_parser.hpp"
#include "../binary_parser/instruction_parser.hpp"

#include "../instrument/block_info.hpp"

using std::cout;
using std::deque;
using std::set;
using std::unordered_map;
using std::unordered_set;

inline bool isJump(int opId) {
    return (opId >= X86_INS_JAE && opId <= X86_INS_JS) || opId == X86_INS_LJMP;
}

inline bool isUncondJump(int opId) {
  return opId == X86_INS_LJMP || opId == X86_INS_JMP;
}

inline bool isCondJump(int opId) {
  return isJump(opId) && !isUncondJump(opId);
}

inline bool isCall(int opId) {
  return opId == X86_INS_CALL || opId == X86_INS_LCALL;
}

inline bool isReturn(int opId) {
  return opId == X86_INS_RET;
}

#define DEBUG(...) fprintf(stderr, __VA_ARGS__);


int main(int argc, char **argv) {
  args::ArgumentParser parser("Trace a replay process.", "A.R.Q.");
  args::Group group(parser, "Required arguments:", args::Group::Validators::All);

  args::Positional<string> file(group, "file", "What file to analyze.");
  args::Positional<string> output(group, "output", "Output file.");
  args::HelpFlag help(parser, "help", "Display help", {'h', "help"});

  try
  {
    parser.ParseCLI(argc, argv);
  }
  catch (args::Help)
  {
    std::cout << parser << std::endl;
    return 0;
  }
  catch (std::exception &e)
  {
    std::cerr << e.what();
    std::cout << parser << std::endl;
    return -1;
  }

  std::cerr << "file " << args::get(file) << " " << args::get(file).size() << std::endl;

  deque<uintptr_t> workQueue;
  Parser::ElfInfo elf(args::get(file));

  // unfortunately, we need a lower api than what my inst decoding stuff provides
  const uint8_t *fileData = nullptr;
  int size;
  csh handle;

  struct stat sbuf;
  int fd = open(args::get(file).c_str(), O_RDONLY);
  if (fd < 0) {
    std::cerr << "Cannot open " << args::get(file) << " " << strerror(errno) << std::endl;
    assert(false);
  }
  if (stat(args::get(file).c_str(), &sbuf) > 0) {
    std::cerr << "Can't stat " << args::get(file) << " " << errno << std::endl;
    assert(false);
  }
  size = sbuf.st_size;
  fileData = (uint8_t*) mmap(0, size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (fileData == MAP_FAILED) {
    std::cerr << "Can't map " << args::get(file) << " " << errno << std::endl;
    assert(false);
  }
  close(fd);

  assert (cs_open(CS_ARCH_X86, CS_MODE_32, &handle) == CS_ERR_OK);
  cs_option(handle, CS_OPT_DETAIL, CS_OPT_ON);
  cs_option(handle, CS_OPT_SYNTAX, CS_OPT_SYNTAX_ATT);

  // FWIW, I'm not certain that this is the *best* thing to use here. I'm not 100% sure that
  // getFunctions gives me *everything*?
  for (auto &foo : elf.getFunctions()) {
    workQueue.push_back(foo->lowPC);
  }

  for (auto &sec : elf.getSections()) {
    const uint8_t *iter = fileData + sec.offset, *end = fileData + sec.offset + sec.size;
    int32_t loadShift = sec.loaded - sec.offset;

    while (iter + 3 < end) {
      if (*iter == 0xf3 && *(iter+1) == 0x0f && *(iter+2) == 0x1e && *(iter+3) == 0xfb) {
        uintptr_t loadAddr = (iter - fileData) + loadShift;
        workQueue.push_back(loadAddr);
        iter += 4;
      }
      else
        ++iter;
    }
  }


  // now, we find all of the reachable direct jump targets. We add these to the set below of all
  // blocks.  We don't figure out the endings just yet:
  set<uintptr_t> blockStarts;
  unordered_set<uintptr_t> processed; // have I seen this instruction before?
  while (!workQueue.empty()) {
    uintptr_t nextItem = workQueue.front();
    workQueue.pop_front();
    auto sec = elf.getSection(nextItem);

    if (sec) {
        // add the starting address to blockStarts
        blockStarts.insert(nextItem);

      if (processed.insert(nextItem).second) {
        // we've never seen this instruction before!

        int32_t loadShift = sec->loaded - sec->offset;
        const uint8_t *bytes = fileData + nextItem - loadShift;
        uint32_t size = sec->size - (nextItem - sec->loaded);
        uintptr_t address = nextItem;

        cs_insn *insn = cs_malloc(handle);
        bool done = false;

        while (!done &&  cs_disasm_iter(handle, &bytes, &size, (uint64_t*)&address, insn)) {
          processed.insert(insn->address);
          auto detail = insn->detail;

          // the iscall seems unnecessary, but we'd otherwise misshidden symbols
          if(isJump(insn->id) || isCall(insn->id)) {

            // the target of any type of jump is a new thing to process
            if (detail->x86.operands[0].type == X86_OP_IMM) {
              workQueue.push_back(detail->x86.operands[0].imm);
            }
          }
          if (isCall(insn->id) || isCondJump(insn->id)) {
            // the next instruction after a call or condJump are both new blocks.
            // I *could* just let a later iteration process them. But, I'm already right here...
            blockStarts.insert(insn->address + insn->size);
          }

          if (isReturn(insn->id) || isUncondJump(insn->id)) {
            // I cannot be sure if the next instruction is even reachable. So, call this iteration
            // quits. add the current block, and give up on this pass.
            done = true;

          }
        }
        cs_free(insn, 1);
      }
    }
  }
  // now that we know all block starts, we can determine where all of the blocks end.
  // since blocks can overlap, this is actually kinda complex. Oh well.
  set<struct blockInfo> blocks;
  for (auto start : blockStarts) {

    auto sec = elf.getSection(start);
    assert (sec);

    int32_t loadShift = sec->loaded - sec->offset;
    const uint8_t *bytes = fileData + start - loadShift;
    uint32_t size = sec->size - (start - sec->loaded);
    uintptr_t address = start;

    cs_insn *insn = cs_malloc(handle);
    bool jump = false;
    bool ujump = false;
    bool notForwardUjump = false;
    bool finished = false;

    while (!finished && cs_disasm_iter(handle, &bytes, &size, (uint64_t*)&address, insn)) {

      auto detail = insn->detail;

      if (isJump(insn->id) || isCall(insn->id)) {
        jump = true;
      }

      // for now just add in all unconditional jumps. There's another possible layer of opt
      if (isUncondJump(insn->id)) {
        ujump = true;
        auto jumpTo = insn->detail->x86.operands[0];

        // we can skip jumps that go to a forwards address (our tiebreak covers it)
        // not sure about non IMM addresses, so we'll include those in the to be counted set.
        // they probably aren't too common *crosses fingers*
	notForwardUjump = !(jumpTo.type == X86_OP_IMM && (uintptr_t)jumpTo.imm < (uintptr_t)insn->address);
        //        if (jumpTo.type == X86_OP_IMM) {
        //          fprintf(stderr, "immediate jump to %lx from %lx\n", (uintptr_t)jumpTo.imm, (uintptr_t)insn->address);
        //        }
      }


      if (isJump(insn->id) || isCall(insn->id) || isReturn(insn->id)) {
        // jumps, calls, and returns area always the end:
        blocks.emplace(start - loadShift, insn->address + insn->size - loadShift, jump, ujump, notForwardUjump);
        assert (start != insn->address + insn->size);
        finished = true;
      }
      else if ((uintptr_t)insn->address != start &&
               blockStarts.find((uintptr_t)insn->address) != blockStarts.end()) {
        // if the current instruction is a block, then we're at the end:
        blocks.emplace(start - loadShift, insn->address - loadShift, jump, ujump, notForwardUjump);
        assert (start != (uintptr_t)insn->address);
        finished = true;
      }
    }
    if (!finished) {
      blocks.emplace(start - loadShift, insn->address - loadShift, jump, ujump, notForwardUjump);
      jump = false;
      ujump = false;
      notForwardUjump = false;
    }
    cs_free(insn, 1);
  }

  // determine uniqueness. one iteration marks how many times each byte is used, another
  // then updates uniqueness of each block by using said uniqueness
  unordered_map<uintptr_t, uint32_t> byteUsedCount;
  for (auto b : blocks) {
    assert (b.start != b.end);
    for (uintptr_t i = b.start; i < b.end; ++i) {
      byteUsedCount[i] += 1;
    }
  }
  for (auto &b : blocks) {
    for (uintptr_t i = b.start; i < b.start + JMP_SIZE; ++i) {
      if (byteUsedCount[i] > 1) {
        b.notUnique();
        break;
      }
    }
  }

  std::cerr << "found " << blocks.size() <<  " blocks" << std::endl;

  //todo: I think that relocations *might* add new targets. My thought is that
  //      relocations are relative to the load address, so we're safe? But I'm not positive.

  auto out = args::get(file), dir = args::get(output);
  char *base = new char[out.size() + 1];
  strcpy(base, out.c_str());
  base = basename(base);

  char *full = new char[dir.size() + strlen(base) + 2];
  strcpy(full, dir.c_str());
  full[dir.size()] = '/';
  strcpy(full + dir.size() + 1, base);


  fd = open(full, O_WRONLY | O_CREAT, 0777);
  for (auto b : blocks) {
    write(fd, (void*)&(b), sizeof(b));
  }
}
