#include <cassert>

#include <sstream>
#include <iostream>

#include <boost/optional.hpp>
#include <clang-c/Index.h>

#include "binary_parser.hpp"
#include "header_parser.hpp"

class unknownName {
 private:
  int _offset;
  std::string _base;
 public:
  unknownName(std::string base) {
    _base = base;
    _offset = 1;
  };

  std::string getNext() {
    std::string rtn = _base + std::to_string(_offset);
    ++_offset;
    return rtn;
  };
};

std::string convertString(const CXString& s)
{
  std::string str = clang_getCString(s);
  clang_disposeString(s);
  return str;
}


std::ostream& operator<<(std::ostream& os, const CXString& str)
{
  return os << convertString(str);
}

Steamdrill::SDExpression *getIntColumn(int i)
{
  Steamdrill::SDExpression *offset = new Steamdrill::SDExpression();
  Steamdrill::Column *c = offset->mutable_c();
  c->set_type(Steamdrill::Column::INTEGER);
  c->set_ival(i);
  return offset;
}

boost::optional<Parser::FunctionInfo>
Parser::HeaderParser::getFunctionInfo(CXCursor func)
{
  unknownName uname("param");
  std::string name = convertString(clang_getCursorSpelling(func));
  CXType funcType = clang_getCursorType(func);
  enum CXCallingConv cConv = clang_getFunctionTypeCallingConv(funcType);
  std::stringstream parameters;
  Parser::FunctionInfo fd;

  if (cConv != CXCallingConv_C)
  {
    std::cerr << "Hmm... I don't get this calling convetion "
              << name << " " << cConv << std::endl;
  }
  fd.name = name;

  /*
   * In cDecl, the stack is assumed to be like:
   *   param(n)
   *   param(n-1)
   *   ...
   *   param(1)
   * return Address <- esp
  */
  Steamdrill::SDExpression *esp = new Steamdrill::SDExpression();
  Steamdrill::Column *c = esp->mutable_c();
  c->set_type(Steamdrill::Column::REGISTER);
  c->set_val("esp");
  int offset = 4;

  int numParams = clang_Cursor_getNumArguments(func);
  for (int i = 0; i < numParams; ++i)
  {
    CXCursor arg = clang_Cursor_getArgument(func, i);
    CXType argType = clang_getArgType(funcType, i);
    std::string argName = convertString(clang_getCursorSpelling(arg));
    std::string aTypeName = convertString(clang_getTypeSpelling(argType));

    switch(argType.kind)
    {
      case CXType_Invalid: {
        std::cerr << "Invalid Type cannot be parsed" << std::endl;
        assert(false);
      }
      case CXType_Typedef:
        argType = clang_getCanonicalType(argType);
        // Intentional fall through:
      case CXType_Long:
      case CXType_LongLong:
      case CXType_LongDouble:
      case CXType_Double:
      case CXType_Pointer:
      case CXType_Int:
      case CXType_UInt:
      case CXType_ConstantArray: {
        Steamdrill::SDExpression *parse = new Steamdrill::SDExpression();
        Steamdrill::AddExpression *ae = parse->mutable_ae();
        ae->set_allocated_e1(esp);
        ae->set_allocated_e2(getIntColumn(offset));
        fd.parameters.emplace(argName, parse);
        break;
      }
      default:
        std::cerr << "Can't handle function " << name
                  << " b/c of unknown cxtype "
                  << clang_getTypeKindSpelling(argType.kind) << std::endl;
        return boost::none;
    }
    long long argSize = clang_Type_getSizeOf(argType);
    if (argSize == CXTypeLayoutError_Dependent ||
        argSize == CXTypeLayoutError_Incomplete)
    {
      std::cerr << "Can't handle function " << name
                << " b/c of type " << aTypeName << std::endl;
      return boost::none;
    }
    offset += argSize;
    if (i != 0)
      parameters << ", ";
    parameters << aTypeName << " " << argName << "[" << argSize << "]";

  }
  std::cout << name << "(";
  std::cout << parameters.str();
  std::cout << ")" << std::endl;
  return fd;
}


Parser::HeaderParser::HeaderParser(std::string headerName)
{
  CXIndex index = clang_createIndex(0, 0);
  CXTranslationUnit unit = clang_parseTranslationUnit(
    index,
    headerName.c_str(), nullptr, 0,
    nullptr, 0,
    CXTranslationUnit_None);
  if (unit == nullptr)
  {
    std::cerr << "cannot parse " << headerName << std::endl;
    return;
  }

  CXCursor cursor = clang_getTranslationUnitCursor(unit);
  clang_visitChildren(
    cursor,
    [](CXCursor c, CXCursor parent, CXClientData client_data)
    {
      if (clang_getCursorKind(c) == CXCursor_FunctionDecl ) {
        auto funcs = static_cast<
          std::vector<Parser::FunctionInfo>*>(client_data);
        auto fd = getFunctionInfo(c);
        if (fd)
          funcs->push_back(fd.get());
      }
      return CXChildVisit_Continue;
    },
    &_funcs);

  clang_disposeTranslationUnit(unit);
  clang_disposeIndex(index);
}



