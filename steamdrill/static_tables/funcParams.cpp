#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "args.hxx"
#include "staticLibrary.hpp"

#include "../binary_parser/variable.hpp"
#include "../binary_parser/binary_parser.hpp"
#include "../query_generator/function_code.hpp"

// lets just do some hacky garbage:
using std::string;
using std::unique_ptr;
using std::unordered_map;
using std::unordered_set;
using std::vector;

unordered_map<string, unordered_set<Parser::Variable>> builtinFunctions =
{
  {"epoll_wait", {Parser::Variable("epfd", "", "{return (int)*(u_long*)(tracerGetEsp() + 0x4);}", "int"),
                   Parser::Variable("events", "", "{return (struct epoll_event*)*(u_long*)(tracerGetEsp() + 0x8);}", "struct epoll_event*"),
                   Parser::Variable("maxevents", "", "{return (int)*(u_long*)(tracerGetEsp() + 0xc);}", "int"),
                   Parser::Variable("timeout", "", "{return (int)*(u_long*)(tracerGetEsp() + 0x10);}", "int")}},

  // libc is.... weird
   {"__poll", {Parser::Variable("fds", "", "{return *(struct pollfd**)(tracerGetEsp() + 0x4);}", "struct pollfd*"),
             Parser::Variable("nfds", "", "{return *(u_long*)(tracerGetEsp() + 0x8);}", "u_long"),
             Parser::Variable("timeout", "", "{return *(int*)(tracerGetEsp() + 0xc);}", "int")}},

   {"writev", {Parser::Variable("fd", "", "{return *(int*)(tracerGetEsp() + 0x4);}", "int"),
               Parser::Variable("iov", "", "{return *(struct iovec **)(tracerGetEsp() + 0x8);}", "struct iovec*"),
               Parser::Variable("iovcnt", "", "{return *(int*)(tracerGetEsp() + 0xc);}", "int")}},

   {"write", {Parser::Variable("fd", "", "{return *(int*)(tracerGetEsp() + 0x4);}", "int"),
              Parser::Variable("buf", "", "{return *(void**)(tracerGetEsp() + 0x8);}", "void*"),
              Parser::Variable("count", "", "{return *(u_int*)(tracerGetEsp() + 0xc);}", "u_int")}},

   {"__read_nocancel", {Parser::Variable("fd", "", "{return *(int*)(tracerGetEsp() + 0x4);}", "int"),
                        Parser::Variable("buf", "", "{return *(void**)(tracerGetEsp() + 0x8);}", "void*"),
                        Parser::Variable("count", "", "{return *(u_int*)(tracerGetEsp() + 0xc);}", "u_int")}},

  {"__send", {Parser::Variable("sockfd", "", "{return *(int*)(tracerGetEsp() + 0x4);}", "int"),
            Parser::Variable("buf", "", "{return *(void **)(tracerGetEsp() + 0x8);}", "void*"),
            Parser::Variable("len", "", "{return *(size_t*)(tracerGetEsp() + 0xc);}", "size_t"),
            Parser::Variable("flags", "", "{return *(size_t*)(tracerGetEsp() + 0x10);}", "int")}},

  {"recv", {Parser::Variable("sockfd", "", "{return *(int*)(tracerGetEsp() + 0x4);}", "int"),
            Parser::Variable("buf", "", "{return *(void **)(tracerGetEsp() + 0x8);}", "void*"),
            Parser::Variable("len", "", "{return *(size_t*)(tracerGetEsp() + 0xc);}", "size_t"),
            Parser::Variable("flags", "", "{return *(size_t*)(tracerGetEsp() + 0x10);}", "int")}},

  {"accept4", {Parser::Variable("sockfd", "", "{return *(int*)(tracerGetEsp() + 0x4);}", "int"),
               Parser::Variable("addr", "", "{return *(struct sockaddr**)(tracerGetEsp() + 0x8);}", "struct sockaddr*"),
               Parser::Variable("addrlen", "", "{return *(u_int**)(tracerGetEsp() + 0xc);}", "u_int*"),
               Parser::Variable("flags", "", "{return *(int*)(tracerGetEsp() + 0x10);}", "int")}}

  // {"read", ""},
  //{"write", ""}
};





int main(int argc, char *argv[]) {

  args::ArgumentParser parser("Get info about all fxns defined in an ELF file.", "A.R.Q.");
  args::Group group(parser, "Required arguments:", args::Group::Validators::All);
  args::Positional<std::string> file(group, "cache_file", "The ELF executable in the cache file");
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

    for (int i = 0; i < argc; ++i) {
      std::cerr << argv[i] << "\n";
    }
    return -1;
  }
  catch (args::ValidationError e) {
    std::cerr << e.what() << std::endl << parser << std::endl;
    for (int i = 0; i < argc; ++i) {
      std::cerr << argv[i] << "\n";
    }
    return -2;
  }

  // setup LLVM Module:
  unordered_map<string, Parser::AliasMap> aliases = StaticLibrary::getAliases();
  std::vector<unique_ptr<Parser::FunctionInfo>> funcs;

  funcs = Parser::getFunctions(args::get(file).c_str());

  for (auto &foo : funcs) {
    // skip _exit b/c it's difficult to instrument
    if (foo->name == "_exit")
      continue;

    auto builtinIter = builtinFunctions.find(foo->name);
    auto params = builtinIter != builtinFunctions.end() ? builtinIter->second : foo->parameters;

    for (auto param : params) {
      StaticLibrary::addOutput(foo->name);
      StaticLibrary::addOutput(foo->source);
      StaticLibrary::addCollection(foo->entryPCs);
      StaticLibrary::addOutput(param.name);
      StaticLibrary::addOutput(param.location);
      StaticLibrary::addOutput(param.type);
      StaticLibrary::endRow();
    }
  }
}
