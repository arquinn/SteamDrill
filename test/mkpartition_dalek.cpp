#include <sys/stat.h>
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
#include "../linux-lts-quantal-3.5.0/include/linux/pthread_log.h"
using namespace std;

unordered_set<short> bad_syscalls({120});

//used simply for the read from the file system
struct replay_timing {
    pid_t    pid;
    u_long   index;
    short    syscall;
    u_int    ut;
    int64_t  st;
    uint64_t cache_misses;
};

//used everywhere else
struct timing_data {
    pid_t     pid;
    short     syscall;
    bool      can_attach;
    bool      should_track; //should I track this rp_timing?
    u_long    call_clock;   //the clock value of the most recent sync operation
    u_long    start_clock;  //when the syscall starts
    u_long    stop_clock;   //when the syscall is done running
    u_long    aindex;
};

//std::unordered_map<int, pid_t> child_map;

static int group_by = 0, filter_syscall = 0, details = 0, use_ckpt = 0, do_split = 0, dump_stats = 0;
static int use_syscalls = 0;

u_long get_ckpt(vector<struct timing_data> &td, u_int index)
{
    // assume that ckpt_clocks is sorted here.

    u_long start_clock = td[index].start_clock;
    for (int start_index = index -2;  start_index > 0; --start_index)
    {
	if ((td[start_index].stop_clock + 1) < start_clock)
	{
	    return td[start_index].stop_clock + 1;
	}
    }
    return 0;
}


void format ()
{
    fprintf (stderr, "Format: mkpartition <timing dir> <# of partitions> [-g group_by] [-f filter syscall] [-s split at user-level] [-v verbose] [-ds dump_stats] \[--fork fork_flags] [--stop stop_tracking_clock] <list of pids to track >\n");
    exit (22);
}

static int cnt_interval (vector<struct timing_data> &td, int start, int end)
{
    int last_aindex = 0;
    for (int i = end; i > start; --i) { 
	if (td[i].aindex > 0) {
	    last_aindex = td[i].aindex;
	    break;
	}
    }
    return last_aindex - td[start].aindex;
}


static inline void print_utimings (vector<struct timing_data> &td, int start, int end, u_int split, int intvl)
{ 
    u_long ndx = td[start].start_clock;
    u_long next_ndx = ndx + intvl;
    printf ("%5d k %6lu u %6lu       0       0 ", td[start].pid, ndx, next_ndx);
    for (u_int i = 0; i < split-2; i++) {
	ndx = next_ndx;
	next_ndx = ndx+intvl;
	printf ("%5d u %6lu u %6lu       0       0 ", td[start].pid, ndx, next_ndx);
    }
    printf ("%5d u %6lu k %6lu       0       0 ", td[start].pid, next_ndx, td[end].start_clock);
}


//model created by correlation analysis
static double estimate_dift(vector<struct timing_data> &td, int i, int j)
{ 
    double rtn_val;
    rtn_val = j - i;

    return rtn_val;
}

inline static void add_timing(vector<pair<int,int>> &timings, int start, int end)
{
    timings.push_back(make_pair(start, end));
}

inline static void print_timing (vector<struct timing_data> &td, int start, int end, bool first, bool last)
{ 
    if (first)
	printf ("%5d k 0 k %6lu", td[start].pid, td[end].start_clock);
    else if (last)
	printf ("%5d k %6lu k 0", td[start].pid, td[start].start_clock);
    else 
	printf ("%5d k %6lu k %6lu", td[start].pid, td[start].start_clock, td[end].start_clock);

    if (use_ckpt ) {
	u_long ckpt = get_ckpt(td, start);
	printf ("  %lu\n", ckpt);
    } else {
	printf ("       0\n");
    }

}

inline static void print_partitions(vector<pair<int,int>> partitions, 
				    vector<struct timing_data> td)
{
    for (u_int i = 0; i < partitions.size(); ++i)
    {
	print_timing(td, partitions[i].first, partitions[i].second, 
		     i == 0, 
		     i == partitions.size() - 1);
    }
}

