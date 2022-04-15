#include <algorithm>
#include <fstream>

#include "spdlog/spdlog.h"
#include "query_generator.hpp"
#include "../utils/mem_object.hpp"
#include "../utils/utils.hpp"

#include "../binary_parser/binary_parser.hpp"
#include "../binary_parser/elf_parser.hpp"
#include "../binary_parser/instruction_parser.hpp"

#include "../utils/address.hpp"
#include "../utils/tracer_configuration.hpp"

extern "C" {
#include <dirent.h>
#include "parse_syscall_input_lib.h"
}

typedef std::unordered_map<std::string, Configuration::TracerConfiguration*> functMap;
//#define DLOPEN "__libc_dlopen_mode"
//#define DLSYM "__libc_dlsym"
#define DLOPEN "_dl_open_springboard"
#define DLSYM "dl_lookup_springboard"


#define LIBC_NAME "libc.so.6"
#define LIBPTHREAD_NAME "libpthread.so.0"
#define LOADER "../../eglibc-2.15/prefix/lib/ld-2.15.so"

using std::any_of;
using std::vector;
using std::unique_ptr;
using Configuration::Address;
using Configuration::TracerConfiguration;

static functMap initFunctMap() {
  functMap rtn;
  rtn.emplace(std::make_pair(DLOPEN, new TracerConfiguration("U")));
  rtn.emplace(std::make_pair(DLSYM, new TracerConfiguration("U")));
  return rtn;
}

void getFiles(string replay, vector<string> part, vector<struct MemObject> &mem) {
  getMappings(replay, [&mem, &part](MemObject &&mm) {
      if (any_of(part.begin(), part.end(), [&mm](string p){return mm.oname.find(p) != string::npos;}))
        mem.push_back(std::move(mm));
    });

  // only take the first such mapping... *sigh*
  std::sort(mem.begin(), mem.end(), [] (const struct MemObject o, const struct MemObject t) {return o.start < t.start;});
  std::unordered_set<std::string> seen;
  auto it = mem.begin();
  while (it != mem.end()) {
    if (seen.find(it->name) != seen.end()) {
      it = mem.erase(it);
    }
    else {
      ++it;
      seen.insert(it->name);
    }
  }
}

void Generate::generateLibcConfig(const string replay, const string config) {
  int rc;
  functMap libcConfig = initFunctMap();
  char *resolved = nullptr;
  std::string loader = std::getPath(LOADER);

  Parser::ElfInfo li(loader);
  if (!(resolved = realpath(loader.c_str(), NULL)))
    std::cerr << "realpath failed " << errno << std::endl;

  vector<unique_ptr<Parser::FunctionInfo>> elfFuncs = li.getFunctions();
  for (auto &f : elfFuncs) {
    auto entry = libcConfig.find(f->name);
    if (entry != libcConfig.end()) {
      auto loc = f->lowPC;
      entry->second->addBreakpoint(new Address(resolved, "x", loc, 0),
                                   new Address(resolved, "x", loc + 1, 0));
    }
  }

  // std::cerr << "Open config and write it out" << std::endl;
  vector<TracerConfiguration*> tpoints;
  tpoints.push_back(libcConfig[DLOPEN]);
  tpoints.push_back(libcConfig[DLSYM]);

  Configuration::writeTPoints(tpoints, config);
}

#define FORK "fork"
void Generate::generateStandardTracepoints(const string replayDir, const string filename) {
  char *resolved = nullptr;
  vector<Configuration::TracerConfiguration*> tpoints;
  std::string loader = std::getPath(LOADER);

  Parser::ElfInfo ei(loader);
  if (!(resolved = realpath(loader.c_str(), NULL)))
    std::cerr << "realpath failed " << errno << std::endl;

  auto elfFuncs = ei.getFunctions();
  u_long init_loc = 0, fini_loc = 0;
  for (auto &f : elfFuncs) {
    if (f->name == "call_init") {
      init_loc = f->lowPC;
    }
    else if (f->name == "_dl_fini") {
      fini_loc = f->lowPC;
    }
  }
  assert(init_loc);
  auto addr = new Configuration::Address(resolved, "x", init_loc, 0);
  auto naddr = new Configuration::Address(resolved, "x", init_loc + 1, 0);
  auto config =  new Configuration::TracerConfiguration("UNUSED");
  config->addBreakpoint(addr, naddr);
  tpoints.push_back(config);

  assert(fini_loc);
  auto exit_addr = new Configuration::Address(resolved, "x", fini_loc, 0);
  auto nexit_addr = new Configuration::Address(resolved, "x", fini_loc + 1, 0);
  auto exit_config =  new Configuration::TracerConfiguration("UNUSED");
  exit_config->addBreakpoint(exit_addr, nexit_addr);
  tpoints.push_back(exit_config);

  vector<MemObject> maps;
  getFiles(replayDir, {LIBPTHREAD_NAME, LIBC_NAME}, maps);
  auto pthreadConfig = new Configuration::TracerConfiguration("UNUSED");
  auto forkConfig = new Configuration::TracerConfiguration("UNUSED");

  for (auto mapping : maps) {
    // std::cerr << "parsing " << mapping.name << std::endl;
    Parser::ElfInfo pinfo(mapping.name);
    elfFuncs = pinfo.getFunctions();

    std::sort(elfFuncs.begin(), elfFuncs.end(),
              [] (const unique_ptr<Parser::FunctionInfo> &f,
                  const unique_ptr<Parser::FunctionInfo> &e) {return f->lowPC < e->lowPC;});

    u_long pthread_loc = 0, fork_loc = 0, fork_high = 0;
    bool grabHigh = false; // UGLY
    for (auto &f : elfFuncs) {
      if (grabHigh && f->lowPC != fork_loc) {
        fork_high = f->lowPC;
        grabHigh = false;
      }

      if (f->name == "start_thread") {
        pthread_loc = f->lowPC + mapping.start;
        pthreadConfig->addBreakpoint(new Address(pthread_loc), new Address(pthread_loc +1));
      }
      else if (f->name == "fork") {
        fork_loc = f->lowPC;
        grabHigh = true;
      }
    }

    if (fork_loc) {
      assert(fork_loc && fork_high);
      // find all returns at the end of fork then:
      Parser::InstructionInfo ii(mapping.name);
      auto esi = pinfo.getSection(fork_loc);
      //std::cerr << "get instrcutions from " << fork_loc << " - " << fork_high << " within "
      //<< esi.offset << " " << esi.loaded << std::endl;
      auto ssi = ii.getSeqInstIter(esi->offset + fork_loc - esi->loaded, fork_high - fork_loc, fork_loc);
      vector<u_long> returns;
      ssi.getInstructions(fork_high, [&returns](cs_insn inst) {
          if (inst.id == X86_INS_RET) returns.push_back(inst.address);
        });
      assert (returns.size() > 0);
      for (auto r : returns) {
        u_long fork_return_loc = mapping.start + r;
        forkConfig->addBreakpoint(new Address(fork_return_loc), new Address(fork_return_loc + 1));
      }
    }
  }
  // std::cerr << "fork config is: " << *forkConfig << std::endl;
  tpoints.push_back(pthreadConfig);
  tpoints.push_back(forkConfig);


  Configuration::writeTPoints(tpoints, filename);

  free(resolved);
  // I think tpoints will call all of the destructors..?
}

