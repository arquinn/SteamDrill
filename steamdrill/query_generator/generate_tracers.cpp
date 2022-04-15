/*
 * Fxns to generate the tracers needed to generated a selected
 * region from a relational tree using LLVM IR;
 */

#include <iostream>
#include <sstream>
#include <fstream>
#include <cstring>
#include <string>
#include <memory>
// #include <boost/optional.hpp>
#include <optional>

#include <vector>
#ifdef SPDLOG_ACTIVE_LEVEL
#undef SPDLOG_ACTIVE_LEVEL
#endif
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_INFO
#include "spdlog/spdlog.h"
#include "../protos/steamdrill.pb.h"

#include "../utils/tracer_configuration.hpp"

#include "query_generator.hpp"
#include "tracer_module.hpp"
#include "tracer_builder.hpp"
#include "tracing_code_manager.hpp"

#include "range_list.hpp"

// This has to go last, o/w macros from w/in this file are resolved
// In the llvm headers (yuck).
extern "C"
{
#include <stdio.h>
  // #include <sys/reg.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/sendfile.h>
}

using Steamdrill::SDExpression;
using Steamdrill::SDRelation;
using Steamdrill::BinaryOperatorExpression;
using Steamdrill::BinaryComparisonExpression;
using Configuration::Address;
using Configuration::TracerConfiguration;
using std::string;
using std::cerr;

namespace Generate {

/*
  class ParseReturn {
 public:

  RangeList ranges;
  uintptr_t literal;
  enum PRType {RANGE, LIT, EIP, RPCLOCK, UNSET};
  PRType type;
  ParseReturn(): type(UNSET) {};
};
*/


using std::optional;

optional<uintptr_t> getLit(const Steamdrill::SDExpression exp) {
  if (exp.has_lit() && exp.lit().type() == Steamdrill::Literal::INTEGER) {
    return exp.lit().ival();
  }
  return {};
}

bool isEip(const Steamdrill::SDExpression exp) {
  if (exp.has_reg() && exp.reg().val() == Steamdrill::Register::EIP) {
    return true;
  }
  return false;
}



//todo: Wrap this up in a visitor pattern (I think?)
//todo: Add that merge thing?
template<class T> optional<T>
parseBounds(const Steamdrill::SDExpression exp,
            const std::function <optional<T> (const Steamdrill::SDExpression&)>f,
            const std::function <optional<T> (BinaryOperatorExpression::Operator, optional<T>, optional<T>)>merge) {

  // check the base condition:
  auto base = f(exp);
  if (base) {
    return base;
  }
  else if (exp.has_boe()) {
    BinaryOperatorExpression boe = exp.boe();
    return merge(boe.op(), parseBounds(boe.e1(), f, merge), parseBounds(boe.e2(), f, merge));
  }
  return {};

  /*
  else if (exp.has_bce()) {
    Steamdrill::BinaryComparisonExpression expr = exp.bce();
    ParseReturn pr1, pr2;

    if (parseBounds(expr.e1(), f, pr1) && parseBounds(expr.e2(), f, pr2)) {
      if (pr1.type == ParseReturn::RANGE || pr2.type == ParseReturn::RANGE ||
          pr1.type == ParseReturn::UNSET || pr2.type == ParseReturn::UNSET ||
          pr1.type == pr2.type) {

        SPDLOG_ERROR("binaryCOmparisonExpression doesn't seem meaningful?");
        return false;
      }
      uintptr_t lit = pr1.type == ParseReturn::LIT ? pr1.literal : pr2.literal;
      bounds.type = ParseReturn::RANGE;
      switch (expr.op()) {
        case Steamdrill::BinaryComparisonExpression::EQUALS:
          bounds.ranges.reset(lit, lit + 1);
          break;
        case Steamdrill::BinaryComparisonExpression::NOT_EQUALS:
          bounds.ranges.reset(0, lit);
          bounds.ranges.unionRanges(RangeList(lit + 1, UINT_MAX));
          break;
        case Steamdrill::BinaryComparisonExpression::GEQ:
          bounds.ranges.reset(lit, UINT_MAX);
          break;
        case Steamdrill::BinaryComparisonExpression::LESS:
          bounds.ranges.reset(0, lit);
          break;
      }
    }
    else {
      return false;
    }
  }
  */
  // return true;
}

string gatherTracingCode(const Steamdrill::SDQuery *query, TracerModule &mod) {
  std::stringstream tracingCode;
  for (auto def : query->tracingcodedefs()) {
    TracingCodeDef *tcd = mod.addTracingCode(def.name(),
                                             TypeManager::getType(def.returntype()));

    tracingCode << tcd->returnType->name << " " << def.name() << "(";

    bool first = true;
    for (auto input : def.inputs()) {
      if (!first) tracingCode << ", ";
      tracingCode << input;
      //spdlog::info("add input {}", input);
      tcd->addInput(input.substr(0, input.find_first_of(' ')));
      first = false;
    }
    tracingCode << ") " <<def.code() << "\n";
  }
  return tracingCode.str();
}

std::string generateTracer(const Steamdrill::SDRelation request, TracerModule &module) {
  TracerBuilder *builder = module.getNewTracer(request.output());
  for (auto col : request.cols()) {
    builder->addInspector(col);
  }

  return builder->getName();
}

optional<RangeList> parseEip(const Steamdrill::SDExpression &exp) {
  if (exp.has_bce()) {

    Steamdrill::BinaryComparisonExpression expr = exp.bce();
    auto lit = getLit(expr.e1()) ? getLit(expr.e1()) : getLit(expr.e2());
    if ((isEip(expr.e1()) || isEip(expr.e2()) && lit)) {
        switch (expr.op()) {
          case Steamdrill::BinaryComparisonExpression::EQUALS:
            return RangeList(*lit, *lit + 1);
          case Steamdrill::BinaryComparisonExpression::NOT_EQUALS:
            return RangeList(0, *lit).unionRanges(RangeList(*lit + 1, UINT_MAX));
          case Steamdrill::BinaryComparisonExpression::GEQ:
            return RangeList(*lit, UINT_MAX);
          case Steamdrill::BinaryComparisonExpression::LESS:
            return RangeList(0, *lit);
        }
      }
  }
  return {};
}

optional<RangeList> mergeRangeList(const BinaryOperatorExpression::Operator &op,
                                   optional<RangeList> one, optional<RangeList> two) {

  switch (op) {
    case BinaryOperatorExpression::OR:
      if (one && two) {
        std::stringstream output;
        output << *one << " and " << *two << std::endl;
        SPDLOG_INFO("merging {}", output.str());
        return one->unionRanges(*two);
      }
      return one ? one : (two ? two : std::nullopt);
    default:
      SPDLOG_ERROR("unsupported merge");
  }
  return {};
}


