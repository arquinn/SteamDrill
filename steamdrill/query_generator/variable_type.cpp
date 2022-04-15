#include <cassert>
#include <memory>


#include "spdlog/spdlog.h"
#include "variable_type.hpp"
#include "sqlite3.h"

using std::make_unique;
using std::shared_ptr;


namespace Generate {

TypeManager* TypeManager::theInstance = new TypeManager();

TypeManager::TypeManager(string path) {
  SPDLOG_DEBUG("type manager with path: {}", path);
  int rc = sqlite3_open(path.c_str(), &_db);
  if (rc) {
    std::cerr << "can't open db" << sqlite3_errmsg(_db) << std::endl;
    sqlite3_close(_db);
    assert(false);
  }
}

vector<unique_ptr<Type>> TypeManager::getTypes(const string &type) {
  char *errMsg = nullptr, *sql = nullptr;
  vector<unique_ptr<Type>> rtn;
  int rc = 0;


  if (type.length() > 0) {
    sql = sqlite3_mprintf("Select * From Types Where Name='%s'", type.c_str());
  }
  else {
    sql = sqlite3_mprintf("Select * From Types");
  }

  rc = sqlite3_exec(theInstance->_db,
                    sql,
                    [](void* in, int num_cols, char ** vals, char **names) {
                      auto rtn = (vector<unique_ptr<Type>>*)(in);
                      string name = "", def = "", format = "";
                      for (int  i = 0; i < num_cols; ++i) {
                        if (!strncasecmp(names[i], "Name", 4) && vals[i]) {
                          name = vals[i];
                        }
                        else if (!strncasecmp(names[i], "Def", 3) && vals[i]) {
                          def = vals[i];
                        }
                        else if (!strncasecmp(names[i], "OutputFormat", 12) && vals[i]) {
                          format = vals[i];
                        }
                      }
                      auto t = make_unique<Type>(name, def, format);
                      rtn->push_back(std::move(t));
                      return 0;
                    },
                    &rtn,
                    &errMsg);
  if (rc) {
    spdlog::error("problem with sqlite3 execution! {} when {}", errMsg, sql);
  }
  if (!rtn.size()) {
    spdlog::error("no entry for {}", type);
  }
  return rtn;
}

unique_ptr<Type> TypeManager::getType(const string &type) {

  // see if we're looking at a pointer or just plain jane:
  int idx = type.find_last_of('*');
  if (idx != string::npos && type != "char *" && type != "void *") {
    auto base = getType(type.substr(0, idx));
    auto ptr = getType("void *"); // hacky builtin that works for this, I swear!
    return make_unique<PointerType>(type, ptr->def, ptr->output_format, std::move(base));
  }
  // this is the base case:
  auto types = getTypes(type);
  if (types.size()) {
    return move(getTypes(type).front());
  }
  // Return a default type. Hopefully the query doesn't actually need this:
  return move(make_unique<Type>(type, "",""));

}
/*
MapType::MapType(shared_ptr<Type> k, shared_ptr<Type> v) : key(k), value(v) {
  format = "%s";
  output = boost::str(boost::format("toString(%%s).c_str()") %key->toString() %value->toString());
  //output = "parseParameters(%s).c_str()";
  name = boost::str(boost::format("unordered_map<%s,%s>") %key->toString() %value->toString());
}

ArrayType::ArrayType (shared_ptr<Type> &type) {
  format = "%s";
  output = "parseVector(%s).c_str()";
  name = boost::str(boost::format("vector<%s>") %type->toString());
}
*/

} // namespace Generate
