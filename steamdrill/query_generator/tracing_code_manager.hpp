#include <memory>
#include <string>
#include <vector>

#include <unordered_map>
#include <boost/optional.hpp>

#include "../protos/steamdrill.pb.h"
#include "variable_type.hpp"

#ifndef __TRACING_CODE_MANAGER
#define __TRACING_CODE_MANAGER

using std::make_unique;
using std::string;
using std::vector;
using std::unordered_map;


namespace Generate {

class TracingCodeDef
{
 public:
  typedef vector<unique_ptr<Type>>::iterator InputIterator;
  unique_ptr<Type> returnType;
  string name;
  vector<unique_ptr<Type>> inputTypes;

  TracingCodeDef(unique_ptr<Type> type, string n):  returnType(move(type)), name(n) {};

  void addInput(string in) {
    inputTypes.push_back(TypeManager::getType(in));
  }
};

class TracingCodeManager
{
private:
  unordered_map<string, unique_ptr<TracingCodeDef>> defs;
public:
  TracingCodeManager() {};

  inline TracingCodeDef* getFunction(string name) {
    auto dpair = defs.find(name);
    if (dpair != defs.end())
      return dpair->second.get();
    return nullptr;
  } // end getFunction()

  inline TracingCodeDef* addTracingCode(string name, unique_ptr<Type> type) {
    auto dpair = defs.emplace(name, make_unique<TracingCodeDef>(move(type), name));
    return dpair.first->second.get();
  } // end addTracingCode()

  string toString() {
    std::stringstream s;
    s << *this;
    return s.str();
  }

  friend std::ostream &operator<<(std::ostream &os, const TracingCodeManager &tcm) {
    for (auto &d : tcm.defs) {
      os << ", " << d.first;
    }
    return os;
  };
}; /* class TracingCodeManager */
} /* namespace Generate */

#endif /*  __TRACING_CODE_MANAGER */
