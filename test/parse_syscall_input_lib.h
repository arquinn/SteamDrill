#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>


#include <stdint.h>

#include <stdlib.h>
#include <unistd.h>

#ifndef __PARSE_CACHEDFILES_LIB__
#define __PARSE_CACHEDFILES_LIB__

#define CACHENAME_LEN 256
// redefined from the kernel
struct mmap_input {
  u_long clock;
  u_long start;
  u_long len;
  u_long offset;
  u_long prot;
  u_long dev;
  u_long ino;
  struct timespec mtime;
};


struct syscall_input_log {
  int fd;
  struct mmap_input item;
};

int syscall_input_next(struct syscall_input_log*, struct mmap_input**);

struct syscall_input_log* syscall_input_open(const char *filename);
int syscall_input_close(struct syscall_input_log *);

//  weird that this is heare, but it clearly should make it broadly into this library..
int get_cache_filename(char *cachename,  uint32_t dev, uint32_t ino, struct timespec mtime);
int get_orig_filename(const char *cachename, char *origname);

#endif /* __PARSE_CACHEDFILES_LIB__ */
