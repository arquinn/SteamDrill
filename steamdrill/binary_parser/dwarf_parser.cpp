#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>

extern "C"
{
#include <dwarf.h>
#include <elf.h>

//why are these at different spots..>? Ubuntu you confuse me!
#include <elfutils/libdw.h>
#include <elfutils/libdwfl.h>
#include <libelf.h>
}

#include <boost/optional.hpp>
#include <boost/format.hpp>

#include <deque>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <sstream>
#include <vector>
#include <unordered_map>


#include "dwarf_parser.hpp"
#include "variable.hpp"

#include "../utils/timer.hpp"
using boost::optional;
using boost::format;

using std::deque;
using std::make_unique;
using std::pair;
using std::string;
using std::stringstream;
using std::vector;
using std::unique_ptr;
using std::unordered_set;

string cFunc;
string cSource;
//#define DWARF_DEBUG_FUNC(...) DEBUG_FUNC(cFunc.c_str(), __VA_ARGS__)
#define DWARF_DEBUG_FUNC(x,...)
#define DWARF_DEBUG_ARGS(x,...)
// #define DWARF_DEBUG_FUNC(...) fprintf(stderr, __VA_ARGS__)
#define DEBUG(...) fprintf(stderr, __VA_ARGS__)

#define ASSERT(condition, message)\
  if (!condition) {                                                     \
    std::cerr << "Assertion `" #condition "`failed in "                 \
              << __FILE__  << " line " << __LINE__ << ": " << message << std::endl; \
  }                                                                     \
  assert (condition)


