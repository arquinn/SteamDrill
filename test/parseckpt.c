#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/resource.h>
#include <time.h>

// redefined from the kernel
#define _NSIGS 64
struct k_sigaction {
	unsigned long sa_handler;
	unsigned long sa_flags;
	unsigned long sa_restorer;
	unsigned long sa_mask;
	unsigned long ka_restorer;
};

int main (int argc, char* argv[])
{
    char buf[4096];
    char filename[4096];
    long copyed, args_cnt, env_cnt, len;
    int fd, i;
    pid_t record_pid;
    uint64_t rg_id;
    uint64_t parent_rg_id;
    struct rlimit rlimits[RLIM_NLIMITS];
    struct k_sigaction sighands[_NSIGS];
    struct timespec time;

    if (argc != 2) {
	printf ("format: parseckpt <dir>\n");
	return -1;
    }

    sprintf (buf, "%s/ckpt", argv[1]);
    fd = open (buf, O_RDONLY);
    if (fd < 0) {
	perror ("open");
	return fd;
    }

    copyed = read(fd, (char *) &record_pid, sizeof(record_pid));
    if (copyed != sizeof(record_pid)) {
	printf ("parseckpt: tried to read record pid, got rc %ld\n", copyed);
	return -1;
    }
    printf ("record pid: %d\n", record_pid);

    copyed = read(fd, (char *) &rg_id, sizeof(rg_id));
    if (copyed != sizeof(rg_id)) {
        printf ("parseckpt: tried to read rg_id, got %ld\n", copyed);
        return -1;
    }
    printf ("record group id: %llu\n", rg_id);

    copyed = read(fd, (char *) &parent_rg_id, sizeof(parent_rg_id));
    if (copyed != sizeof(parent_rg_id)) {
        printf ("parseckpt: tried to read parent_rg_id, got %ld\n", copyed);
        return -1;
    }
    printf ("parent record group id: %llu\n", parent_rg_id);

    copyed = read(fd, (char *) &len, sizeof(len));
    if (copyed != sizeof(len)) {
	printf ("parseckpt: tried to read filename size, got rc %ld\n", copyed);
	return -1;
    }

    copyed = read(fd, filename, len);
    if (copyed != len) {
	printf ("parseckpt: tried to read filename size, got rc %ld\n", copyed);
	return -1;
    }
    printf ("record filename: %s\n", filename);

    copyed = read(fd, (char *) &rlimits, sizeof(rlimits));
    if (copyed != sizeof(rlimits)) {
	printf ("parseckpt: tried to read rlimits, got rc %ld\n", copyed);
	return -1;
    }
    printf ("record pid: %d\n", record_pid);

    // Next, read the sighands
    copyed = read(fd, (char *) &sighands, sizeof(struct k_sigaction) * _NSIGS);
    if (copyed != (64 * 20)) {
        printf ("parseckpt: tried to read sighands, got %ld", copyed);
        return -1;
    }

    // Next, read the number of arguments
    copyed = read(fd, (char *) &args_cnt, sizeof(args_cnt));
    if (copyed != sizeof(args_cnt)) {
	printf ("parseckpt: tried to read record pid, got rc %ld\n", copyed);
	return -1;
    }
	
    // Now read in each argument
    for (i = 0; i < args_cnt; i++) {
	copyed = read(fd, (char *) &len, sizeof(len));
	if (copyed != sizeof(len)) {
	    printf ("parseckpt: tried to read argument %d len, got rc %ld\n", i, copyed);
	    return -1;
	}
	copyed = read(fd, buf, len);
	if (copyed != len) {
	    printf ("parseckpt: tried to read argument %d, got rc %ld\n", i, copyed);
	    return -1;
	}
	printf ("Argument %d is %s\n", i, buf);
    }

    // Next, read the number of env. objects
    copyed = read(fd, (char *) &env_cnt, sizeof(env_cnt));
    if (copyed != sizeof(env_cnt)) {
	printf ("parseckpt: tried to read record pid, got rc %ld\n", copyed);
	return -1;
    }

    // Now read in each env. object
    for (i = 0; i < env_cnt; i++) {
	copyed = read(fd, (char *) &len, sizeof(len));
	if (copyed != sizeof(len)) {
	    printf ("parseckpt: tried to read env. %d len, got rc %ld\n", i, copyed);
	    return -1;
	}
	copyed = read(fd, buf, len);
	if (copyed != len) {
	    printf ("parseckpt: tried to read env. %d, got rc %ld\n", i, copyed);
	    return -1;
	}
	printf ("Env. var. %d is %s\n", i, buf);
    }

    copyed = read(fd, (char *) &time, sizeof(time));
    if (copyed != sizeof(time)) {
        printf ("parseckpt: tried to read time, got %ld\n", copyed);
	printf ("This might be an older recording that doesn't have this time\n");
	return -1;
    }
    printf ("time of replay is: %ld sec %ld nsec\n", time.tv_sec, time.tv_nsec);

    close (fd);
    return 0;
}
