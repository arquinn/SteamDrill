extern "C"
{
#include <dwarf.h>
#include <elf.h>

//why are these at different spots..>? Ubuntu you confuse me!
#include <elfutils/libdw.h>
#include <elfutils/libdwfl.h>
#include <libelf.h>
}


#include "binary_parser.hpp"

#include "../protos/steamdrill.pb.h"

#ifndef __DWARF_PARSER__
#define __DWARF_PARSER__

namespace Parser {
using std::unique_ptr;

class DwLineParser {
  Dwarf_Lines *lines;
  size_t nLines, cLine;

 public:
  DwLineParser(Dwarf_Lines *li, size_t nli): lines(li), nLines(nli), cLine(0) {};
  bool skipPrologue(Dwarf_Addr &entry, Dwarf_Addr low, Dwarf_Addr high);
};

class BaseFxn {
  BaseFxn(Dwarf_Addr s, Dwarf_Addr e, string f): start(s), end(e), fxn(f) {};
  static unordered_map<string, vector<BaseFxn>> cache;
 public:
  Dwarf_Addr start, end;
  string fxn;

  static vector<BaseFxn> getBaseFunctions(string in, Dwarf_Die *die);
};

class DwInfo
{
 private:
  Dwfl *_dwfl;
  int _fd;
  string _binName;

  void updateFunctions(std::vector<unique_ptr<FunctionInfo>> &);
 public:
  DwInfo(std::string filename);
  std::vector<unique_ptr<FunctionInfo>> getFunctions();
  // std::vector<unique_ptr<Type>>         getTypes();
  std::unordered_set<Variable>          getGlobals();
};



} /*namespace Parser*/

#endif /* __DWARF_PARSER__ */
