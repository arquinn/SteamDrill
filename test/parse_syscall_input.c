#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include "parse_syscall_input_lib.h"


int main (int argc, char* argv[])
{
  int rc;
  struct mmap_input *input;
  struct syscall_input_log *log;

  if (argc != 2) {
    printf ("format: parse_cachedfiles <dir>\n");
    return -1;
  }

  log = syscall_input_open(argv[1]);
  if (log == NULL)
    return -1;

  while ((rc = syscall_input_next(log, &input)) > 0) {
    char cachename[CACHENAME_LEN + 1];
    char origname[PATH_MAX + 1];

    get_cache_filename(cachename, input->dev, input->ino, input->mtime);
    get_orig_filename(cachename, origname);
    printf("[%ld] (%lx -> %lx) @ %lx %c%c%c %s %s\n", input->clock, input->start, input->start + input->len,
           input->offset * 0x1000,
           input->prot & PROT_READ  ? 'r' : '-',
           input->prot & PROT_WRITE ? 'w' : '-',
           input->prot & PROT_EXEC  ? 'x' : '-',
           cachename, origname);
  }

  syscall_input_close(log);
  return 0;
}
