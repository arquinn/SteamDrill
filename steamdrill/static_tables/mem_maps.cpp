#include <sys/types.h>
#include <sys/wait.h>

#include <dirent.h>
#include <elf.h>
#include <fcntl.h>
#include <unistd.h>


#include <iostream>
#include <fstream>
#include <memory>
#include <sstream>
#include <unordered_map>

#include <boost/regex.hpp>

#include "args.hxx"
#include "staticLibrary.hpp"

#include "../utils/mem_object.hpp"
#include "../binary_parser/binary_parser.hpp"
#include "../query_generator/function_code.hpp"

extern "C" {
#include "parse_syscall_input_lib.h"
}

using std::string;
using std::vector;
using std::unique_ptr;
using std::unordered_map;

// we assume that the replay doesn't unmap executable regions willy-nilly..
// we dump a lot of duplicate entries for now, rather than understanding all of the mmap semantics
// we also assume that executable libraries are mapped BEFORE any call to fork
// -- this dramaticaly simplifies handling different address spaces w/ fork

int main(int argc, char *argv[]) {
  args::ArgumentParser parser("Get info about memory mmaped by a replay.", "A.R.Q.");
  args::Group group(parser, "Required arguments:", args::Group::Validators::All);
  args::Positional<std::string> replay(group, "replay", "The replay program");
  args::HelpFlag help(parser, "help", "Display help", {'h', "help"});

  try {
    parser.ParseCLI(argc, argv);
  }
  catch (args::Help) {
   std::cout << parser << std::endl;
   return 0;
  }
  catch (args::ParseError e) {
    std::cerr << e.what() << std::endl << parser << std::endl;
    return -1;
  }
  catch (args::ValidationError e) {
    std::cerr << e.what() << std::endl << parser << std::endl;
    return -2;
  }

  // right now this assumes that all mappings are GLOBAL.. which is, I guess, like not true.
  std::unordered_map<std::string, struct MemObject> mem;
  getMappings(args::get(replay), [&mem](MemObject &&mm) {
      auto it = mem.emplace(mm.name, mm);
      if (!it.second) {
        it.first->second.start = std::min(mm.start, it.first->second.start);
        it.first->second.end = std::max(mm.end, it.first->second.end);
      }
    });


  for (auto &pair : mem) {
    auto mo = pair.second;
    unique_ptr<Elf32_Ehdr> &hdr = getElf(mo.name);
    if (hdr) {
      StaticLibrary::addOutput(mo.start);
      // StaticLibrary::addOutput(mo.end - mo.start);
      //StaticLibrary::addOutput(mo.offset * 0x1000);
      StaticLibrary::addOutput(mo.name);
      StaticLibrary::addOutput(mo.oname);
      StaticLibrary::addOutput(hdr->e_type == ET_DYN ? "true" : "false");
      StaticLibrary::endRow();
    }
  }
  return 0;
}
