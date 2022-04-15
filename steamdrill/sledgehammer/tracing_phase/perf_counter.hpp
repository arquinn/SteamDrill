#include <sys/types.h>
#include <linux/perf_event.h>

#ifndef __PERFCOUNTER_H_
#define __PERFCOUNTER_H_

#define DEFAULT_EVENT 0x5101c4

namespace ThreadGroup {
class PerfCounter {
 private:
  int _fd;
  int _slack;

  static int perf_event_open(struct perf_event_attr *pe,
                             int pid,
                             int cpu,
                             int unused1,
                             int unused2);
  void resetCounter();
 public:
  PerfCounter(pid_t pid,
              int counter_type = PERF_TYPE_RAW,
              int counter_value = DEFAULT_EVENT);

  long long get();
  void pause();
  void start();

  void addSlack(int i);
};
} /* Namespace ThreadGroup*/

#endif /* __TASK_H_*/
