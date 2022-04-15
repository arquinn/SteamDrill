#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <deque>
#include <cstdlib>
#include <string>
#include <sstream>

#include <stdio.h>

#include <boost/format.hpp>

#include "../query_generator/query_generator.hpp"
#include "../binary_parser/elf_parser.hpp"
#include "../utils/mem_object.hpp"
#include "../instrument/instrument.hpp"
#include "mkpartition_lib.h"
#include "partition_manager.hpp"

#include "args.hxx"

#include "sh_session.hpp"

#include "../utils/utils.hpp"

#define TPS_SUFFIX "-tps"
#define IPS_SUFFIX "-ips"
#define TRACER_SUFFIX "-tracers.c"
#define COMPILED_SUFFIX "-tracers.so"
#define ASSEMB_SUFFIX "-gas"
#define LIBC_SUFFIX "-libc_config"
#define STPS_SUFFIX "-standardT_config"
#define TFS_BASE "tracing_function_base.c"
#define BUILD_SCRIPT "build_bc.sh"

#define BLOCK_CACHE "block_cache/"
#define PART_CACHE "parts"



// not 100 why this doesn't use spdlog
#define DEBUG(x,...)
// #define DEBUG(...)  fprintf(stdout, __VA_ARGS__);
#define INFO(...) fprintf(stdout, __VA_ARGS__);

using std::string;

Timer::StampManager sm;
bool compileTracers(string tracer, string tps, string ips, string dest, string assemb, string replay) {
  pid_t child = fork();
  int status = 0;
  if (child == 0) {
    const char* args[256];
    int argcnt = 0;
    args[argcnt++] = BUILD_SCRIPT;
    args[argcnt++] = "-o";
    args[argcnt++] = dest.c_str();
    args[argcnt++] = "-t";
    args[argcnt++] = tps.c_str();
    args[argcnt++] = "-i";
    args[argcnt++] = ips.c_str();
    args[argcnt++] = "-f";
    args[argcnt++] = tracer.c_str();
    args[argcnt++] = "-r";
    args[argcnt++] = replay.c_str();
    args[argcnt++] = "-a";
    args[argcnt++] = assemb.c_str();
    args[argcnt++] = NULL;

    execvp (std::getPath(BUILD_SCRIPT), (char **)args);
    std::cerr << "hmm, exeve error " << strerror(errno)
              << " " << (u_long)args << std::endl;
    return false;
  }

  waitpid(child, &status, 0);

  if (WEXITSTATUS(status) != 0) {
    std::cout << "child exited with "
              << WEXITSTATUS(status) << std::endl;
    return false;
  }
  std::cout << "child exited with "
            << WEXITSTATUS(status) << std::endl;
  return true;
}


void deleteFiles(string &tLoc, string &compLoc, string &libcLoc, string &tpLoc,
              string &ipLoc, string &standardTLoc) {

  unlink(tLoc.c_str());
  unlink(compLoc.c_str());
  unlink(libcLoc.c_str());
  unlink(tpLoc.c_str());
  unlink(ipLoc.c_str());
  unlink(standardTLoc.c_str());

  // SPDLOG_INFO("files {} {} {} {} {} {}", tLoc, compLoc, tpLoc, ipLoc, libcLoc, standardTLoc);
}

struct partition getPart(string replay, u_int cores, u_int offset) {
  assert (offset < cores);
  return getParts(std::getPath(PART_CACHE), replay, cores)[offset];
}

