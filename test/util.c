//-*-c-basic-offset: 4 -*-
// ^^ so that emacs will live with this formatting
#include <sys/ioctl.h> // ioctl
#include <sys/stat.h>  // open
#include <sys/types.h> // fork, wait
#include <sys/wait.h>  // wait
#include <sys/mman.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>   // malloc
#include <string.h>    // memset
#include <errno.h>     // errno
#include <fcntl.h>     // open
#include <unistd.h>   // write, close
#include <stdarg.h>
#include <errno.h>
#include "util.h"
#define __user 
#include "dev/devspec.h" 

int debugLevel = 0;

#define PAGE_SIZE 4096

#define SUCPRINT(x,...)

#ifndef AUTOBASH_OPT
void DPRINT(int lvl, char* fmt,...)
{
    va_list ap;
    if (lvl <= debugLevel) {
	va_start(ap,fmt);
	vfprintf(stderr,fmt,ap);
	fprintf(stderr,"\n");
	va_end(ap);
    }
}
#endif

void EPRINT(char* fmt,...) 
{
    va_list ap; 
    time_t    now = time(NULL);
    struct tm tmnow;
    localtime_r (&now, &tmnow);
    fprintf (stderr, "%02d/%02d %02d:%02d:%02d ", tmnow.tm_mon, tmnow.tm_mday,
	     tmnow.tm_hour, tmnow.tm_min, tmnow.tm_sec);
    va_start(ap,fmt); 
    vfprintf(stderr,fmt,ap); 
    fprintf(stderr,", errno=%d\n", errno);
    va_end(ap); 
}

int devspec_init (int* fd_spec)
{
    // yysu: prepare for speculation
    *fd_spec = open (SPEC_DEV, O_RDWR);
    if (*fd_spec < 0) {
	EPRINT ("cannot open spec device");
	return errno;
    }

    return 0;
}

int replay_fork (int fd_spec, const char** args, const char** env,
    char* linkpath, char* logdir, int save_mmap, int pipe_fd)
{
    struct record_data data;
    data.args = args;
    data.env = env;
    data.linkpath = linkpath;
    data.save_mmap = save_mmap;
    data.fd = fd_spec;
    data.logdir = logdir;
    data.pipe_fd = pipe_fd;
    return ioctl (fd_spec, SPECI_REPLAY_FORK, &data);
}

int resume_with_ckpt (int fd_spec, int pin, int gdb, int follow_splits, int save_mmap, 
		      char* logdir, char* linker, loff_t attach_index, int attach_pid, int ckpt_at,
		      int record_timing, u_long nfake_calls, u_long* fake_calls, u_long exit_clock)
{
    struct wakeup_data data;
    data.pin = pin;
    data.gdb = gdb;
    data.logdir = logdir;
    data.linker = linker;
    data.fd = fd_spec;
    data.follow_splits = follow_splits;
    data.save_mmap = save_mmap;
    data.attach_index = attach_index;
    data.attach_pid = attach_pid;
    data.ckpt_at = ckpt_at;
    data.record_timing = record_timing;
    data.nfake_calls = nfake_calls;
    data.fake_calls = fake_calls;
    data.exit_clock = exit_clock;
    return ioctl (fd_spec, SPECI_RESUME, &data);    
}

int resume (int fd_spec, int pin, int gdb, int follow_splits, int save_mmap, 
	    char* logdir, char* linker, loff_t attach_index, int attach_pid, int record_timing,
	    u_long nfake_calls, u_long* fake_calls, u_long exit_clock)
{
    return resume_with_ckpt (fd_spec, pin, gdb, follow_splits, save_mmap, logdir, linker, 
			     attach_index, attach_pid, 0, record_timing, nfake_calls, fake_calls, exit_clock);
}

