#include <cassert>
#include "tracer_configuration.hpp"

int main(int argc, char **argv)
{

  assert (argc >= 2);

  std::string filename = argv[1];

  Configuration::parseTPoints(filename);


  return 0;
}
