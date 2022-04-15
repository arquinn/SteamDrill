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
#include <iostream>


#include "parseklib.h"
#include "../linux-lts-quantal-3.5.0/include/linux/pthread_log.h"
using namespace std;

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
    u_long    index;
    short     syscall;
    u_int     ut; 
    int64_t   st; 
    bool      can_attach; 
    short     blocking_syscall; //the syscall number that is forcing this to be unattachable

    pid_t     forked_pid; //the pid of the forked process (if applicable).
    bool      should_track; //should I track this rp_timing? 
    u_long    call_clock;   //the clock value of the most recent sync operation
    u_long    start_clock;  //when the syscall starts
    u_long    stop_clock;   //when the syscall is done running
    u_long    aindex;

    //used to estimate dift time
    double    dtiming;
    double    stiming;
};


static int details = 0;


std::vector<u_long> ckpt_clocks; 

int cmp (const void* a, const void* b)
{
    return (u_long)a < (u_long)b;
}

void format ()
{
    fprintf (stderr, "Format: mkpartition <timing dir> <# of partitions> [-g group_by] [-f filter syscall] [-s split at user-level] [-v verbose] [-ds dump_stats] \[--fork fork_flags] [--stop stop_tracking_clock] <list of pids to track >\n");
    exit (22);
}


inline static void read_timing(vector<pair<int,int>> &parts, const char *epoch_file)
{
    int rc;
    FILE *file = fopen(epoch_file, "r");
    if (file == NULL) {
	fprintf (stderr, "Unable to open epoch description file %s, errno=%d\n", epoch_file, errno);
	return;
    }
    fprintf(stderr, "oepned file %p\n", file);


    while (!feof(file)) {
	char line[256];
	if (fgets (line, 255, file) > 0) {


	    char unusedFirst, unusedSecond;
	    int startPid;
	    int startClock, stopClock, ckptClock;
	    rc = sscanf (line,
			 "%d %c %u %c %u %u\n", 
			 &startPid, 
			 &unusedFirst,
			 &startClock,
			 &unusedSecond, 
			 &stopClock, 
			 &ckptClock);

	    if (rc != 6) {
		fprintf (stderr, "Unable to parse line of epoch descrtion file: %s\n", line);
		return;
	    }
	    parts.push_back(make_pair(startClock, stopClock));
	}
    }
    fclose(file);    
}

inline static void print_stats (vector<struct timing_data> &td, int start, int end, FILE *out)
{ 

//    fprintf(stderr, "found %lu %lu (%d %d): ", td[start].start_clock, td[end].start_clock, start, end);
//    fprintf (stderr, " %lf, %lf,  %lf, %lf\n", 
//	     td[end].dtiming, td[start].dtiming, td[end].stiming, td[start].stiming); 


    assert (end > start);
    assert (td[end].start_clock > td[start].start_clock);
    assert (td[end].dtiming > td[start].dtiming);
    assert (td[end].stiming > td[start].stiming);

    fprintf (out, "%6lu, %6lu, %d, %lu,  %lf, %lf\n", 
	    td[start].start_clock,
	    td[end].start_clock,
	    end - start,
	    td[end].start_clock - td[start].start_clock,
	    td[end].dtiming - td[start].dtiming,
	    td[end].stiming - td[start].stiming); 
}

