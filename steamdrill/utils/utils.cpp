#include "utils.hpp"

#include <gelf.h>
#include <fcntl.h>
#include <unistd.h>

#include <memory>
#include <unordered_map>

using std::string;
using std::unordered_map;
using std::unique_ptr;


// check if is elf (maybe this is a good idea?)
static unordered_map<string, unique_ptr<Elf32_Ehdr>> ElfHdrMap;
unique_ptr<Elf32_Ehdr>& getElf(string pathname) {
  auto it = ElfHdrMap.find(pathname);
  if (it != ElfHdrMap.end()) {
    return it->second;
  }

  int fd = open(pathname.c_str(), O_RDONLY);
  bool fail = true;
  auto hdr = std::make_unique<Elf32_Ehdr>();
  if (fd > 0) {
    int rc = read(fd, hdr.get(), sizeof(Elf32_Ehdr));
    if (rc == sizeof(Elf32_Ehdr)) {
      if (hdr->e_ident[EI_MAG0] == ELFMAG0 &&
          hdr->e_ident[EI_MAG1] == ELFMAG1 &&
          hdr->e_ident[EI_MAG2] == ELFMAG2 &&
          hdr->e_ident[EI_MAG3] == ELFMAG3) {
        fail = false;
        it = ElfHdrMap.emplace(pathname, std::move(hdr)).first;
        return it->second;
      }
    }
  }

  it = ElfHdrMap.emplace(pathname, nullptr).first;
  return it->second;
}
