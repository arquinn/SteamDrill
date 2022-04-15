#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include <dirent.h>
#include <cstdint>
#include <math.h>
#include <map>
#include <queue>
#include <set>
#include <algorithm>
#include <limits>


#include "mkpartition_utils.h"
using namespace std;

struct ckpt ckpts[MAX_CKPT_CNT];
int ckpt_cnt = 0;

static int group_by = 0;
int filter_syscall = 0;
int use_ckpt = 0, do_split = 0, verbose = 0, do_repartition = 0;
int lowmem = 0;
double ut_arg = 0.0;
double ui_arg = 0.0;
my_map ninsts; //the number of instructions based on trace_id


void format ()
{
    fprintf (stderr, "Format: parse_timings <replay_dir> <timing file> [-v verbose] [--stop stop_tracking_clock] [-fork fork_flags] <list of processes in replay>\n");
    exit (22);
}

int cmp (const void* a, const void* b)
{
    const struct ckpt* c1 = (const struct ckpt *) a;
    const struct ckpt* c2 = (const struct ckpt *) b;
    return c1->rp_clock - c2->rp_clock;
}


static long ms_diff (struct timeval tv1, struct timeval tv2)
{
    return ((tv1.tv_sec - tv2.tv_sec) * 1000 + (tv1.tv_usec - tv2.tv_usec) / 1000);
}

bool mycmp(struct timing_data t, struct timing_data u) {return t.start_clock < u.start_clock;}

int main (int argc, char* argv[])
{
    char filename[256];
    struct replay_timing* timings;
    struct stat st;
    int fd, rc, num, i, parts;
    char following[256];   
    unordered_set<pid_t> procs;
    FILE *file;

    u_long stop_clock = 0;

    char *timings_file, *replay_dir;
    
    if (argc < 3) {
	format ();
    }
    
    replay_dir = argv[1];
    timings_file = argv[2];

    sprintf (filename, "%s", timings_file);
    
    for (i = 2; i < argc; i++) {
	if(!strcmp(argv[i], "-fork")) { 
	    i++;
	    if (i < argc) {
		strcpy(following,argv[i]);
	    } else {
		format();
	    }	
	}
	else if (!strcmp(argv[i], "-v")) {
	    verbose = 1;
	}
	else if (!strcmp(argv[i], "-s")) {
	    do_split = 1;
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
	else { 
	    //the assumption is that if we get to this point that its listing of the procs now
	    procs.insert(atoi(argv[i]));
	}
    }


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

    for ( i = 0; i < num; i++) 
    {		
	struct timing_data next_td;
	next_td.pid = timings[i].pid;
	next_td.index = timings[i].index;
	next_td.syscall = timings[i].syscall;
	next_td.start_clock = numeric_limits<u_long>::max();
	next_td.ut = timings[i].ut;
	next_td.fft  = timings[i].ut;
	next_td.taint_in = 0;
	next_td.taint_out = 0;	
	next_td.imisses = 0;
	next_td.num_merges = 0;
	next_td.cache_misses = timings[i].cache_misses;
	next_td.forked_pid = -1; // assume that syscalls don't have forked_pids. we fix this later if wrong
	next_td.should_track = false; //assume that we don't track it, it will be fixed if we should
	next_td.can_attach = true; //set it to be true first
	td[i] = next_td; 
    }
    free(timings); //free all dem memories

    rc = parse_klogs(td, stop_clock, replay_dir, following, procs);

    if (verbose)
    {
	for (auto t : td) 
	{
	    printf ("%d, %lu, %lu, %d, %d\n", t.pid, t.start_clock, t.stop_clock, t.ut, t.fft);
	    fflush(stdout);
	}
    }

    rc = parse_timing_data(td);
    fprintf(stderr, "beginning the sort\n");

    sort (td.begin(), td.end(), mycmp); //do this before I assign can_attach
    fprintf(stderr, "finished sorting, parsing td\n");


    for (auto t : td) { 
	printf ("%d, %lu, %lu, %lf, %lf\n", t.pid, t.start_clock, t.stop_clock, t.ftiming, t.dtiming);
	fflush(stdout);
    }


    return 0;
}


