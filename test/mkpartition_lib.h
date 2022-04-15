#include <stdlib.h>

#include <vector>

#ifndef __MKPARTITION_LIB
#define __MKPARTITION_LIB

struct partition {
  pid_t  start_pid;
  u_long start_clock;
  pid_t  stop_pid;
  u_long stop_clock;
  u_long ckpt_clock;

};

// creates an array for partitions. returns the number of partitions created (better be num_partitions)
int get_partitions(const char *dir, const size_t num_partitions, const u_long start_clock, const u_long stop_clock, bool watchdog,
                  std::vector<struct partition> &parts);

#endif/* __MKPARTITION_LIB */
