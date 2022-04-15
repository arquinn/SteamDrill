#include <string>
#include <unordered_map>
#include <vector>
#include "../protos/steamdrill.pb.h"

#include "../utils/tracer_configuration.hpp"

#ifndef __SH_QUERY_GENERATOR_
#define __SH_QUERY_GENERATOR_

using std::string;


namespace Generate {

std::vector<Configuration::TracerConfiguration*>
generateSHQuery(const Steamdrill::SDQuery *query,
                const string tracerFilename,
                const string tpFilename,
                const string tracersBaseFile);

void generateLibcConfig(const string replayDir, const string configLocation);
void generateStandardTracepoints(const string replayDir, const string filename);

} // namespace Generator
#endif /*__SH_QUERY_GENERATOR_*/
