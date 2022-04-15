#ifndef _BINARY_PARSER_H
#define _BINARY_PARSER_H

#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

extern "C"
{
#include <dwarf.h>
#include <elf.h>

//why are these at different spots..>? Ubuntu you confuse me!
#include <elfutils/libdw.h>
#include <libelf.h>
}

// #include "../protos/steamdrill.pb.h"


// #include "type.hpp"
#include "variable.hpp"
#include "variable_function.hpp"

#define DEBUG_FUNC_NAME "process_arithmetic_command"
#ifdef DEBUG_FUNC_NAME
#define DEBUG_FUNC(x, ...) if (!strcmp(x, DEBUG_FUNC_NAME)) fprintf(stderr, __VA_ARGS__);

#else
#define DEBUG_FUNC(x,...)
#endif

namespace Parser {

using std::unique_ptr;
using std::string;
using std::stringstream;
using std::unordered_map;
using std::unordered_set;
using std::vector;

typedef unordered_map<std::string, std::string> AliasMap;

//extern bool debug;
class VariableType {
 public:
  int size;
  string name;
  VariableType(int s, string n): size(s), name(n) {}
};


class FunctionInfo {
 public:
  std::string                     name;
  std::unordered_set<std::string> aliases;
  std::string                     source;
  u_long                          lowPC;
  u_long                          highPC;
  std::unordered_set<u_long>      PCs;
  std::unordered_set<u_long>      entryPCs;
  std::unordered_set<u_long>      returnPCs;
  //std::unordered_map<string, string> argTypes;

  char bind;
  unordered_set<Variable> parameters;

  // this should be a Variable, no?
  unique_ptr<VariableFunction> returnValue;

  unordered_set<Variable> locals;

  FunctionInfo(): name(""),
                  source(""),
                  lowPC(0),
                  highPC(0),
                  bind(STB_LOCAL),
                  returnValue(nullptr) {

    //if (debug)
    //std::cerr << "create fi " << (void*)this << std::endl;
  };
  ~FunctionInfo() {
    //if (debug)
    //std::cerr << "delete fi " << (void*)this << std::endl;
  }

  void merge(unique_ptr<FunctionInfo> other);
  void finish(void);

  bool operator==(const FunctionInfo &rhs)
  {
    return *entryPCs.begin() == *rhs.entryPCs.begin() &&
        name == rhs.name;
  }

  void addEntryPC(u_long entryPC)
  {
    entryPCs.insert(entryPC);
  }

  friend std::ostream& operator<<(std::ostream &os,
                                  const FunctionInfo &fi)
  {
    os << std::hex << fi.name << " (" << fi.lowPC
       << ", " << fi.highPC  << ")";
    for (auto entr : fi.entryPCs)
      os << ", " << entr;
    for (auto rtn : fi.returnPCs)
      os << ", " << rtn;
    return os;

  }
};


class DefinedFunctions {
 private:
  std::unordered_map<u_long, unique_ptr<FunctionInfo>> _funcMap;

 public:
  void addOrUpdateFunction(unique_ptr<FunctionInfo> func);
  void mergeFxns(u_long firstLowPC, u_long secondLowPC);
  std::vector<unique_ptr<FunctionInfo>> getReps();
};

/*
class GlobalInfo {
 public:
  std::string                     name;
  std::string                     binary;
  uintptr_t                       loc;

  // might be necessary? (need to figure out better way to handle this anwyay)
  std::string                     type;
  char                            bind;
  };*/

std::unordered_set<Variable> getGlobals(const string &binary);
// std::vector<unique_ptr<Type>> getTypes(const string &binary);
std::vector<unique_ptr<FunctionInfo>> getFunctions(const string &binaryName, const AliasMap &aliases = {});
} /* namespace Parser */
#endif /*_BINARY_PARSER_H*/
