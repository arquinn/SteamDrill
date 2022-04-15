#include <string>
#include <sstream>
#include <iostream>
#include <stdarg.h>


#pragma once

using std::string;
using std::stringstream;

namespace Generate {
class FunctionCode {
  //private:

 public:
  stringstream _body;
  FunctionCode(string proto) {
    _body << proto;
    _body << "{";
  }
  FunctionCode(void) {}

  FunctionCode(FunctionCode &c) {
    _body << c._body.str();
  }

  FunctionCode(FunctionCode &&c) {
    _body << c._body.str();
  }

  FunctionCode& operator=(const FunctionCode &c) {
    _body.str(string());
    _body << c._body.str();
    return *this;
  }

  friend inline bool operator==(const FunctionCode &left, const FunctionCode &right) {
    return left._body.str() == right._body.str();
  }

  virtual inline void addLine(string line) {_body << line;}

  // void addLine(const char *line) {_body << line;}
  virtual string toString(void) {
    stringstream out;
    out << _body.str() << "}";
    return out.str();
  }
};
} // end namespace Generate

namespace std {
template <> struct hash<Generate::FunctionCode>
{
  size_t operator()(const Generate::FunctionCode &fc) const {
    return std::hash<string>{}(fc._body.str());
  }
};
}

