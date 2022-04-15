#include <cstring>  // for std::strcmp

extern "C" {
#include <libgen.h>
#include <dirent.h>
}

#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <list>
#include <string>
#include <unordered_map>
#include <vector>

#define D_steamdrill 9

#ifdef SPDLOG_ACTIVE_LEVEL
#undef SPDLOG_ACTIVE_LEVEL
#endif
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_ERROR

// use to add in extracallLoc debugging
#define ECALLLOC_DEBUG
#include "spdlog/spdlog.h"
#include "spdlog/fmt/ostr.h"
#include "../utils/address.hpp"
#include "../utils/tracer_configuration.hpp"
#include "../utils/replay_bin_info.hpp"
#include "../utils/mem_object.hpp"
#include "../binary_parser/elf_parser.hpp"
#include "../binary_parser/instruction_parser.hpp"

#include "block_info.hpp"
#include "instrument.hpp"

#include "config.h" // contains the type of jump counter that we'll be using

// I could do this with the pass api, but not worth it given
// no reuse for this code
using std::function;
using std::initializer_list;
using std::list;
using std::make_pair;
using std::filesystem::path;
using std::string;
using std::stringstream;
using std::unordered_map;
using std::unique_ptr;
using std::vector;

using Configuration::Address;
using Configuration::RegionIter;
using Configuration::TracerConfiguration;
using Configuration::InstConfiguration;

#define GET_BYTE(x, shift) static_cast<u_char>(((x) >> (shift*8)) & 0xff)
#define GET_BYTES(x, sep) GET_BYTE((x),0) sep GET_BYTE((x),1) sep GET_BYTE((x),2) sep GET_BYTE((x),3)

#define TBEGIN 0x1
#define TEND 0x2


//static Timer::StampManager mrManager;
enum JumpCounter jumpCounterType = DEFAULT;
namespace Instrument {
class fxnPointer {
    uintptr_t nextFxn;
    unordered_map<string, uintptr_t> index;
    unordered_map<uintptr_t, string> fxns;
    static unique_ptr<fxnPointer> theInstance;

  public:
    static fxnPointer *getInstance(void) {
        if (theInstance == nullptr) {
            theInstance = unique_ptr<fxnPointer>(new fxnPointer());
            theInstance->index.emplace("tracerBegin", TBEGIN);
            theInstance->index.emplace("tracerEnd", TEND);
            theInstance->fxns.emplace(TBEGIN,"tracerBegin");
            theInstance->fxns.emplace(TEND, "tracerEnd");

            theInstance->nextFxn = TEND + 1;
        }
        return theInstance.get();
    }

    uintptr_t getBegin(void) {return TBEGIN;}
    uintptr_t getEnd(void) {return TEND;}
    uintptr_t getIndex(string input) {
        auto it = index.find(input);
        if (it == index.end()) {
            uintptr_t fxnLoc = nextFxn++;
            it = index.emplace(input, fxnLoc).first;
            fxns.emplace(fxnLoc, input);
        }
        return it->second;
    }

    std::string getFxn(uintptr_t index) {
        auto it = fxns.find(index);
        assert (it != fxns.end());
        return it->second;
    }
};
unique_ptr<fxnPointer> fxnPointer::theInstance = nullptr;

inline bool isJmp(int opId) {
    return (opId >= X86_INS_JAE && opId <= X86_INS_JS) || opId == X86_INS_LJMP;
}

inline bool isUncondJump(int opId) {
  return opId == X86_INS_LJMP || opId == X86_INS_JMP;
}

inline bool isCondJmp(int opId) {
    return opId >= X86_INS_JAE && opId <= X86_INS_JS && opId != X86_INS_JMP;
}


inline bool isCall(int opId) {
  return opId == X86_INS_CALL || opId == X86_INS_LCALL;
}

class Block {
 private:
  cs_insn * insns;
  uint32_t size;
  bool cfInstFlag; // flag for if this should have cf instrumentation
  bool unique; // uniqueness flag
  uintptr_t startAddr;
  uintptr_t endAddr;
  uintptr_t loadShift;


 public:
  Block(uintptr_t s, uintptr_t e, uintptr_t ls):
      startAddr(s), endAddr(e), loadShift(ls),
      insns(NULL), size(0), cfInstFlag(false), unique(false) {};

