#define __USE_LARGEFILE64
#define _LARGEFILE64_SOURCE

#include <sys/stat.h>
#include <sys/types.h>

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "parse_syscall_input_lib.h"

struct syscall_input_log* syscall_input_open(const char *filename) {
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

int syscall_input_next(struct syscall_input_log* log, struct mmap_input** syscall) {

  // the age old question: is it faster to mmap or read??
  int copied = read(log->fd, (char *) &log->item, sizeof(struct mmap_input));
  if (copied > 0) {
    // removes a slightly more expensive copy...?
    (*syscall) = &log->item;
  }
  else {
    (*syscall) = NULL;
  }
  return copied;

}

int get_cache_filename(char *cachename, uint32_t dev, uint32_t ino, struct timespec mtime) {

  struct stat64 st;
  sprintf (cachename, "/replay_cache/%x_%x", dev, ino);
  int rc = stat64 (cachename, &st);
  if (rc == 0) {
    if (st.st_mtim.tv_sec > mtime.tv_sec ||
        (st.st_mtim.tv_sec == mtime.tv_sec && st.st_mtim.tv_nsec > mtime.tv_nsec)) {
      // Exists and new file is past version
      sprintf (cachename, "/replay_cache/%x_%x_%lu_%lu", dev, ino, mtime.tv_sec, mtime.tv_nsec);
    }
  }
  else {
    perror("get_cache_filename open");
    return rc;
  }

  return 0;
}

int get_orig_filename(const char *cachename, char *origname) {
  // determine the file name:
  char orig_mapping[CACHENAME_LEN+1];
  sprintf (orig_mapping, "%s.orig", cachename);
  int fd = open(orig_mapping, O_RDONLY);
  if (fd > 0) {
    int rc = read(fd, origname, PATH_MAX);
    if (rc < 0) {
      perror("get_cache_filename read");
      return rc;
    }
    origname[rc] = '\0';
    close(fd);
  }
  else {
    strcpy(origname, "");
  }
  return 0;
}