int gen_timings (vector<pair<int,int>> &parts,
		 vector<timing_data> &td,
		 int start, 
		 int end, 
		 int partitions) {

    double biggest_gap = 0.0, goal;
    int gap_start, gap_end, last, i, new_part;

    assert (start < end); 
    assert (partitions <= cnt_interval(td,start, end));

    if (partitions == 1) {
	add_timing (parts, start, end);
	return 0;
    }

    double total_time = estimate_dift(td, start, end);
    // find the largest gap
    if (details) {
	printf ("Consider [%d,%d]: %d partitions %.3f time\n", start, end, partitions, total_time);
    }

    last = start;
    gap_start = start;
    gap_end = start + 1;
	
    for (i = start+1; i < end; i++) {

	if (td[i].can_attach && td[i].should_track){
	    double gap = estimate_dift(td, last, i);
	    if (gap > biggest_gap) {
		gap_start = last;
		gap_end = i;
		biggest_gap = gap;
	    }
	    last = i;
	}
    }

    if (details) {
	printf ("Biggest gap from %d to %d is %.3f\n", gap_start, gap_end, biggest_gap);
    }
    if (partitions > 2 && biggest_gap >= total_time/partitions) {
	// Pivot on this gap

	u_int split = 1;
	int intvl = 1;
	if (do_split && biggest_gap >= 2*total_time/partitions) {
	    split = biggest_gap*partitions/total_time-1;
	    if (partitions-split < 2) {
		split = partitions-2;
	    }
	    if (details) {
		printf ("would like to split this gap into %d partitions\n", split);
		printf ("begins at clock %lu ends at clock %lu\n", td[gap_start].start_clock, td[gap_end].start_clock);
	    }
	    if (td[gap_end].start_clock-td[gap_start].start_clock < split) split = td[gap_end].start_clock - td[gap_start].start_clock;
	    intvl = (td[gap_end].start_clock-td[gap_start].start_clock)/split;
	    if (details) {
		printf ("Interval is %d\n", intvl);
	    }
	} 
	total_time -= biggest_gap;
	partitions -= split;
	if (gap_start == start) {
	    if (split > 1) {
		print_utimings (td, gap_start, gap_end, split, intvl);
	    } else {
		add_timing (parts, gap_start, gap_end);
	    }
	    return gen_timings (parts, td, gap_end, end, partitions);
	}

	double front_gap = estimate_dift(td, start, gap_start);

	new_part = 0.5 + (partitions * front_gap) / total_time;
	if (details) {
	    printf ("gap - new part %d\n", new_part);
	}
	if (partitions - new_part > cnt_interval(td, gap_end, end)) new_part = partitions-cnt_interval(td, gap_end, end);
	if (details) {
	    printf ("gap - new part %d\n", new_part);
	}
	if (new_part > cnt_interval(td, start, gap_start)) new_part = cnt_interval(td, start, gap_start);
	if (new_part < 1) new_part = 1;
	if (new_part > partitions-1) new_part = partitions-1;
	if (details) {
	    printf ("gap - new part %d\n", new_part);
	}
	
	gen_timings (parts, td, start, gap_start, new_part);
	if (split > 1) {
	    print_utimings (td, gap_start, gap_end, split, intvl);
	} else {
	    add_timing (parts, gap_start, gap_end);
	}
	return gen_timings (parts, td, gap_end, end, partitions - new_part);
    } else {
	// Allocate first interval
	goal = total_time/partitions;
	if (details) {
	    printf ("step: goal is %.3f\n", goal);
	}
	for (i = start+1; i < end; i++) {
	    if (td[i].can_attach ) {
		double gap = estimate_dift(td, start, i);
		if (gap > goal || cnt_interval(td, i, end) == partitions-1) {	
//		    fprintf(stderr, "gap %lf goal %lf\n", gap, goal);
		    add_timing (parts, start, i);
		    return gen_timings(parts, td, i, end, partitions - 1);
		}
	    }
	}
    }
    return -1;
}

 //populate timings_data with info from the res log:
 void pop_with_kres(vector<struct timing_data> &td, 
		    u_long &call_clock,
		    const u_int index,
		    const klog_result *res) { 

     td[index].start_clock = res->start_clock;
     td[index].stop_clock = res->stop_clock;
     td[index].should_track = true;

     td[index].call_clock = call_clock;
     call_clock = res->stop_clock;
 }

 class my_comp
 {
 public:
     bool operator() (const pair<u_long,u_long>& lhs, const pair<u_long,u_long>&rhs) const
	 {
	     return lhs.second < rhs.second;
	 }
 };


 // There seems to be a better way of doing this by keeping multiple klog
 // files open at the same time. Then we only iterate through the timings list once. 
 int parse_klogs(vector<struct timing_data> &td,
		 const u_long stop_clock,
		 const char* dir)
 {
     struct klog_result* res;
     u_int lindex = 0;
     u_int stop_looking_index = numeric_limits<u_long>::max();
     u_long most_recent_clock = 0;
     pid_t pid = 0;

     char path[1024];

     priority_queue<pair<pid_t, u_long>> pids_to_track;
     pair<pid_t, u_long> new_pair = make_pair(td[0].pid, 0);
     pids_to_track.push(new_pair);

     while(!pids_to_track.empty()) { 
	 pair<pid_t, u_long> pid_pair = pids_to_track.top();
	 pids_to_track.pop();
	 most_recent_clock = 0; //indicates that we've found the first record for a process
	 pid = pid_pair.first;
	 if (most_recent_clock >= stop_looking_index) stop_looking_index = numeric_limits<u_long>::max();

	 sprintf (path, "%s/klog.id.%d", dir, pid);	
	 struct klogfile* log = parseklog_open(path);
	 if (!log) {
	     fprintf(stderr, "%s doesn't appear to be a valid klog file!\n", path);
	     return -1;
	 }
	 lindex = 0;

	 //our first pid has its record_timings off by one b/c we don't have timings data on exec
	 if (pid == td[0].pid) { 
	     res = parseklog_get_next_psr(log); 
	 }

	 while ((res = parseklog_get_next_psr(log)) != NULL && lindex < td.size()) {
	     while (td[lindex].pid != pid && lindex < td.size()) {
		 lindex++;
	     }
	     if (lindex >= td.size() || td[lindex].start_clock > stop_looking_index) break;


	     pop_with_kres(td, most_recent_clock, lindex, res);
	     if (stop_clock && td[lindex].start_clock > stop_clock) {		
		 td[lindex].should_track = false; 	       
	     }

	     //we found a fork
	     if (res->psr.sysnum == 120) {
//		 child_map.emplace(lindex, (pid_t)res->retval);
		 pair<pid_t, u_long> new_pair = make_pair(res->retval, res->start_clock);
		 pids_to_track.push(new_pair);		    
	     }
	     lindex++;
	 }    
	 parseklog_close(log);
     }
     return 0;
 }

 class ulog_data { 
 public: 
     ulog_data(int f, u_long tc): fd(f), total_clock(tc) {};
     ulog_data(): fd(0), total_clock(0) {};
     int fd;
     u_long total_clock; //current clock_index w/in the ulog
 }; 

 int open_ulog(const pid_t pid,
	       const char *dir) 
 { 
     char path[1024]; 
     int unused; 
     int fd;
     int rc;

     sprintf (path, "%s/ulog.id.%d", dir, pid);	    
     fd = open(path, O_RDONLY);
     if (fd < 0) { 
	 fprintf(stderr, "couldn't open ulog: %s\n",path);
	 return -1;
     }
     //we don't care about num_bytes, but just need to skip it in this fd:
     rc = read(fd, &unused, sizeof(int)); 
     (void) rc;
     return fd;
 }

 int pop_with_ulog(vector<struct timing_data> &td, u_int td_index, ulog_data &udata) 
 {
     u_long entry;
     u_long i;
     int skip, unused, rc;

     int fd = udata.fd;

     u_long last_pthread_block = 0; 

     //as long as udata.total_clock < start_clock, then they're user ops that ran
     //between the prev syscall and the current one. 

     while (udata.total_clock < td[td_index].start_clock) 
     { 
	 rc = read (fd, &entry, sizeof(u_long));
	 for (i = 0; i < (entry&CLOCK_MASK); i++) {
	     udata.total_clock++; 
	 }

	 //if this happens, it means that we call into the kernel
	 if (entry&SKIPPED_CLOCK_FLAG) {
	     rc = read (fd, &skip, sizeof(int));
	     udata.total_clock += skip + 1;
	     if (udata.total_clock < td[td_index].start_clock) {
		 last_pthread_block = udata.total_clock - 1; //the last clock val we are blocking
	     }
	 } else {
	     udata.total_clock++;
	 }
	 if (entry&NONZERO_RETVAL_FLAG) {
	     rc = read (fd, &unused, sizeof(int));
	 }
	 if (entry&ERRNO_CHANGE_FLAG) {
	     rc = read (fd, &unused, sizeof(int));
	 }
	 if (entry&FAKE_CALLS_FLAG) {
	     rc = read (fd, &unused, sizeof(int));
	 }
     }    
     if (last_pthread_block > td[td_index].call_clock) 
     { 
	 td[td_index].call_clock = last_pthread_block;
     }

     (void) rc;
     return 0;
 }

 int parse_ulogs(vector<struct timing_data> &td, const char* dir)
 {
     map<pid_t, ulog_data> pid_fd_map;
     u_int i;
     for (i = 0; i < td.size(); i ++) {
	 if (pid_fd_map.count(td[i].pid) == 0) {
	     int fd = open_ulog(td[i].pid, dir);
	     ulog_data u(fd, 0);
	     pid_fd_map[td[i].pid] = u;
	 }
	 if (pid_fd_map[td[i].pid].fd > 0)
	     pop_with_ulog(td, i, pid_fd_map[td[i].pid]);
     }
     return 0;
 }

