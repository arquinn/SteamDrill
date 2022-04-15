// Shell program for launching gdb on top of a replay

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <linux/limits.h>

#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include "util.h"

#define MAX_PROCESSES  2
#define BUFFER_SIZE 1024

void print_help(const char *program) {
  fprintf (stderr, "format: %s <logdir> [extra args] \n",  program);
}


const char* get_path(const char *str) {
  // get the relative directory of this executable based on proc:
  char dest[PATH_MAX * 2], path[PATH_MAX] = "/proc/self/exe";
  memset(dest, 0, sizeof(dest));

  int rc = readlink(path, dest, PATH_MAX);
  if (rc < 0) {
    fprintf(stderr, "problem with readlink %d\n", errno);
    return "";
  }

  // convert to just the directory,
  char *dir = dirname(dest);
  strncat(dir, "/", PATH_MAX * 2);
  strncat(dir, str, PATH_MAX * 2);
  char *rpath = realpath(dir, NULL);
  return rpath;

}


int main (int argc, char* argv[]) {

  char cpids[80];
  char *dirname = NULL;
  const char *args[256];

  pid_t cpid;
  int fd, rc, argcnt = 0, i = 2, exit_clock = -1;

  if (argc < 2) {
    print_help(argv[0]);
    return -1;
  }

  dirname = argv[1];
  if (argc > 2 && !strcmp("-exit", argv[i])) {
    exit_clock = i + 1;
    i += 2;
    fprintf(stderr, "exit at %s (%d)", argv[exit_clock], exit_clock);
  }

  fd = open ("/dev/spec0", O_RDWR);
  if (fd < 0) {
    perror("open /dev/spec0");
    return fd;
  }

  cpid = fork ();
  if (cpid == 0) {

    args[argcnt++] = "resume";
    args[argcnt++] = "-g";
    args[argcnt++] = dirname;
    args[argcnt++] = "--pthread";
    args[argcnt++] = get_path("../eglibc-2.15/prefix/lib");
    if (exit_clock > 0) {
      args[argcnt++] = "--exit_clock";
      args[argcnt++] = argv[exit_clock];
    }
    args[argcnt++] = NULL;

    rc = execv(get_path("./resume"), (char**) args);
    fprintf (stderr, "execl of resume failed, rc=%d, errno=%d\n", rc, errno);
    return -1;
  }
  do {
    // Wait until we can attach pin
    rc = get_attach_status (fd, cpid);
  } while (rc <= 0);

  sprintf (cpids, "-ex=attach %d", cpid);

  args[argcnt++] = "gdb";
  args[argcnt++] = "-ex=set non-stop on";
  args[argcnt++] = cpids;

  for (; i < argc; ++i)
    args[argcnt++] = argv[i];
  args[argcnt++] = NULL;

  rc = execv ("/usr/local/bin/gdb", (char**) args);
  //rc = execv ("/home/arquinn/gdb-install/bin/gdb", (char**) args);

  fprintf (stderr, "execv of gdb failed, rc=%d, errno=%d\n", rc, errno);
  return -1;
}
