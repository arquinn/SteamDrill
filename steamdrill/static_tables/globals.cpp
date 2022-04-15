#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_map>

#include <boost/regex.hpp>

#include "args.hxx"
#include "staticLibrary.hpp"

extern "C" {
#include "parse_syscall_input_lib.h"
}

#include "../binary_parser/binary_parser.hpp"
#include "../query_generator/function_code.hpp"


//static llvm::LLVMContext theContext;
using std::unique_ptr;

int main(int argc, char *argv[])
{
  args::ArgumentParser parser("Get info about all globals defined in an ELF file.", "A.R.Q.");
  args::Group group(parser, "Required arguments:", args::Group::Validators::All);
  args::Positional<std::string> file(group, "file", "The ELF executable in the cache file");
  args::Positional<std::string> stats(group, "stats_file", "Location of where to place stats");
  args::HelpFlag help(parser, "help", "Display help", {'h', "help"});

  Timer::StampManager sm;
  sm.addStamp("globalGeneratorStart");
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

  string cache = args::get(file);
  char oname[PATH_MAX+1];
  get_orig_filename(cache.c_str(), oname);


  auto globals = Parser::getGlobals(cache);

  for (auto &goo : globals) {
    StaticLibrary::addOutput(oname);
    StaticLibrary::addOutput(cache);
    StaticLibrary::addOutput(goo.name);
    StaticLibrary::addOutput(goo.ptrLoc);
    StaticLibrary::addOutput(goo.type + "*");
    // StaticLibrary::addOutput(goo.location);
    StaticLibrary::endRow();
  }

  sm.addStamp("globalGeneratorStop");
  sm.writeStamps(args::get(stats));

}
