

#include <iostream>

#include "staticLibrary.hpp"
namespace StaticLibrary{

std::ostream& output = std::cout;

template<> void addOutput<Parser::Variable>(Parser::Variable v, char sep) {
  output << std::hex
         << sep 
         << v.name << FIELD_SEP
         << v.location << FIELD_SEP
         << v.type << " ";
}

void endRow() {
  output << std::endl;
}

std::unordered_map<std::string, Parser::AliasMap> getAliases(void) {
  std::unordered_map<std::string, Parser::AliasMap> am;
  Parser::AliasMap libcAlias = {{"read", "__read_nocancel"}};
  am.emplace("libc.so.6", libcAlias);
  am.emplace("libpthread.so.0", libcAlias);
  return am;
}

} /*end namespace StaticLibrary */
