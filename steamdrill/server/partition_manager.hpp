
#include <string>
#include "mkpartition_lib.h"


#ifndef _PARTS_MANAGER
#define _PARTS_MANAGER

#define PARTITION_PATH "%s/%s.%d"
struct partition *getParts(std::string base, std::string replay_name, u_int cores);
void addParts(std::string base, std::string replay_name, u_int cores, u_long exit_clock = 0, bool watchdog=false);
#endif
