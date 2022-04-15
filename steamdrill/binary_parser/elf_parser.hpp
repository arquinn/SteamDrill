#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>
#include <unordered_set>

#include "binary_parser.hpp"
#include "variable.hpp"

extern "C"
{
#include <dwarf.h>
#include <elf.h>

//why are these at different spots..>? 
#include <elfutils/libdw.h>
#include <libelf.h>
}

#ifndef __ELF_PARSER__
#define __ELF_PARSER__

namespace Parser {

using std::unique_ptr;

class ElfSecInfo {
 public:
  std::string name;
  uint32_t loaded;
  uint32_t offset;
  uint32_t size;
  uint32_t flags;

  ElfSecInfo(std:: string n, uint32_t la, uint32_t eo, uint32_t s, uint32_t f) {
    name = n;
    loaded = la;
    offset = eo;
    size = s;
    flags = f;
  }
  ElfSecInfo() : loaded(0), offset(0), size(0), flags(0) {};

  bool operator<(const ElfSecInfo &other) const {
    return loaded < other.loaded;
  }

  bool operator==(const ElfSecInfo &other) const {
    return loaded == other.loaded &&
           offset == other.offset &&
           size == other.size &&
           flags == other.flags;
  }

  friend class ElfInfo;
};



class ElfInfo {
 private:
  Elf *_elf;
  Elf32_Ehdr *_header;
  int  _fd;
  string _binName;

  std::set<ElfSecInfo> _functionSections;

  unique_ptr<FunctionInfo> parseFunction(GElf_Sym *sym, GElf_Shdr *shdr, string extraName);

 public:
  ElfInfo(const std::string &binaryName);
  ~ElfInfo();

  bool isSharedLibrary(void) {
    return _header->e_type == ET_DYN;
  }

  std::optional<ElfSecInfo>  getSection(uint32_t addr);
  std::vector<unique_ptr<FunctionInfo>> getFunctions();
  std::unordered_set<Variable> getGlobals();

  std::set<ElfSecInfo> getSections() { return _functionSections;}
};
}

#endif /* ELF_PARSER */