void RunEpoch(const ::Steamdrill::SDQuery* commands,
              int cores,
              int offset,
              vector<string> outputs,
              string stats,
              string tag,
              char loglevel,
              JumpCounter jc) {

  string tracers = tag + TRACER_SUFFIX,
      compiled = tag + COMPILED_SUFFIX,
      libc = tag + LIBC_SUFFIX,
      tps = tag + TPS_SUFFIX,
      ips = tag + IPS_SUFFIX,
      standardTPs = tag + STPS_SUFFIX,
      assemb = tag + ASSEMB_SUFFIX;

  // first get files for everything:
  sm.addStamp("BuildCodeStart");
  auto tconfigs = Generate::generateSHQuery(commands, tracers, tps, std::getPath(TFS_BASE));

  // Next figure out the libc configuration
  Generate::generateLibcConfig(commands->replay(), libc);
  Generate::generateStandardTracepoints(commands->replay(), standardTPs);
  sm.addStamp("GenerateStop");
  // build tracerpoints correctly to be passed into this:
  sm.addCount("Blocks", Instrument::instrument(tconfigs, assemb, ips, commands->replay(), std::getPath(BLOCK_CACHE), jc));
  sm.addStamp("InstrumentStop");
  if (!compileTracers(tracers, tps, ips, compiled, assemb, commands->replay())) {
    throw "Couldn't compile the object";
  }
  sm.addStamp("BuildCodeStop");
  struct partition p = getPart(commands->replay(), cores, offset);
  sm.addStamp("GetPartsStop");

  cerr << "my part: " << p.start_clock << ", " << p.start_pid << ", " << p.stop_clock << std::endl;

  unique_ptr<SHSession::SHSession> s =
      std::make_unique<SHSession::SHSession>(commands->replay(),
                                             standardTPs,
                                             ips,
                                             tps,
                                             compiled,
                                             outputs,
                                             stats,
                                             (uint32_t)p.start_clock,
                                             (uint32_t)p.start_pid,
                                             (uint32_t)p.stop_clock,
					     (uint32_t)commands->exitclock(),
                                             (uint32_t)p.ckpt_clock,
					     loglevel,
                                             jc);

  //std::cerr << "bout ta do the epoch" << std::endl;
  (*s).doEpoch();
  (*s).wait();
  // the epoch is actually done BEFORE this point (SH itself alerts spark)
  sm.writeStamps(stats);
  (*s).stats();
}


void update_tracing_code(Steamdrill::SDExpression *col, unordered_map<string, string> tcdefs) {
  switch (col->exp_case())
  {
    case Steamdrill::SDExpression::kTc:
      col->mutable_tc()->set_functionname(tcdefs[col->tc().functionname()]);
      break;
    case Steamdrill::SDExpression::kMeta:
    case Steamdrill::SDExpression::kReg:
    case Steamdrill::SDExpression::kLit:
      break;
    case Steamdrill::SDExpression::kBce:
      update_tracing_code(col->mutable_bce()->mutable_e1(), tcdefs);
      update_tracing_code(col->mutable_bce()->mutable_e2(), tcdefs);
      break;
    case Steamdrill::SDExpression::kBoe:
      update_tracing_code(col->mutable_boe()->mutable_e1(), tcdefs);
      update_tracing_code(col->mutable_boe()->mutable_e2(), tcdefs);
      break;

    case Steamdrill::SDExpression::kMd:
      update_tracing_code(col->mutable_md()->mutable_operand(), tcdefs);
      break;
    case Steamdrill::SDExpression::kCat:
      update_tracing_code(col->mutable_cat()->mutable_expr(), tcdefs);
      break;
    case Steamdrill::SDExpression::kField:
      update_tracing_code(col->mutable_field()->mutable_expr(), tcdefs);
      break;
    default:
      assert (false);
      break;
  }
}


