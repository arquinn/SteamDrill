// Shell program for running a sequential multi-stage DIFT
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <assert.h>
#include <fcntl.h>
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#include "util.h"

#define MAX_PROCESSES  2
#define BUFFER_SIZE 1024
int main (int argc, char* argv[])
{
  struct timeval tv_start, tv_end;

  const char *dirname = NULL, *pin_tool = NULL,
      *begin_pid = NULL, *begin_clock = NULL, *end_clock = NULL;

  pid_t cpid, mpid;
  int fd, rc, status, index;

  if (argc < 3) {
    fprintf (stderr, "format: seqtt <replay dir> <pin_tool>\n");
    return -1;
  }

  static struct option long_options[] =
      {{"begin_pid",  required_argument, 0, 'p'},
       {"begin_clock", required_argument, 0, 'c'},
       {"endclock", required_argument, 0, 'e'},
       {0, 0, 0, 0}};

  while (true)
  {
    int c = getopt_long (argc, argv, "p:c:e:", long_options, &index);

      /* Detect the end of the options. */
      if (c == -1)
        break;

      switch (c)
      {
        case 'p':
          begin_pid = optarg;
          break;
        case 'c':
          begin_clock = optarg;
          break;
        case 'e':
          end_clock = optarg;
          break;
        default:
          abort ();
      }
  }
  assert (optind == argc - 2);
  dirname = argv[optind];
  pin_tool = argv[optind + 1];


  fd = open ("/dev/spec0", O_RDWR);
  if (fd < 0) {
    perror("open /dev/spec0");
    return fd;
  }

  cpid = fork ();
  if (cpid == 0)
  {
    char start[80];
    const char *args[256];
    int argcnt = 0;
    args[argcnt++] = "resume";
    args[argcnt++] = dirname;
    args[argcnt++] = "--pthread";
    args[argcnt++] = "../../eglibc-2.15/prefix/lib";
    args[argcnt++] = "-p";

    if (begin_clock)
    {
      sprintf(start, "--attach_offset=%s,%s", begin_pid, begin_clock);
      args[argcnt++] = start;
      // fprintf(stderr, "attach to %s\n", start);
    }
    args[argcnt++] = NULL;

    rc = execv("../../test/resume", (char **) args);
    fprintf (stderr, "execl of resume failed errstr %s\n", strerror(errno));
    return -1;
  }

  // Wait until we can attach pin
  do {
    rc = get_attach_status (fd, cpid);
  } while (rc <= 0);
  // fprintf(stderr, "replay is ready, attach to %d\n", rc);

  gettimeofday (&tv_start, NULL);
  mpid = fork();
  if (mpid == 0) {
    const char *args[256];
    char cpids[80];
    int argcnt = 0;

    args[argcnt++] = "pin";
    args[argcnt++] = "-pid";
    sprintf (cpids, "%d", rc);
    args[argcnt++] = cpids;
    args[argcnt++] = "-t";
    args[argcnt++] = pin_tool;
    if (end_clock)
    {
      args[argcnt++] = "-s";
      args[argcnt++] = end_clock;
    }
    args[argcnt++] = NULL;

    rc = execv("../../../../pin/pin", (char **) args);
    fprintf (stderr, "execl of pin tool failed %s\n", strerror(errno));
    return -1;
  }

  // Wait for cpid to complete
  rc = wait_for_replay_group(fd, cpid);

  rc = waitpid (cpid, &status, 0);
  if (rc < 0) {
    fprintf (stderr, "waitpid returns %d, errno %d for pid %d\n", rc, errno, cpid);
  }
  gettimeofday (&tv_end, NULL);

  long diff_usec = tv_end.tv_usec - tv_start.tv_usec;
  long carryover = 0;
  if(diff_usec < 0) {
    carryover = -1;
    diff_usec = 1 - diff_usec;
  }
  long diff_sec = tv_end.tv_sec - tv_start.tv_sec - carryover;

  printf ("time: %ld.%06ld\n", diff_sec, diff_usec);
  return 0;
}
