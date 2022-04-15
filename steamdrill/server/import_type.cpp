#include <cassert>

#include <iostream>
#include <sstream>
#include <string>

#include "args.hxx"
#include "sqlite3.h"


using std::string;
using std::stringstream;

int main(int argc, char **argv) {
  args::ArgumentParser parser("Improt a type to the typedb.", "A.R.Q.");
  args::Group group(parser, "Required arguments:", args::Group::Validators::All);
  args::Group opt(parser, "Optional arguments:", args::Group::Validators::DontCare);

  args::Positional<string> path(group, "dbname", "Path to db");
  args::Positional<string> name(group, "name", "Name of the type");
  args::Positional<string> def(opt, "def", "type definition.. if applicable?");
  args::Positional<string> format(opt, "output format", "Name of output fxn");

  args::HelpFlag help(parser, "help", "Display help", {'h', "help"});

  sqlite3 *db;
  int rc;
  char *errMsg = nullptr, *sql = nullptr;

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

  rc = sqlite3_open(args::get(path).c_str(), &db);
  if (rc) {
    std::cerr << "can't open db" << sqlite3_errmsg(db) << std::endl;
    sqlite3_close(db);
    assert(false);
  }

  stringstream typeBuf;
  stringstream argList;

  typeBuf << "name";
  argList << "\'" << args::get(name) << "\'";
  if (def) {
    typeBuf << ", Def";
    argList << ", \'" << args::get(def) << "\'";
  }
  if (format) {
    typeBuf << ", OutputFormat";
    argList << ", \'" << args::get(format) << "\'";
  }

  sql = sqlite3_mprintf("INSERT INTO Types(%s) Values (%s);",
                        typeBuf.str().c_str(), argList.str().c_str());

  rc = sqlite3_exec(db, sql, nullptr, nullptr, &errMsg);
  if (rc) {
    std::cerr << "problem with sqlite3 execution! " << errMsg
              << " running " << sql << std::endl;
  }
  return 0;
}
