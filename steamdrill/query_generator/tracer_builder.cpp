#include <memory>
#include <sstream>


#include <boost/format.hpp>
#include <boost/io/detail/quoted_manip.hpp>
#include <boost/range/combine.hpp>

#include "spdlog/spdlog.h"

#include "tracer_builder.hpp"
#include "../utils/utils.hpp"

// #define DEBUG_ME


using std::shared_ptr;
using std::to_string;
using boost::optional;

namespace Generate {

TracerVariable SteamdrillBuilder::convertExpression(const Steamdrill::Metadata &meta) {
  string name = "";
  string typeName = "long";
  switch (meta.val()) {
    case Steamdrill::Metadata::REPLAY_CLOCK:
      name = "tracerReplayclock()";
      break;
    case Steamdrill::Metadata::LOGICAL_CLOCK:
      name = "tracerLogicalClock()";
      break;
    case Steamdrill::Metadata::THREAD:
      name =  "tracerGettid()";
      break;
    case Steamdrill::Metadata::PROCESS:
      name ="tracerGetpid()";
      break;
    default:
      spdlog::error("No builtin support for: {}",
                    Steamdrill::Metadata::MetadataType_Name(meta.val()));
      name = "";
  }
  return TracerVariable(name, TypeManager::getType(typeName));
}


TracerVariable SteamdrillBuilder::convertExpression(const Steamdrill::Register &reg) {
  string name = "";
  switch(reg.val()) {
    case Steamdrill::Register::EAX:
      name = "tracerGetEax()";
      break;
    case Steamdrill::Register::EBX:
      name = "tracerGetEbx()";
      break;
    case Steamdrill::Register::ECX:
      name = "tracerGetEcx()";
      break;
    case Steamdrill::Register::EDX:
      name = "tracerGetEdx()";
      break;
    case Steamdrill::Register::EDI:
      name = "tracerGetEdi()";
      break;
    case Steamdrill::Register::ESI:
      name = "tracerGetEsi()";
      break;
    case Steamdrill::Register::ESP:
      name = "tracerGetEsp()";
      break;
    case Steamdrill::Register::EBP:
      name = "tracerGetEbp()";
      break;
    case Steamdrill::Register::EIP:
      name = "tracerGetEip()";
      break;
    default:
      spdlog::error("Cannot handle register: {} ",
                    Steamdrill::Register::DoubleWord_Name(reg.val()));
  }
  return TracerVariable(name, TypeManager::getType("long"));
}

TracerVariable SteamdrillBuilder::convertExpression(const Steamdrill::Literal &lit) {
  //std::stringstream output;
  switch(lit.type()) {
    case Steamdrill::Literal::INTEGER:
      //output << std::hex << lit.ival();
      //spdlog::error("parsed an integer to {}", output.str());
      return TracerVariable(to_string(lit.ival()), TypeManager::getType("long"));
    case Steamdrill::Literal::SHORT:
      //output << std::hex << lit.ival();
      //spdlog::error("parsed an integer to {}", output.str());
      return TracerVariable(to_string(lit.ival()), TypeManager::getType("short"));

    case Steamdrill::Literal::BOOLEAN:
      return TracerVariable(lit.bval() ? "1" : "0", TypeManager::getType("long"));

    case Steamdrill::Literal::STRING:
      return TracerVariable(boost::str(boost::format{"%s"} %boost::io::quoted(lit.sval())),
                            TypeManager::getType("string"));
    default:
      spdlog::error("Cannot handle column type: {} ",
                    Steamdrill::Literal::LiteralType_Name(lit.type()));
      return TracerVariable("", TypeManager::getType("string"));
  }
}


TracerVariable SteamdrillBuilder::convertExpression(const Steamdrill::MemoryDereference &md) {
  auto tv = convertExpression(md.operand());
  auto ptr = dynamic_cast<PointerType*>(tv.type.get());
  assert (ptr);
  return TracerVariable ("(*" + tv.name + ")", move(ptr->ptsTo));
}

TracerVariable SteamdrillBuilder::convertExpression(const Steamdrill::BinaryOperatorExpression &bce) {
  auto tv1 = convertExpression(bce.e1()), tv2 = convertExpression(bce.e2());

  string op = "";
  switch(bce.op())
  {
    case Steamdrill::BinaryOperatorExpression::ADD:
      op = " + " ;
      break;
    case Steamdrill::BinaryOperatorExpression::SUB:
      op = " - " ;
      break;
    case Steamdrill::BinaryOperatorExpression::MULTIPLY:
      op = " + " ;
      break;
    case Steamdrill::BinaryOperatorExpression::OR:
      op = " || " ;
      break;
    case Steamdrill::BinaryOperatorExpression::AND:
      op = " && " ;
      break;
  }

  return TracerVariable("(" + tv1.name + op  + tv2.name + ")", move(tv1.type));
}

TracerVariable SteamdrillBuilder::convertExpression(const Steamdrill::SDCast &c) {
  auto tv = convertExpression(c.expr());
  return TracerVariable(
      boost::str(boost::format{"((%s)(%s))"} %c.type() %tv.name),
      TypeManager::getType(c.type()));
}

TracerVariable SteamdrillBuilder::convertExpression(const Steamdrill::SDField &f) {
  auto tv = convertExpression(f.expr());
  return TracerVariable(
      boost::str(boost::format{"((%s)%s)"} %tv.name %f.field()),
      TypeManager::getType("long"));  // this doesn't actually work.
}

TracerVariable SteamdrillBuilder::convertExpression(const Steamdrill::TracingCodeCall &tc) {
  stringstream call;
  bool first = true;

  if (!tc.functionname().size()) {
    return TracerVariable("\"\"", TypeManager::getType("char *"));
  }

  auto def = _man->getFunction(tc.functionname());
  if (def == nullptr) {
    spdlog::error("missing def of {} in {}", tc.functionname(), _man->toString());
    assert (false);
  }

  call <<  tc.functionname() << "(";

  // cast each parameter to the 'correct' type for this function.
  auto op = tc.operands().begin(), opEnd = tc.operands().end();

  for (auto &input : def->inputTypes) {
    if (!first) call << ",";

    call << "(" << input->name <<")(";

    auto v = convertExpression(*op);
    call << v.name << ")";
    op++;
    first = false;
  }
  call << ")";

 // need the tracing code manager for this part! VVV
  return TracerVariable(call.str(), TypeManager::getType(def->returnType->name));
}


TracerVariable SteamdrillBuilder::convertExpression(const Steamdrill::BinaryComparisonExpression &bce) {
  auto tv1 = convertExpression(bce.e1()), tv2 = convertExpression(bce.e2());
  string op = "";
  switch(bce.op())
  {
    case Steamdrill::BinaryComparisonExpression::EQUALS:
      op = " == " ;
      break;
    case Steamdrill::BinaryComparisonExpression::GREATER:
      op = " > " ;
      break;
    case Steamdrill::BinaryComparisonExpression::GEQ:
      op = " >= " ;
      break;
    case Steamdrill::BinaryComparisonExpression::LESS:
      op = " < " ;
      break;
    case Steamdrill::BinaryComparisonExpression::LEQ:
      op = " <= " ;
      break;
    case Steamdrill::BinaryComparisonExpression::NOT_EQUALS:
      op = " != " ;
      break;
    default:
      assert (false);

  }
  return TracerVariable("(" + tv1.name + op  + tv2.name + ")", TypeManager::getType("long"));
}


TracerVariable SteamdrillBuilder::convertExpression(const Steamdrill::SDExpression &e) {

  switch (e.exp_case())
  {
    case Steamdrill::SDExpression::kMeta:
      return convertExpression(e.meta());
    case Steamdrill::SDExpression::kReg:
      return convertExpression(e.reg());
    case Steamdrill::SDExpression::kLit:
      return convertExpression(e.lit());
    case Steamdrill::SDExpression::kNe:
      assert (false);
      break;
    case Steamdrill::SDExpression::kBce:
      return convertExpression(e.bce());
    case Steamdrill::SDExpression::kBoe:
      return convertExpression(e.boe());
    case Steamdrill::SDExpression::kTf:
      assert (false);
      break;
    case Steamdrill::SDExpression::kMd:
      return convertExpression(e.md());
    case Steamdrill::SDExpression::kTc:
      return convertExpression(e.tc());
    case Steamdrill::SDExpression::kCat:
      return convertExpression(e.cat());
    case Steamdrill::SDExpression::kField:
      return convertExpression(e.field());
    case Steamdrill::SDExpression::EXP_NOT_SET:
      assert (false);
      break;
    default:
      assert(false);
  }

  spdlog::error("unknown expression type {}", e.exp_case());
  assert (false);
}


void TracerBuilder::addInspector(const Steamdrill::SDExpression& expr) {
  auto tv = convertExpression(expr);
  addLine(tv.type->output(_output, tv.name));
  //_formats.push_back(tv.type->format);
  //_args.push_back(
  //boost::str(
  //boost::format {tv.type->arg_format } %tv.name));

  // string nxt = getNextVar();
      //string line = boost::str(boost::format{"char * %s = %s(%s);\n"} %nxt %tv.type->serial %tv.name);
  //spdlog::trace("adding inspector for {} of {}: {}", nxt, tv.debugString(), line);
  //addLine(line);

#ifdef DEBUG_ME
  line = boost::str(boost::format{"tracerlogDebug(\"var named %s (%%p)\\n\", %s);\n"} %nxt %nxt);
  addLine(line);
#endif
} // TracerBuilder::addInspector


string TracerBuilder::toString() {
#define SEP '\u0002'
  // #define TRACER_STOP '\u0001'

  /*stringstream fmt;
  stringstream args;
  for (auto tup : boost::combine(_formats, _args)) {
    string nextFormat, nextArg;
    boost::tie(nextFormat, nextArg) = tup;
    fmt << SEP << nextFormat;
    args << ", " << nextArg;
  }
  //fmt << TRACER_STOP; // not needed, the null terminator is enough.
  addLine(boost::str(boost::format{"tracerlogPrintf(%d, \"%s\"%s);\n"} %_output %fmt.str() %args.str()));
  */

  //   for (int i = 0; i < nxtVar; ++i) {
  //#ifdef DEBUG_ME
  //    addLine(boost::str(boost::format{"tracerlogDebug(\"free (%%p)\\n\", var%d);\n"} %i));
  //#endif
  //    addLine(boost::str(boost::format{"free(var%d);\n"} %i));
  //  }
  // addLine(");");
  addLine(boost::str(boost::format{"endRow(%d);\n"} %_output));
  return FunctionCode::toString();

} // TracerBuilder::toString

void FilterBuilder::setReturn(const Steamdrill::SDExpression& expr) {
  TracerVariable tv = convertExpression(expr);

  // we actually don't want to getOutput...
  // is this the only feasible place to put that optional..?
  addLine(boost::str(boost::format("return %s;") %tv.name));
} // FilterBuilder::setReturn

} // namespace Generate