  // getters:
  bool      isCF() const { return cfInstFlag;}
  bool      isUnique() const { return unique;}
  uintptr_t getLow() const {return startAddr + loadShift;}
  uintptr_t getHigh() const {return endAddr + loadShift;}

  //uintptr_t getLow() const {return insns[0].address;}
  //uintptr_t getHigh() const {return insns[size - 1].address + insns[size-1].size;}
  uintptr_t getSize() {return getHigh() - getLow();}
  uintptr_t getCount() { return size / sizeof(cs_insn);}

  // setters:
  void setInsns(cs_insn *ins, uint32_t s) {
    size = s;
    insns = ins;
  }

  void setCF() { cfInstFlag = true;}
  void setUnique() { unique = true;}

  // iterators:
  cs_insn * begin() {return insns;}
  cs_insn * end() {return insns + size;}
  friend ostream& operator<<(ostream &os, const Block &b);
  friend class Elf;
};
ostream& operator<<(ostream &os, const Block &b) {
  return os << std::hex << "[" << b.getLow() << "," << b.getHigh() << ") un=" << b.isUnique() << " cf=" << b.isCF();
}

// An Elf executable
class Elf {
 private:
  map<uintptr_t, Block*> blocks;
  uint64_t loadShift;  // loadaddr - loadShift = offset.
  uintptr_t stop;

  Parser::InstructionInfo _instrInfo;

 public:
  std::string name;
  uintptr_t start;

  Elf(std::string name, const std::string blockCacheDir, uintptr_t s, uintptr_t e, uintptr_t offset):
      _instrInfo(name){
    Parser::ElfInfo elfInfo(name);

    this->name = name;
    this->start = s;
    this->stop = e;
    this->loadShift = s - (offset * 0x1000); 
    //    SPDLOG_INFO("{} start={:x}, stop={:x}, offset={:x}, loadShift={:x}", name, s, e, offset, loadShift);

    char *temp = new char[name.size() + 1];
    strcpy(temp, name.c_str());
    char *base = basename(temp);

    char *fullPath = new char[blockCacheDir.size() + strlen(temp) + 2];
    strcpy(fullPath, blockCacheDir.c_str());
    fullPath[blockCacheDir.size()] = '/';
    strcpy(fullPath + blockCacheDir.size() + 1, base);

    SPDLOG_DEBUG("{} + {} = {}", blockCacheDir, name, fullPath);

    int blockFD = open(fullPath, O_RDONLY);
    if (blockFD < 0) {
      char cwd[1024];
      getcwd(cwd, 1024);
      SPDLOG_ERROR("cannot get file in blockCache {} fron {}", fullPath, cwd);
      assert (false);
    }
    delete [] temp;
    delete [] fullPath;


    struct blockInfo bi;
    int rc ;
    while ( (rc = read(blockFD, &bi, sizeof(struct blockInfo))) == sizeof(struct blockInfo)) {
      assert (offset <= bi.start && offset <= bi.end);
      auto blk = new Block(bi.start, bi.end, loadShift);

      // this *might* be expensive to do at this time?
      // cs_insn *insts;
      //uint32_t size = _instrInfo.getInstructions(bi.start,
      //bi.end - bi.start,
      //bi.start + loadShift,
      //&insts);
      //blk->setInsns(insts, size);
      if (bi.unique)  blk->setUnique();

      switch (jumpCounterType) {
        case INST_ALL:
          if (bi.jump) blk->setCF(); //what happens if I always instrument everything..? (for debugging)
          //blk->setCF();
          break;
        case INST_OPT:
	  if (bi.notForwardUncondJump) blk->setCF();
          break;
        case DEFAULT:
          // default just doesn't se any extras.
          break;
        default:
          SPDLOG_ERROR("never set jump counter!?");
          assert(false);
      }

      // I suspect that this means that I have bogus values coming from the static disas
      if (!(this->start <= blk->getLow() && blk->getHigh() <= this->stop)) {
        SPDLOG_ERROR("what the heck! block [{:x}-{:x}, {:x}] doesn't fit [{:x}-{:x}]",
                     blk->getLow(), blk->getHigh(), loadShift,
                     this->start, this->stop);
      }
      assert (this->start <= blk->getLow() && blk->getHigh() <= this->stop);
      blocks.emplace(blk->getLow(), blk);
    }
  }
  void prepBlock(Block *blk) {
      cs_insn *insts;
      uint32_t size = _instrInfo.getInstructions(blk->startAddr,
                                                 blk->getSize(),
                                                 blk->getLow(),
                                                 &insts);
      blk->setInsns(insts, size);

    }


