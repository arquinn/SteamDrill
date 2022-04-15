#include <string>

#include "../utils/tracer_configuration.hpp"
#include "../config.h"

#ifndef __INSTRUMENT_H_
#define __INSTRUMENT_H_
namespace Instrument {
int instrument(std::vector<Configuration::TracerConfiguration*> tracer,
               std::string assembly,
               std::string ipoints,
               std::string replay,
               std::string blockCacheDir,
               JumpCounter jumpType);
}
#endif /* __INSTRUMENT_H_ */
