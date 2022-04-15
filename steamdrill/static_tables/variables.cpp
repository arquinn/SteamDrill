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
#include "../binary_parser/type.hpp"

using std::unique_ptr;


int main(int argc, char *argv[]) {
  args::ArgumentParser parser("Get info about all globals defined in an ELF file.", "A.R.Q.");
  args::Group group(parser, "Required arguments:", args::Group::Validators::All);
  args::Positional<std::string> cacheA(group, "file", "The ELF executable in the cache file");
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

  // first get all of the function local variables
  string cache = args::get(cacheA);
  char oname[PATH_MAX+1];
  get_orig_filename(cache.c_str(), oname);

  auto funcs = Parser::getFunctions(cache);
  for (auto &foo : funcs) {
    // skip _exit b/c its basically impossible to run code in
    if (foo->name == "_exit")
      continue;

    for (auto local : foo->locals) {
      StaticLibrary::addOutput(oname);
      StaticLibrary::addOutput(cache);
      StaticLibrary::addOutput(foo->name);
      StaticLibrary::addOutput(local.name);
      StaticLibrary::addOutput(foo->lowPC);
      StaticLibrary::addOutput(foo->highPC);
      StaticLibrary::addOutput(local.location);
      StaticLibrary::addOutput(local.type);
      StaticLibrary::endRow();
    }
  }
}
