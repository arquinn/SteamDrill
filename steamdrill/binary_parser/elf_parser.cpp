#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

extern "C"
{
#include <dwarf.h>
#include <elf.h>

//why are these at different spots..>? Ubuntu you confuse me!
#include <elfutils/libdw.h>
#include <libelf.h>
}

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>


#include "variable.hpp"
#include "elf_parser.hpp"
#include "binary_parser.hpp"


namespace Parser {

using std::make_unique;
using std::unique_ptr;

ElfInfo::ElfInfo(const std::string &binaryName)
{
  _fd = -1;
  _binName = binaryName;
  // get the elf and check for some basic errors:
  if ((_fd = open(binaryName.c_str(), O_RDONLY)) < 0) {
    std::cerr << "Can't open " << binaryName << " " << errno << std::endl;
    return;
  }
  if (elf_version(EV_CURRENT) == EV_NONE) {
    std::cerr << "elf cannot be initialized"
              << elf_errmsg(-1)
              << std::endl;
    return;
  }

  _elf = elf_begin(_fd, ELF_C_READ, NULL);
  if (_elf == NULL) {
    std::cerr << " elf_begin failed"
              << elf_errmsg(-1)
              << std::endl;
    return;
  }
  _header = elf32_getehdr(_elf);
  size_t strtab_idx;
  elf_getshdrstrndx(_elf, &strtab_idx);

  // start a scan to get all elf sections
  Elf_Scn *scn = NULL;
  while ((scn = elf_nextscn(_elf, scn)) != NULL) {
    GElf_Shdr shdrMem;
    GElf_Shdr *shdr = gelf_getshdr(scn, &shdrMem);
    char *name = elf_strptr(_elf, strtab_idx, shdr->sh_name);

    //    if (shdr)
    //      std::cerr << std::hex <<  "section " << name
    //<< " load=" << shdr->sh_addr
    //<< " off=" << shdr->sh_offset
    //<< " size=" << shdr->sh_size
    //<< " flags=" << shdr->sh_flags
    //<< " exec=" << (shdr->sh_flags & SHF_EXECINSTR)
    //<< " alloc=" << (shdr->sh_flags & SHF_ALLOC)
    //<< std::endl;
    if (shdr != NULL &&
        shdr->sh_type == SHT_PROGBITS &&
        shdr->sh_addr &&
        (shdr->sh_flags & SHF_EXECINSTR) &&
        (shdr->sh_flags & SHF_ALLOC) &&
        (!strstr(name, "got") && !strstr(name, "plt"))) {
      _functionSections.emplace(name, shdr->sh_addr, shdr->sh_offset, shdr->sh_size, shdr->sh_flags);
    }
  }
}


ElfInfo::~ElfInfo() {
  elf_end(_elf);
  close(_fd);
}

unique_ptr<FunctionInfo> ElfInfo::parseFunction(GElf_Sym *sym, GElf_Shdr *shdr, string extraName) {
  auto func = unique_ptr<FunctionInfo>(new FunctionInfo());
  func->name = elf_strptr(_elf, shdr->sh_link, sym->st_name);
  func->name.append(extraName);
  func->lowPC = sym->st_value;
  func->bind = GELF_ST_BIND(sym->st_info);
  return func;
}


std::vector<unique_ptr<FunctionInfo>> ElfInfo::getFunctions() {
  Elf_Scn *scn = NULL;
  std::vector<unique_ptr<FunctionInfo>> funcs;
  std::unordered_set<uintptr_t> locations;

  Elf_Data *versions = nullptr, *versionDefs = nullptr;
  Elf32_Word defsStridx = 0;

  while ((scn = elf_nextscn(_elf, scn)) != NULL) {
    GElf_Shdr shdrMem;
    GElf_Shdr *shdr = gelf_getshdr(scn, &shdrMem);
    if (shdr) {
      if (shdr->sh_type == SHT_GNU_versym) {
        versions = elf_getdata(scn, NULL);
      }
      else if (shdr->sh_type == SHT_GNU_verdef)  {
        /* This is the information about the defined versions.  */
        versionDefs = elf_getdata (scn, NULL);
        defsStridx = shdr->sh_link;
      }
    }
  }

  scn = NULL;
  while ((scn = elf_nextscn(_elf, scn)) != NULL) {
    GElf_Shdr shdrMem;
    GElf_Shdr *shdr = gelf_getshdr(scn, &shdrMem);

    if (shdr != NULL) {
      if (shdr->sh_type == SHT_SYMTAB || shdr->sh_type == SHT_DYNSYM)  {
        Elf_Data *symData = elf_getdata(scn, NULL);
        if (symData == NULL) {
          continue;
        }
        int numSyms = shdr->sh_size / shdr->sh_entsize;
        for (int i = 0; i < numSyms; ++i) {
          GElf_Sym symMem, *sym;
          GElf_Versym verMem, *ver;

          sym = gelf_getsym(symData, i, &symMem);
          if (sym && sym->st_shndx != SHN_UNDEF && sym->st_value &&
              (GELF_ST_TYPE(sym->st_info) == STT_FUNC || GELF_ST_TYPE(sym->st_info) == STT_GNU_IFUNC)) {

            std::stringstream versionName;
            if (versions) {
              ver = gelf_getversym(versions, i, &verMem);
              if (ver && ((*ver & 0x8000) || *ver > 1)) {
                size_t off = 0;
                if (versionDefs) {
                  GElf_Verdef defMem, *def = gelf_getverdef (versionDefs, 0, &defMem);
                  while (def != NULL) {
                    if (def->vd_ndx == (*ver & 0x7fff))
                      /* Found the definition.  */
                      break;

                    off += def->vd_next;
                    def = (def->vd_next == 0 ? NULL : gelf_getverdef (versionDefs, off, &defMem));
                  }

                  if (def) {
                    GElf_Verdaux auxMem, *aux =  gelf_getverdaux (versionDefs, off + def->vd_aux, &auxMem);
                    if (aux && defsStridx) { // I have a feeling that this isn't right.
                      char * name = elf_strptr (_elf, defsStridx,  aux->vda_name);
                      // std::cerr << "name=" << name << std::endl;
                      versionName << ((*ver & 0x8000) ? "@" : "@@") << name;
                    }
                  }
                }
              }
            }
            auto foo = parseFunction(sym, shdr, versionName.str());
            if (locations.find(foo->lowPC) == locations.end() &&
                getSection(foo->lowPC)) {
              locations.insert(foo->lowPC);
              funcs.push_back(move(foo));
            }
          }
        }
      }
    }
  }

  std::sort(funcs.begin(),
            funcs.end(),
            [](const unique_ptr<Parser::FunctionInfo> &o, const unique_ptr<Parser::FunctionInfo> &t) {
              return o->lowPC < t->lowPC;
            });

  return funcs;
}

std::unordered_set<Variable>  ElfInfo::getGlobals() {
  Elf_Scn *scn = NULL;
  std::unordered_set<Variable> syms;
  while ((scn = elf_nextscn(_elf, scn)) != NULL)
  {
    GElf_Shdr shdrMem;
    GElf_Shdr *shdr = gelf_getshdr(scn, &shdrMem);

    if (shdr != NULL) {
      if (shdr->sh_type == SHT_SYMTAB || shdr->sh_type == SHT_DYNSYM)  {
        Elf_Data *symData = elf_getdata(scn, NULL);
        if (symData == NULL) {
          continue;
        }
        int numSyms = shdr->sh_size / shdr->sh_entsize;
        for (int i = 0; i < numSyms; ++i) {
          GElf_Sym symMem;
          GElf_Sym *sym = gelf_getsym(symData, i, &symMem);

          // Not 100% sure what this second check gives us..
          if (GELF_ST_TYPE(sym->st_info) == STT_OBJECT &&
              sym->st_shndx != SHN_UNDEF &&
              sym->st_value) {
            string name = elf_strptr(_elf, shdr->sh_link, sym->st_name);
            uintptr_t loc = sym->st_value;

            // elf doesn't have types, size or a 'location' function
            syms.insert(Variable(name, _binName, "", "", loc));
          }
        }
      }
    }
  }
  return syms;
}

std::optional<ElfSecInfo> ElfInfo::getSection(uint32_t addr) {
  for (auto &esi : _functionSections) {
    if (addr >= esi.loaded && addr < esi.loaded + esi.size) {
      return esi;
    }
  }
  return {};
}
} // namespace Parser
