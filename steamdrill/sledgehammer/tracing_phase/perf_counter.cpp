#include <sys/types.h>
#include <sys/ioctl.h>
#include <asm/unistd.h>

#include <linux/perf_event.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


#include "perf_counter.hpp"
#include "log.hpp"

int ThreadGroup::PerfCounter::perf_event_open(
    struct perf_event_attr *pe,
    int pid,
    int cpu,
    int unused1,
    int unused2)
{
  return syscall(__NR_perf_event_open, pe, pid, cpu, unused1, unused2);
}


ThreadGroup::PerfCounter::PerfCounter(pid_t pid,
                                      int counter_type,
                                      int counter_value)
{
  struct perf_event_attr pe;

  memset(&pe, 0, sizeof(struct perf_event_attr));
  pe.size = sizeof(struct perf_event_attr);
  pe.type = counter_type;
  pe.config = counter_value;
  pe.disabled = 0;
  pe.exclude_kernel = 1;
  pe.exclude_hv = 1;
  // pe.exclude_host = 1; //idk what this does vs. the stuff above?

  _fd = perf_event_open(&pe, pid, -1, -1, 0);

  if (_fd < 0) {
    ERROR("Can't build perfcounter for %u, errno %d", pid, errno);
  }
  resetCounter();
  _slack = 0;
}

long long ThreadGroup::PerfCounter::get()
{
  int rc;
  long long count;

  rc = read(_fd, &count, sizeof(count));
  if (rc != sizeof(count))
    ERROR("Can't read counter, errno %d", errno);
  return count + _slack;
}

void ThreadGroup::PerfCounter::pause()
{
  int rc = ioctl(_fd, PERF_EVENT_IOC_DISABLE, 0);
  if (rc)
    ERROR("Can't pause counter, errno %d", errno);
}

void ThreadGroup::PerfCounter::start()
{
  int rc = ioctl(_fd, PERF_EVENT_IOC_ENABLE, 0);
  if (rc)
    ERROR("Can't start counter, errno %d", errno);
}

void ThreadGroup::PerfCounter::addSlack(int i)
{
  _slack += i;
}

void ThreadGroup::PerfCounter::resetCounter()
{
  int rc = ioctl(_fd,PERF_EVENT_IOC_RESET, 0);
  if (rc)
    ERROR("Can't reset counter, errno %d", errno);
}
