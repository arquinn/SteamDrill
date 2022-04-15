#include <cassert>
#include <vector>
#include <memory>

#include "args/args.hxx"

#include "../binary_parser/binary_parser.hpp"
#include "../binary_parser/elf_parser.hpp"
#include "../binary_parser/instruction_parser.hpp"
#include "../query_generator/function_code.hpp"

using std::vector;
using std::unique_ptr;


int main(int argc, char *argv[]) {

  args::ArgumentParser parser("Get info about all fxns defined in an ELF file.", "A.R.Q.");
  args::Group group(parser, "Required arguments:", args::Group::Validators::All);
  args::Positional<std::string> file(group, "cache_file", "The ELF executable in the cache file");
  args::HelpFlag help(parser, "help", "Display help", {'h', "help"});

  try {
    parser.ParseCLI(argc, argv);
  }
  catch (args::Help) {
    std::cout << parser << std::endl;
    return 0;
  }
  catch (args::ParseError e) {
    std::cerr << e.what() << std::endl << parser << std::endl;

    for (int i = 0; i < argc; ++i) {
      std::cerr << argv[i] << "\n";
    }
    return -1;
  }
  catch (args::ValidationError e) {
    std::cerr << e.what() << std::endl << parser << std::endl;
    for (int i = 0; i < argc; ++i) {
      std::cerr << argv[i] << "\n";
    }
    return -2;
  }

  // setup LLVM Module:
  std::vector<unique_ptr<Parser::FunctionInfo>> funcs;
  funcs = Parser::getFunctions(args::get(file).c_str());
  Parser::InstructionInfo ii(args::get(file).c_str());
  Parser::ElfInfo elf(args::get(file).c_str());

  Parser::SeqInstIter ssi;
  Parser::ElfSecInfo esi;
  u_long shortCount = 0, longCount = 0;
  uint32_t loaded = esi.loaded, offset = esi.offset, size= esi.size;
  for (auto &fi : funcs) {

    // just checking against 0 in the initializetion case:
    if (esi.loaded + esi.size <= fi->lowPC) {
      esi = elf.getSection(fi->lowPC);
      loaded = esi.loaded;
      offset = esi.offset;
      size= esi.size;
      ssi = ii.getSeqInstIter(offset, size, loaded);
    }

    if (!ssi.contains(fi->highPC)) {
      // this means that we don't have this whole function loaded... there are two options:
      // (1) reload the ssi.getInstructions starting from lowPC and see how higth we get
      // (2) reload the ssi.getInstructions with JUST the fxn

      // seems like once we're not linear disassembling we should give up, so I choose (2)
      // std::cerr << "ssi missing " << (*fiPrev)->name << " " << std::hex <<  (*fiPrev)->highPC << std::endl;

      uint32_t secOffset = fi->lowPC - loaded;
      ssi = ii.getSeqInstIter(offset + secOffset, fi->highPC - fi->lowPC + 1, fi->lowPC);
    }
    assert (!ssi.finished());
    ssi.getInstructions( fi->highPC, [&fi, &shortCount, &longCount](cs_insn inst) {
        if (inst.id == X86_INS_LCALL) {
          std::cerr << "long call at " << inst.address << std::endl;
          longCount++;
        }
        if (inst.id == X86_INS_CALL) {
          shortCount++;
        }
        if (inst.id == X86_INS_JMP) {
          auto dest = inst.detail->x86.operands[0];
          if (dest.type == X86_OP_IMM && dest.imm < inst.address) {
            std::cerr << std::hex << inst.address << " to " << dest.imm << std::endl;
          }

        }
      });
  }
  std::cerr << "near: " << shortCount << " long: " << longCount << std::endl;
  return 0;
}

