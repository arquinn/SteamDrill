#include "tracer_configuration.hpp"
#include "args.hxx"

using std::string;

int main(int argc, char *argv[]) {
  args::ArgumentParser parser("Determine a tracing parsing problem");
  args::Group group(parser, "Required arguments:", args::Group::Validators::All);

  args::Positional<string>
      file(group, "tpoints file", "where to store the stats for this trace");
  args::HelpFlag help(parser, "help", "Display help", {'h', "help"});

  try {
    parser.ParseCLI(argc, argv);
  }
  catch (args::Help) {
    std::cout << parser << std::endl;
    return 0;
  }
  catch (args::ParseError e) {
    std::cout << parser << std::endl;
    return -1;
  }
  catch (args::ValidationError e) {
    std::cout << parser << std::endl;
    return -2;
  }


  Configuration::parseTPoints(args::get(file));
  return 0;
}
