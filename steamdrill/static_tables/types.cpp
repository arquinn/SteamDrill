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

#include "../binary_parser/binary_parser.hpp"
#include "../binary_parser/type.hpp"

using std::unique_ptr;

int main(int argc, char *argv[]) {
  args::ArgumentParser parser("Get info about all globals defined in an ELF file.", "A.R.Q.");
  args::Group group(parser, "Required arguments:", args::Group::Validators::All);
  args::Positional<std::string> file(group, "file", "The ELF executable in the cache file");
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

  auto types =  Parser::getTypes(args::get(file).c_str());

  for (auto &goo : types) {
    StaticLibrary::addOutput(goo->name);
    StaticLibrary::addOutput(goo->scalaJson());
    StaticLibrary::endRow();
  }
}