  bool operator<(Elf o) const {return this->start < o.start;}
  friend ostream& operator<<(ostream &os, const Elf &eo);
  bool contains(uintptr_t addr) {return start <= addr && addr < stop;}

  bool containsBlk(uintptr_t addr) {return blocks.find(addr) != blocks.end();}
  map<uintptr_t, Block*>::iterator begin() { return blocks.begin();}
  map<uintptr_t, Block*>::iterator end()   { return blocks.end();}
};

ostream& operator<<(ostream &os, const Elf &eo)  {
  return os << std::hex << eo.name << " (" << eo.start << ", " << eo.stop << ")";
}

class InstCall {
 public:
  std::string output, filter;
  InstCall(std::string o, std::string f) {
    output = o;
    filter = f;
  }
  friend ostream& operator<<(ostream &os, const InstCall &ic);
};

ostream& operator<<(ostream &os, const InstCall &ic) {
  os << "(" << ic.output;
  if (ic.filter != "")
    os << " " << ic.filter;
  return os << ")";
}

class InstRegion {
 public:
  uintptr_t low, high;
  vector<InstCall> calls;
  int useCount;

  InstRegion(void): low(0), high(0), useCount(0) {}

  void addCall(InstCall ic) {
    calls.push_back(ic);
  }

  vector<InstCall>::iterator begin() { return calls.begin();}
  vector<InstCall>::iterator end() { return calls.end();}
  friend ostream& operator<<(ostream &os, const InstRegion &ir);
};

ostream& operator<<(ostream &os, const InstRegion &ir) {
  os << std::hex << "(" << ir.low << ", " << ir.high << ")";
  for (auto ic : ir.calls) os << " " << ic;
  return os;
}


class SteamdrillSO {
 private:
  vector<std::string> pushInsts, popInsts;
  std::ofstream assemb;

  std::string cfTramp;
  std::string jecxzTramp;
  vector<InstConfiguration*> instructionMapping;
  std::unordered_set<uintptr_t> imAddrs;
  std::unordered_set<string> tracerIsoBlocks;

  template<typename iter> vector<string> buildInstBlock(uintptr_t origEip, iter begin, iter end);

  static inline string buildLabel(string lname);
 public:

  SteamdrillSO(string filename);
  ~SteamdrillSO();

