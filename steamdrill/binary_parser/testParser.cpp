#include <iostream>

#include "binary_parser.hpp"

using std::cerr;
using std::cout;

using Parser::ObjectDefinition;

int main(int argc, char **argv )
{
  if (argc < 2)
    cerr << "I need some arguments!";

  cout << "searching for " << argv[1];
  for (auto &f : Parser::getFunctions(argv[1] , {}))
  {
    std::cout << std::hex << f->name << ", " << f->lowPC << std::endl;
  }
}
