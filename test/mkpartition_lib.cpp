#include <sys/stat.h>

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include <dirent.h>
#include <cstdint>
#include <math.h>
#include <algorithm>
#include <map>
#include <vector>
#include <queue>
#include <set>
#include <limits>
#include <unordered_set>
#include <unordered_map>
#include <iostream>

#include "parseklib.h"
#include "mkpartition_lib.h"

#define __user
#include "../linux-lts-quantal-3.5.0/include/linux/pthread_log.h"

//#define DEBUG(...) fprintf(stderr, __VA_ARGS__);
#define DEBUG(x,...) (x)

using namespace std;

unordered_set<short> bad_syscalls({120});

//used everywhere else
struct timing_data {
    pid_t     pid;
    bool      can_attach;
    u_long    start_clock;  //when the syscall starts
    u_long    stop_clock;   //when the syscall is done running

  timing_data(pid_t p, bool ca, u_long start, u_long stop):
      pid(p), can_attach(ca), start_clock(start), stop_clock(stop)  {};

  timing_data(): pid(0), can_attach(false), start_clock(0), stop_clock(0) {};

};

u_long get_ckpt(const vector<struct timing_data> &td, const size_t index)
{
    // assume that ckpt_clocks is sorted here.
    u_long start_clock = td[index].start_clock;
    for (int start_index = index - 2;  start_index > 0; --start_index) {
      if ((td[start_index].stop_clock + 1) < start_clock) {
	    return td[start_index].stop_clock + 1;
	}
    }
    return 0;
}

//model created by correlation analysis
static double estimate_time(const vector<struct timing_data> &td, const int i, const int j) {
  (void) td;
  return j - i;
}

static void add_partition(const vector<struct timing_data> &td,
                          const size_t ind1, const size_t ind2,
                          vector<struct partition> &parts) {

  struct partition p;
  p.start_pid = td[ind1].pid;
  p.start_clock = ind1 == 0 ? 0 : td[ind1].start_clock;
  p.stop_pid = td[ind2].pid;
  p.stop_clock = ind2 == td.size() - 1 ? 0 : td[ind2].start_clock;
  p.ckpt_clock= get_ckpt(td, ind1);

  parts.push_back(p);
}

static void
gen_timings (const vector<timing_data> &td, const int num_parts, const size_t start, const size_t end,
             const bool watchdog, vector<struct partition> &parts) {

  if (num_parts == 1) {
    add_partition(td, start, end, parts);
  }
  else {
    int partitions = watchdog ? num_parts - 1 : num_parts;
    double ticks = td[end].start_clock - td[start].start_clock;
    double goal = ticks / partitions;

    for (size_t i = start + 1; i <= end; i++) {
      double gap = td[i].start_clock - td[start].start_clock;
      if (gap > goal) {
        add_partition (td, start, i, parts);
        gen_timings(td, partitions - 1, i, end, false, parts);
        break;
      }
    }
  }

  if (watchdog) {
    // split the last partition: assign it's stop to the last possible timing_data:
    auto last = &parts.back();
    last->stop_clock = td.back().start_clock;

    // and add a partition that goes from the last syscall to the end:
    add_partition(td, td.size() - 1, td.size() - 1, parts);
  }
}

struct kiter {

  struct klog_result *res;
  struct klogfile *log;
  u_long prev_clock;
  pid_t pid;

  kiter(const char *path, pid_t p) {
    log = parseklog_open(path); // slower, but at least it isn't broken, right? right?
    prev_clock = 0;
    pid = p;
    res = nullptr;
    next();

    DEBUG("created kiter %s, %d %p\n", path, pid, res);
  }

  void next() {
    if (res) prev_clock = res->start_clock;
    res = parseklog_get_next_psr(log);
  }

  void close() {
    parseklog_close(log);
  }

  bool operator<(const struct kiter &other) {
    return res->start_clock < other.res->start_clock;
  }
};


int parse_klogs(const u_long start_clock, const u_long stop_clock,
                const char* dir, vector<struct timing_data> &td) {
  DIR *d = nullptr;
  vector<struct kiter> kiters;
  struct dirent *dire = nullptr;

  long unnattachable = -1;

  // get all directories
  d = opendir(dir);
  if (!d) {
    fprintf( stderr, "cannot open %s error (%d) %s\n", dir, errno, strerror(errno));
    return -1;
  }
  while ((dire = readdir(d))) {
    pid_t pid;
    if (sscanf(dire->d_name, "klog.id.%d", &pid) == 1) {
      char klog [PATH_MAX];
      sprintf(klog, "%s/klog.id.%d", dir, pid);
      kiters.emplace_back(klog, pid);
    }
  }

  assert (!td.size());

  // now populate the td data.
  while(!kiters.empty()) {
    auto kiter = min_element(kiters.begin(), kiters.end());

    // do something special on fork
    if (kiter->res->psr.sysnum == 120) {
      // udpate prev_clock for the child:
      // must be comming from this, correct?
      pid_t child = kiter->res->retval;
      for (auto &cki : kiters) {
        // this is an invalid read...?
        if (cki.pid == child) {
          cki.prev_clock = kiter->res->start_clock;
          break;
        }
      }

      // block out syscalls backwards
      auto bw_tds = td.rbegin();
      /// either bw_tds->start_clock is not created or the rpev_clock..?
      u_long call_clock = kiter->prev_clock;
      while (bw_tds != td.rend()) {// && bw_tds->start_clock > kiter->prev_clock) {

        u_long start_clock = bw_tds->start_clock;

        if (start_clock < call_clock) {
            break;
        }
        bw_tds->can_attach = false;
        ++bw_tds; //this is .... weird?
      }

      // block out future systemcalls
      unnattachable = max(unnattachable, (long) kiter->res->stop_clock);
    }
    if (kiter->res->start_clock >= start_clock &&  kiter->res->start_clock <= stop_clock) {
      td.emplace_back(kiter->pid,
                      (long)kiter->res->start_clock > unnattachable,
                      kiter->res->start_clock,
                      kiter->res->stop_clock );
    }

    kiter->next();
    if (!kiter->res) {
      kiter->close();
      kiters.erase(kiter);
    }
  }
  return 0;
}

