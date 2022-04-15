#include <vector>

#include <boost/optional.hpp>
#include <clang-c/Index.h>

#include "binary_parser.hpp"
#include "../protos/steamdrill.pb.h"

#ifndef __HEADER_PARSER__
#define __HEADER_PARSER__

namespace Parser {

class HeaderParser {
  typedef std::vector<FunctionInfo>::iterator HeaderIterator;
 private:
  std::vector<FunctionInfo> _funcs;

  static boost::optional<Parser::FunctionInfo>
  getFunctionInfo(CXCursor func);

 public:
  HeaderParser(std::string headerName);

  std::vector<FunctionInfo>::iterator begin()
  {
    return _funcs.begin();
  };
  std::vector<FunctionInfo>::iterator end()
  {
    return _funcs.end();
  };
};
}
#endif /* __HEADER_PARSER__ */
