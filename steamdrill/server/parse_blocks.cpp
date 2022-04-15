
#include <sys/types.h>

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <cstring>

#include <iostream>
#include <string>

#include "args.hxx"
#include "../instrument/block_info.hpp"

using std::string;

int main(int argc, char **argv) {
  args::ArgumentParser parser("Trace a replay process.", "A.R.Q.");
  args::Group group(parser, "Required arguments:", args::Group::Validators::All);

  args::Positional<string> file(group, "blocks", "blocks to dump.");
  args::Positional<int> increment(group, "increment", "an integer to increment start by");
  args::HelpFlag help(parser, "help", "Display help", {'h', "help"});

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

  int fd = open(args::get(file).c_str(), O_RDONLY);
  if (fd < 0) {
    fprintf(stderr, "Cannot open %s, error %s\n", args::get(file).c_str(), strerror(errno));
    return -1;
  }

  struct blockInfo bi;
  int rc;
  while ( (rc = read(fd, &bi, sizeof(struct blockInfo))) == sizeof(struct blockInfo)) {
    printf("blockinfo: [%x,%x), un=%s, jm=%s, ujm=%s, unfjm=%s\n",
           bi.start + args::get(increment),
           bi.end + args::get(increment),
           bi.unique ? "true" : "false",
           bi.jump ? "true" : "false",
	   bi.uncondJump ? "true" : "false",
	   bi.notForwardUncondJump ? "true" : "false");
  }
}
