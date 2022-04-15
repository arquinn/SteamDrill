#include <iostream>
#include "args.hxx"

#include "partition_manager.hpp"

int main(int argc, char **argv) {
  args::ArgumentParser parser("Trace a replay process.", "A.R.Q.");
  args::Group group(parser, "Required arguments:", args::Group::Validators::All);
  args::Group opt(parser, "Optional arguments:", args::Group::Validators::DontCare);

  args::Positional<std::string> replay(group, "stats", "replay path");
  args::Positional<int> cores(group, "count", "Number of cores");
  args::Positional<int> end_clock(opt, "clock", "When should we terminate the replay?");
  args::Flag watchdog(opt, "watchdog", "Assume the replay has a watchdog?", {'w', "watchdog"});


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

  int end = end_clock ? args::get(end_clock) : 0;
  addParts("parts", args::get(replay), args::get(cores), watchdog, end);
  return 0;
}
