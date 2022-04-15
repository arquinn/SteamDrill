#include <climits>

#include "args.hxx"

#include "staticLibrary.hpp"

#include "../binary_parser/binary_parser.hpp"
#include "../binary_parser/elf_parser.hpp"
#include "../binary_parser/instruction_parser.hpp"
#include "../query_generator/function_code.hpp"

using std::pair;

std::string regToString(x86_reg reg) {
  switch (reg) {
    case X86_REG_AH:
    case X86_REG_AL:
    case X86_REG_AX:
    case X86_REG_EAX:
      return "eax";
    case X86_REG_BH:
    case X86_REG_BL:
    case X86_REG_BX:
    case X86_REG_EBX:
      return "ebx";
    case X86_REG_CH:
    case X86_REG_CL:
    case X86_REG_CX:
    case X86_REG_ECX:
      return "ecx";
    case X86_REG_DH:
    case X86_REG_DL:
    case X86_REG_DX:
    case X86_REG_EDX:
      return "edx";
    case X86_REG_EDI:
      return "edi";
    case X86_REG_ESI:
      return "esi";
    case X86_REG_EBP:
      return "ebp";
    case X86_REG_ESP:
      return "esp";
    default:
      std::cerr << "not sure what to do with this reg " << reg << std::endl;
      return "";
  }
}

std::string memToFunc(x86_op_mem &mem) {
  Parser::VariableFunction func;
  std::stringstream assign;

  string ret = func.setType("u_long");

  assign << (mem.base == X86_REG_INVALID ? "0" : func.getRegister(regToString(mem.base)));
  assign << " + (" << (mem.index == X86_REG_INVALID ? "0" : func.getRegister(regToString(mem.index)))
         << " * " << mem.scale << ")";
  assign << " + " << mem.disp;


  if (mem.segment != X86_REG_INVALID) {
    // I Think we just like, add the segment register? for now.. IGNORE
    return "";
    // assign << " + " << func.getRegister(regToString(mem.segment));
  }

  func.addAssignment(assign.str(), "u_long", ret);
  return func.toString();
}

std::string regToFunc(x86_reg reg) {
  Parser::VariableFunction func;
  std::stringstream assign;

  string ret = func.setType("u_long");
  switch (reg) {
    case X86_REG_AH: case X86_REG_AL: case X86_REG_AX:
      reg = X86_REG_EAX;
      break;
    case X86_REG_BH: case X86_REG_BL: case X86_REG_BX:
      reg = X86_REG_EBX;
      break;
    case X86_REG_CH: case X86_REG_CL: case X86_REG_CX:
      reg = X86_REG_ECX;
      break;
    case X86_REG_DH: case X86_REG_DL: case X86_REG_DX:
      reg = X86_REG_EDX;
      break;
  }
  assign << std::hex << reg;

  func.addAssignment(assign.str(), "u_long", ret);
  return func.toString();
}

void dumpInstructions(Parser::InstructionInfo &ii, u_long offset, u_long low, u_long high, std::string cache) {

  std::cerr << "dumping " << offset << " from " << low << " size " << high - low << std::endl;
  auto ssi = ii.getSeqInstIter(offset, high - low, low);
  assert (!ssi.finished());
  ssi.getInstructions(high, [cache](cs_insn inst) {

      cs_detail *detail = inst.detail;
      std::vector<string> reads, writes;

      // shouldn't I care about more than registers...??
      for (int i = 0; i < detail->x86.op_count; ++i) {
        auto operand = detail->x86.operands[i];
        string val = "";

        if (operand.type == X86_OP_REG) {
          val = regToFunc(operand.reg);
        }
        else if (operand.type == X86_OP_MEM) {
          val = memToFunc(operand.mem);
        }

        if (!val.empty() && (operand.access | CS_AC_READ)) {
          reads.push_back(val);
        }
        if (!val.empty() && (operand.access | CS_AC_WRITE)) {
          writes.push_back(val);
        }
      }

      StaticLibrary::addOutput(inst.address);
      StaticLibrary::addOutput(inst.mnemonic);
      StaticLibrary::addCollection(reads);
      StaticLibrary::addCollection(writes);
      StaticLibrary::endRow();
    });
}

int main(int argc, char *argv[]) {

  args::ArgumentParser parser("Get info about the instructions defined in an ELF file.", "A.R.Q.");
  args::Group group(parser, "Required arguments:", args::Group::Validators::All);
  args::Positional<std::string> file(group, "cache_file", "The cached ELF executable");
  args::Positional<std::string> lowPC(parser, "lowPC", "The LowPC of the region to disassemb", "0");
  args::Positional<std::string> highPC(parser, "highPC", "The HighPC of the region to dissasemb", "0xffffffff");
  args::Positional<std::string> unused(parser, "unused", "Ostensibly for the stats file");
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
  auto cache = args::get(file);

  Parser::InstructionInfo ii(cache.c_str());
  Parser::ElfInfo elf(cache.c_str());

  auto low = stoul(args::get(lowPC));
  auto high = stoul(args::get(highPC));

  std::cerr << std::hex << "low " << low << " " << high << std::endl;

  if (low > 0 && high < INT_MAX) {
    auto esi = elf.getSection(low);
    dumpInstructions(ii, esi->offset + low - esi->loaded, low, high,  cache);
  }
  else {
    for (auto sec : elf.getSections()) {
      if ((sec.flags & SHF_EXECINSTR) && (sec.flags & SHF_ALLOC)) {
        dumpInstructions(ii, sec.offset, sec.loaded, sec.size + sec.loaded, cache);
      }
    }
  }
  return 0;
}