int main(int argc, char **argv) {
  args::ArgumentParser parser("Trace a replay process.", "A.R.Q.");
  args::Group group(parser, "Required arguments:", args::Group::Validators::All);
  args::Group opt(parser, "Optional arguments:", args::Group::Validators::DontCare);

  args::Positional<string> stats(group, "stats", "Path to write the stats");
  args::Positional<int> round(group, "round", "Which round this replay is doing");
  args::Positional<int> cores(group, "cores", "Number of cores used in full query");
  args::Positional<int> offset(group, "offset", "offset of this epoch in cores");
  args::Positional<string> tmpTag(group, "tmpLocation", "Prefix for temporary files");

  args::ValueFlag<char> loglevel(opt, "logLevel", "level of logging", {'l', "level"});
  args::ValueFlag<string> counterType(opt, "counterType", "type of jump coutner", {'c', "jumpCounter"});

  args::PositionalList<string> commandFiles(group, "commands", "Path to the commands");
  args::HelpFlag help(parser, "help", "Display help", {'h', "help"});

  sm.addStamp("EpochStart");
  try
  {
    parser.ParseCLI(argc, argv);
  }
  catch (args::Help)
  {
    std::cout << parser << std::endl;
    return 0;
  }
  catch (std::exception &e)
  {
    std::cerr << e.what();
    std::cout << parser << std::endl;
    return -1;
  }

  INFO("stats=%s round=%d cores=%d offset=%d\n",
       args::get(stats).c_str(), args::get(round), args::get(cores), args::get(offset));

  long exitClock = 0;
  vector<string> outputs;
  auto commands = new Steamdrill::SDQuery();
  {
    for (auto cmds : args::get(commandFiles)) {

      auto tmp = new Steamdrill::SDQuery();
      fstream input(cmds, ios::in | ios::binary);
      if (!tmp->ParseFromIstream(&input)) {
        fprintf(stderr, "cannot parse protobuf file!\n");
        return -1;
      }

      DEBUG("next commands: %s\n", tmp->DebugString().c_str());

      // don't remap these for now: (good luck!)
      unordered_map<string, string> tcdefs;
      for (auto trace_code : tmp->tracingcodedefs()) {
        tcdefs[trace_code.name()] = boost::str(boost::format{"%s_%d"} %trace_code.name() %outputs.size());
        auto tcd = commands->add_tracingcodedefs();
        tcd->CopyFrom(trace_code);
        tcd->set_name(tcdefs[trace_code.name()]);
      }

      //now find and replace all tcd instances?? (HOW)
      for (int r = 0; r < tmp->rels_size(); ++r) {
        auto rel = tmp->mutable_rels(r);

        for (int i = 0; i < rel->cols_size(); ++i) {

          auto col = rel->mutable_cols(i);
          update_tracing_code(col, tcdefs);
        }
        for (int i = 0; i < rel->filters_size(); ++i) {
          auto fil = rel->mutable_filters(i);
          update_tracing_code(fil, tcdefs);
        }
      }

      DEBUG("update com: %s\n", tmp->DebugString().c_str());


      if (commands->has_replay() && commands->replay() != tmp->replay()) {
        std::cerr << "I cannot mix these!!";
        std::cerr << commands->replay() << " vs. " << tmp->replay();
        return -EINVAL;
      }
      commands->set_replay(tmp->replay());

      if (commands->has_exitclock() && commands->exitclock() != tmp->exitclock()) {
        std::cerr << "I cannot mix these!!";
        std::cerr << commands->exitclock() << " vs. " << tmp->exitclock();
        return -EINVAL;
      }
      commands->set_exitclock(tmp->exitclock());

      // add in an output for this command:
      string output = cmds;
      //std::replace(output.begin(), output.end(), '/', '.');
      outputs.push_back(output + "-output");
      for (auto tmp_rel : tmp->rels()) {
        auto rel = commands->add_rels();
        rel->CopyFrom(tmp_rel);
        rel->set_output(outputs.size() - 1);
      }
    }
  }

  JumpCounter jc = DEFAULT;
  if (counterType && args::get(counterType) == "all")
    jc = INST_ALL;
  else if (counterType && args::get(counterType) == "opt")
    jc = INST_OPT;
  RunEpoch(commands,
           args::get(cores),
           args::get(offset),
           outputs,
           args::get(stats),
           args::get(tmpTag),
           loglevel ? args::get(loglevel) : 'e',
           jc);
  return 0;
}

