#include <iostream>
#include <vector>
#include <unordered_map>

#include <boost/regex.hpp>

#include "../binary_parser/binary_parser.hpp"
#include "../utils/utils.hpp"

#ifndef __STATIC_LIB
#define __STATIC_LIB
namespace StaticLibrary {
/*
 * global variables
 */
extern std::ostream& output;

/*
 * Output Functions
 */
template <class T> void addOutput(T input, char sep = SEP)  {
  output << std::hex << sep << input << " ";
}
template<> void addOutput<Parser::Variable>(Parser::Variable v, char sep);

template <class T1, class T2>
void addOutput(std::pair<T1,T2> pair, char sep = SEP) {
  output << std::hex << sep << pair.first << FIELD_SEP << pair.second;
}


template <class T>
void addCollection(T &input)  {
  output << SEP;
  for (auto &item : input)
    addOutput(item, ',');
}

template <class T1, class T2>
void addCollection(std::unordered_map<T1,T2> &input)  {
  output << SEP;
  for (auto &item : input)
    output << std::hex << "," << item.first << ":" << item.second << " ";
}

void endRow();

std::unordered_map<std::string, Parser::AliasMap> getAliases(void);
} /* namespace StaticLibrary*/


#endif /* __STATIC_LIBRARY_H_ */