int resume_after_ckpt (int fd_spec, int pin, int gdb, int follow_splits, int save_mmap, 
		       char* logdir, char* linker, char* filename,char* uniqueid, loff_t attach_index, int attach_pid
		       , u_long nfake_calls, u_long* fake_calls, u_long exit_clock)
{
    // fprintf(stderr, "calling resume_after_ckpt\n");
    struct wakeup_ckpt_data data;
    data.pin = pin;
    data.gdb = gdb;
    data.logdir = logdir;
    data.filename = filename;
    data.linker = linker;
    data.uniqueid = uniqueid;
    data.fd = fd_spec;
    data.follow_splits = follow_splits;
    data.save_mmap = save_mmap;
    data.attach_index = attach_index;
    data.attach_pid = attach_pid;
    data.nfake_calls = nfake_calls;
    data.fake_calls = fake_calls;
    data.exit_clock = exit_clock;
    return ioctl (fd_spec, SPECI_CKPT_RESUME, &data);    

}

int resume_proc_after_ckpt (int fd_spec, char* logdir, char* filename, char* uniqueid, int ckpt_pos)
{
    struct wakeup_ckpt_data data;
    data.logdir = logdir;
    data.filename = filename;
    data.uniqueid = uniqueid;
    data.ckpt_pos = ckpt_pos;
    data.fd = fd_spec;
    return ioctl (fd_spec, SPECI_CKPT_PROC_RESUME, &data);    
}

int set_pin_addr (int fd_spec, u_long app_syscall_addr, void* pthread_data, void** pcurthread, int* pattach_ndx)
{
    struct set_pin_address_data data;
    long rc;

    data.pin_address = app_syscall_addr;
    data.pthread_data = (u_long) pthread_data;
    data.pcurthread = (u_long *) pcurthread;
    rc = ioctl (fd_spec, SPECI_SET_PIN_ADDR, &data);
    *pattach_ndx = data.attach_ndx;
    return rc;
}

int check_clock_before_syscall (int fd_spec, int syscall)
{
    return ioctl (fd_spec, SPECI_CHECK_BEFORE, &syscall);
}

int check_clock_after_syscall (int fd_spec)
{
    return ioctl (fd_spec, SPECI_CHECK_AFTER);
}

long check_for_redo (int fd_spec)
{
    return ioctl (fd_spec, SPECI_CHECK_FOR_REDO);
}

long redo_mmap (int fd_spec, u_long* prc, u_long* plen) 
{
    struct redo_mmap_data redo;
    long retval =  ioctl (fd_spec, SPECI_REDO_MMAP, &redo);
    *prc = redo.rc;
    *plen = redo.len;
    return retval;
}
long redo_munmap (int fd_spec)
{
    long retval =  ioctl (fd_spec, SPECI_REDO_MUNMAP);
    return retval;
}

int get_log_id (int fd_spec, pid_t pid)
{
    return ioctl (fd_spec, SPECI_GET_LOG_ID, &pid);
}

int get_log_tgid (int fd_spec)
{
    return ioctl (fd_spec, SPECI_GET_LOG_TGID);
}

long get_clock_value (int fd_spec)
{
    return ioctl (fd_spec, SPECI_GET_CLOCK_VALUE);
}

int get_used_addresses (int fd_spec, struct used_address* paddrs, int naddrs)
{
    struct get_used_addr_data data;
    data.plist = paddrs;
    data.nlist = naddrs;
    return ioctl (fd_spec, SPECI_GET_USED_ADDR, &data);
}

int get_replay_stats (int fd_spec, struct replay_stat_data* stats)
{
    return ioctl (fd_spec, SPECI_GET_REPLAY_STATS, stats);
}

unsigned long get_replay_args (int fd_spec)
{
    return ioctl (fd_spec, SPECI_GET_REPLAY_ARGS);
}

unsigned long get_env_vars (int fd_spec)
{
    return ioctl (fd_spec, SPECI_GET_ENV_VARS);
}

int get_record_group_id (int fd_spec, uint64_t* rg_id)
{
    return ioctl (fd_spec, SPECI_GET_RECORD_GROUP_ID, rg_id);
}

int get_num_filemap_entries (int fd_spec, int fd, loff_t offset, int size)
{
    struct filemap_num_entry fnentry;
    fnentry.fd = fd;
    fnentry.offset = offset;
    fnentry.size = size;
    return ioctl (fd_spec, SPECI_GET_NUM_FILEMAP_ENTRIES, &fnentry);
}