inline static void get_stats(vector<pair<int, int>> &partitions, 
			     vector<struct timing_data> &td,
			     const char *outfilepath)
{
    u_int startIndex = 0, currIndex = 0, currPartition = 0; 
    FILE *outfile = fopen(outfilepath, "w+");
    if (outfile == NULL) {
	fprintf (stderr, "Unable to open out file %s, errno=%d\n", outfilepath, errno);
	return;
    }
    fprintf(stderr, "oepned file %p\n", outfile);

    if (partitions.size() == 1)
	currIndex = td.size();
    while (currIndex < td.size())
    {
	if (td[currIndex].start_clock == (u_long) partitions[currPartition].second)
	{
	    //okay, this WAS our attach index:
	    
	    if (td[currIndex].start_clock <= td[startIndex].start_clock)
	    {
		fprintf(stderr, "what gives? %d %d (%lu %lu)\n", startIndex, currIndex,
			td[startIndex].start_clock, td[currIndex].start_clock);
	    }
	    print_stats(td, startIndex, currIndex, outfile);
	    startIndex = currIndex;
	    currPartition++;
	    if (details) 
	    {
		fprintf(stderr, "stats for %d %d...", 
			partitions[currPartition].first,
			partitions[currPartition].second);
	    }

	    if (currPartition >= partitions.size() - 1)
	    {
		break;
	    }	    
	}
	++currIndex;
    }    

    print_stats(td, startIndex, td.size() - 1, outfile);
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
		 const char* dir, 
		 const char* fork_flags, 
		 const set<pid_t> procs)
 {
     struct klog_result* res;
     u_int lindex = 0;
     u_int stop_looking_index = numeric_limits<u_long>::max();
     u_long most_recent_clock = 0;
     pid_t pid = 0;
     u_int ff_index = 0;

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
	     if (res->psr.sysnum == 120 && procs.count(res->retval) > 0) { 
//		 if (details)
//		     fprintf(stderr,"%d, %lu (%u) forking %lu\n", td[lindex].pid, res->start_clock, lindex, res->retval);

		 if (fork_flags[ff_index] == '1') { 
		     //stop tracking this pid, already covered all of its traced syscalls
		     td[lindex].forked_pid = res->retval; //this is important for can_attach logic
		     stop_looking_index = res->start_clock; 
		     pair<pid_t, u_long> new_pair = make_pair(res->retval, res->start_clock);
		     pids_to_track.push(new_pair);
		     break; 
		 }
		 ff_index++;
	     }
	     //found a thread_fork
	     else if (res->psr.sysnum == 120) { 
		 if (details) 
		     fprintf(stderr,"%d, %lu (%u) forking %lu\n", td[lindex].pid, res->start_clock, lindex, res->retval);
		 td[lindex].forked_pid = res->retval; //this is important for can_attach logic
		 pair<pid_t, u_long> new_pair = make_pair(res->retval, res->start_clock);
		 pids_to_track.push(new_pair);		    
	     }
	     lindex++;
	 }    
	 parseklog_close(log);
     }
     return 0;
 }


 void inline update_ut( vector<struct timing_data> &td, 
			u_int &total_time,
			map<pid_t, u_int> &last_time,
			const u_int td_index) 
 { 
     pid_t pid = td[td_index].pid;
     auto iter = last_time.find(pid);
     if (iter == last_time.end()) {
	 total_time += td[td_index].ut;
     } else {
	 total_time += td[td_index].ut - iter->second;
     }
     last_time[pid] = td[td_index].ut;
     td[td_index].ut = total_time;
 }

 void inline update_st( vector<struct timing_data> &td, 
			int64_t &st_time,
			const u_int td_index) 
 { 
     if (td_index == 0)
     {
	 td[td_index].st = 0;
     }

     st_time += td[td_index].st;
     td[td_index].st = st_time;
 }



 int parse_timing_data( vector<struct timing_data> &td) 
 {
     u_int total_time = 0;
     int64_t st_time = 0;
 //    uint64_t total_cache_misses = 0;
     map<pid_t,u_int> last_time;
     u_int i, j, k; 

     for (i = 0; i < td.size(); i++) {
	 if (! td[i].should_track) { 
	     td[i].can_attach = false; //mark it as not attachable, and continue
	     continue; 
	 }

	 update_ut(td, total_time, last_time, i);
	 update_st(td, st_time, i);
     }

     u_long aindex = 1;
     for (auto &t : td) {
	 if (t.can_attach) { 
	     t.aindex = aindex++;
	 } else {
	     t.aindex = 0;
	 }
     }

     // interpolate timing vals when incrememnt is small 
     for (i = 0; i < td.size(); i++) {
	 for (j = i+1; j < td.size(); j++) {
	     if (td[i].ut != td[j].ut) break;
	 }
	 for (k = i; k < j; k++) {
	     td[k].dtiming = (double) td[k].ut + (double) (k-i) / (double) (j-i);
	 }
	 i = j-1;
     }


     // interpolate timing vals when incrememnt is small 
     for (i = 0; i < td.size(); i++) {
	 for (j = i+1; j < td.size(); j++) {
	     if (td[i].st != td[j].st) break;
	 }
	 for (k = i; k < j; k++) {
	     td[k].stiming = (double) td[k].st + (double) (k-i) / (double) (j-i);
	 }
	 i = j-1;
     }

     return 0;
 }

 int main (int argc, char* argv[])
 {
     char filename[256];
     struct replay_timing* timings;
     struct stat st;
     int fd, rc, num, i;
     u_long stop_clock = 0;

     char following[256];   
     char *epoch_filename, *output_dir ;
     set<pid_t> procs;


     output_dir = argv[2];

     if (argc < 3) {
	 format ();
     }
     sprintf (filename, "%s/cloudlab.timings", argv[1]);
     fd = open (filename, O_RDONLY);
     if (fd < 0) {
	 sprintf (filename, "%s/timings", argv[1]);
	 fd = open (filename, O_RDONLY);
	 if (fd < 0) {
	     fprintf (stderr, "no timings files, rc=%d, errno=%d\n", fd, errno);	
	     return -1;
	 }
	 fprintf (stderr, "using local timings file\n.");
     }

     rc = fstat (fd, &st);
     if (rc < 0) {
	 fprintf (stderr, "Cannot stat timing file, rc=%d, errno=%d\n", rc, errno);
	 return -1;
     }

     timings = (struct replay_timing *) malloc (st.st_size);
     if (timings == NULL) {
	 fprintf (stderr, "Unable to allocate timings buffer of size %lu\n", st.st_size);
	 return -1;
     }

     rc = read (fd, timings, st.st_size);
     if (rc < st.st_size) {
	 fprintf (stderr, "Unable to read timings, rc=%d, expected %ld\n", rc, st.st_size);
	 return -1;
     }

     num = st.st_size / sizeof(struct replay_timing);
     vector<struct timing_data> td(num); 

     /*
      * start by populating the td vector with all of the exiting
      * timings_info. 
      */

     for ( i = 0; i < num; i++) { 

	 struct timing_data next_td;
	 next_td.pid = timings[i].pid;
	 next_td.index = timings[i].index;
	 next_td.syscall = timings[i].syscall;
	 next_td.ut = timings[i].ut;
	 next_td.st = timings[i].st;
	 next_td.forked_pid = -1; //just assume that syscalls don't have forked_pids. this will get fixed later if we're wrong
	 next_td.should_track = false; //assume that we don't track it, it will be fixed if we should
	 next_td.can_attach = true; //set it to be true first
	 td[i] = next_td; 
     }
     free (timings);
     fprintf(stderr, "coppied %d entries\n", num);

     rc = parse_klogs(td, stop_clock, argv[1], following, procs);    
     rc = parse_timing_data(td);



     for (i = 3; i < argc; i++) {
	 epoch_filename = argv[i];
	 char output_file[256];
	 sprintf(output_file, "%s/%s", output_dir, basename(epoch_filename));

	 fprintf(stderr, "reading timing for %s\n", epoch_filename);

	 vector<pair<int,int>> partitions;
	 read_timing(partitions, epoch_filename);
	 get_stats(partitions, td, output_file);
     }



    return 0;
}
