#include <string>
#include <sstream>
#include <unordered_map>

#include <boost/optional.hpp>

#include "function_code.hpp"
#include "tracing_code_manager.hpp"
#include "variable_type.hpp"

#include "../protos/steamdrill.pb.h"

#ifndef __TRACER_BUILDER__
#define __TRACER_BUILDER__

using std::string;
using std::stringstream;
using boost::optional;

//typedef optional<Generate::TracerVariable> oTV;
typedef Generate::TracerVariable TV;

namespace Generate {

class SteamdrillBuilder : public FunctionCode {
 protected:
  string _name;
  TracingCodeManager *_man;
  int _output;

  TV convertExpression(const Steamdrill::SDCast&);
  TV convertExpression(const Steamdrill::SDField&);
  TV convertExpression(const Steamdrill::Metadata&);
  TV convertExpression(const Steamdrill::Register&);
  TV convertExpression(const Steamdrill::Literal&);
  TV convertExpression(const Steamdrill::MemoryDereference&);
  TV convertExpression(const Steamdrill::TracingFunction&);
  TV convertExpression(const Steamdrill::TracingCodeCall&);
  TV convertExpression(const Steamdrill::BinaryOperatorExpression&);
  TV convertExpression(const Steamdrill::BinaryComparisonExpression&);

  TV convertExpression(const Steamdrill::SDExpression &);

 public:
  SteamdrillBuilder(std::string proto, TracingCodeManager *m, int output = 0):
      _name(proto), _man(m), _output(output) {}
  inline string getName(void) const {return _name;}
};

class TracerBuilder: public SteamdrillBuilder{
 private:
  stringstream _fmtString;
  // std::vector<TracerVariable> _args; I don't think I need this, actually?

  std::vector<std::string> _formats;
  std::vector<std::string> _args;

  int nxtVar;
  string getNextVar() {
    return boost::str(boost::format{"var%d"} %nxtVar++);
  }

 public:
  TracerBuilder(string proto, TracingCodeManager *m, int output=0) : SteamdrillBuilder(proto, m, output), nxtVar(0) {
    addLine("void ");
    addLine(_name);
    addLine("() {");
    addLine(boost::str(boost::format{"startRow(%d);\n"} %output));
  }
  void addInspector(const Steamdrill::SDExpression& expr);
  string toString();
  // inherit toString from FunctionCode
};

class FilterBuilder: public SteamdrillBuilder {
 public:
  bool hasBody;
  FilterBuilder(string proto, TracingCodeManager *m) : SteamdrillBuilder(proto, m)  {
    _name = proto;
    //addLine("extern \"C\" bool ");
    //addLine(_name);
    //addLine("() {");
    hasBody = false;
  }

  FilterBuilder(string proto,
                TracingCodeManager *m,
                const Steamdrill::SDExpression &expr): SteamdrillBuilder(proto, m) {
    _name = proto;
    setReturn(expr);
  }

  void setReturn(const Steamdrill::SDExpression& expr);

  inline void addLine(string line) {
    hasBody = true;
    FunctionCode::addLine(line);
  }

  string toString(void) const {
    std::stringstream out;
    out << "int " << _name << "() {"
        << _body.str()
        << "}";
    return out.str();
  }
};

} // namespace Generate

namespace std {
template <> struct hash<Generate::FilterBuilder>
{
  size_t operator()(const Generate::FilterBuilder &fb) const {
    return std::hash<string>{}(fb._body.str());
  }
};
}

#endif /*__TRACER_BUILDER__*/