int get_filemap (int fd_spec, int fd, loff_t offset, int size, void* entries, int num_entries) 
{
    struct filemap_entry fentry;
    fentry.fd = fd;
    fentry.offset = offset;
    fentry.size = size;
    fentry.entries = entries;
    fentry.num_entries = num_entries;
    return ioctl (fd_spec, SPECI_GET_FILEMAP, &fentry);
}

int get_open_fds (int fd_spec, struct open_fd* entries, int num_entries) 
{
    struct open_fds_data oentry;
    oentry.entries = entries;
    oentry.num_entries = num_entries;
    return ioctl (fd_spec, SPECI_GET_OPEN_FDS, &oentry);
}

long reset_replay_ndx(int fd_spec)
{
    return ioctl (fd_spec, SPECI_RESET_REPLAY_NDX);
}

pid_t get_current_record_pid(int fd_spec, pid_t nonrecord_pid)
{
    struct get_record_pid_data data;
    data.nonrecordPid = nonrecord_pid;
    return ioctl(fd_spec, SPECI_GET_CURRENT_RECORD_PID, &data);
}

long get_attach_status(int fd_spec, pid_t pid)
{
    return ioctl (fd_spec, SPECI_GET_ATTACH_STATUS, &pid);
}

int wait_for_replay_group(int fd_spec, pid_t pid) 
{
    return ioctl(fd_spec,SPECI_WAIT_FOR_REPLAY_GROUP, &pid);
}

long try_to_exit(int fd_spec, pid_t pid)
{
    return ioctl (fd_spec, SPECI_TRY_TO_EXIT, &pid);
}


pid_t get_replay_pid(int fd_spec, pid_t parent_pid, pid_t record_pid)
{
    struct get_replay_pid_data data;
    data.parent_pid = parent_pid;
    data.record_pid = record_pid;

    return ioctl (fd_spec, SPECI_GET_REPLAY_PID, &data);
}

int is_pin_attaching(int fd_spec)
{
    return ioctl (fd_spec, SPECI_IS_PIN_ATTACHING);
}



long ptrace_syscall_begin(int fd_spec, pid_t pid)
{
    return ioctl (fd_spec, SPECI_PTRACE_SYSCALL_BEGIN, &pid);
}

long ptrace_syscall_end (int fd_spec, pid_t pid, int **pthread_status, int **libc_pthread_status)
{
    struct ptrace_syscall_end_data data;
    data.pid = pid;
    data.pthread_status = pthread_status;
    data.libc_pthread_status = libc_pthread_status;
    return ioctl (fd_spec, SPECI_PTRACE_SYSCALL_END, &data);
}


