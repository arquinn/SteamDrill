#ifndef __DEVSPEC_H__
#define __DEVSPEC_H__

#define SPEC_PSDEV_MAJOR 149

#define SPEC_NAME "spec0"
#define SPEC_DEV "/dev/" SPEC_NAME


#define ROLLED_BACK 1

struct record_data {
	u_long                           app_syscall_addr;
	const char __user *const __user *args;
	const char __user *const __user *env;
	int                              save_mmap;
	char __user *                    linkpath;
	int                              fd;
	char __user *                    logdir;
	int                              pipe_fd;
};

struct wakeup_data {
	int             pin;
	int             gdb;
	char __user *   logdir;
	char __user *   linker;
        char __user *   uniqueid;
	int             fd;
	int             follow_splits;
	loff_t          attach_index;
	int             attach_pid;
	int	        save_mmap;
	int             ckpt_at;
	int             record_timing;
	u_long          nfake_calls;
	u_long __user * fake_calls;
        u_long          exit_clock;
};

struct wakeup_ckpt_data {
	int           pin;
	int           gdb;
	char __user * logdir;
	char __user * filename;
	char __user * linker;
        char __user * uniqueid;
	int           fd;
	int           follow_splits;
	loff_t        attach_index;
	int           attach_pid;
	int	      save_mmap;
        int           ckpt_pos;
	u_long          nfake_calls;
	u_long __user * fake_calls;
        u_long          exit_clock;    
};

struct get_used_addr_data {
	struct used_address __user * plist;
	int                          nlist;
};

struct replay_stats_data {
	int started;
	int finished;
	int mismatched;
};

struct filemap_num_data {
	int fd;
	loff_t offset;
	int size;
};

struct filemap_entry_data {
	int fd;
	loff_t offset;
	int size;
	void __user* entries;
	int num_entries;
};

struct open_fds_data {
	void __user* entries;
	int num_entries;
};

struct replay_processes_data {
        pid_t pid;
        int __user* entries;
	int num_entries;
};

struct get_record_pid_data {
	pid_t nonrecordPid;
};

struct get_replay_pid_data {
	pid_t record_pid;
        pid_t parent_pid;
};


struct set_pin_address_data {
	u_long pin_address;
	u_long pthread_data;
	u_long __user* pcurthread;
	int attach_ndx;
};

struct redo_mmap_data {
	u_long rc;
	u_long len;
};

struct ptrace_add_output_data {
        int fd; 
        const char __user * output_name;
        pid_t pid;
};

struct mprotect_other_data {
        u_long start;
        size_t len;
        u_long prot;
        pid_t pid;
};

struct ignore_segfault_data {
        int ignore_segfault;
        pid_t pid;
};

struct get_record_time_data {
        int64_t output;
};

struct ptrace_syscall_end_data {
        pid_t pid;
        int **pthread_status;
        int **libc_pthread_status;
};

struct report_syscall_data {
        pid_t pid;
        u_long clock;
};

struct set_active_tracers_data {
        pid_t pid;
        int active;
};

struct add_counter_data {
        int __user *counter;
        int is_special;
};

struct get_counter_data {
        pid_t pid;
        int64_t output;
};


#define SPECI_REPLAY_FORK _IOR('u', 0, struct record_data)
#define SPECI_RESUME _IOR('u', 1, struct wakeup_data)
#define SPECI_SET_PIN_ADDR _IOWR('u',2,struct set_pin_address_data)
#define SPECI_CHECK_BEFORE _IOR('u',3,int)
#define SPECI_CHECK_AFTER _IOR('u',4,int)
#define SPECI_GET_LOG_ID _IOR('u',5, pid_t)
#define SPECI_GET_USED_ADDR _IOR('u',6,struct get_used_addr_data)
#define SPECI_GET_REPLAY_STATS _IOW('u',7,struct replay_stats_data)
#define SPECI_GET_CLOCK_VALUE _IO('u',8)
#define SPECI_GET_REPLAY_ARGS _IO('u',9)
#define SPECI_GET_ENV_VARS _IO('u',10)
#define SPECI_GET_RECORD_GROUP_ID _IOW('u',11, u_long)
#define SPECI_GET_NUM_FILEMAP_ENTRIES _IOR('u',12,struct filemap_num_data)
#define SPECI_GET_FILEMAP _IOR('u', 13,struct filemap_entry_data)
#define SPECI_RESET_REPLAY_NDX _IO('u', 14)
#define SPECI_GET_CURRENT_RECORD_PID _IOR('u', 15, struct get_record_pid_data)
#define SPECI_CKPT_RESUME _IOR('u', 16, struct wakeup_ckpt_data)
#define SPECI_CKPT_PROC_RESUME _IOR('u', 17, struct wakeup_ckpt_data)
#define SPECI_GET_ATTACH_STATUS _IOR('u', 18, pid_t)
#define SPECI_WAIT_FOR_REPLAY_GROUP _IOR('u', 19, pid_t)
#define SPECI_TRY_TO_EXIT _IOR('u', 20, pid_t)
#define SPECI_GET_REPLAY_PID _IOR('u', 21, struct get_replay_pid_data)
#define SPECI_MAP_CLOCK _IO('u',22)
#define SPECI_GET_OPEN_FDS _IOR('u', 23, struct open_fds_data)
#define SPECI_CHECK_FOR_REDO _IO('u', 24)
#define SPECI_REDO_MMAP _IOW('u', 25, struct redo_mmap_data)
#define SPECI_IS_PIN_ATTACHING _IO('u', 26)
#define SPECI_REDO_MUNMAP _IO('u', 27)
#define SPECI_PTRACE_SYSCALL_BEGIN _IOR('u', 28, pid_t)
#define SPECI_PTRACE_SYSCALL_END _IOW('u', 29,  struct ptrace_syscall_end_data)
#define SPECI_MAP_OTHER_CLOCK _IOR('u', 30, pid_t)
#define SPECI_GET_REPLAY_PROCESSES _IOR('u', 31, struct replay_processes_data)
#define SPECI_UPDATE_ATTACHING_STATUS _IOWR('u', 32, pid_t)
#define SPECI_PTRACE_ADD_OUTPUT _IOR('u', 33, struct ptrace_add_output_data)
#define SPECI_WAIT_FOR_ATTACH _IOR('u', 34, pid_t)
#define SPECI_MPROTECT_OTHER _IOR('u', 35, struct mprotect_other_data)
#define SPECI_GET_FAULT_ADDRESS _IOR('u', 36, pid_t)
#define SPECI_SET_IGNORE_SEGFAULT _IOW('u', 37, struct ignore_segfault_data)
#define SPECI_GET_RECORD_TIME _IOW('u', 38, struct get_record_time_data)
#define SPECI_GET_NEXT_RECORD_TIME _IOW('u', 39, struct get_record_time_data)
#define SPECI_GET_LOG_TGID _IO('u',40)
#define SPECI_SET_PTRACE_ADDR _IOR('u', 41, int*)
#define SPECI_SET_REPORT_SYSCALL _IOR('u', 42, struct report_syscall_data)
#define SPECI_SET_ACTIVE_TRACERS _IOR('u', 43, struct report_syscall_data)
#define SPECI_ADD_PERF_FD _IOR('u', 44, int)
#define SPECI_ADD_COUNTER _IOR('u', 45, struct add_counter_data)
#define SPECI_GET_COUNTER _IOWR('u', 46, struct get_counter_data)

#endif