namespace Parser {

//forward decls
// unique_ptr<Type> parseBaseType(Dwarf_Die *cu);

/* Stub libdwfl callback, only the ELF handle already open is ever used.  */
static int
find_no_debuginfo (Dwfl_Module *mod __attribute__ ((unused)),
                   void **userdata __attribute__ ((unused)),
                   const char *modname __attribute__ ((unused)),
                   Dwarf_Addr base __attribute__ ((unused)),
                   const char *file_name __attribute__ ((unused)),
                   const char *debuglink_file __attribute__ ((unused)),
                   GElf_Word debuglink_crc __attribute__ ((unused)),
                   char **debuginfo_file_name __attribute__ ((unused)))
{
  return -1;
}
static void binaryOp(deque<string> &stack, const char *fmt) {
  //assert (("not enough elements on stack for " + cFunc,stack.size() >= 2));
  // wtf isn't this working here?
  ASSERT(stack.size() >= 2, "Not enough elements on stack for " + cFunc);
  std::string twoBack, back;
  back = stack.back();
  stack.pop_back();
  twoBack = stack.back();
  stack.pop_back();

  stack.push_back(boost::str(format{fmt}%twoBack %back));
}

static void unaryOp(deque<string> &stack, const char *fmt) {
  std::string curr = stack.back();
  stack.pop_back();
  stack.push_back(boost::str(format{fmt}%curr));
}


static unordered_map<u_int, string> numToRegMap = {{0, "eax"},
                                                   {1, "ecx"},
                                                   {2, "edx"},
                                                   {3, "ebx"},
                                                   {4, "esp"},
                                                   {5, "ebp"},
                                                   {6, "esi"},
                                                   {7, "edi"},
                                                   {9, "eflags"}};

//#define REG "reg"
//#define DEREF "deref"
// should use these ^
enum DwarfStackStatus {CONT, QUIT, DONE};

DwarfStackStatus updateStack(Dwarf_Op op,
                             deque<string> &stack,
                             VariableFunction *func,
                             string base = "") {
  string back, sback;

  // DWARF_DEBUG_FUNC("update stack %x\n", op.atom);
  switch(op.atom)
  {
    case DW_OP_reg0:
    case DW_OP_reg1:
    case DW_OP_reg2:
    case DW_OP_reg3:
    case DW_OP_reg4:
    case DW_OP_reg5:
    case DW_OP_reg6:
    case DW_OP_reg7:
    case DW_OP_reg9: {
      stack.push_back(func->getRegister(numToRegMap[op.atom - DW_OP_reg0]));
      break;
    }
    case DW_OP_reg8:
    case DW_OP_reg10:
    case DW_OP_reg11:
    case DW_OP_reg12:
    case DW_OP_reg13:
    case DW_OP_reg14:
    case DW_OP_reg15: {
      // no support for these (currently)
      return QUIT;
    }
    case DW_OP_breg0:
    case DW_OP_breg1:
    case DW_OP_breg2:
    case DW_OP_breg3:
    case DW_OP_breg4:
    case DW_OP_breg5:
    case DW_OP_breg6:
    case DW_OP_breg7: {
      stack.push_back(boost::str(
          format{"*(u_long*)(%s)"} %func->getRegister(numToRegMap[op.atom - DW_OP_breg0])));
      break;
    }
    case DW_OP_bregx: {
      stack.push_back(boost::str(
          format{"*(u_long*)(%s + %lld)"} %func->getRegister(numToRegMap[op.number]) %(int64_t)op.number2));
      break;
    }

    case DW_OP_fbreg: {
      if (base == "") return QUIT;
      stack.push_back(boost::str(
          format{"*(u_long*)(%lld + %s)"} %(int64_t)op.number %base));
      break;
    }
    case DW_OP_call_frame_cfa: {
      if (base == "")
        return QUIT;
      stack.push_back(base);
      break;
    }
    case DW_OP_implicit_value: {
      stack.push_back(boost::str(format{"%llx"}%op.number2));
      break;
    }
    case DW_OP_piece: {
      stack.push_back(boost::str(format{"p%llx"} %op.number));
      break;
    }
    case DW_OP_stack_value: {
      // do nothing to curr.
      // This should mean that we needn't dereference (?) (is this not the common case?)
      return DONE;
      break;
    }
    case DW_OP_bra:
    case DW_OP_GNU_entry_value: {
      return QUIT;
    }
    case DW_OP_addr:
      // this might be wrong --  (should you *always* deref?)
      stack.push_back(boost::str(format{"*(u_long*)0x%llx"}%op.number));
      break;
    case DW_OP_const1s:
    case DW_OP_const2s:
    case DW_OP_const4s:
    case DW_OP_const8s: {
      stack.push_back(boost::str(format{"%lld"}%(int64_t)op.number));
      break;
    }
    case DW_OP_const1u:
    case DW_OP_const2u:
    case DW_OP_const4u:
    case DW_OP_const8u: {
      stack.push_back(boost::str(format{"%llu"}%op.number));
     break;
    }
    case DW_OP_dup:
      stack.push_back(stack.back());
      break;
    case DW_OP_swap:
      back = stack.back(); stack.pop_back();
      sback = stack.back(); stack.pop_back();
      stack.push_back(back);
      stack.push_back(sback);
      break;

    case DW_OP_lit0:
    case DW_OP_lit1:
    case DW_OP_lit2:
    case DW_OP_lit3:
    case DW_OP_lit4:
    case DW_OP_lit5:
    case DW_OP_lit6:
    case DW_OP_lit7:
    case DW_OP_lit8:
    case DW_OP_lit9:
    case DW_OP_lit10:
    case DW_OP_lit11:
    case DW_OP_lit12:
    case DW_OP_lit13:
    case DW_OP_lit14:
    case DW_OP_lit15:
    case DW_OP_lit16:
    case DW_OP_lit17:
    case DW_OP_lit18:
    case DW_OP_lit19:
    case DW_OP_lit20:
    case DW_OP_lit21:
    case DW_OP_lit22:
    case DW_OP_lit23:
    case DW_OP_lit24:
    case DW_OP_lit25:
    case DW_OP_lit26:
    case DW_OP_lit27:
    case DW_OP_lit28:
    case DW_OP_lit29:
    case DW_OP_lit30:
    case DW_OP_lit31: {
      stack.push_back(boost::str(format{"%d"}%(op.atom - DW_OP_lit0)));
      break;
    }
    case DW_OP_shl:
      binaryOp(stack, "%s << %s");
      break;
    case DW_OP_shr:
      binaryOp(stack, "((unsigned int)%s) >> %s");
      break;
    case DW_OP_shra:
      binaryOp(stack, "%s >> %s");
      break;

    case DW_OP_and:
      binaryOp(stack, "%s & %s");
      break;
    case DW_OP_div:
      binaryOp(stack, "%s / %s");
      break;
    case DW_OP_minus:
      binaryOp(stack, "%s - %s");
      break;
    case DW_OP_mod:
      binaryOp(stack, "%s %% %s");
      break;
    case DW_OP_mul:
      binaryOp(stack, "%s * %s");
      break;
    case DW_OP_neg:
      unaryOp(stack, "-1 * (%s)");
      break;
    case DW_OP_not:
      unaryOp(stack, "!(%s)");
      break;
    case DW_OP_or:
      binaryOp(stack, "%s | %s");
      break;

    case DW_OP_plus:
      binaryOp(stack, "%s + %s");
      break;
    case DW_OP_xor:
      binaryOp(stack, "%s ^ %s");
      break;

    case DW_OP_eq:
      binaryOp(stack, "%s == %s");
      break;
    case DW_OP_ge: {
      binaryOp(stack, "%s >= %s");
      break;
    }
    case DW_OP_gt: {
      binaryOp(stack, "%s > %s");
      break;
    }
    case DW_OP_ne: {
      binaryOp(stack, "%s != %s");
      break;
    }
    case DW_OP_plus_uconst: {
      std::string curr = stack.back();
      stack.pop_back();
      stack.push_back(boost::str(format{"(%s+%llu)"}%curr %op.number));
      break;
    }

    case DW_OP_deref: {
      std::string curr = stack.back();
      stack.pop_back();
      stack.push_back(boost::str(format{"*(u_long*)(%s)"}%curr));
      break;
    }
    case DW_OP_deref_size: {
      std::string curr = stack.back();
      stack.pop_back();
      stack.push_back(boost::str(format{"*(u_long*)(%s)"}%curr));
      break;
    }

    default: {
      if (op.atom != DW_OP_GNU_implicit_pointer &&
          op.atom != DW_OP_GNU_push_tls_address &&
          !(op.atom >= DW_OP_reg21 && op.atom <= DW_OP_reg31)) {
        std::cerr << std::hex << "unhandeled dwarf op "
                  << (int) op.atom << std::endl;
      }
      return QUIT;
    }
  }

  return CONT;
  /*  if ((op.atom >= DW_OP_breg0 && op.atom <= DW_OP_breg7) ||
      op.atom == DW_OP_bregx)
  {
    std::string curr = stack.back();
    stack.pop_back();
    sprintf(code, "%llu + %s", op.number2, curr.c_str());
    stack.push_back(code);
  }
  */
}

static string calcDwarfOps(Dwarf_Op *ops,
                           size_t oplen,
                           const string base,
                           VariableFunction *func) {

  deque<string> stack;
  DwarfStackStatus stat;
  for (size_t i = 0; i < oplen; ++i)
    if ((stat = updateStack(ops[i], stack, func, base)) == QUIT)
        return "";

  if (stat == DONE || stack.size() == 1)
    return stack.back();

  assert (stack.size() > 1);

  // figure out total size:
  uint64_t totalBytes = 0, pieceBytes, shift = 0;
  for (auto piece : stack)
    if (sscanf(piece.c_str(), "p%llx", &pieceBytes) == 1)
      totalBytes += pieceBytes;

  std::string byteArr =
      func->addVariable(boost::str(format{"char[%lld]"}%totalBytes));

  while (stack.size())
  {
    std::string piece, oper, type, lhs;

    piece = stack.back(); stack.pop_back();
    if (stack.size() == 0)
      return "";

    if (sscanf(piece.c_str(), "p%llx", &pieceBytes) != 1)
      return "";

    type = boost::str(format{"uint%d_t"}%(pieceBytes*4));
    lhs = boost::str(format{"(%s*)(%s + %d)"} %type %byteArr %shift);

    oper = stack.back(); stack.pop_back();
    func->addAssignment(oper, type, lhs);
    shift += pieceBytes;
  }
  return byteArr;
}

uint64_t getDieUdata(Dwarf_Die *cu, int attrTag)
{
  Dwarf_Attribute attr;
  uint64_t size;
  int rc;

  rc = (int) dwarf_attr(cu, attrTag, &attr);
  assert (rc);

  rc = (int)dwarf_formudata(&attr, &size);
  assert (!rc);
  return size;
};

void getDieTypeRef(Dwarf_Die *cu, Dwarf_Die *rtn)
{
  Dwarf_Attribute attr;
  int rc;

  assert (dwarf_hasattr(cu, DW_AT_type));
  rc = (int) dwarf_attr(cu, DW_AT_type, &attr);
  assert (rc);
  rc = (int) dwarf_formref_die(&attr, rtn);
  assert (rc);
}

string getNamedType(Dwarf_Die *type) {

  int dtag = dwarf_tag(type);
  Dwarf_Die recurse;

  if (dwarf_hasattr(type, DW_AT_name))
    return dwarf_diename(type);

  if (dtag == DW_TAG_base_type) {
    assert (dwarf_hasattr(type, DW_AT_name));
    return dwarf_diename(type);
  }

  else if (dtag == DW_TAG_atomic_type ||
           dtag == DW_TAG_const_type ||
           dtag == DW_TAG_reference_type ||
           dtag == DW_TAG_typedef ||
           dtag == DW_TAG_volatile_type) {
    if (dtag == DW_TAG_reference_type) {
      if (dwarf_hasattr(type, DW_TAG_reference_type)) {
        std::cerr << "I can't handle reference_type" << std::endl;
        return "";
      }
    }
    if (dwarf_hasattr(type, DW_AT_type)) {
      getDieTypeRef(type, &recurse);
      return getNamedType(&recurse);
    }
  }
  else if (dtag == DW_TAG_array_type) {
    assert (dwarf_hasattr(type, DW_AT_type));
    getDieTypeRef(type, &recurse);
    return getNamedType(&recurse) + "*";
  }
  else if (dtag == DW_TAG_pointer_type) {
    if (dwarf_hasattr(type, DW_AT_type)) {
      getDieTypeRef(type, &recurse);
      string name = getNamedType(&recurse);
      return name.size() > 0 ? name + "*" : "";
    }
    else {
      // assume that 'type-free' pointers are void*s.
      return "void*";
    }
  }
  return "";
}

optional<VariableType> resolveType(Dwarf_Die &child)
{
  Dwarf_Die typeDie, curr = child;
  Dwarf_Attribute attr;
  int rc;
  uint64_t size = 0;
  bool resolved = false;
  deque<string> stack;

  do {
    rc = (int) dwarf_attr(&curr, DW_AT_type, &attr);
    if (rc == 0) break;
    rc = (int) dwarf_formref_die(&attr, &typeDie);
    if (rc == 0) break;

    rc = dwarf_peel_type(&typeDie, &curr);
    if (rc != 0) break;

    switch (dwarf_tag(&curr))
    {
      // treat arrays as pointers.
      case DW_TAG_pointer_type:
      case DW_TAG_array_type:
        stack.push_back("%s*");
        size = 4;
        break;
      case DW_TAG_union_type:
        //case DW_TAG_structure_type:
        return boost::none;
      case DW_TAG_const_type:
        // ignore const
        break;
      default:
        if (dwarf_hasattr(&curr, DW_AT_name)) {
          stack.push_back(dwarf_diename(&curr));
        }
        else {
          // from what I've seen, this happens b/c of a struct that is
          // only referenced through a typedef. So, I just ignore these
          // for now.
          return nullptr;
        }
        if (!size)
        {
          rc = (int) dwarf_attr(&curr, DW_AT_byte_size, &attr);
          if (!rc)
            std::cerr << "\tcannot get size?";
          else
            dwarf_formudata(&attr, &size);
        }
        resolved = true;
        break;
    }
  } while(!resolved);

  // hacky way to get a pointer w/out a type to resolve to void*
  std::string name = "void";
  while (stack.size() > 0)
  {
    std::string fmt = stack.back();
    stack.pop_back();
    if (fmt.find('%') != string::npos)
      name = boost::str(format(fmt) %name);
    else
      name = fmt;
  }
  if (size > 8)
    return boost::none;
  return VariableType((int)size, name);
  // typeDie = dwfl_addrdie(_dwfl, addr, NULL);
}



void resolveLocation(Dwarf_Die &child,
                     const string &type,
                     const string &varName,
                     const vector<BaseFxn> &baseFxns,
                     VariableFunction *func)
{
  string eip = func->getRegister("eip"), value;
  int rc;
  Dwarf_Attribute attr;
  ptrdiff_t offset = 0;
  Dwarf_Addr basep, startp, endp;
  Dwarf_Op *expr;
  size_t exprlen;
  bool done = false;


  rc = (int) dwarf_attr(&child, DW_AT_location, &attr);
  assert (rc != 0);
  while (!done && (offset = dwarf_getlocations(&attr, offset, &basep,
                                               &startp, &endp,
                                               &expr, &exprlen))){
    for (auto bs : baseFxns) {
      uint64_t low, high;

      DWARF_DEBUG_ARGS("get_location for base %llx %llx, %s\n", bs.start, bs.end, bs.fxn.c_str());
      if (startp > bs.end || bs.start > endp)
        continue;

      low = startp > bs.start ? startp : bs.start;
      high = endp < bs.end ? endp : bs.end;

      if (low != 0 || high != (Dwarf_Addr)-1) {
        func->startIf(boost::str(format{"%s>=0x%llx && %s<0x%llx"} %eip %low %eip %high));
      }
      DWARF_DEBUG_ARGS("dwarfOps from %d exprs\n", exprlen);
      value = calcDwarfOps(expr, exprlen, bs.fxn, func);
      DWARF_DEBUG_ARGS("dwarfOps %s=%s\n", varName.c_str(), value.c_str());

      //      std::cerr << "dwarfOps " << value << std::endl;
      if (value.size() > 0)
        func->addAssignment(value, type, varName);

      if (low != 0 || high != (Dwarf_Addr)-1) {
        func->endIf();
      }
    }
    done = startp == 0 && endp == (Dwarf_Addr)-1;
  }
}

unordered_set<Variable> getVariablesForTag(Dwarf_Die *function, string name, int tag) {
  Dwarf_Die child;
  unordered_set<Variable> variables;


  auto baseFoo = unique_ptr<VariableFunction>(new VariableFunction());

  // DWARF_DEBUG_FUNC("getVariablesForTagn %s %d\n", cFunc.c_str(), tag);

  if (dwarf_child(function, &child) != 0) {
    DWARF_DEBUG_FUNC("no kids\n");
    return variables;
  }
  vector<BaseFxn> baseFxns = BaseFxn::getBaseFunctions(name, function);
  do {
    if (dwarf_tag(&child) == tag) {
      if (!dwarf_hasattr(&child, DW_AT_type) ||
          !dwarf_hasattr(&child, DW_AT_location) ||
          !dwarf_hasattr(&child, DW_AT_name))  {
        continue;
        //return;
      }
      Variable var;
      string returnVar;
      Dwarf_Die type;

      var.name = dwarf_diename(&child);
      getDieTypeRef(&child, &type);

      var.type = getNamedType(&type);
      if (var.type.empty()) {
        if (dwarf_tag(&type) != DW_TAG_union_type) {
          // why are we just quitting with an unnamed type??
          DWARF_DEBUG_FUNC("unnamed type for %s in %s@%s\n",
                           var.name.c_str(), cFunc.c_str(), cSource.c_str());
        }
        continue;
      }
      VariableFunction func = *baseFoo.get();
      //
      returnVar = func.setType(var.type);

      // DWARF_DEBUG_FUNC("resolving %s %s\n", var.type.c_str(), var.name.c_str());
      resolveLocation(child, var.type, returnVar, baseFxns, &(func));

      var.location = func.toString();
      variables.insert(var);
    }
  } while(dwarf_siblingof(&child, &child) == 0);
  return variables;
}


unique_ptr<VariableFunction> getFunctionForTag(Dwarf_Die *function,
                                               string name,
                                               int tag,
                                               unordered_map<string,string> *typeMap = nullptr) {
  Dwarf_Die child;

  auto varFunc =
      unique_ptr<VariableFunction>(new VariableFunction());

  DWARF_DEBUG_FUNC("getFunctionsForTag %s %d\n", cFunc.c_str(), tag);

  if (dwarf_child(function, &child) != 0) {
    DWARF_DEBUG_FUNC("no kids\n");
    return nullptr;
  }
  vector<BaseFxn> baseFxns = BaseFxn::getBaseFunctions(name, function);
  DWARF_DEBUG_FUNC("base fxns %u\n", baseFxns.size());

  do {
    if (dwarf_tag(&child) == tag) {
      if (!dwarf_hasattr(&child, DW_AT_type) ||
          !dwarf_hasattr(&child, DW_AT_location) ||
          !dwarf_hasattr(&child, DW_AT_name)) {
        return nullptr;
      }

      std::string fpname, var, code, eip, value;

      fpname = dwarf_diename(&child);
      optional<VariableType> type = resolveType(child);
      if (!type) {
        DWARF_DEBUG_FUNC("cannot resolve type for %s\n", fpname.c_str());
        return nullptr;
      }
      if (typeMap != nullptr)
        typeMap->emplace(fpname, type.get().name);

      var = varFunc->addVariable(type->name);
      resolveLocation(child, type->name, var, baseFxns, varFunc.get());
      varFunc->addOutputVal(fpname, var); // this needs to go last!
    }
  } while(dwarf_siblingof(&child, &child) == 0);

  return varFunc;
}



bool DwLineParser::skipPrologue(Dwarf_Addr &entry, Dwarf_Addr low, Dwarf_Addr high) {

  //DEBUG("sp on %llx-%llx\n", low, high);

  while (cLine < nLines) {
    Dwarf_Addr addr;
    bool pend;
    Dwarf_Line *l = dwarf_onesrcline(lines, cLine);

    dwarf_lineprologueend(l, &pend);
    dwarf_lineaddr(l, &addr);

    if (addr > high) {
      return false;
    }
    else if (pend && addr >= low) {
      entry = addr;
      cLine++;
      // DEBUG("found prologue entry at %llx\n", entry);
      return true;
    }

    cLine++;
  }
  return false;

}

unique_ptr<FunctionInfo> parseFunction(Dwfl_Module *mod,
                                       Dwarf_Die *cu,
                                       Dwarf_CFI *cfi)
{
  auto fd = unique_ptr<FunctionInfo>(new FunctionInfo());

  int line, rc;
  const char *source_name, *function_name;
  char source[1024];
  Dwarf_Addr entryPc = 0, lowPc = 0, highPc = 0;

  Dwarf_Attribute linkageName, *arc, hpcAttr;

  // we ignore declarations
  if (dwarf_hasattr(cu, DW_AT_declaration))
    return nullptr;

  // TODO: ignoring inlines for now
  if (dwarf_func_inline(cu))
    return nullptr;

  // try entry & lowpcs -- some compilers produce one or the other --
  dwarf_entrypc (cu, &entryPc);
  dwarf_lowpc (cu, &lowPc);
  if (!lowPc && !entryPc) {
    std::cerr << " has neither low nor entry pcs" << std::endl;
    return nullptr;
  }

  if (!lowPc || ! entryPc)
    lowPc = entryPc = std::max(lowPc, entryPc);

  /*
   * If the defined function is linked to another function, then I
   * update the function's name with the linkage. Otherwise I take it
   * as is.
   */
  function_name = dwarf_diename(cu);
  arc = dwarf_attr(cu, DW_AT_linkage_name, &linkageName);
  if (arc != NULL) {
    function_name = dwarf_formstring(&linkageName);
  }
  cFunc = function_name;
  source_name = dwarf_decl_file(cu);
  cSource = source_name;
  dwarf_decl_line(cu, &line);
  sprintf(source, "%s:%d", source_name, line);

  arc = dwarf_attr(cu, DW_AT_high_pc, &hpcAttr);
  switch (dwarf_whatform(&hpcAttr)) {
    case DW_FORM_addr: {
      dwarf_formaddr(&hpcAttr, &highPc);
      break;
    }
    case DW_FORM_data1:
    case DW_FORM_data2:
    case DW_FORM_data4:
    case DW_FORM_data8: {
      Dwarf_Sword off = 0;
      dwarf_formsdata(&hpcAttr, &off);
      highPc = lowPc + off;
      break;
    }
    default: {
      std::cerr << "cannot parse highPc form "
               << dwarf_whatform(&hpcAttr) << std::endl;
    }
  }
  assert (highPc);

  // this must happen AFTER calculating the highPc


  // non-trivial (it looks like?)

  // this is a no go
  fd->parameters = getVariablesForTag(cu, function_name, DW_TAG_formal_parameter);
  fd->locals = getVariablesForTag(cu, function_name, DW_TAG_variable);

  fd->name = function_name;
  fd->source = source;

  fd->lowPC = lowPc;
  fd->highPC = highPc;

  return fd;
}

uint64_t getMemberCalculation(Dwarf_Op *ops, size_t oplen) {
  assert (oplen == 1); // for now (I think?)

  switch (ops[oplen].atom) {
    case DW_OP_const1s:
    case DW_OP_const2s:
    case DW_OP_const4s:
    case DW_OP_const8s:
    case DW_OP_const1u:
    case DW_OP_const2u:
    case DW_OP_const4u:
    case DW_OP_const8u:
      return ops[oplen].number;
    default:
      std::cerr << "cannot handle " << std::hex << (u_long)ops[oplen].atom << std::endl;
      assert (false);
      return 0;
  }
}


class dwflmod_args {
  //  std::vector<unique_ptr<T>> output;
 public:
  std::function<void(Dwfl_Module *,
                     Dwarf_Die*,

                     Dwarf_CFI *)> foo;
  std::function<void(DwLineParser&)> finishFoo;
};



int dwflmodIterateCUs(Dwfl_Module *dwflmod,
                      void **userdata __attribute__((unused)),
                      const char *name __attribute__((unused)),
                      Dwarf_Addr base __attribute__((unused)),
                      void *arg)
{
  auto oper = static_cast<dwflmod_args*>(arg);

  Dwarf_Addr dwbias; // what for?
  Dwarf_CFI *cfi;
  Dwarf *dbg;
  Dwarf_Die cu, cuIter;
  Dwarf_Off offset = 0, nxtOffset = 0;
  int rc;
  size_t cuHeaderLength = 0;

  cfi = dwfl_module_dwarf_cfi(dwflmod, &dwbias);
  if (cfi == NULL) {
    /*
     * Try again, this time with eh_frame  (.debug_frame is rarely defined)
     */
    cfi = dwfl_module_eh_cfi(dwflmod, &dwbias);
    if (cfi == NULL) {
      std::cerr << "Couldn't get cfi? " << dwfl_errmsg(-1) << std::endl;
      return -1;
    }
  }

  dbg = dwfl_module_getdwarf(dwflmod, &dwbias);
  if (dbg == NULL) {
    std::cerr << "Couldn't get dwarf? " << dwfl_errmsg(-1) << std::endl;
    return -1;
  }

  while (true)  {
    offset = nxtOffset;
    rc = dwarf_nextcu (dbg, offset, &nxtOffset, &cuHeaderLength, 0, 0, 0);
    if (rc < 0) {
      std::cerr << "nextcu fail? " << dwarf_errmsg(-1) << std::endl;
      assert (false);
    }
    else if (rc == 1) {
      //std::cerr << "Finished with CompUnits" << std::endl;
      break;
    }

    offset += cuHeaderLength;
    if (dwarf_offdie(dbg, offset, &cu) == NULL) {
      std::cerr << "Couldn't get CU? " << dwarf_errmsg(-1) << std::endl;
      assert (false);
      break;
    }

    if (dwarf_child(&cu, &cuIter) != 0) {
      continue;
    }

    do {
      oper->foo(dwflmod, &cuIter, cfi);
    } while (!dwarf_siblingof(&cuIter, &cuIter));

    size_t nlines = 0;
    Dwarf_Lines *lines = nullptr;
    rc = dwarf_getsrclines(&cu, &lines, &nlines);
    DwLineParser p(lines, nlines);
    if (oper->finishFoo)
      oper->finishFoo(p); //can I just do this?
  }

  return 0;
}

std::vector<unique_ptr<FunctionInfo>> DwInfo::getFunctions() {
  std::vector<unique_ptr<FunctionInfo>> funcs;
  struct dwflmod_args funcsPtr;

  funcsPtr.foo = [&funcs](Dwfl_Module *mod,
                          Dwarf_Die *cu,
                          Dwarf_CFI * cfi) {

    int dtag = dwarf_tag(cu);
    if (dtag == DW_TAG_subprogram ||
        dtag == DW_TAG_entry_point ||
        dtag == DW_TAG_inlined_subroutine) {
      unique_ptr<FunctionInfo> func = parseFunction(mod, cu, cfi);
      if (func)
        funcs.push_back(std::move(func));
    }
  };

  funcsPtr.finishFoo = [&funcs](DwLineParser &parser) {

    // sort and update:
    std::sort(funcs.begin(), funcs.end(),
              [] (const unique_ptr<FunctionInfo> &f,
                  const unique_ptr<FunctionInfo> &e) -> bool {return f->lowPC < e->lowPC;});
    for (auto &f : funcs) {
      Dwarf_Addr entryPc = 0;
      cFunc = f->name;
      if (parser.skipPrologue(entryPc, f->lowPC, f->highPC)) {
        DWARF_DEBUG_FUNC("adding %llx to %d\n", entryPc, f->entryPCs.size());

        assert (entryPc >= f->lowPC && entryPc <= f->highPC);
        f->entryPCs.insert(entryPc);
      }
    }
  };

  dwfl_getmodules(_dwfl, dwflmodIterateCUs, &funcsPtr, 0);
  return funcs;
}


std::unordered_set<Variable> DwInfo::getGlobals() {
  std::unordered_set<Variable> vars;

  struct dwflmod_args fPtr;
  fPtr.foo = [&vars, this](Dwfl_Module *mod, Dwarf_Die *cu,  Dwarf_CFI * cfi) {

    int dtag = dwarf_tag(cu);
    if (dtag == DW_TAG_variable && !dwarf_hasattr(cu, DW_AT_specification)) {
      // at this point we know that cu is a meaningful value...
      string name = dwarf_diename(cu);
      // get a bunch of info for this variable:
      Dwarf_Die type;
      std::flush(std::cerr);
      getDieTypeRef(cu, &type);

      string sType = getNamedType(&type);

      // I could *try* to interpret the location thing... but that seems hard.
      vars.emplace(name, _binName, "", sType, 0);
    }
  };
  fPtr.finishFoo = nullptr;

  dwfl_getmodules(_dwfl, dwflmodIterateCUs, &fPtr, 0);
  return vars;
}



DwInfo::DwInfo(std::string filename)
{
  _fd = -1;
  _binName = filename;
  // get the elf and check for some basic errors:
  if ((_fd = open(filename.c_str(), O_RDONLY)) < 0)
  {
    std::cerr << "Can't open " << filename
              << " err " << errno << std::endl;
    return;
  }

  /*
    Taken from readelf -- use libdwfl in trivial way to open
    the libdw handle for us
  */
  static Dwfl_Callbacks callbacks;
  callbacks.find_elf = NULL;
  callbacks.find_debuginfo = find_no_debuginfo;
  callbacks.section_address = dwfl_offline_section_address;
  callbacks.debuginfo_path = NULL;

  _dwfl = dwfl_begin(&callbacks);
  if (_dwfl == NULL)
  {
    std::cerr << "Can't begin dwfl " << dwfl_errmsg(-1) << std::endl;
    return;
  }

  // Might be somethinga bout next_offline_address..
  // tell dwfl to look at the file we're interested in
  dwfl_report_offline(_dwfl, filename.c_str(), filename.c_str(), _fd);
  dwfl_report_end(_dwfl, NULL, NULL);
}


unordered_map<string, vector<BaseFxn>> BaseFxn::cache;

vector<BaseFxn> BaseFxn::getBaseFunctions(string in, Dwarf_Die *die) {
  auto it = cache.find(in);
  if (it == cache.end()) {
    Dwarf_Attribute attr;
    Dwarf_Addr basep, startp, endp;
    Dwarf_Op *expr;
    ptrdiff_t offset = 0;
    size_t exprlen;
    int rc;
    auto baseFoo = unique_ptr<VariableFunction>(new VariableFunction());
    std::vector<BaseFxn> baseFxns;

    rc = (int)dwarf_attr(die, DW_AT_frame_base, &attr);
    if (rc != 0) {
      DWARF_DEBUG_FUNC("parsing frame_base!\n");
      while ((offset = dwarf_getlocations(&attr, offset, &basep,
                                          &startp, &endp,
                                          &expr, &exprlen))) {
        string value = calcDwarfOps(expr, exprlen, "", baseFoo.get());
        DWARF_DEBUG_FUNC("dwarfOps %s\n", value.c_str());
        BaseFxn bf(startp, endp, value);
        baseFxns.push_back(bf);
      }
    }
    else {
      DWARF_DEBUG_FUNC("no base -- %s\n", dwarf_errmsg(-1));
    }
    it = cache.emplace(in, baseFxns).first;
  }
  return it->second;
}

/*
std::vector<unique_ptr<Type>> DwInfo::getTypes() {
  std::vector<unique_ptr<Type>> typeV;
  std::unordered_map<std::string, unique_ptr<Type>> types;

  struct dwflmod_args fPtr;

  fPtr.foo = [&types](Dwfl_Module *mod,
                      Dwarf_Die *cu,
                      DwLineParser &parser,
                      Dwarf_CFI * cfi) {

    int dtag = dwarf_tag(cu);
    if (dtag == DW_TAG_base_type ||
        dtag == DW_TAG_const_type ||
        dtag == DW_TAG_pointer_type ||
        dtag == DW_TAG_reference_type ||
        dtag == DW_TAG_structure_type ||
        dtag == DW_TAG_typedef ||
        dtag == DW_TAG_union_type ||
        dtag == DW_TAG_volatile_type ||
        dtag == DW_TAG_enumeration_type) {
      unique_ptr<Type> t = parseType(mod, cu, parser, cfi);
      if (t) {
        types.emplace(t->name, std::move(t));
      }
    }
  };
  dwfl_getmodules(_dwfl, dwflmodIterateCUs, &fPtr, 0);
  typeV.resize(types.size());

  //  assert (types["rtld_global_ro"]);
  for (auto &p : types) {
    auto ptr = dynamic_cast<PointerType*>(p.second.get());
    if (ptr && ptr->ptype && ptr->ptype->name != "") {
      auto it = types.find(ptr->ptype->name);
      if (it != types.end())
        ptr->ptype.reset(it->second->clone());
    }
  }

  std::transform(types.begin(), types.end(), typeV.begin(),
                 [](std::pair<const string, unique_ptr<Type>> &p)
                 { return std::move(p.second); });

  return typeV;
}


void parseCommonType(Dwarf_Die *cu, Type &type) {
  if (dwarf_hasattr(cu, DW_AT_name))
    type.name = dwarf_diename(cu);

  if (dwarf_hasattr(cu, DW_AT_byte_size))
    type.size = 4 * getDieUdata(cu, DW_AT_byte_size);
}

void parseCommonType(Dwarf_Die *cu, Type *type)
{
  parseCommonType(cu, *type);
}


unique_ptr<Type> parseBaseType(Dwarf_Die *cu)
{
  uint64_t encoding;

  Type base;
  parseCommonType(cu, base);
  auto type = unique_ptr<BaseType>(new BaseType(base));

  assert (dwarf_hasattr(cu, DW_AT_encoding));
  encoding = getDieUdata(cu, DW_AT_encoding);
  switch (encoding)
  {
    case DW_ATE_boolean:
      type->type = BuiltinType::BOOL;
      break;
    case DW_ATE_signed:
      type->type = BuiltinType::INT;
      break;
    case DW_ATE_signed_char:
      type->type = BuiltinType::CHAR;
      break;
    case DW_ATE_unsigned:
      type->type = BuiltinType::UINT;
      break;
    case DW_ATE_unsigned_char:
      type->type = BuiltinType::UCHAR;
      break;
    case DW_ATE_float:
      type->type = BuiltinType::FLOAT;
      break;
    default:
      std::cerr << "Unsupported type encoding "  << encoding << std::endl;
  }
  return std::move(type);
}

unique_ptr<Type>
parseType(Dwfl_Module *mod, Dwarf_Die *cu, DwLineParser &parser, Dwarf_CFI *cfi) {
  int rc, dtag = dwarf_tag(cu);
  Dwarf_Attribute attr;
  Dwarf_Die childCU;

  if (dtag == DW_TAG_base_type)
    return parseBaseType(cu);
  else if (dtag == DW_TAG_atomic_type ||
           dtag == DW_TAG_const_type ||
           dtag == DW_TAG_reference_type ||
           dtag == DW_TAG_typedef ||
           dtag == DW_TAG_volatile_type) {
    if (dtag == DW_TAG_reference_type) {
      if (dwarf_hasattr(cu, DW_TAG_reference_type)) {
        std::cerr << "I can't handle reference_type" << std::endl;
      }
    }
    if (!dwarf_hasattr(cu, DW_AT_type)) {
      return nullptr;
    }

    getDieTypeRef(cu, &childCU);
    auto type = parseType(mod, &childCU, parser, cfi);
    if (!type) return type;

    // update with this modification
    parseCommonType(cu, type.get());
    return std::move(type);
  }

  else if (dtag == DW_TAG_array_type) {
    getDieTypeRef(cu, &childCU);
    auto type = parseType(mod, &childCU, parser, cfi);
    auto bt = dynamic_cast<BaseType*>(type.get());
    if (bt && bt->type == BuiltinType::CHAR) {
      bt->type = BuiltinType::STRING;
      return type;
    }

    auto arr = unique_ptr<ArrayType>(new ArrayType());
    parseCommonType(cu, arr.get());
    arr->ptype = std::move(type);
    if (arr->name == "" && arr->ptype->name != "")
      arr->name = arr->ptype->name + "*";
    return std::move(arr);
  }
  else if (dtag == DW_TAG_pointer_type) {
    if (dwarf_hasattr(cu, DW_AT_type)) {
      rc = (int) dwarf_attr(cu, DW_AT_type, &attr);
      assert (rc);
      rc = (int) dwarf_formref_die(&attr, &childCU);
      assert (rc);
      auto type = unique_ptr<Type>(new Type());
      parseCommonType(&childCU, type.get());

      auto ptr = unique_ptr<PointerType>(new PointerType());
      ptr->ptype = std::move(type);

      parseCommonType(cu, ptr.get());
      if (ptr->name == "" && ptr->ptype->name != "")
        ptr->name = ptr->ptype->name + "*";
      else if (ptr->name == "") {
        ptr->name = "void*";
        ptr->ptype = nullptr;
      }

      return std::move(ptr);
    }
    else {
      // assume that 'type-free' pointers are void*s.
      auto ptr = unique_ptr<PointerType>(new PointerType());
      ptr->name = "void*";
      return std::move(ptr);
    }
  }
  else if (dtag == DW_TAG_enumeration_type) {
    auto et = unique_ptr<EnumerationType>(new EnumerationType());
    parseCommonType(cu, et.get());

    dwarf_child(cu, &childCU);
    do {
      std::string name = dwarf_diename(&childCU);
      u_long value = ULONG_MAX;
      if (dwarf_hasattr(&childCU, DW_AT_const_value))
        value = getDieUdata(&childCU, DW_AT_const_value);

      et->types.push_back(std::make_pair(name, value));

    } while(!dwarf_siblingof(&childCU, &childCU));

    return std::move(et);
  }

  else if (dtag == DW_TAG_union_type) {
    auto ut = unique_ptr<UnionType>(new UnionType());
    // unions have memebers (like structs!)
    if (dwarf_hasattr(cu, DW_AT_name))
      ut->name = dwarf_diename(cu);

    if (dwarf_child(cu, &childCU)) {
      std::cerr << "no kids! " << ut->name << " " << dwarf_cuoffset(cu) << std::endl;
      assert (false);
    }

    std::cerr << std::hex << "union " << ut->name <<  " @ " << dwarf_cuoffset(cu) << std::endl;

    do {
      if (dwarf_tag(&childCU) != DW_TAG_member)
          continue;
      std::string name = "";
      if (dwarf_hasattr(&childCU, DW_AT_name)) {
        name = dwarf_diename(&childCU);
      }
      Dwarf_Die type;
      getDieTypeRef(&childCU, &type);

      std::cerr << std::hex << ut->name << " memba " << name << std::endl;

      auto fieldType = parseType(mod, &type, parser, cfi);
      if (!fieldType) {
        std::cerr << ut->name << " gave up b/c of " << name << std::endl;
        return nullptr;
      }
      std::cerr << ut->name <<":" << name << " type " << fieldType->name << std::endl;
      uint64_t offset = 0;
      if (dwarf_hasattr(&childCU, DW_AT_data_bit_offset)) {
        offset = getDieUdata(cu, DW_AT_data_bit_offset);
      }
      else if (dwarf_hasattr(&childCU, DW_AT_data_member_location)) {
        Dwarf_Op *expr;
        size_t exprlen;

        rc = (int)dwarf_attr(&childCU, DW_AT_data_member_location, &attr);
        assert (rc);
        switch (dwarf_whatform(&attr)) {
          case DW_FORM_data1:
          case DW_FORM_data2:
          case DW_FORM_data4:
          case DW_FORM_data8: {
            Dwarf_Sword off = 0;
            dwarf_formsdata(&attr, &off);
            offset = off;
            break;
          }
          default: {
            rc = dwarf_getlocation(&attr, &expr, &exprlen);
            assert (!rc);
            offset = getMemberCalculation(expr, exprlen);
          }
        }
      }
      ut->types.emplace_back(name, fieldType.release(), offset);
    } while(!dwarf_siblingof(&childCU, &childCU));


    return std::move(ut);
  }

  else if (dtag == DW_TAG_structure_type) {
    if (dwarf_hasattr(cu, DW_AT_declaration))
      return nullptr;

    auto st = unique_ptr<StructType>(new StructType());
    if (dwarf_hasattr(cu, DW_AT_name))
      st->name = dwarf_diename(cu);

    if (dwarf_child(cu, &childCU)) {
      std::cerr << "no kids! " << st->name << " " << dwarf_cuoffset(cu) << std::endl;
      assert (false);
    }

    std::cerr << std::hex << "struct " << st->name <<  " @ " << dwarf_cuoffset(cu) << std::endl;

    do {
      if (dwarf_tag(&childCU) != DW_TAG_member)
          continue;
      std::string name = "";
      if (dwarf_hasattr(&childCU, DW_AT_name)) {
        name = dwarf_diename(&childCU);
      }
      Dwarf_Die type;
      getDieTypeRef(&childCU, &type);

      std::cerr << std::hex << st->name << " memba " << name << std::endl;

      auto fieldType = parseType(mod, &type, parser, cfi);
      if (!fieldType) {
        std::cerr << st->name << " gave up b/c of " << name << std::endl;
        return nullptr;
      }

      std::cerr << " type " << fieldType->name << std::endl;

      uint64_t offset = 0;
      if (dwarf_hasattr(&childCU, DW_AT_data_bit_offset)) {
        offset = getDieUdata(cu, DW_AT_data_bit_offset);
      }
      else if (dwarf_hasattr(&childCU, DW_AT_data_member_location)) {
        Dwarf_Op *expr;
        size_t exprlen;

        rc = (int)dwarf_attr(&childCU, DW_AT_data_member_location, &attr);
        assert (rc);
        switch (dwarf_whatform(&attr)) {
          case DW_FORM_data1:
          case DW_FORM_data2:
          case DW_FORM_data4:
          case DW_FORM_data8: {
            Dwarf_Sword off = 0;
            dwarf_formsdata(&attr, &off);
            offset = off;
            break;
          }
          default: {
            rc = dwarf_getlocation(&attr, &expr, &exprlen);
            assert (!rc);
            offset = getMemberCalculation(expr, exprlen);
          }
        }
      }
      else {
        std::cerr << st->name << " gave up" << std::endl;
        return nullptr;
      }

      st->fields.emplace_back(name, fieldType.release(), offset);
    } while(!dwarf_siblingof(&childCU, &childCU));

    std::cerr << st->name << " returning" << std::endl;
    return std::move(st);
  }
  else
    return nullptr;
}

*/

} /* end namespace Parser */