u_long* map_shared_clock (int fd_spec)
{
    u_long* clock;

    int fd = ioctl (fd_spec, SPECI_MAP_CLOCK);
    if (fd < 0) {
	fprintf (stderr, "map_shared_clock: iotcl returned %d, errno=%d\n", fd, errno);
	return NULL;
    }

    clock = mmap (0, 4096, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (clock == MAP_FAILED) {
	fprintf (stderr, "Cannot setup shared page for clock\n");
	return NULL;
    }

    close (fd);
    return clock;
}

u_long* map_other_clock (int fd_spec, pid_t otherpid)
{
    u_long* clock;
    pid_t pid = otherpid; 
    int fd = ioctl (fd_spec, SPECI_MAP_OTHER_CLOCK, &pid);
    if (fd < 0) {
	fprintf (stderr, "map_shared_clock: iotcl returned %d, errno=%d\n", fd, errno);
	fprintf (stderr, "pid %d addr %p\n",pid, &pid);
	return NULL;
    }

    clock = mmap (0, 4096, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (clock == MAP_FAILED) {
	fprintf (stderr, "Cannot setup shared page for clock\n");
	return NULL;
    }

    close (fd);
    return clock;
}

int get_replay_processes (int fd_spec, pid_t pid, int* entries, int num_entries)
{
    struct replay_processes_data data;
    data.pid = pid; 
    data.entries = entries;
    data.num_entries = num_entries; 
    
    return ioctl(fd_spec, SPECI_GET_REPLAY_PROCESSES, &data);
}

int update_attaching_status (int fd_spec, pid_t pid)
{    
    return ioctl(fd_spec, SPECI_UPDATE_ATTACHING_STATUS, &pid);
}


int ptrace_add_output (int fd_spec, int fd, const char *output_name, pid_t pid)
{
    struct ptrace_add_output_data data;
    data.fd = fd;
    data.output_name = output_name;
    data.pid = pid;
    
    return ioctl(fd_spec, SPECI_PTRACE_ADD_OUTPUT, &data);
}

long wait_for_attach(int fd_spec, pid_t pid)
{
    return ioctl (fd_spec, SPECI_WAIT_FOR_ATTACH, &pid);
}

long mprotect_other (int fd_spec, u_long start, size_t len, u_long prot, pid_t pid)
{
    struct mprotect_other_data data;
    data.start = start;
    data.len = len;
    data.prot = prot;
    data.pid = pid;

    return ioctl (fd_spec, SPECI_MPROTECT_OTHER, &data);
}


u_long get_fault_address (int fd_spec, pid_t pid)
{
    return ioctl (fd_spec, SPECI_GET_FAULT_ADDRESS, &pid);
}

int ignore_segfaults(int fd_spec, int ignore, pid_t pid)
{
    struct ignore_segfault_data data;
    data.pid = pid;
    data.ignore_segfault = ignore;
    //fprintf(stderr, "ignore_setfaults %d %d %lu\n", fd_spec, pid, ignore);
    return ioctl(fd_spec, SPECI_SET_IGNORE_SEGFAULT, &data);
}


int get_record_time(int fd_spec, int64_t *time)
{
    struct get_record_time_data data;

    int rtnVal = ioctl(fd_spec, SPECI_GET_RECORD_TIME, &data);
//    fprintf(stderr, "get_record_time %d, %lld\n", rtnVal, data.ns);
    *time = data.output;
    return rtnVal;
}

int get_next_record_time(int fd_spec, int64_t *time)
{
    struct get_record_time_data data;

    int rtnVal = ioctl(fd_spec, SPECI_GET_NEXT_RECORD_TIME, &data);
//    fprintf(stderr, "get_record_time %d, %lld\n", rtnVal, data.ns);
    *time = data.output;
    return rtnVal;
}

int set_ptrace_addr(int fd_spec, int *ptrace_addr) {
    return ioctl(fd_spec, SPECI_SET_PTRACE_ADDR, &ptrace_addr);
}

int set_report_syscall(int fd_spec, pid_t pid, u_long clock) {
    struct report_syscall_data data;
    int rc;
    data.pid = pid;
    data.clock = clock;
    rc = ioctl(fd_spec, SPECI_SET_REPORT_SYSCALL, &data);
    return rc;
}

int set_active_tracers(int fd_spec, pid_t pid, int active) {
    struct set_active_tracers_data data;
    int rc;
    data.pid = pid;
    data.active = active;

    rc = ioctl(fd_spec, SPECI_SET_ACTIVE_TRACERS, &data);
    return rc;
}

int add_perf_fd(int fd_spec, int perf_fd) {
    return ioctl(fd_spec, SPECI_ADD_PERF_FD, &perf_fd);
}

int add_ticking_counter(int fd_spec, int * counter) {
    struct add_counter_data data;
    data.counter = counter;
    data.is_special = 1;
    return ioctl(fd_spec, SPECI_ADD_COUNTER, &data);
}
int add_counter(int fd_spec, int *counter) {
    struct add_counter_data data;
    data.counter = counter;
    data.is_special = 0;
    return ioctl(fd_spec, SPECI_ADD_COUNTER, &data);
}

int get_counter(int fd_spec, pid_t pid, int64_t *time) {
    struct get_counter_data data;
    data.pid = pid;
    int retval = ioctl(fd_spec, SPECI_GET_COUNTER, &data);
    *time = data.output;
    return retval;
}
