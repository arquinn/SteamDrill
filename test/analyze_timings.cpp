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


//used simply for the read from the file system
struct replay_timing {
    pid_t     pid;
    u_long    index;
    short     syscall;
    u_int     ut;
    uint64_t  st;
    uint64_t  cache_misses;
};

//used everywhere else
struct timing_data {
    pid_t     pid;
    u_long    index;
    short     syscall;
    u_int     ut;      //should I not include this as it is only used to calculate dtiming? 
    u_int     st;      //should I not include this as it is only used to calculate dtiming? 

    bool      can_attach; 
    short     blocking_syscall; //the syscall number that is forcing this to be unattachable

    pid_t     forked_pid; //the pid of the forked process (if applicable).
    bool      should_track; //should I track this rp_timing? 
    u_long    call_clock;   //the clock value of the most recent sync operation before this syscall is called. 
    u_long    start_clock;  //when the syscall starts
    u_long    stop_clock;   //when the syscall is done running
    u_long    aindex;

    //used to estimate dift time
    double    dtiming;
    uint64_t  cache_misses;
};

struct syscall_data {
    u_long count;
    double time;
};

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

int parse_timing_data( vector<struct timing_data> &td) 
{
    u_int total_time = 0;
    map<pid_t,u_int> last_time;
    u_int i, j, k; 

    for (i = 0; i < td.size(); i++) {
	update_ut(td, total_time, last_time, i);
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

    return 0;
}

int main (int argc, char* argv[])
{
    char filename[256];
    struct replay_timing* timings;
    struct stat st;
    int fd, rc, num, i;

    sprintf (filename, "%s/timings", argv[1]);

    fd = open (filename, O_RDONLY);
    if (fd < 0) {
	fprintf (stderr, "Cannot open timing file %s, rc=%d, errno=%d\n", filename, fd, errno);
	return -1;
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
       
	next_td.cache_misses = timings[i].cache_misses;
	next_td.forked_pid = -1; //just assume that syscalls don't have forked_pids. this will get fixed later if we're wrong
	next_td.should_track = false; //assume that we don't track it, it will be fixed if we should
	next_td.can_attach = true; //set it to be true first
	td[i] = next_td; 

    }
    rc = parse_timing_data(td);

    std::unordered_map<u_long, struct syscall_data> syscallCrap;

    for (auto &t : td)
    {
	auto crapIter = syscallCrap.find(t.syscall);
	if (crapIter == syscallCrap.end())
	{
	    struct syscall_data sd;
	    sd.count = 0;
	    sd.time = 0;
	    crapIter = syscallCrap.insert(std::make_pair(t.syscall, sd)).first;
	}

	// how long did this take: 
	double time = t.st;

	crapIter->second.count++;
	crapIter->second.time += (time / 1000000);
    }

    for (auto &crap : syscallCrap)
    {
	printf("%lu: %lu %lf %lf\n", crap.first, crap.second.count, crap.second.time, crap.second.time / crap.second.count);
    }
    

//    for (auto t : td) { 
//	printf ("%d %d (%lf %u) (%lf %u)\n", t.pid, t.syscall, t.dtiming, t.ut, t.stiming, t.st);
//    }

    return 0;
}


