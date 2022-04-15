// copyright @ Andrew Quinn

#pragma once
#include <unordered_map>
#include "../query_generator/function_code.hpp"

namespace Parser {

using std::string;
using std::unordered_map;

class VariableFunction : public Generate::FunctionCode {
 private:
  int _nxtVar;
  string _str;
  string _type;

  bool _inBlock;
  bool _hasOutput;
  unordered_map<string, string> _outputMap;

  string getNextVar(void);
  string addOutputDeclaration(void);

 public:
  VariableFunction (void);

  string getRegister (string regName);

  string addVariable(string type);
  void addAssignment(string rhs, string type, string var);
  void startIf(string condition);
  void endIf(void);

  void addOutputVal(string name, string value);
  string setType(string name);

  virtual string toString(void);
  virtual ~VariableFunction(){}


}; // end VariableFunction
} // end namespace Parser
