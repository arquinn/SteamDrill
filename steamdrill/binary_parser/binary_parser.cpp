#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>

extern "C"
{
#include <dwarf.h>
#include <elf.h>

//why are these at different spots..>? Ubuntu you confuse me!
#include <elfutils/libdw.h>
#include <libelf.h>
}

#include <boost/format.hpp>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <vector>

#include "elf_parser.hpp"
#include "dwarf_parser.hpp"
#include "instruction_parser.hpp"
#include "binary_parser.hpp"
#include "regnum_list.hpp"

#include "../utils/timer.hpp"


using boost::format;
using std::pair;

namespace Parser {
//bool debug = false;

#define PROTO "{"
VariableFunction::VariableFunction () :
    _nxtVar(0), _str(""), _inBlock(false), _hasOutput(false) {
  addLine(PROTO);
}


string VariableFunction::getNextVar(void) {
  char nxtVar[64];
  sprintf(nxtVar, "var%d", _nxtVar++);
  return nxtVar;
}

string VariableFunction::addVariable(string type) {
  string varName = getNextVar();
  addLine(boost::str(format {"%s %s;"} %type %varName));
  return varName;
}

void
VariableFunction::addAssignment(string rhs, string type, string var) {
  if (type.empty()) {
    addLine(boost::str(format{"%s = %s;"} %var %rhs));
  }
  else {
    addLine(boost::str(format{"%s = (%s)%s;"} %var %type %rhs));
  }
}

string VariableFunction::getRegister (string regName) {
  assert(regName.size() > 1);
  regName[0] = toupper(regName[0]);
  return boost::str(format{"tracerGet%s()"} %regName);
}

void VariableFunction::startIf(string conditions) {
  char code[128];
  sprintf(code, "if (%s) {", conditions.c_str());
  addLine(code);
  _inBlock = true;
}

void VariableFunction::endIf(void) {
  assert (_inBlock);
  addLine("}");
  _inBlock = false;
}

string VariableFunction::addOutputDeclaration() {
  if (_type.size() == 0)
    addLine("unordered_map<std::string, long> rtn;");
  else
    addLine(boost::str(format{"%s rtn;"} %_type));
  _hasOutput = true;
  return "rtn";
}

void VariableFunction::addOutputVal(string name, string value) {
  if (!_hasOutput)
    addOutputDeclaration();

  char code[256];
  sprintf(code, "rtn[\"%s\"] = (long)%s;", name.c_str(), value.c_str());
  addLine(code);
}

string VariableFunction::setType(string t) {
  _type = t;
  return addOutputDeclaration();
}


// I *really* think this should be setup differently.
string VariableFunction::toString(void) {
  if (_str != "") return _str;
  if (!_hasOutput) return "";

  addLine ("return rtn;");
  _str = FunctionCode::toString();
  return _str;
}




void FunctionInfo::merge(unique_ptr<FunctionInfo> other) {

  /* std::cerr << std::hex << "Merge " << other->name
           << " into " << name
            << "(" << lowPC << ", " << highPC << ") "
            << "(" << other->lowPC << ", " << other->highPC << ")"
            << std::endl;
  */

  // deal with params and returnValues
  if (locals.empty()) {
    locals = std::move(other->locals);
  }

  // I don't want a bunch of copies of parameters:
  if (parameters.size() == 0) {
    parameters.insert(other->parameters.begin(), other->parameters.end());
  }


  if (!returnValue) {
    returnValue = std::move(other->returnValue);
  }

  // deal with names and aliases
  if ((bind == STB_LOCAL || bind == STB_WEAK) && other->bind == STB_GLOBAL) {
    aliases.insert(name);
    name = other->name;
    bind = other->bind;
  }
  else {
    aliases.insert(other->name);
    aliases.insert(other->aliases.begin(), other->aliases.end());
  }

  if (lowPC && other->lowPC && lowPC != other->lowPC) {
    lowPC = std::min (lowPC, other->lowPC);
  }
  else {
    // choose the non-0 one.
    lowPC = std::max(lowPC, other->lowPC);
  }
  highPC = std::max(highPC, other->highPC);

  //take other's entries if I don't have any
  if (!entryPCs.size()) {
    entryPCs = other->entryPCs;
  }

  // and lastly, the collections:
  returnPCs.insert(other->returnPCs.begin(), other->returnPCs.end());

}

void FunctionInfo::finish(void)
{
  if (entryPCs.size() == 0)
    entryPCs.insert(lowPC);
}

void DefinedFunctions::addOrUpdateFunction(unique_ptr<FunctionInfo> func) {
  if (!func->lowPC) {
    std::cerr << "yikes, lowPC is 0? " << func->name << std::endl;
    assert (false);
  }

  auto pair = _funcMap.find(func->lowPC);
  if (pair == _funcMap.end()) {
    _funcMap.insert(std::make_pair(func->lowPC, std::move(func)));
  }
  else {
    pair->second->merge(std::move(func));
  }
}

void DefinedFunctions::mergeFxns(u_long firstLowPC, u_long secondLowPC) {
  std::cerr << "merge " << firstLowPC << " and " << secondLowPC << std::endl;
  auto fpair = _funcMap.find(firstLowPC), sPair = _funcMap.find(secondLowPC);

  std::cerr << "found? " << (fpair == _funcMap.end())  << " " << (sPair == _funcMap.end()) << std::endl;
  fpair->second->merge(move(sPair->second));
  _funcMap.erase(sPair);

  std::cerr << "removed "
            << (_funcMap.find(secondLowPC) == _funcMap.end()) << std::endl;
}

std::vector<unique_ptr<FunctionInfo>> DefinedFunctions::getReps() {
  std::vector<unique_ptr<FunctionInfo>> funcs;
  for (auto &f : _funcMap) {
    //FunctionInfo *fi = f.second.release();
    f.second->finish();
    funcs.push_back(std::move(f.second));
  }
  return funcs;
}

// alias mappings?
vector<unique_ptr<FunctionInfo>> getFunctions(const string &binaryName, const AliasMap &aliases) {

  DefinedFunctions functions;
  unordered_map<string, u_long> aliasLowPC;
  Timer::StampManager sm;

  sm.addStamp(binaryName + "getFuncsStart");
  for (auto ai : aliases) {
    aliasLowPC.emplace(ai.first, 0);
    aliasLowPC.emplace(ai.second, 0);
  }
  sm.addStamp(binaryName + "elfStart");
  ElfInfo elf(binaryName);
  for (auto &f : elf.getFunctions()) {
    auto alp = aliasLowPC.find(f->name);
    if (alp != aliasLowPC.end()) {
      alp->second = f->lowPC;
    }
    functions.addOrUpdateFunction(std::move(f));
  }
  sm.addStamp(binaryName + "elfStop");
  sm.addStamp(binaryName + "dwStart");
  DwInfo dw(binaryName);
  for (auto &f : dw.getFunctions()) {
    for (auto pc : f->entryPCs) {
      if (pc < f->lowPC || pc > f->highPC) {
        fprintf(stderr, "bad entry on %s\n", f->name.c_str());
      }
      assert (pc >= f->lowPC && pc < f->highPC);
    }
    for (auto pc : f->returnPCs) {
      assert (pc > f->lowPC && pc < f->highPC);
    }
    auto alp = aliasLowPC.find(f->name);
    if (alp  != aliasLowPC.end()) {
      alp->second = f->lowPC;
    }
    functions.addOrUpdateFunction(std::move(f));
  }
  for (auto a : aliases) {
    functions.mergeFxns(aliasLowPC[a.first], aliasLowPC[a.second]);
  }

  sm.addStamp(binaryName + "dwStop");
  std::vector<unique_ptr<FunctionInfo>> funcs = functions.getReps();

  sm.addStamp(binaryName + "returnsStart");
  // Cannot use highPC b/c that comes from dwarf(!)
  InstructionInfo ii(binaryName);
  u_long addCount = 0;

  std::sort(funcs.begin(), funcs.end(),
            [] (const unique_ptr<FunctionInfo> &f,
                const unique_ptr<FunctionInfo> &e) -> bool
            {return f->lowPC < e->lowPC;});

#define USE_NEW

  auto fi = funcs.begin(), fiend = funcs.end();
  SeqInstIter ssi;
  ElfSecInfo esi;
  uint32_t loaded = esi.loaded, offset = esi.offset, size= esi.size;

  while (fi != fiend) {
    auto fiPrev = fi++;

    // just checking against 0 in the initializetion case:
    if (esi.loaded + esi.size <= (*fiPrev)->lowPC) {
      esi = *elf.getSection((*fiPrev)->lowPC);
      loaded = esi.loaded;
      offset = esi.offset;
      size= esi.size;
#ifdef USE_NEW
      ssi = ii.getSeqInstIter(offset, size, loaded);
#endif
    }

    if (!(*fiPrev)->highPC) {
      (*fiPrev)->highPC = (loaded + size) - 1;
      if (fi != fiend && (*fi)->lowPC < (*fiPrev)->highPC) {
        (*fiPrev)->highPC = (*fi)->lowPC - 1;
      }
    }

#ifdef USE_NEW
    if (!ssi.contains((*fiPrev)->highPC)) {
      // this means that we don't have this whole function loaded... there are two options:
      // (1) reload the ssi.getInstructions starting from lowPC and see how high we get
      // (2) reload the ssi.getInstructions with JUST the fxn

      // seems like once we're not linear disassembling we should give up, so I choose (2)
      // std::cerr << "ssi missing " << (*fiPrev)->name << " " << std::hex <<  (*fiPrev)->highPC << std::endl;

      uint32_t secOffset = (*fiPrev)->lowPC - loaded;
      ssi = ii.getSeqInstIter(offset + secOffset,  //offset w/in section?
                              (*fiPrev)->highPC - (*fiPrev)->lowPC + 1,
                              (*fiPrev)->lowPC);
      addCount += 1;

    }
    // debugging info can be sketchy and off-by-one. Just trust that worked ^^
    // assert (ssi.contains((*fiPrev)->highPC));

    assert (!ssi.finished());

    ssi.getInstructions((*fiPrev)->highPC,
                        [&fiPrev](cs_insn inst) {

                          if (inst.id == X86_INS_RET) {
                            (*fiPrev)->returnPCs.insert(inst.address);
                          }
                          else if (inst.id == X86_INS_JMP) {
                            auto dest =  inst.detail->x86.operands[0];
                            if (dest.type == X86_OP_IMM &&
                                (dest.imm < (*fiPrev)->lowPC || dest.imm > (*fiPrev)->highPC)) {
                              // std::cerr << "I'm honeslty surprised..?" << inst.address << std::endl;
                              (*fiPrev)->returnPCs.insert(inst.address);
                            }
                          }
                        });


    if ((*fiPrev)->returnPCs.size() == 0) {
      (*fiPrev)->returnPCs.insert(ssi.getLastInstAddr());
    }
#else
    addCount += 1;
    (*fiPrev)->returnPCs = ii.getInstructions((*fiPrev)->lowPC,
                                              (*fiPrev)->highPC,
                                              offset,
                                              loaded,
                                              X86_INS_RET);
#endif

  }
  sm.addCount(binaryName + "addCount", addCount);
  sm.addStamp(binaryName + "returnsStop");
  sm.addStamp(binaryName + "getFuncsStop");
  //sm.writeStamps("stats.out");
  return funcs;
}

std::unordered_set<Variable> getGlobals(const std::string &binaryName) {
  ElfInfo elf(binaryName);
  DwInfo dw(binaryName);
  auto eglobals = elf.getGlobals();
  auto dwglobals = dw.getGlobals();

  for (auto &eg : eglobals) {
    auto dwIter = dwglobals.find(eg);
    if (dwIter != dwglobals.end()) {
      // merge 'em
      // I oughta have most of what I need, but I don't know the type:
      eg.type = dwIter->type;
    }

    // you ought to be able to also get info from whatever llvm code we have hanging around
    if (!eg.type.empty()) {
      // now create their 'functions' which uhh, need input? weirdio
      // We don't include the prototype nor the input names.. which is gross? yes.
      VariableFunction foo;
      auto rtn = foo.setType(eg.type);

      // ptr is a void*. *sigh* this is gonna get ugly:
      foo.addAssignment(boost::str(boost::format{"*(%s *)(0x%lx)"} %eg.type %eg.ptrLoc), "", rtn);
      eg.location = foo.toString();

    }
    else {
      eg.type = "void*";
    }
  }
  // should try to get globals outta dwarf as well :D

  return eglobals;
}

} /*end namespace Parser */