  /*
bool parseRPClock(const Steamdrill::SDExpression &exp, ParseReturn &bounds) {
  if (exp.has_meta() && exp.meta().val() == Steamdrill::Metadata::REPLAY_CLOCK) {
    bounds.type = ParseReturn::RPCLOCK;
    return true;
  }
  return false;
}
  */
// todo: we assume self-contained eip bounds (e.g., no 'eip = ... || *global = ...')
string generateFilter(const Steamdrill::SDRelation request, TracerModule &module,
                      RangeList &eipBounds, RangeList &clockBounds) {
  SDExpression expr;
  bool set = false;
  string rtn = "";

  for (auto fil : request.filters()) {
    // SPDLOG_DEBUG("fil {}", fil.DebugString());
    auto eipRange = parseBounds<RangeList>(fil, parseEip, mergeRangeList);
    // todo: clock case
    if (eipRange) {
      eipBounds = eipBounds.intersectRanges(*eipRange);
      std::stringstream s;

      s << *eipRange << " => " << eipBounds;
      SPDLOG_TRACE("eip based with {}", s.str());
    }
    else  {
      if (!set) {
        expr = fil;
        set = true;
      }
      else {
        Steamdrill::BinaryOperatorExpression boe;
        Steamdrill::SDExpression newe;

        *boe.mutable_e2() = expr;
        *boe.mutable_e1() = fil;
        boe.set_op(Steamdrill::BinaryOperatorExpression::AND);
        *newe.mutable_boe() = boe;
        expr = newe;
      }
    }
  }
  if (set) {
    rtn = module.getFilter(expr);
  }

  return rtn;
}

TracerConfiguration * getTracerConfig(const SDRelation request,
                                      RangeList &bounds, RangeList &clockBounds,
                                      string filterName, string tpName) {
  Configuration::TracerConfiguration *config =
      new Configuration::TracerConfiguration(tpName);

  Configuration::TracerConfiguration *filter = nullptr;
  if (!filterName.empty())
    filter = new Configuration::TracerConfiguration(filterName);

  if (bounds.empty()) {
    SPDLOG_WARN("{}: no eip bounds", tpName);
  }

  for (auto &r : bounds.ranges) {
    assert (r.first <= r.second);
    if (r.first && r.first != r.second) {
      config->addBreakpoint(new Address(r.first), new Address(r.second));
    }
  }

  for (auto &r : clockBounds.ranges) {
    assert (r.first <= r.second);
    if (r.first && r.first != r.second) {
      config->addClockRange(r.first, r.second);
    }
  }

  if (filter)
    config->setFilter(shared_ptr<Configuration::TracerConfiguration>(filter));

  return config;
}

vector<Configuration::TracerConfiguration*>
generateSHQuery(const Steamdrill::SDQuery *query,
                const string tracerFilename,
                const string tpFilename,
                const string tracersBaseFile) {
  std::vector<Configuration::TracerConfiguration*> configs;
  TracerModule module(tracerFilename);
  module.addBase(tracersBaseFile);
  module.addString(gatherTracingCode(query, module));

  if (query->rels_size() == 0) {
    SPDLOG_ERROR("no relations!?!");
  }

  // SPDLOG_INFO("using {} relations", query->rels_size());
  // std::cerr << "commands:" << query->DebugString();

  // Prepare the tracepoint file:
  for (auto rel : query->rels()) {
    // SPDLOG_DEBUG("query rel {}", rel.DebugString());
    RangeList bounds, clockBounds;
    string tracerName = generateTracer(rel, module);
    string filterName = generateFilter(rel, module, bounds, clockBounds);
    configs.push_back(getTracerConfig(rel, bounds, clockBounds, filterName, tracerName));
  }

  writeTPoints(configs, tpFilename);
  module.writeToFile();
  return configs;
}
} // namespace Generate