int get_watchdog_for_log(int fd, bool debug) {
  int wd = INT_MAX, rc;
  u_long clock = 0;

  size_t bytes_read  = 0;
  struct stat st;

  rc = fstat(fd, &st);
  while (bytes_read < st.st_size) {
    u_long num_bytez, count = 0;

    rc = read(fd, &num_bytez, sizeof(long));
    assert (rc);
    //fprintf(stderr, "** reading %lu bytez **\n", num_bytez);
    bytes_read += rc;


    while (count < num_bytez) {
      u_long entry;
      int unused, skip = 0;

      rc = read (fd, &entry, sizeof(u_long));
      assert (rc);
      count += rc;
      bytes_read += rc;
      if (debug)
        printf ("clock %lu entry %lx usual recs %ld non-zero retval? %d errno change? %d fake calls? %d skip? %d\n",
                clock,
                entry, (entry&CLOCK_MASK), !!(entry&NONZERO_RETVAL_FLAG), !!(entry&ERRNO_CHANGE_FLAG), !!(entry&FAKE_CALLS_FLAG), !!(entry&SKIPPED_CLOCK_FLAG));

      clock += (entry&CLOCK_MASK);
      clock ++;

      if (entry&SKIPPED_CLOCK_FLAG) {
        rc = read (fd, &skip, sizeof(int));
        assert (rc == sizeof(int));

        count += rc;
        bytes_read += rc;
        clock += skip;
      }

      if (entry&NONZERO_RETVAL_FLAG) {
        rc = read (fd, &unused, sizeof(int));
        assert (rc == sizeof(int));

        count += rc;
        bytes_read += rc;
      }
      if (entry&ERRNO_CHANGE_FLAG) {
        rc = read (fd, &unused, sizeof(int));
        assert (rc == sizeof(int));
        count += rc;
        bytes_read += rc;
      }
      if (entry&FAKE_CALLS_FLAG) {
        rc = read (fd, &unused, sizeof(int));
        assert (rc == sizeof(int));
        count += rc;
        bytes_read += rc;

        // now check this out:
        rc = read (fd, &unused, sizeof(int));
        if (unused == -1) {
          // not sure how this works??
          return clock - 1; // this maybe should actually be minus skip
        }
        lseek(fd, -1 * sizeof(int), SEEK_CUR);
      }
    }
  }
  return wd;
}

int get_watchdog_for_replay(const char *dir) {
  DIR *d = nullptr;
  vector<struct kiter> kiters;
  struct dirent *dire = nullptr;
  int min_wd = INT_MAX;

  // get all directories
  d = opendir(dir);
  if (!d) {
    fprintf( stderr, "cannot open %s error (%d) %s\n", dir, errno, strerror(errno));
    return -1;
  }
  while ((dire = readdir(d))) {
    pid_t pid;
    if (sscanf(dire->d_name, "ulog.id.%d", &pid) == 1) {
      char log [PATH_MAX];
      sprintf(log, "%s/ulog.id.%d", dir, pid);
      int fd = open(log, O_RDONLY);
      int wd = get_watchdog_for_log(fd, false);
      if (wd < INT_MAX) {
        fprintf(stderr, "%d thinks watchdog at %d \n", pid, wd);
      }
      if (wd < min_wd) {
        min_wd = wd;
      }
    }
  }
  return min_wd;
}


// add a watchdog tag
int get_partitions(const char *dir, const size_t num_parts, const u_long start_clock, const u_long stop_clock,
                   const bool watchdog,
                   std::vector<struct partition> &parts) {

  vector<struct timing_data> td;
  u_long stop = stop_clock > 0 ? stop_clock : ULONG_MAX;
  u_long start = start_clock > 0 ? start_clock : 0;
  int rc = parse_klogs(start, stop, dir, td);
  if (rc) {
    fprintf(stderr, "cannot parse_klogs? \n");
    return -1;
  }

  u_int wd = get_watchdog_for_replay(dir);
  int wd_idx = -1;
  if (wd != INT_MAX) {
    for (u_int i = 0; i < td.size(); ++i) {
      if (td[i].start_clock > wd) {
        wd_idx = i;
        break;
      }
    }
  }
  if (wd_idx > 0)
    td.resize(wd_idx);
  
  DEBUG("found %u system calls\n", td.size());

  gen_timings(td, num_parts, 0, td.size() - 1, watchdog, parts);

  assert (parts.size() == num_parts);
  return 0;
}
