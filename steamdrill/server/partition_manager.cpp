#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <unistd.h>


#include <cassert>
#include <cstring>


#include <algorithm>
#include <string>

#include "mkpartition_lib.h"
#include "partition_manager.hpp"

struct partition *getParts(std::string base, std::string replay_name, u_int cores) {
  char part_filename[PATH_MAX];
  std::string replay = replay_name;
  std::replace(replay.begin(), replay.end(), '/', '.');

  sprintf(part_filename, PARTITION_PATH, base.c_str(), replay.substr(1).c_str(), cores);
  struct partition *parts = new struct partition[cores];


  int fd = open(part_filename, O_RDONLY);
  // might want to adjust this for later.
  if (fd < 0) {
    fprintf(stderr, "couldn't open %s\n", part_filename);
  }
  assert (fd > 0);

  ssize_t consumed = read(fd, parts, sizeof(struct partition) * cores);
  assert (consumed == sizeof(struct partition) * cores);

  return parts;
}

void addParts(std::string base, std::string replay_name, u_int cores, u_long end_clock, bool watchdog) {

  char part_filename[PATH_MAX];
  std::string replay = replay_name;
  std::replace(replay.begin(), replay.end(),  '/', '.');

  sprintf(part_filename, PARTITION_PATH, base.c_str(), replay.substr(1).c_str(), cores);

  struct partition *parts = new struct partition[cores];

  int fd = open(part_filename, O_CREAT | O_WRONLY, 0777);
  if (fd < 0) {
    fprintf(stderr, "couldn't create %s, errno %s(%d)\n", part_filename, strerror(errno), errno);
  }
  assert (fd > 0);

  std::vector<struct partition> vparts;
  int rc = get_partitions(replay_name.c_str(), cores, 0, end_clock, watchdog, vparts);
  for (auto i = 0; i < cores; ++i) {
    parts[i] = vparts[i];
    fprintf(stderr, "vpart (%d, %lu), -> (%d, %lu)\n", vparts[i].start_pid, vparts[i].start_clock,
            vparts[i].stop_pid, vparts[i].stop_clock);
    fprintf(stderr, "part (%d, %lu), -> (%d, %lu)\n", parts[i].start_pid, parts[i].start_clock,
            parts[i].stop_pid, parts[i].stop_clock);
            
  }
  ssize_t written = write(fd, parts, sizeof(struct partition) * cores);
  assert (written == sizeof(struct partition) * cores);
}
