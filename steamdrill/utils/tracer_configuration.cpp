#include <boost/regex.hpp>
#include <boost/lexical_cast.hpp>

#include <iostream>
#include <fstream>
#include <sstream>

#include "tracer_configuration.hpp"
#include "address.hpp"


using std::string;
using std::vector;

namespace Configuration{

std::vector<TracerConfiguration*> parseTPoints(string tracepoints) {
  vector<TracerConfiguration*> tracers;
  std::ifstream myfile(tracepoints);
  boost::archive::text_iarchive ia(myfile);
  // std::cerr << "trying to load tracing points from " << tracepoints;

  ia >> tracers;
  myfile.close();
  return tracers;
}

void writeTPoints(vector<TracerConfiguration*> tracers, string loc) {
  // is there a problem with copies on that line 31...?
  std::ofstream myfile(loc);
  boost::archive::text_oarchive oa(myfile);
  oa << tracers;
  myfile.close();
}

vector<InstConfiguration*> parseInstPoints(string insts) {

  vector<InstConfiguration*> instPoints;
  std::ifstream myfile(insts);
  boost::archive::text_iarchive ia(myfile);

  ia >> instPoints;
  myfile.close();

  return instPoints;

}

void writeInstPoints(vector<InstConfiguration*> insts, string loc) {

  std::ofstream myfile(loc);
  boost::archive::text_oarchive oa(myfile);

  oa << insts;
  myfile.close();
}
} // end namespace Configuration