  void instrument(Block&, Elf &, bool useJump, list<InstRegion*>);
  void writeIPoints(string instPoints);
  string addInstConfiguration(uintptr_t address, bool useJump, bool &isNew);
};


string SteamdrillSO::buildLabel(string lname) {
    stringstream label;
    label << ".global " << lname << "\n" << lname << ":\n";
    return label.str();
}


SteamdrillSO::SteamdrillSO(string assemblyFile) {
    pushInsts = { "\tpusha", "\tpushf"};
    popInsts = {"\tpopf", "\tpopa"};
    assemb.open(assemblyFile);
    assemb << ".text\n";


    cfTramp = "cfJmpTramp";
    stringstream tramp;
    tramp << buildLabel(cfTramp) << "\tjmp *cfPtr\n";
    assemb << tramp.str() << "\n";

    jecxzTramp = "jecxzTramp";
    stringstream jecxzTrampInst;
    jecxzTrampInst << buildLabel(jecxzTramp)
                   << "\tpopf\n"
                   << "\tjmp *cfPtr\n";
    assemb << jecxzTrampInst.str() << "\n";

}

SteamdrillSO::~SteamdrillSO() {
    assemb.close(); // make sure this is flushed out when we donzo
}


template <typename iter>
vector<string> SteamdrillSO::buildInstBlock(uintptr_t origEip, iter begin, iter end) {
  std::vector<std::string> instrs;
  std::stringstream key;
  char eipInst[128];

  for (iter tmp = begin; tmp != end; ++tmp)
    key << tmp->output << "ISO" << tmp->filter;

  auto pair = tracerIsoBlocks.insert(key.str());
  if (pair.second) {

    // build a tracerIsolationBlocks for this collection of activeCalls:

    assert(!pushInsts.empty() && !popInsts.empty());
    uintptr_t next = 0;
    stringstream isoBlkInstrs, nextLabel;
    auto label = buildLabel(key.str());
    nextLabel << "te" << key.str();

    isoBlkInstrs << label;
    for (auto inst : pushInsts)
      isoBlkInstrs << inst << "\n";

    isoBlkInstrs << "\tcall tracerBegin\n";
    for (iter tmp = begin; tmp != end; ++tmp) {
      if (!tmp->filter.empty()) {
        string endLabel = nextLabel.str() + to_string(next++);

        isoBlkInstrs << "\tcall " << tmp->filter + "\n";
        isoBlkInstrs << "\ttestl %eax, %eax\n";
        isoBlkInstrs << "\tjz " << endLabel <<  "\n";
        isoBlkInstrs << "\tcall " << tmp->output << "\n";
        isoBlkInstrs << endLabel << ":\n";
      }
      else {
        isoBlkInstrs << "\tcall " + tmp->output + "\n";
      }
    }

    isoBlkInstrs << "\tcall tracerEnd\n";
    for (auto inst : popInsts) isoBlkInstrs << inst << "\n";

    isoBlkInstrs << "\tret\n";
    assemb << isoBlkInstrs.str() << "\n";
    // this->insts.push_back(isoBlkInstrs.str());
  }

  // now build the instrumentation for just this guy:
  instrs.push_back("\tmov %esp, (tempEsp)");
  instrs.push_back("\tmov tracerStack, %esp");
  sprintf(eipInst, "\tmovl $0x%x, (origEip)", origEip);
  instrs.push_back(eipInst);
  instrs.push_back("\tcall " + key.str());
  instrs.push_back("\tmov tempEsp, %esp");

  return instrs;
}

// final paramemter is an out parameter
std::string SteamdrillSO::addInstConfiguration(uintptr_t address, bool useJump, bool &isNew) {
    stringstream label;
    label << std::hex << "link" << address;
    InstConfiguration *ic = new InstConfiguration;
    ic->setJump(useJump);
    ic->setLink(label.str());
    ic->setBreakpoint(new Address("", "", address, 0)); // pid is unused
    instructionMapping.push_back(ic);

    isNew = imAddrs.insert(address).second;
    assert (isNew || !useJump);

    return buildLabel(label.str());
}

unordered_map<uintptr_t, uintptr_t> usedJumpBytes;

void SteamdrillSO::instrument(Block &b, Elf &elf, bool useJump, list<InstRegion*> regions) {

  auto irIter = regions.begin(), irEnd = regions.end();
  auto blkPos = b.getLow();
  auto blkSize = b.getSize();
  auto blkHigh = b.getHigh();

  //SPDLOG_INFO("inst  [{:x}, {:x}]", (*irIter)->low, (*irIter)->high);
  SPDLOG_TRACE("instrument: block={:x}-{:x}, inst={:x}-{:x}",
               blkPos, blkHigh, (*irIter)->low, (*irIter)->high);


  SPDLOG_TRACE("instrumenting!!");
  stringstream fxn;
  if (useJump) {
    bool unused;
    string label = addInstConfiguration(blkPos, useJump, unused);
    (void) unused;
    fxn << label;

    for (int x = 0; x < 5; ++x) {
      auto val = usedJumpBytes.emplace(blkPos + x, blkPos);
      if (!val.second) {
        SPDLOG_ERROR("{:x} is already using {:x} (we're {:x})",
                     val.first->second, blkPos + x, blkPos);
        assert (false);
      }
    }
  }
  for (auto i = b.begin(), e = b.end(); i != e; ++i) {
    // control flow instructions should be considered for two cases:
    // (1) correctness: do they jump to something relative (?)

    stringstream assemblyInst;
    auto detail = i->detail;

    if (isJmp(i->id) || isCall(i->id)) {
      if (jumpCounterType == INST_ALL ||
          (jumpCounterType == INST_OPT && isUncondJump(i->id))) {
        assemblyInst << "\tmovb %ah, (tmpAH)\n"
                     << "\tlahf\n"
                     << "\tincl (jmpCounter)\n"
                     << "\tsahf\n"
                     << "\tmovb (tmpAH), %ah\n";
      }

      auto detail = i->detail;
      if (detail->x86.operands[0].type == X86_OP_IMM) {

        assemblyInst << "\tmovl $0x"  << std::hex << detail->x86.operands[0].imm  << ", (cfPtr)\n";


        // we kinda let calls get away with murder here, because of the plt.
        // also, it's really hard to imagine that I'm broken in catching all
        // call destinations?
        if (!isCall(i->id) && !elf.containsBlk(detail->x86.operands[0].imm)) {
          SPDLOG_ERROR("blk {:x}, problem at {:x}, cannot find block for {:x}",
                       blkPos,      i->address, detail->x86.operands[0].imm);
          assert (elf.containsBlk(detail->x86.operands[0].imm));
        }

        // there are no indirect conditional branches, so use a trampolene
        if (isCondJmp(i->id)) {
          if (i->id != X86_INS_JECXZ) {
            assemblyInst << "\t" << i->mnemonic << " (" << cfTramp << ")\n";
          }
          else {
            assemblyInst << "\t pushf\n"
                         << "\t testl %ecx, %ecx\n"
                         << "\t je (" << jecxzTramp << ")\n"
                         << "\t popf\n";
          }
        }

        // call instructions can leak, so patch them as push+jmp
        else if (isCall(i->id)) {
          // we're removing a call instruction, tell the library that for the counters:
          assemblyInst << "\taddl $1, (extraCalls)\n";
#ifdef ECALLLOC_DEBUG
          assemblyInst << "\tmovl $0x" << std::hex << i->address << ", (extraCallLoc)\n";
#endif
          // return address is instruction location + instruction length
          assemblyInst << "\tpush $0x" << std::hex << i->address + i->size << "\n";

          assert (elf.containsBlk(i->address + i->size));

          assemblyInst << "\tjmp (" << cfTramp << ")\n";
        }
        else {
          assemblyInst << "\t" << i->mnemonic << " *cfPtr\n";
        }
      }
      else {
        // we can just allow reg & mem indirect jumps as is...
        assemblyInst << "\t" << i->mnemonic << " " << i->op_str << "\n";
      }
    }
    else {
      assemblyInst << "\t" << i->mnemonic <<  " " << i->op_str << "\n";
    }

    // update active and/or irIter
    if (irIter != irEnd &&  i->address >= (*irIter)->high) {
      ++irIter;
    }
    if (irIter != irEnd && i->address >= (*irIter)->low && i->address < (*irIter)->high) {
      if (!useJump) {
        // if this isn't a new label, then someone else already took care of it.
        // this might mean that we duplicate assembly code, oh well.
        bool newLabel;
        string label = addInstConfiguration(i->address, useJump, newLabel);
        if (newLabel)
          fxn << label;

      }
      auto toCall = buildInstBlock(i->address, (*irIter)->calls.begin(), (*irIter)->calls.end());
      std::copy(std::begin(toCall), std::end(toCall), std::ostream_iterator<std::string>(fxn, "\n"));
    }
    fxn << assemblyInst.str();
  }
  // now add a jump for this block to head to non-instrumentation land:
  char jmpToInst[64];
  sprintf(jmpToInst, "\tmovl $0x%x, (instEnd)\n", blkHigh);
  fxn << jmpToInst;
  fxn << std::hex <<"\tjmp *instEnd\n";
  SPDLOG_TRACE("instrumented: {}", fxn.str());

  /*
  if (!elf.containsBlk(blkHigh)) {
    SPDLOG_DEBUG("suspicious that {:x} not herr!", blkHigh);
  }
  */
  assemb << fxn.str() << "\n";
}

void SteamdrillSO::writeIPoints(string ipoints) {
    writeInstPoints(instructionMapping, ipoints);
}

int instrument(std::vector<TracerConfiguration*> tps,
               std::string assembly,
               std::string ipoints,
               std::string replay,
               std::string blockCache,
               JumpCounter jumpType) {

    SteamdrillSO instrumented(assembly);
    ReplayBinInfo rbi(replay);  //would be better to just make this a singleton, no?

    // get all of the Elfs
    vector<Elf*> elfs;
    jumpCounterType = jumpType;

    //    SPDLOG_INFO("replay bin info: {} has {}", replay, rbi.files.size());
    std::for_each(rbi.begin(), rbi.end(), [&elfs, blockCache] (const MemObject &mo) {
        if (mo.prot & PROT_EXEC)
          elfs.push_back(new Elf(mo.name, blockCache, mo.start, mo.end, mo.offset));
      });
    std::sort(elfs.begin(), elfs.end(), [](const Elf *x, const Elf *y) { return x->start < y->start;});

    assert (elfs.size() > 0);

    // we need some new special and fun data structure to get this working all righty.

    // duplpicates and orders the instrumentation that we need to accomplish:
    list<unique_ptr<InstRegion>> regions;
    for (auto tp : tps) {
        tp->forEachBreakpoint(
            [&regions](const RegionIter r, const TracerConfiguration &tp) {
              auto ic = make_unique<InstRegion>();
              ic->addCall(InstCall(tp.getOutputFunction(),
                                   tp.getFilter() != nullptr ? tp.getFilter()->getOutputFunction() : ""));
              ic->low = r.first->getOffset();
              ic->high = r.second->getOffset();
              regions.push_back(move(ic));
            });
    }
    regions.sort([](const unique_ptr<InstRegion> &o, const unique_ptr<InstRegion> &t)
                 {return (o->low < t->low) || (o->low == t->low && o->high < t->high);});
    if (regions.size() == 0) {
      SPDLOG_INFO("must be continuous tracers?");
    }
    // assert (regions.size() > 0);

    // now we need to merge the InstRegions!
    // it seems quite hard to merge the InstRegions in order, because I won't know if I should delete something?
    auto it = regions.begin(), end = regions.end(), nxt = std::next(it);

    while (nxt != end) {
      // determine if overlap:
      if ((*nxt)->low < (*it)->high) {
        // special case for the same range:
        if ((*it)->low == (*nxt)->low  && (*it)->high == (*nxt)->high) {
          for (auto call : *(*nxt).get()) {
            (*it)->addCall(call);
          }
          nxt = regions.erase(nxt);
        }
        else {
          // we don't support this much more complex scenario now.
          assert (0);
        }
      }
      else {
        ++it;
        ++nxt;
      }
    }

    // This requires that the above regions are already sorted.
    // We next assign each instrumentation region to the function that it instruments:
    // std::list<FunctionInst> funcs;
    auto regIter = regions.begin(), regend = regions.end();
    int blksInstr = 0;
    for (auto &binary : elfs) {
      for (auto blkPair : *binary) {
        auto blk = blkPair.second;

        list<InstRegion*> regions;

        // SPDLOG_INFO("block {}", *blk);

        // fastForwared RegIter until it is plausible that it might work:
        // note, bounds for all objects are [low, high);
        while (regIter != regend && blk->getLow() >= (*regIter)->high) {
	  //          if (!(*regIter)->useCount) {
	  //            SPDLOG_INFO("what the hay man? {}", *(*regIter));
	  //          }
          ++regIter;
        }

        // determine all the relevant region instrumentation. Let's start by reasoning
        // if the block and the next region iterator overlap by determining if they don't:
        // (1) ..iterhigh) [blk->low... (2) ... blk->high) [iterlow ...
        // so, neither is !(iterhigh <= blk->low || blk->high <= iterlow)

        auto checker = regIter;
        while (checker != regend &&
               (*checker)->high > blk->getLow() &&
               blk->getHigh() > (*checker)->low) {
          (*checker)->useCount++;
          regions.push_back(checker->get());
          ++checker;
        }

        // now instrument:
        if (blk->isCF() || regions.size() > 0) {
          //          std::stringstream ss;
          //          ss << "instrumenting " << *blk << " with";
          //          for (auto r : regions) {
          //            ss << " " << *r;
          //          }
          //          SPDLOG_INFO("{}", ss.str());
          bool useJump = blk->getSize() >= JMP_SIZE && blk->isUnique();
          blksInstr += 1;
          binary->prepBlock(blk);
          instrumented.instrument(*blk, *binary, useJump, regions);
        }
      }
    }
    assert(blksInstr > 0);

    instrumented.writeIPoints(ipoints);
    return blksInstr;
}
}
