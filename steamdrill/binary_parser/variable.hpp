// copyright @ Andrew Quinn (not that you want this lol)

#pragma once

#include <functional>
#include <string>
#include "variable_function.hpp"

namespace Parser {
using std::string;

class Variable {
 public:
  string           name;
  string           binary;
  //VariableFunction location;
  // hacky bullshit so that I can modify these even though we're iterating an unordered_set
  mutable string           location;
  mutable string           type;
  mutable uintptr_t         ptrLoc; // pointer location (if applicable -- lookin' at you globals!)

  Variable(void): name(""), binary(""), location(""), type(""), ptrLoc(0) {};
  Variable(string n, string b, string l, string t, uintptr_t p = 0):
      name(n), binary(b), location(l), type(t), ptrLoc(p) {};
  Variable(const Variable &o) {
    name = o.name;
    binary = o.binary;
    location = o.location;
    type = o.type;
    ptrLoc = o.ptrLoc;
  }

  // should also probably consider binary in this equality + hash
  bool operator==(const Variable &other) const  {
    return name == other.name;
  }


}; // end class Variable
} // end namespace Parser

namespace std {
template <> struct hash<Parser::Variable>
{
  size_t operator()(const Parser::Variable &v) const
  {
    return std::hash<string>{}(v.name);
  }
};
}
