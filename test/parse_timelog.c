#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

//used simply for the read from the file system
struct record_timing {
    uint32_t clock;
    int64_t  elapsed;
};



void print_usage()
{

    printf("./parse_timelog <time_log>\n");
}

void print_args(int argc, char *argv[])
{
    for (int i = 0; i < argc; ++i)
    {
	printf("%d: %s\n", i, argv[i]);
    }
}

int main (int argc, char* argv[])
{
    char filename[256];
    struct record_timing* timings;
    struct stat st;
    int fd = 0, rc = 0;
    u_long num;
    int64_t timing = 0;
    if (argc < 2)
    {
	print_usage();
	print_args(argc, argv);
	return -1;
    }

    sprintf (filename, "%s", argv[1]);

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

    timings = (struct record_timing *) malloc (st.st_size);
    if (timings == NULL) {
	fprintf (stderr, "Unable to allocate timings buffer of size %lu\n", st.st_size);
	return -1;
    }
   
    rc = read (fd, timings, st.st_size);
    if (rc < st.st_size) {
	fprintf (stderr, "Unable to read timings, rc=%d, expected %ld\n", rc, st.st_size);
	return -1;
    }

    num = st.st_size / sizeof(struct record_timing);


    for (struct record_timing *curr_time = timings,
	     *end_time = timings + num;
	 curr_time < end_time; 
	 curr_time++)
    {

	printf ("%u %llu\n", curr_time->clock, curr_time->elapsed);
    }
}
