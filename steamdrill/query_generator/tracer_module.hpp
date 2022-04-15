#include <string>
#include <sstream>
#include <vector>
#include <unordered_set>

#include "../protos/steamdrill.pb.h"

#include "tracer_builder.hpp"

#ifndef __TRACER_MODULE_H_
#define __TRACER_MODULE_H_

using std::string;
using std::stringstream;
using std::vector;
using std::unordered_set;

namespace Generate {
class TracerModule {
 private:
  TracingCodeManager _man;
  string _filename;
  vector<string> _includes; // not used just yet
  stringstream _baseFile;
  stringstream _moduleContents;
  vector<TracerBuilder*> _tracers;
  unordered_set<FilterBuilder> _filters;

  static int _nextTracer;
  static int _nextFilter;

 public:
  TracerModule(std::string name): _filename(name) {}

  string getName(void) {return _filename;}

  TracerBuilder* getNewTracer(int output = 0);
  std::string getFilter(const Steamdrill::SDExpression &expr);
  void addString(std::string);
  void addBase(std::string);
  void addInclude(std::string);
  void writeToFile(void);
  TracingCodeDef* addTracingCode(std::string name, unique_ptr<Type> type);
};
} // namespace Generate
#endif /*__TRACER_MODULE_H_ */
