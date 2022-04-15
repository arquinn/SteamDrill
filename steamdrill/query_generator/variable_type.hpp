#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <boost/format.hpp>

#include "../utils/utils.hpp"
#include "../protos/steamdrill.pb.h"
#include "sqlite3.h"

using std::shared_ptr;
using std::string;
using std::unique_ptr;
using std::vector;

#pragma once

namespace Generate{

class Type {
 public:
  // BuiltinType builtin;
  string name, def, output_format;
  Type(string n, string d, string of) : name(n), def(d), output_format(of)  {}
  virtual ~Type() = default;

  string output(int outputIdx, string input) {
    return boost::str(boost::format { output_format } %outputIdx %input);
  }
  string debugString() {
    std::stringstream ss;
    ss << "n:" <<  name << " d: " << def << " of: " << output_format;
    return ss.str();
  }
}; // end class Type

class TypeManager {
 private:
  #define TYPE_MANAGER_PATH "tmp.db"
  sqlite3 *_db;

  TypeManager(string path = std::getPath(TYPE_MANAGER_PATH));
 public:
  static TypeManager* theInstance;
  //  static shared_ptr<VariableType> getVariableType(const Steamdrill::DataType &dt);
  static unique_ptr<Type> getType(const string &type);
  static unique_ptr<Type> derefType(const unique_ptr<Type> &type);
  static vector<unique_ptr<Type>> getTypes(const string &type);
};

class PointerType : public Type {
 public:
  unique_ptr<Type> ptsTo;
  PointerType(string n, string s, string of, unique_ptr<Type> t):
      Type(n, s, of), ptsTo(move(t)) {}
}; //end class PointerType

// "man polymorphism never seems to work in c++"

class TracerVariable {
 public:
  string name;
  unique_ptr<Type> type;

  TracerVariable(string n, unique_ptr<Type> t): name(n), type(move(t)) {}

  string debugString() {
    std::stringstream ss;
    ss << name << " " << type->debugString();
    return ss.str();
  }

}; // end TracerVariable

} //namspace Generate
