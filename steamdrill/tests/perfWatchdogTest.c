#define _GNU_SOURCE

#include <linux/perf_event.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>


#define __NR_perf_event_open 336

static long
perf_event_open(struct perf_event_attr *hw_event, pid_t pid,
                int cpu, int group_fd, unsigned long flags)
{
  int ret;

  ret = syscall(__NR_perf_event_open, hw_event, pid, cpu,
                group_fd, flags);
  return ret;
}

int main()
{
  struct perf_event_attr pe;
  pid_t pid = getpid();

  printf("hello world!");

  memset(&pe, 0, sizeof(struct perf_event_attr));

  pe.size = sizeof(struct perf_event_attr);
  pe.type = PERF_TYPE_HARDWARE;
  pe.config = PERF_COUNT_HW_INSTRUCTIONS;
  pe.exclude_kernel = 1;


  pe.sample_period = 0x10000;
  int watchdog_fd = perf_event_open(&pe, 0, -1, -1, 0);
  if (watchdog_fd < 0) {
    printf("cannot create watchdog");
  }

  printf("Pid %d watchdog started (I hope)\n", pid);

  int rc = fcntl(watchdog_fd, F_SETOWN, pid);
  if (rc) {
    printf("couldn't setown?  %d", rc);
  }

  rc = fcntl(watchdog_fd, F_SETFL, O_ASYNC);
  if (rc) {
    printf("couldn't OASYNC? %d", rc);
  }

  rc = fcntl(watchdog_fd, F_SETSIG, SIGTERM);
  if (rc) {
    printf("couldn't change signal? %d", rc);
  }

  int i = 0;
  while (1) {
    ++i;
  }
}