void inline update_can_attach( vector<struct timing_data> &td,
			       const u_int td_index)
{
    if (bad_syscalls.count(td[td_index].syscall)) {
	//this means we have a bad syscall on the very first syscall, which evidently can happen (evince has this with a brk)
	//since the logic around the original fork covers from fork-> this sysall, we just need to block out things from the
	//start_clock onwards
	if (!td[td_index].call_clock) {
	    td[td_index].call_clock = td[td_index].start_clock;
	}
	int i = 0;
	for (auto &t : td){
          //if the record's attach clock occurs during this syscall
	    if (t.start_clock > td[td_index].call_clock &&
		t.start_clock < td[td_index].stop_clock) {
		t.can_attach = false;
	    }
	    i++;
	}
    }
}


 int parse_timing_data( vector<struct timing_data> &td) 
 {
     map<pid_t,u_int> last_time;
     u_int i;

     for (i = 0; i < td.size(); i++) {
	 if (! td[i].should_track) { 
	     td[i].can_attach = false; //mark it as not attachable, and continue
	     continue; 
	 }
	 update_can_attach(td, i);
     }

     u_long aindex = 1;
     for (auto &t : td) {
	 if (t.can_attach) { 
	     t.aindex = aindex++;
	 } else {
	     t.aindex = 0;
	 }
     }

     return 0;
 }

 int main (int argc, char* argv[])
 {
     char filename[256];
//     struct replay_timing* timings;
     struct stat st;
     int fd, rc, i, parts;
     u_int num;
     u_long stop_clock = 0;

     set<pid_t> procs;

     if (argc < 3) {
	 format ();
     }
     sprintf (filename, "%s/cloudlab.timings", argv[1]);
     parts = atoi(argv[2]);
     for (i = 3; i < argc; i++) {
	 if (!strcmp(argv[i], "-g")) {
	     i++;
	     if (i < argc) {
		 group_by = atoi(argv[i]);
	     } else {
		 format();
	     }
	 }
	 else if(!strcmp(argv[i], "--stop")) { 
	     i++;
	     if (i < argc) {
		 stop_clock = atoi(argv[i]);
		 fprintf(stderr, "stop clock is %lu\n",stop_clock);
	     } else {
		 format();
	     }

	 }
	 else if (!strcmp(argv[i], "-f")) {
	     i++;
	     if (i < argc) {
		 filter_syscall = atoi(argv[i]);
	     } else {
		 format();
	     }
	 }
	 else if (!strcmp(argv[i], "-v")) {
	     details = 1;
	 }
	 else if (!strcmp(argv[i], "-ds")) {
	     dump_stats = 1;
	 }
	 else if (!strcmp(argv[i], "-c")) {
	     use_ckpt = 1;
	 }
	 else if (!strcmp(argv[i], "-s")) {
	     use_syscalls = 1;
	 }
	 else {
	     //the assumption is that if we get to this point that its listing of the procs now
	     procs.insert(atoi(argv[i]));
	 }
     }

     fd = open (filename, O_RDONLY);
     if (fd < 0) {
	 sprintf (filename, "%s/timings", argv[1]);
	 fd = open (filename, O_RDONLY);
	 if (fd < 0) {
	     fprintf (stderr, "no timings files, rc=%d, errno=%d\n", fd, errno);
	     return -1;
	 }
     }

     rc = fstat (fd, &st);
     if (rc < 0) {
	 fprintf (stderr, "Cannot stat timing file, rc=%d, errno=%d\n", rc, errno);
	 return -1;
     }

     //timings = (struct replay_timing *) malloc (st.st_size);
     //if (timings == NULL) {
//	 fprintf (stderr, "Unable to allocate timings buffer of size %lu\n", st.st_size);
//	 return -1;
     //   }

//     rc = read (fd, timings, st.st_size);
//     if (rc < st.st_size) {
//	 fprintf (stderr, "Unable to read timings, rc=%d, expected %ld\n", rc, st.st_size);
//	 return -1;
//     }

     vector<struct timing_data> td (st.st_size / sizeof(struct replay_timing));
     /*
      * start by populating the td vector with all of the exiting
      * timings_info. 
      */

     struct replay_timing timing;
     rc = read(fd, &timing, sizeof(struct replay_timing));
     assert(rc == sizeof(struct replay_timing));
     num = 0;
     while (rc > 0)
     {
	 assert(rc == sizeof(struct replay_timing));
	 if (timing.pid == 0)
	     fprintf(stderr, "CARP! can't read @ %d\n", num);

	 assert(timing.pid != 0);
	 td[num].pid = timing.pid;
	 td[num].syscall = timing.syscall;
//	 td[num].forked_pid = -1; //just assume that syscalls don't have forked_pids. this will get fixed later if we're wrong
	 td[num].should_track = false; //assume that we don't track it, it will be fixed if we should
	 td[num].can_attach = true; //set it to be true first
	 rc = read(fd, &timing, sizeof(struct replay_timing));
	 num++;
     }
     if (rc < 0)
     {
	 fprintf(stderr, "huh, read failed %d %d\n",rc, errno);
     }

     assert ((num * sizeof(struct replay_timing)) == (u_int)st.st_size);

     rc = parse_klogs(td, stop_clock, argv[1]);
     rc = parse_ulogs(td, argv[1]);
     rc = parse_timing_data(td);


    if(dump_stats) {
	 for (auto t : td) { 
	     printf ("%d, %lu, %lu\n", t.pid, t.start_clock, t.stop_clock);
	 }
	 return 0;
    }

     //get final index:
     int lindex = td.size() - 1;
     while(td[lindex].aindex == 0) {
	 lindex --;
     }

    //this is just plain dumb how this works now
          vector<pair<int,int>> partitions;
     gen_timings(partitions, td, 0, lindex, parts);

    print_partitions(partitions, td);

    return 0;
}
