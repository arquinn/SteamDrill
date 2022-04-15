#include "parse_syscall_input_lib.h"

struct syscall_input_log* sysacll_input_open(const char *filename) {
  struct syscall_input_log *rtn = NULL;
  int fd = 0;

  fd = open (filename, O_RDONLY);
  if (fd < 0) {
    perror ("open");
    return NULL;
  }
  else {
    rtn = malloc(sizeof(struct syscall_input_log));
    rtn->fd = fd;
  }

  return rtn;

}

int syscall_input_close(struct syscall_input_log *log) {
  int rc = close(log->fd);
  free(log);
  return rc;
}

int syscall_inputs_next(struct syscall_input_log* log, struct syscall_input* syscall) {

  int copied = read(log->fd, (char *) &(syscall->header), sizeof(struct syscall_input_header));
  if (copied > 0) {
    switch (syscall->header.syscall) {
      case 192:
        copied = read(log->fd, (char *)&(syscall->mmap), sizeof(struct mmap_input));
        break;
      case 125:
        copied = read(log->fd, (char *)&(syscall->mprotect), sizeof(struct mprotect_input));
        break;
    }
  }
  else
    syscall = NULL;

  return copied;

}

