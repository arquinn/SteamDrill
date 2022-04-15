#include <stdint.h>
#include <time.h>
#include <errno.h>

//#include "tracer_library.h"

#ifndef __TRACER_TIMER__
#define __TRACER_TIMER__

struct TracerTimer {
  uint64_t total_us;
  uint64_t start;
};


#endif /* __TRACER_TIMER__ */
