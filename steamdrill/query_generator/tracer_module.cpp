#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>

#include <cstdio>
#include <unistd.h>
#include <sys/stat.h>

#include "spdlog/spdlog.h"

#include "tracer_module.hpp"
#include "tracer_builder.hpp"

#define TRACER_FORMAT "tracer%d"
#define FILTER_FORMAT "filter%d"
#define INCLUDE_STR "#include %s"

using std::string;
using std::stringstream;
using std::fstream;

namespace Generate {

int TracerModule::_nextTracer = 0;
int TracerModule::_nextFilter = 0;


TracingCodeDef* TracerModule::addTracingCode(std::string name, unique_ptr<Type> type) {
  return _man.addTracingCode(name, move(type));
}

TracerBuilder* TracerModule::getNewTracer(int output) {
  char tname[32];
  sprintf(tname, TRACER_FORMAT, _nextTracer);
  _nextTracer++;
  TracerBuilder *tb = new TracerBuilder(tname, &_man, output);
  _tracers.push_back(tb);
  return tb;
} // Module::getNewTracer

std::string TracerModule::getFilter(const Steamdrill::SDExpression &expr) {

  // let's check if this works:

  char tname[32];
  sprintf(tname, FILTER_FORMAT, _nextFilter);
  auto pair = _filters.emplace(tname, &_man, expr);
  if (pair.second) {
    _nextFilter++;
  }
  return pair.first->getName();

} // Module::getNewTracer

// unused I guess?
void TracerModule::addInclude(std::string name) {
  _includes.push_back(name);
} // Module::addInclude


// should delay the insertion of this?
void TracerModule::addString(std::string contents) {
  _moduleContents << contents;
} // TracerModule::addString

void TracerModule::addBase(std::string filename) {
  int infd, rc;
  size_t count = 0, size;
  struct stat statBuf;

  infd = open(filename.c_str(), O_RDONLY);
  if (infd < 0)
    spdlog::error("can't open {} {}", filename, strerror(errno));

  rc = fstat(infd, &statBuf);
  if (rc != 0)
    spdlog::error("can't fstate {} {}", filename, strerror(errno));

  size = statBuf.st_size;
  while (count < size) {
    char buffer[128];
    size_t rcount;
    rcount = read(infd, buffer, std::min(size - count, (size_t)127));
    buffer[rcount] = '\0';

    count += rcount;
    _baseFile << buffer;
  } //while

  close(infd);

} // Module::addFile



void TracerModule::writeToFile(void) {
  spdlog::info("writing module to file {}", _filename);
  std::ofstream output(_filename);
  char line[256];
  for (auto header : _includes)  {
    sprintf(line, INCLUDE_STR, header.c_str());
    spdlog::info("adding include {}", line);
    output << line << "\n";
    assert(output.good());
  }
  output << _baseFile.str();
  // module contents hasn't been written yet, quick! add in our types!
  // Todo: better to pre-compile or only grab the ones we need!
  auto types = TypeManager::getTypes("");
  for (auto &t : types) {
    output << t->def << "\n";
    // output << t->serial_def << "\n"; unnecessary!
  }
  output << _moduleContents.str();
  assert(output.good());
  for (auto &tb : _tracers) {
    output << tb->toString() << "\n";
    assert(output.good());
  }

  for (auto &fb : _filters) {
    if (fb.hasBody) {
      output << fb.toString() << "\n";
      assert(output.good());
    }
  }

  output.close();

} //writeToFile
} // namespace Generator
