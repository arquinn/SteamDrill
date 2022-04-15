/* test1.c */

#include <iostream>

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <inttypes.h>
#include <assert.h>

#include "instruction_parser.hpp"

int main(int argc, char **argv)
{
  csh handle;
  cs_insn *insn;
  size_t count;

  if (argc < 3)
  {
    std::cerr << "You didn't give me enough params!" << std::endl;
    return -1;
  }

  Parser::InstructionInfo ii(argv[1]);
  //  ii.getReturns(0, strtoul(argv[2], NULL, 0));

  return 0;
}
