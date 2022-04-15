#include "pin.H"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <assert.h>
#include <sys/types.h>
#include <syscall.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sched.h>
#include <errno.h>
#include <stdint.h>
#include <netdb.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <linux/net.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <map>
#include <unordered_map>
using namespace std;

#include "util.h"
#include "list.h"
#include "bw_slice.h"
#include "graph_interface/graph_interface.h"
#include "graph_interface/filters.h"
#include "graph_interface/input_output.h"
#include "xray_monitor.h"
#include "taint_nw.h"

#ifdef DIFT
#include "graph_interface/taint_linkage_function.h"
#elif NO_TRACK
#include "graph_interface/null_linkage_function.h"
#else
#include "graph_interface/slice_linkage_function.h"
#endif

#define PIN_NORMAL         0
#define PIN_ATTACH_RUNNING 1
#define PIN_ATTACH_BLOCKED 2
#define PIN_ATTACH_REDO    4

#define LINKAGE_DATA_OFFSET

uint64_t full_instcount = 0;
u_int redo_syscall = 0;

int s = -1;

// Debug Macros
#define ERROR_PRINT fprintf

//#define LOGGING_ON
#ifdef LOGGING_ON
#define INSTRUMENT_PRINT fprintf
#define LOG_PRINT(x,...);
#define PRINTX fprintf
#define SYSCALL_DEBUG fprintf

#else
#define LOG_PRINT(x,...);
#define INSTRUMENT_PRINT(x,...);
#define SYSCALL_DEBUG(x,...);
#define PRINTX(x,...);
#endif

#define SPECIAL_REG(X) (X == LEVEL_BASE::REG_EBP || X == LEVEL_BASE::REG_ESP)




//#define EXEC_INPUTS

/* Global state */

TLS_KEY tls_key; // Key for accessing TLS. 
int dev_fd; // File descriptor for the replay device
int get_record_pid(void);
#ifndef NO_FILE_OUTPUT
void init_logs(void);
#endif
struct thread_data* current_thread; // Always points to thread-local data (changed by kernel on context switch)
int first_thread = 1;
int child = 0;
char** main_prev_argv = 0;

//FILE* log_f = NULL; // For debugging
#ifdef VERBOSE
ofstream log_f; 
unordered_set<nodeid> traceids;
unordered_set<nodeid> debug_set = {0xe01349dd};
#endif

unsigned long global_syscall_cnt = 0;
struct xray_monitor* open_fds = NULL; // List of open fds
struct xray_monitor* open_socks = NULL; // list of open sockets


char group_directory[256];


#ifdef TRACE_TAINT_OPS
int trace_taint_outfd = -1;
#endif

unsigned long long inst_count = 0;
u_long segment_length = 0;
int splice_output = 0;
int all_output = 0;
map<pid_t,struct thread_data*> active_threads;
u_long* ppthread_log_clock = NULL;


//slice variables
u_long slice_clock = 0;
u_long slice_ip = 0; 
u_long slice_location;
u_long slice_size;

bool slice_on_errors = false;


//added for multi-process replay
const char* fork_flags = NULL;
u_int fork_flags_index = 0;
bool produce_output = true; 


//for recording traces (left in from JetStream)
#ifdef RECORD_TRACE_INFO
bool record_trace_info = true;
static u_long trace_total_count = 0;
static u_long trace_inst_total_count = 0;
#endif


// for the map from instruction address to instruction
std::unordered_map<ADDRINT, INS> ip_to_instruction;

#ifdef OUTPUT_FILENAMES
FILE* filenames_f = NULL; // Mapping of all opened filenames
#endif


KNOB<string> KnobFilterInputFile(KNOB_MODE_WRITEONCE,
    "pintool", "input_file", "",
    "filter inputs");
KNOB<string> KnobFilterDiftFile(KNOB_MODE_WRITEONCE,
    "pintool", "dift_file", "", "dift output");

KNOB<unsigned int> KnobFilterOutputsBefore(KNOB_MODE_WRITEONCE,
    "pintool", "ofb", "",
    "if set, specific clock before which we do not report output taints");
KNOB<bool> KnobRecordOpenedFiles(KNOB_MODE_WRITEONCE,
    "pintool", "o", "",
    "print all opened files");
KNOB<unsigned int> KnobSegmentLength(KNOB_MODE_WRITEONCE,
    "pintool", "l", "",
    "segment length"); //FIXME: take into consideration offset of being attached later. Remember, this is to specify where to kill application.
KNOB<bool> KnobSpliceOutput(KNOB_MODE_WRITEONCE,
    "pintool", "so", "",
    "generate output splice file");
KNOB<bool> KnobAllOutput(KNOB_MODE_WRITEONCE,
    "pintool", "ao", "",
    "generate output file of changed taints");
KNOB<int> KnobMergeEntries(KNOB_MODE_WRITEONCE,
    "pintool", "me", "",
    "merge entries"); 
KNOB<string> KnobNWHostname(KNOB_MODE_WRITEONCE,
    "pintool", "host", "",
    "hostname for nw output");
KNOB<int> KnobNWPort(KNOB_MODE_WRITEONCE,
    "pintool", "port", "",
    "port for nw output");
KNOB<string> KnobForkFlags(KNOB_MODE_WRITEONCE,
    "pintool", "fork_flags", "",
    "flags for which way to go on each fork");

KNOB<unsigned int> KnobSliceClock(KNOB_MODE_WRITEONCE,
			    "pintool", "slice_clock", "",
			    "clock where we take the slice");
KNOB<unsigned int> KnobSliceIP(KNOB_MODE_WRITEONCE,
			 "pintool", "slice_ip", "",
			 "instruction pointer to slice on");
KNOB<unsigned int> KnobSliceLocation(KNOB_MODE_WRITEONCE,
			       "pintool", "slice_location", "",
			       "memory location to slice on");
KNOB<unsigned int> KnobSliceSize(KNOB_MODE_WRITEONCE,
			   "pintool", "slice_size", "",
			   "bytes in memory location that we slice on ");

KNOB<string> KnobStartingAS(KNOB_MODE_WRITEONCE,
			   "pintool", "starting_as", "",
			    "starting memory address to taint");

KNOB<string> KnobEndingAS(KNOB_MODE_WRITEONCE,
			  "pintool", "ending_as", "",
			  "relevant ending addresses");

#ifdef TAINT_DEBUG
extern u_long debug_taint_cnt;
FILE* debug_f;
u_long taint_debug_inst = 0;
#endif

#ifdef TAINT_STATS
struct timeval begin_tv, end_tv;
u_long inst_instrumented = 0;
u_long traces_instrumented = 0;
uint64_t instrument_time = 0;


//u_long entries = 0;
FILE* stats_f;
#endif

static int terminated = 0;

extern void write_token_finish (int fd);
extern void output_finish (int fd);

//In here we need to mess with stuff for if we are no longer following this process
static int dift_done ()
{    
    PIN_LockClient();
    if (terminated) {
	PIN_UnlockClient();
	return 0;  // Only do this once
    }    
    terminated = 1;
    PIN_UnlockClient();

    std::cerr << std::dec << getpid() << ": DIFT done at " << *ppthread_log_clock << std::endl;

#ifdef TAINT_STATS

    gettimeofday(&end_tv, NULL);
    fprintf (stats_f, "Instructions instrumented: %ld\n", inst_instrumented);
    fprintf (stats_f, "Traces instrumented: %ld\n", traces_instrumented);
    
    fprintf (stats_f, "Instrument time: %lld us\n", instrument_time);
    
    fprintf (stats_f, "DIFT began at %ld.%06ld\n", begin_tv.tv_sec, begin_tv.tv_usec);
    fprintf (stats_f, "DIFT ended at %ld.%06ld\n", end_tv.tv_sec, end_tv.tv_usec);   

#endif
    cerr << "full_instcount " << full_instcount << endl;
    finish_and_print_graph_stats(stats_f);
    finish_input_output();

    init_dump(group_directory);
    if (all_output) {       
	int thread_ndx = 0;
	if (splice_output) {
	    // Dump out the active registers in order of the record thread id
	    for (map<pid_t,struct thread_data*>::iterator iter = active_threads.begin(); 
		 iter != active_threads.end(); iter++) {
		dump_reg_taints(iter->second->shadow_reg_table, thread_ndx++);
	    }
	    dump_mem_taints();
	} else {
	    // Dump out the active registers in order of the record thread id
	    for (map<pid_t,struct thread_data*>::iterator iter = active_threads.begin(); 
		 iter != active_threads.end(); iter++) {
		dump_reg_taints_start(iter->second->shadow_reg_table, thread_ndx++);
	    }
	    dump_mem_taints_start();
	}
    }
    finish_dump();


    // Send "done" message to aggregator
    if (s > 0) { 
	int rc = write (s, &group_directory, sizeof(group_directory));
	if (rc != sizeof(group_directory)) {
	    fprintf (stderr, "write of directory failed, rc=%d, errno=%d\n", rc, errno);
	}
    }


    return 1; //we are the one that acutally did the dift done
}

static inline void increment_syscall_cnt (int syscall_num)
{
    // ignore pthread syscalls, or deterministic system calls that we don't log (e.g. 123, 186, 243, 244)
    if (!(syscall_num == 17 || syscall_num == 31 || syscall_num == 32 || 
	  syscall_num == 35 || syscall_num == 44 || syscall_num == 53 || 
	  syscall_num == 56 || syscall_num == 58 || syscall_num == 98 || 
	  syscall_num == 119 || syscall_num == 123 || syscall_num == 127 ||
	  syscall_num == 186 || syscall_num == 243 || syscall_num == 244)) {
        if (current_thread->ignore_flag) {
            if (!(*(int *)(current_thread->ignore_flag))) {
                global_syscall_cnt++;
                current_thread->syscall_cnt++;
            }
        } else {
            global_syscall_cnt++;
            current_thread->syscall_cnt++;
        }
#ifdef VERBOSE
	log_f << "pid " << current_thread->record_pid 
	      << " syscall " << current_thread->syscall_cnt
	      << " global syscall cnt " << global_syscall_cnt
	      << " num " << syscall_num 
	      << " clock " << *ppthread_log_clock << endl;
#endif
    }
}
/////////////////////////
//  open and close fds //
/////////////////////////

static inline void sys_open_start(struct thread_data* tdata, char* filename, int flags)
{
    SYSCALL_DEBUG (stderr, "open_start: filename %s\n", filename);

    struct open_info* oi = (struct open_info *) malloc (sizeof(struct open_info));
    strncpy(oi->name, filename, OPEN_PATH_LEN);
    oi->flags = flags;
    tdata->save_syscall_info = (void *) oi;
}

static inline void sys_open_stop(int rc)
{
    SYSCALL_DEBUG (stderr, "open_stop, fd: %d\n",rc);
    if (rc > 0) {
        monitor_add_fd(open_fds, rc, current_thread->save_syscall_info);
    }
    current_thread->save_syscall_info = NULL;
}

static inline void sys_close_start(struct thread_data* tdata, int fd)
{
    SYSCALL_DEBUG (stderr, "close_start:fd = %d\n", fd);
    tdata->save_syscall_info = (void *) fd;
}

static void sys_socket_start (struct thread_data* tdata, int domain, int type, int protocol)
{
    struct socket_info* si = (struct socket_info*) malloc(sizeof(struct socket_info));
    if (si == NULL) {
	fprintf (stderr, "Unable to malloc socket info\n");
	assert (0);
    }
    si->call = SYS_SOCKET;
    si->domain = domain;
    si->type = type;
    si->protocol = protocol;
    si->fileno = -1; // will be set in connect/accept/bind
    si->ci = NULL;

    tdata->save_syscall_info = si;
}

static void sys_socket_stop(int rc)
{
    SYSCALL_DEBUG(stderr, "socket_stop, fd: %d\n",rc);
    if (rc > 0) {
        struct socket_info* si = (struct socket_info *) current_thread->save_syscall_info;
        monitor_add_fd(open_socks, rc, si);
        current_thread->save_syscall_info = NULL; // Giving si to the monitor
    }
}

static inline void sys_close_stop(int rc)
{
    int fd = (int) current_thread->save_syscall_info;
    // remove the fd from the list of open files
    if (!rc) {
        if (monitor_has_fd(open_fds, fd)) {
            struct open_info* oi = (struct open_info *) monitor_get_fd_data(open_fds, fd);

	    free (oi);
            monitor_remove_fd(open_fds, fd);
	    SYSCALL_DEBUG (stderr, "close: remove fd %d\n", fd);
        } 
	if (monitor_has_fd(open_socks, fd)) {
		monitor_remove_fd(open_socks, fd);
		SYSCALL_DEBUG (stderr, "close: remove sock fd %d\n", fd);
	}
    }
    current_thread->save_syscall_info = 0;
}


void get_channel_name(int fd, char * channel_name) 
{
    strcpy(channel_name, "--"); 
    
    if (monitor_has_fd(open_fds, fd)) {
	struct open_info* oi;
	oi = (struct open_info *)monitor_get_fd_data(open_fds, fd);
	strcpy(channel_name, oi->name);
	
    } else if (monitor_has_fd(open_socks, fd)) {
	struct socket_info* si;
	si = (struct socket_info *) monitor_get_fd_data(open_socks, fd);	       
	if (si->domain == AF_INET || si->domain == AF_INET6) {
	    strcpy(channel_name, "inetsocket");	    
	} else {
	    strcpy(channel_name, "recvsocket");
	}
    } 
}

//////////////////////////////
//  input / output syscalls //
//////////////////////////////


static inline void sys_read_start(struct thread_data* tdata, int fd, char* buf, int size)
{
    SYSCALL_DEBUG(stderr, "sys_read_start: fd = %d\n", fd);
    struct read_info* ri = &tdata->read_info_cache;
    ri->fd = fd;
    ri->buf = buf;
    tdata->save_syscall_info = (void *) ri;
}

static inline void sys_read_stop(int rc)
{
    struct read_info* ri = (struct read_info*) &current_thread->read_info_cache;

    if (rc > 0) {
	char channel_name[256];
	get_channel_name(ri->fd, channel_name);
        syscall_input(ri->buf, rc, input_info_type::READ, ri->fd, channel_name);
    }

    memset(&current_thread->read_info_cache, 0, sizeof(struct read_info));
    current_thread->save_syscall_info = 0;
}

static inline void sys_pread_start(struct thread_data* tdata, int fd, char* buf, int size)
{
    SYSCALL_DEBUG(stderr, "pread fd = %d\n", fd);
    struct read_info* ri = &tdata->read_info_cache;
    ri->fd = fd;
    ri->buf = buf;
    tdata->save_syscall_info = (void *) ri;

}

static inline void sys_pread_stop(int rc)
{
    struct read_info* ri = (struct read_info*) &current_thread->read_info_cache;

    // If global_syscall_cnt == 0, then handled in previous epoch
    if (rc > 0) {
	char channel_name[256];
	get_channel_name(ri->fd, channel_name);

        syscall_input(ri->buf, rc, input_info_type::PREAD, ri->fd, channel_name);
    }

    memset(&current_thread->read_info_cache, 0, sizeof(struct read_info));
    current_thread->save_syscall_info = 0;
}


static void sys_mmap_start(struct thread_data* tdata, u_long addr, int len, int prot, int fd)
{
    struct mmap_info* mmi = &tdata->mmap_info_cache;
    mmi->addr = addr;
    mmi->length = len;
    mmi->prot = prot;
    mmi->fd = fd;
    tdata->save_syscall_info = (void *) mmi;
}

static void sys_mmap_stop(int rc)
{
    struct mmap_info* mmi = (struct mmap_info*) current_thread->save_syscall_info;
    // If global_syscall_cnt == 0, then handled in previous epoch
    // this doesn't track if mmi->fd == -1... should it? 

    if (rc != -1 && (mmi->fd != -1)) {
	char channel_name[256];
	get_channel_name(mmi->fd, channel_name);

//	fprintf(stderr, "mmap: buffer %#lx, %d from %d %s \n", (u_long) rc, 
//		mmi->length, mmi->fd, channel_name);

	syscall_input((void *)rc, mmi->length, input_info_type::MMAP, mmi->fd, channel_name);
	    


    }
    current_thread->save_syscall_info = 0;
}

#if 0
static inline void sys_writev_start(struct thread_data* tdata, int fd, struct iovec* iov, int count)
{
    SYSCALL_DEBUG(stderr, "sys_writev_start: fd = %d\n", fd);
    struct writev_info* wvi;
    wvi = (struct writev_info *) &tdata->writev_info_cache;
    wvi->fd = fd;
    wvi->count = count;
    wvi->vi = iov;
    tdata->save_syscall_info = (void *) wvi;
}

static inline void sys_writev_stop(int rc)
{
    // If syscall cnt = 0, then write handled in previous epoch
    if (rc > 0) {

	struct taint_creation_info tci;
	struct writev_info* wvi = (struct writev_info *) &current_thread->writev_info_cache;
	int channel_fileno = -1;
	if (monitor_has_fd(open_fds, wvi->fd)) {
	    struct open_info* oi;
	    oi = (struct open_info *) monitor_get_fd_data(open_fds, wvi->fd);
	    assert(oi);
	    channel_fileno = oi->fileno;
	} if (monitor_has_fd(open_socks, wvi->fd)) {
	    struct socket_info* si;
	    si = (struct socket_info *) monitor_get_fd_data(open_socks, wvi->fd);
	    channel_fileno = si->fileno;
	} else {
	    channel_fileno = -1;
	}
	
	tci.type = 0;
	tci.rg_id = current_thread->rg_id;
	tci.record_pid = current_thread->record_pid;
	tci.syscall_cnt = current_thread->syscall_cnt;
	tci.offset = 0;
	tci.fileno = channel_fileno;
	
	for (int i = 0; i < wvi->count; i++) {
	    struct iovec* vi = (wvi->vi + i);
	    if (produce_output) { 
		output_buffer_result(vi->iov_base, vi->iov_len, &tci, output_fd);
	    }
	    tci.offset += vi->iov_len;
	}
    }
    memset(&current_thread->writev_info_cache, 0, sizeof(struct writev_info));
}

#ifdef 0
static inline void sys_write_start(struct thread_data* tdata, int fd, char* buf, int size)
{
    SYSCALL_DEBUG(stderr, "sys_write_start: fd = %d\n", fd);
    struct write_info* wi = &tdata->write_info_cache;
    wi->fd = fd;
    wi->buf = buf;
    tdata->save_syscall_info = (void *) wi;
}

static inline void sys_write_stop(int rc)
{
    struct write_info* wi = (struct write_info *) &current_thread->write_info_cache;
    if (rc > 0) {
	fprintf(stderr, "I don't support write output\n");
//	static_assert(0);
//        output(ri->buf, rc, input_info::WRITE, ri->fd,, input_fd, channel_name);	
//	output(wi->buf, rc, &ii, output_fd, false);
    }
}
#endif


static void sys_recv_start(thread_data* tdata, int fd, char* buf, int size) 
{
    // recv and read are similar so they can share the same info struct
    struct read_info* ri = (struct read_info*) &tdata->read_info_cache;
    ri->fd = fd;
    ri->buf = buf;
    tdata->save_syscall_info = (void *) ri;
    LOG_PRINT ("recv on fd %d\n", fd);
}

static void sys_recv_stop(int rc) 
{
    struct read_info* ri = (struct read_info *) &current_thread->read_info_cache;
    LOG_PRINT ("Pid %d syscall recv returns %d\n", PIN_GetPid(), rc);
    SYSCALL_DEBUG (stderr, "Pid %d syscall recv returns %d\n", PIN_GetPid(), rc);

    // If syscall cnt = 0, then write handled in previous epoch
    if (rc > 0) {
        struct taint_creation_info tci;
        char* channel_name = (char *) "--";
        int read_fileno = -1;
        if(ri->fd == fileno(stdin)) {
            read_fileno = 0;
            channel_name = (char *) "stdin";
        } else if (monitor_has_fd(open_socks, ri->fd)) {
            struct socket_info* si;
            si = (struct socket_info *) monitor_get_fd_data(open_socks, ri->fd);
            read_fileno = si->fileno;
            // FIXME
	    if (si->domain == AF_INET || si->domain == AF_INET6) {
		channel_name = (char *) "inetsocket";
		//fprintf (stderr, "Recv inet rc %d from fd %d at clock %lu\n", rc, ri->fd, *ppthread_log_clock);
	    } else {
		channel_name = (char *) "recvsocket";
	    }
        } else if (monitor_has_fd(open_fds, ri->fd)) {
            struct open_info* oi;
            oi = (struct open_info *)monitor_get_fd_data(open_fds, ri->fd);
            read_fileno = oi->fileno;
            channel_name = oi->name;
        }

        tci.rg_id = current_thread->rg_id;
        tci.record_pid = current_thread->record_pid;
        tci.syscall_cnt = current_thread->syscall_cnt;
        tci.offset = 0;
        tci.fileno = read_fileno;
        tci.data = 0;

        LOG_PRINT ("Create taints from buffer sized %d at location %#lx, clock %lu\n",
		   rc, (unsigned long) ri->buf, *ppthread_log_clock );
        create_taints_from_buffer(ri->buf, rc, &tci, tokens_fd, channel_name);
    }
    memset(&current_thread->read_info_cache, 0, sizeof(struct read_info));
    current_thread->save_syscall_info = 0;
}

static void sys_recvmsg_start(struct thread_data* tdata, int fd, struct msghdr* msg, int flags) 
{
    struct recvmsg_info* rmi;
    rmi = (struct recvmsg_info *) malloc(sizeof(struct recvmsg_info));
    if (rmi == NULL) {
	fprintf (stderr, "Unable to malloc recvmsg_info\n");
	assert (0);
    }
    rmi->fd = fd;
    rmi->msg = msg;
    rmi->flags = flags;

    tdata->save_syscall_info = (void *) rmi;
}

static void sys_recvmsg_stop(int rc) 
{
    struct recvmsg_info* rmi = (struct recvmsg_info *) current_thread->save_syscall_info;

    // If syscall cnt = 0, then write handled in previous epoch
    if (rc > 0) {
        struct taint_creation_info tci;
        char* channel_name = (char *) "recvmsgsocket";
        u_int i;
        int read_fileno = -1;
        if (monitor_has_fd(open_socks, rmi->fd)) {
            struct socket_info* si;
            si = (struct socket_info *) monitor_get_fd_data(open_socks, rmi->fd);
            read_fileno = si->fileno;
        } else {
            read_fileno = -1;
        }

        //fprintf (stderr, "rcvmsg_stop from channel: %s\n", channel_name);
        tci.rg_id = current_thread->rg_id;
        tci.record_pid = current_thread->record_pid;
        tci.syscall_cnt = current_thread->syscall_cnt;
        tci.offset = 0;
        tci.fileno = read_fileno;
        tci.data = 0;

        for (i = 0; i < rmi->msg->msg_iovlen; i++) {
            struct iovec* vi = (rmi->msg->msg_iov + i);
            // TODO support filtering on certain sockets
            LOG_PRINT ("Create taints from buffer sized %d at location %#lx\n",
                        vi->iov_len, (unsigned long) vi->iov_base);
            create_taints_from_buffer(vi->iov_base, vi->iov_len, &tci, tokens_fd,
                                        channel_name);
            tci.offset += vi->iov_len;
        }
    }
    SYSCALL_DEBUG (stderr, "recvmsg_stop done\n");
    free(rmi);
}

static void sys_sendmsg_start(struct thread_data* tdata, int fd, struct msghdr* msg, int flags)
{
    struct sendmsg_info* smi;
    smi = (struct sendmsg_info *) malloc(sizeof(struct sendmsg_info));
    if (smi == NULL) {
	fprintf (stderr, "Unable to malloc sendmsg_info\n");
	assert (0);
    }
    smi->fd = fd;
    smi->msg = msg;
    smi->flags = flags;

    tdata->save_syscall_info = (void *) smi;
}

static void sys_sendmsg_stop(int rc)
{
    u_int i;
    struct sendmsg_info* smi = (struct sendmsg_info *) current_thread->save_syscall_info;
    int channel_fileno = -1;

    // If syscall cnt = 0, then write handled in previous epoch
    if (rc > 0) {

	struct taint_creation_info tci;
	SYSCALL_DEBUG (stderr, "sendmsg_stop: sucess sendmsg of size %d\n", rc);
	if (monitor_has_fd(open_socks, smi->fd)) {
	    struct socket_info* si;
	    si = (struct socket_info *) monitor_get_fd_data(open_socks, smi->fd);
		channel_fileno = si->fileno;
	} else {
	    channel_fileno = -1;
	}
	
	tci.type = 0;
	tci.rg_id = current_thread->rg_id;
	tci.record_pid = current_thread->record_pid;
	tci.syscall_cnt = current_thread->syscall_cnt;
	tci.offset = 0;
	tci.fileno = channel_fileno;
	    
	for (i = 0; i < smi->msg->msg_iovlen; i++) {
	    struct iovec* vi = (smi->msg->msg_iov + i);
	    if (produce_output) { 
		output_buffer_result(vi->iov_base, vi->iov_len, &tci, outfd);
	    }
	    tci.offset += vi->iov_len;
	}
    }

    SYSCALL_DEBUG (stderr, "sys_sendmsg_stop done\n");
    free(smi);
}

#endif //ifdef 0


void syscall_start(struct thread_data* tdata, int sysnum, ADDRINT syscallarg0, ADDRINT syscallarg1,
		   ADDRINT syscallarg2, ADDRINT syscallarg3, ADDRINT syscallarg4, ADDRINT syscallarg5)
{

#ifdef VERBOSE
    log_f << "syscall_start " 
	  << "pid " << current_thread->record_pid 
	  << " syscall " << current_thread->syscall_cnt
	  << " global syscall cnt " << global_syscall_cnt
	  << " num " << sysnum 
	  << " clock " << *ppthread_log_clock << endl;
#endif

    switch (sysnum) {
    case SYS_open:
	sys_open_start(tdata, (char *) syscallarg0, (int) syscallarg1);
	break;
    case SYS_close:
	sys_close_start(tdata, (int) syscallarg0); 
	break;
    case SYS_read:
	sys_read_start(tdata, (int) syscallarg0, (char *) syscallarg1, (int) syscallarg2);
	break;
    case SYS_mmap:
    case SYS_mmap2:
	sys_mmap_start(tdata, (u_long)syscallarg0, (int)syscallarg1, (int)syscallarg2, (int)syscallarg4);
	break;

    case SYS_socketcall:
    {
	int call = (int) syscallarg0;
	unsigned long *args = (unsigned long *)syscallarg1;
	tdata->socketcall = call;
	switch (call) {
	case SYS_SOCKET:
	    sys_socket_start(tdata, (int)args[0], (int)args[1], (int)args[2]);
	    break;   
	default:
	    break;
	}	
	break;
    }
    default:
	break;

    }
}

void syscall_end(int sysnum, ADDRINT ret_value)
{
    int rc = (int) ret_value;
    switch(sysnum) {
    case SYS_open:
	sys_open_stop(rc);
	break;
    case SYS_close:
	sys_close_stop(rc);
	break;	
    case SYS_read:
	sys_read_stop(rc);
	break;
    case SYS_mmap:
    case SYS_mmap2:
	sys_mmap_stop(rc);
	break;
    case SYS_socketcall:
	switch (current_thread->socketcall) {
	case SYS_SOCKET:
	    sys_socket_stop(rc);
	    break;
	default:
	    break;
	}
	current_thread->socketcall = 0;
	break;
    }
}


// called before every application system call
void instrument_syscall(ADDRINT syscall_num, 
			ADDRINT syscallarg0, ADDRINT syscallarg1, ADDRINT syscallarg2,
			ADDRINT syscallarg3, ADDRINT syscallarg4, ADDRINT syscallarg5)
{   
    int sysnum = (int) syscall_num;

    // Because of Pin restart issues, this function alone has to use PIN thread-specific data
    struct thread_data* tdata = (struct thread_data *) PIN_GetThreadData(tls_key, PIN_ThreadId());
    tdata->sysnum = sysnum;
    tdata->syscall_in_progress = true;

#ifdef RECORD_TRACE_INFO
    if (record_trace_info &&
	!(sysnum == 17 || sysnum == 31 || sysnum == 32 ||
	  sysnum == 35 || sysnum == 44 || sysnum == 53 ||
          sysnum == 56 || sysnum == 58 || sysnum == 98 ||
          sysnum == 119 || sysnum == 123 || sysnum == 127 ||
	  sysnum == 186 || sysnum == 243 || sysnum == 244)) {

	if (current_thread->ignore_flag) {
	    if (!(*(int *)(current_thread->ignore_flag))) {
		flush_trace_hash(sysnum);
	    }
        } else {
	    flush_trace_hash(sysnum);
        }
    }
#endif


#ifdef TAINT_DEBUG
      fprintf (debug_f, "Thread %d sees sysnum %d in progress\n", tdata->record_pid, sysnum);
      if (current_thread != tdata) fprintf (debug_f, "current thread %d tdata %d\n", current_thread->record_pid, tdata->record_pid);
#endif

    if (sysnum == 31) {
	tdata->ignore_flag = (u_long) syscallarg1;
    }
    if (sysnum == 45 || sysnum == 91 || sysnum == 120 || sysnum == 125 || 
	sysnum == 174 || sysnum == 175 || sysnum == 190 || sysnum == 192) {
	check_clock_before_syscall (dev_fd, (int) syscall_num);
    }
    if (sysnum == 252) {
	cerr << "dift done b/c sysnmum is 252\n";
	dift_done();
    }


#ifdef RETAINT
    if (retaint_next_clock && *ppthread_log_clock >= retaint_next_clock) {
	fprintf(stderr,"resetting taints\n");
	reset_taints();
	char* p;
	for (p = retaint_str; *p != '\0' && *p != ','; p++);
	*p = '\0';
	retaint_next_clock = strtoul(retaint_str, NULL, 10);
	if (retaint_next_clock) fprintf (stderr, "Next epoch to retaint: %lu\n", retaint_next_clock);
	retaint_str = p+1;
	fprintf(stderr, "merrily we go along\n");
    }
#endif

    if (segment_length && *ppthread_log_clock >= segment_length) {
	// Done with this replay - do exit stuff now because we may not get clean unwind
#ifdef TAINT_DEBUG
	fprintf (debug_f, "Pin terminating at Pid %d, entry to syscall %ld, term. clock %ld cur. clock %ld\n", PIN_GetTid(), global_syscall_cnt, segment_length, *ppthread_log_clock);
#endif
	cerr << "dift done b/c clock " << *ppthread_log_clock << " greater than " << segment_length << endl;

	/*
	 * there's a race condition here if we are still attaching to multiple threads. A thread that skips 
	 * dift_done might fly through the rest of this and exit before dift_done has been called. This
	 * *is* what is happening in a partitioning for firefox (weird). 
	 */
	//we can't exit if we haven't aren't the one calling dift_done or if some threads are still attaching

	int calling_dd = dift_done ();
	while (!calling_dd || is_pin_attaching(dev_fd)) { 
	    usleep(1000); 
	}

	fprintf(stderr, "%d: calling try_to_exit\n", PIN_GetTid());
	try_to_exit(dev_fd, PIN_GetPid());
	PIN_ExitApplication(0); 

    }
	
    syscall_start(tdata, sysnum, syscallarg0, syscallarg1, syscallarg2, 
		  syscallarg3, syscallarg4, syscallarg5);
    
    tdata->app_syscall = syscall_num;
}



static void syscall_after_redo (ADDRINT ip)
{
    if (redo_syscall) {
	u_long rc, len, retval;
	int syscall_to_redo = check_for_redo(dev_fd);
	if (syscall_to_redo == 192) {
	    redo_syscall--;
	    //fprintf (stderr, "Instruction %x redo mmap please %d\n", ip, redo_syscall);
	    retval = redo_mmap (dev_fd, &rc, &len);
	    if (retval) fprintf (stderr, "redo_mmap failed, rc=%ld\n", retval);
#if 0
	    fprintf (stderr, "syscall_after, eax is %x\n", PIN_GetContextReg(ctxt, LEVEL_BASE::REG_EAX));
	    fprintf (stderr, "syscall_after, ebx is %x\n", PIN_GetContextReg(ctxt, LEVEL_BASE::REG_EBX));
	    fprintf (stderr, "syscall_after, ecx is %x\n", PIN_GetContextReg(ctxt, LEVEL_BASE::REG_ECX));
#endif
	    //fprintf (stderr, "Clearing taints %lx,%lx\n", rc, len);
	    current_thread->app_syscall = 0;  
	}
	else if(syscall_to_redo == 91) { 
	    redo_syscall--;
	    //fprintf (stderr, "Instruction %x redo mmap please %d\n", ip, redo_syscall);
	    retval = redo_munmap (dev_fd);
	    fprintf(stderr, "running the redo_munmap!\n");
	    if (retval) fprintf (stderr, "redo_mmap failed, rc=%ld\n", retval);
#if 0
	    fprintf (stderr, "syscall_after, eax is %x\n", PIN_GetContextReg(ctxt, LEVEL_BASE::REG_EAX));
	    fprintf (stderr, "syscall_after, ebx is %x\n", PIN_GetContextReg(ctxt, LEVEL_BASE::REG_EBX));
	    fprintf (stderr, "syscall_after, ecx is %x\n", PIN_GetContextReg(ctxt, LEVEL_BASE::REG_ECX));
#endif
	    //fprintf (stderr, "Clearing taints %lx,%lx\n", rc, len);
	    current_thread->app_syscall = 0;
	}      
    } else if (current_thread->app_syscall == 999) {
	check_clock_after_syscall (dev_fd);
	current_thread->app_syscall = 0;  
    }
}


void instrument_syscall_ret(THREADID thread_id, CONTEXT* ctxt, SYSCALL_STANDARD std, VOID* v)
{
    if (current_thread->app_syscall != 999) current_thread->app_syscall = 0;

    if (current_thread->sysnum == SYS_gettid) {
	// Pin "helpfully" changes the return value to the replay tid - change it back here
	//printf ("eax is %d changing to %d\n", PIN_GetContextReg (ctxt, LEVEL_BASE::REG_EAX), current_thread->record_pid);
	PIN_SetContextReg (ctxt, LEVEL_BASE::REG_EAX, current_thread->record_pid);
    }

    if (segment_length && *ppthread_log_clock > segment_length) {
#ifdef TAINT_DEBUG
	fprintf (debug_f, "Skip Pid %d, exit from syscall %ld due to termination, term. clock %ld cur. clock %ld\n", PIN_GetPid(), global_syscall_cnt, segment_length, *ppthread_log_clock);
#endif
    } else {
    ADDRINT ret_value = PIN_GetSyscallReturn(ctxt, std);
	syscall_end(current_thread->sysnum, ret_value);
    }

    if (!current_thread->syscall_in_progress) {
	/* Pin restart oddity: initial write will nondeterministically return twice (once with rc=0).
	   Just don't increment the global syscall cnt when this happens. */
	if (global_syscall_cnt == 0) {
	    if (current_thread->sysnum != SYS_write) {
#ifdef TAINT_DEBUG
		fprintf (debug_f, "First syscall %d not in progress and not write\n", current_thread->sysnum);
#endif
	    }
	} else {
#ifdef TAINT_DEBUG
	  fprintf (debug_f, "Syscall not in progress for global_syscall_cnt %ld sysnum %d thread %d\n", global_syscall_cnt, current_thread->sysnum, current_thread->record_pid);
	  struct thread_data* tdata = (struct thread_data *) PIN_GetThreadData(tls_key, PIN_ThreadId());
	  fprintf (debug_f, "tdata is %p current_thread is %p\n", tdata, current_thread);
#endif
	}
    } else {
	// reset the syscall number after returning from system call
	increment_syscall_cnt (current_thread->sysnum);
	current_thread->syscall_in_progress = false;
    }

    // The first syscall returns twice 
    if (global_syscall_cnt > 1) { 
	current_thread->sysnum = 0;
    }
}

int get_record_pid()
{
    //calling kernel for this replay thread's record log
    int record_log_id;

    record_log_id = get_log_id (dev_fd);
    if (record_log_id == -1) {
        int pid = PIN_GetPid();
        fprintf(stderr, "Could not get the record pid from kernel, pid is %d\n", pid);
        return pid;
    }
    return record_log_id;
}


void instrument_copy_reg2reg(INS ins, REG dstreg, REG srcreg, int extend)
{
    int dst_treg;
    int src_treg;
    UINT32 dst_regsize;
    UINT32 src_regsize;

    dst_treg = translate_reg((int)dstreg);
    src_treg = translate_reg((int)srcreg);
    dst_regsize = REG_Size(dstreg);
    src_regsize = REG_Size(srcreg);


    if (dst_regsize == src_regsize) {
        switch(dst_regsize) {
            case 1:
                if (REG_is_Lower8(dstreg) && REG_is_Lower8(srcreg)) {
		    COPY_REG2REG(1,0,0,dst_treg, src_treg);
                } else if (REG_is_Lower8(dstreg) && REG_is_Upper8(srcreg)) {
		    COPY_REG2REG(1,0,1,dst_treg, src_treg);
                } else if (REG_is_Upper8(dstreg) && REG_is_Lower8(srcreg)) {
		    COPY_REG2REG(1,1,0,dst_treg, src_treg);
                } else if (REG_is_Upper8(dstreg) && REG_is_Upper8(srcreg)) {
		    COPY_REG2REG(1,1,1,dst_treg, src_treg);
                }
                break;
            case 2:
		COPY_REG2REG(2,0,0,dst_treg, src_treg);
                break;
            case 4:
		COPY_REG2REG(4,0,0,dst_treg, src_treg);
                break;
            case 8:
		COPY_REG2REG(8,0,0,dst_treg, src_treg);
                break;
            case 16:
		COPY_REG2REG(16,0,0,dst_treg, src_treg);
                break;
            default:
                assert(0);
                break;
        }
    } else if (dst_regsize > src_regsize){ 
//	if (!extend) 
//	    cerr << INS_Disassemble(ins) << " dsize " << dst_regsize << " ssize " << src_regsize << endl;
	assert(extend || REG_is_xmm(dstreg) || REG_is_xmm(srcreg));

	//should probably have the 0'd bytes depend on me
	switch(src_regsize) { 
            case 1:
                if (REG_is_Lower8(srcreg)) {
		    COPY_REG2REG(1,0,0,dst_treg, src_treg);
                } else {
                    //srcreg is upper8 
		    COPY_REG2REG(1,0,1,dst_treg, src_treg);
		} 
		break;
            case 2:
		COPY_REG2REG(2,0,0,dst_treg, src_treg);
                break;
            case 4:
		COPY_REG2REG(4,0,0,dst_treg, src_treg);
                break;
            case 8:
		COPY_REG2REG(8,0,0,dst_treg, src_treg);
                break;
            case 16:
		COPY_REG2REG(16,0,0,dst_treg, src_treg);
                break;
            default:
                assert(0);
                break;
	}       
    }
    else {
	//case where src_regsize > dst_regsize
	switch(dst_regsize) { 
            case 1:
                if (REG_is_Lower8(dstreg)) {
		    COPY_REG2REG(1,0,0,dst_treg, src_treg);
		} 
		else {
		    //dstreg is upper8 
		    COPY_REG2REG(1,1,0,dst_treg, src_treg);
		}
                break;
            case 2:
		COPY_REG2REG(2,0,0,dst_treg, src_treg);
                break;
            case 4:
		COPY_REG2REG(4,0,0,dst_treg, src_treg);
                break;
            case 8:
		COPY_REG2REG(8,0,0,dst_treg, src_treg);
                break;
            default:
                assert(0);
                break;
	}       
    }
}

void instrument_copy_reg2mem(INS ins, REG reg, int extend)
{
    int treg = translate_reg((int)reg);
    UINT32 regsize = REG_Size(reg);
    UINT32 memsize = INS_MemoryWriteSize(ins);
   
    if (memsize == regsize) {
        switch(memsize) {
            case 1:
                if (REG_is_Lower8(reg)) {
		    COPY_REG2MEM(1,0,treg);
                } else if (REG_is_Upper8(reg)) {
		    COPY_REG2MEM(1,1,treg);
		}
                break;
            case 2:
		COPY_REG2MEM(2,0,treg);
                break;
            case 4:
		COPY_REG2MEM(4,0,treg);
                break;
            case 8:
		COPY_REG2MEM(8,0,treg);
                break;
            case 16:
		COPY_REG2MEM(16,0,treg);
		break;
            default:
                fprintf(stderr, "[ERROR] instrument_taint_reg2mem: unknown reg size %d\n", regsize);
		fprintf(stderr, "%s ms %u rs %u\n",INS_Disassemble(ins).c_str(), memsize, regsize);
                assert(0);
                break;
        }
    } else if (memsize > regsize){ 
//	if (!extend && memsize != 16) 
//	    cerr << "huh? " << INS_Disassemble(ins) << endl;
	assert(extend || memsize == 16 || memsize == 8);

	//should probably have the 0'd bytes depend on me
        switch(regsize) {
            case 1:
                if (REG_is_Lower8(reg)) {
		    COPY_REG2MEM(1,0,treg);
                } else if (REG_is_Upper8(reg)) {
		    COPY_REG2MEM(1,1,treg);
		}
                break;
            case 2:
		COPY_REG2MEM(2,0,treg);
                break;
            case 4:
		COPY_REG2MEM(4,0,treg);
                break;
            case 8:
		COPY_REG2MEM(8,0,treg);
                break;
            case 16:
		COPY_REG2MEM(16,0,treg);
		break;
            default:
                fprintf(stderr, "[ERROR] instrument_taint_reg2mem: unknown reg size %d\n", regsize);
		fprintf(stderr, "%s ms %u rs %u\n",INS_Disassemble(ins).c_str(), memsize, regsize);
                assert(0);
                break;
        }
    }
    else {
	//case where src_regsize > dst_regsize
        switch(memsize) {
            case 1:
                if (REG_is_Lower8(reg)) {
		    COPY_REG2MEM(1,0,treg);
                } else if (REG_is_Upper8(reg)) {
		    COPY_REG2MEM(1,1,treg);
		}
                break;
            case 2:
		COPY_REG2MEM(2,0,treg);
                break;
            case 4:
		COPY_REG2MEM(4,0,treg);
                break;
            case 8:
		COPY_REG2MEM(8,0,treg);
                break;
            case 16:
		COPY_REG2MEM(16,0,treg);
		break;
            default:
                fprintf(stderr, "[ERROR] instrument_taint_reg2mem: unknown reg size %d\n", regsize);
                assert(0);
                break;
        }
    } 
}

void instrument_copy_mem2reg(INS ins, REG dstreg, int extend)
{
    int treg = translate_reg((int)dstreg);
    UINT32 regsize = REG_Size(dstreg);
    UINT32 memsize = INS_MemoryReadSize(ins);

    assert(regsize >= memsize);
	    
    if (regsize == memsize) {
        switch(regsize) {
            case 1:
                if (REG_is_Lower8(dstreg)) {
		    COPY_MEM2REG(1,0, treg);
                } else if (REG_is_Upper8(dstreg)) {
		    COPY_MEM2REG(1,1, treg);
		}
                break;
            case 2:
		COPY_MEM2REG(2,0, treg);
                break;
            case 4:
		COPY_MEM2REG(4,0, treg);
		break;
            case 8:
		COPY_MEM2REG(8,0, treg);
                break;
            case 16:
		COPY_MEM2REG(16,0, treg);
                break;
            default:
                assert(0);
                break;
        }
    } else {
	if (!extend && !REG_is_xmm(dstreg))
	    cerr << INS_Disassemble(ins) << endl;
	assert(extend || REG_is_xmm(dstreg));
        switch(memsize) {
            case 1:
		COPY_MEM2REG(1,0, treg);
                break;
            case 2:
		COPY_MEM2REG(2,0, treg);
                break;
            case 4:
		COPY_MEM2REG(4,0, treg);
		break;
            case 8:
		COPY_MEM2REG(8,0, treg);
                break;
            case 16:
		COPY_MEM2REG(16,0, treg);
                break;
            default:
                assert(0);
                break;
        }
    }    
}

void instrument_copy_mem2mem(INS ins, int extend)
{
    UINT32 dstsize = INS_MemoryWriteSize(ins);
    UINT32 srcsize = INS_MemoryReadSize(ins);

    assert(dstsize >= srcsize);
    if (dstsize == srcsize){
        switch(dstsize) {
	case 1:
	    COPY_MEM2MEM(1);
	    break;
	case 2:
	    COPY_MEM2MEM(2);
	    break;
	case 4:
	    COPY_MEM2MEM(4);
	    break;
	case 8:
	    COPY_MEM2MEM(8);
	    break;
	    break;
	case 16:
	    COPY_MEM2MEM(16);
	    break;
	default:
	    assert(0);
	    break;
        }
    } else {
	assert(extend); //not sure that this is true
	fprintf(stderr, "don't yet support extend!\n");	
    } 
}

void instrument_merge_regreg2reg(INS ins, REG dstreg,REG srcreg) 
{
    int dst_treg;
    int src_treg;
    UINT32 dst_regsize;
    UINT32 src_regsize;

    dst_treg = translate_reg((int)dstreg);
    src_treg = translate_reg((int)srcreg);
    dst_regsize = REG_Size(dstreg);
    src_regsize = REG_Size(srcreg);

    assert(dst_regsize >= src_regsize);

    switch(src_regsize) {
    case 1:
	if (REG_is_Lower8(dstreg) && REG_is_Lower8(srcreg)) {
	    MERGE_REGREG2REG(1,1,0,0,0,dst_treg, dst_treg, src_treg);
	} else if (REG_is_Lower8(dstreg) && REG_is_Upper8(srcreg)) {
	    MERGE_REGREG2REG(1,1,0,0,1,dst_treg, dst_treg, src_treg);
	} else if (REG_is_Upper8(dstreg) && REG_is_Lower8(srcreg)) {
	    MERGE_REGREG2REG(1,2,1,1,0,dst_treg, dst_treg, src_treg);
	} else if (REG_is_Upper8(dstreg) && REG_is_Upper8(srcreg)) {
	    MERGE_REGREG2REG(1,1,1,1,1,dst_treg, dst_treg, src_treg);
	}
	break;
    case 2:
	MERGE_REGREG2REG(2,2,0,0,0,dst_treg, dst_treg, src_treg);
	break;
    case 4:
	MERGE_REGREG2REG(4,4,0,0,0,dst_treg, dst_treg, src_treg);
	break;
    case 8:
	MERGE_REGREG2REG(8,8,0,0,0,dst_treg, dst_treg, src_treg);
	break;
    case 16:
	MERGE_REGREG2REG(16,16,0,0,0,dst_treg, dst_treg, src_treg);
	break;
    default:
	assert(0);
	break;
    }
}

void instrument_merge_regmem2reg(INS ins, REG dstreg) 
{
    int treg = translate_reg((int)dstreg);
    UINT32 regsize = REG_Size(dstreg);
    UINT32 memsize = INS_MemoryReadSize(ins);

    assert(regsize >= memsize);
	    
    switch(regsize) {
    case 1:
	if (REG_is_Lower8(dstreg)) {
	    MERGE_REGMEM2REG(1,1,0,0,treg,treg);
	} else if (REG_is_Upper8(dstreg)) {
	    MERGE_REGMEM2REG(1,1,1,1,treg,treg);
	}
	break;
    case 2:
	MERGE_REGMEM2REG(2,2,0,0, treg, treg);
	break;
    case 4:
	MERGE_REGMEM2REG(4,4,0,0, treg, treg);
	break;
    case 8:
	MERGE_REGMEM2REG(8,8,0,0, treg, treg);
	break;
    case 16:
	MERGE_REGMEM2REG(16,16,0,0, treg, treg);
	break;
    default:
	assert(0);
	break;
    }
}

void instrument_merge_regmem2mem(INS ins, REG reg) 
{
    int treg = translate_reg((int)reg);
    UINT32 regsize = REG_Size(reg);
    UINT32 memsize = INS_MemoryWriteSize(ins);
    
    assert(memsize >= regsize || regsize == 4 || regsize == 16);

    switch(regsize) {
    case 1:
	if (REG_is_Lower8(reg)) {
	    MERGE_REGMEM2MEM(1,1,0,treg);
	} else if (REG_is_Upper8(reg)) {
	    MERGE_REGMEM2MEM(1,1,1,treg);
	}
	break;
    case 2:
	MERGE_REGMEM2MEM(2,2,0,treg);
	break;
    case 4:
	switch(memsize) 
	{
	case 1: 
	    MERGE_REGMEM2MEM(1,4,0,treg);
	    break;
	case 2:
	    MERGE_REGMEM2MEM(2,4,0,treg);
	    break;
	case 4:
	    MERGE_REGMEM2MEM(4,4,0,treg);
	    break;
	case 8:
	    MERGE_REGMEM2MEM(8,4,0,treg);
	    break;
	case 16:
	    MERGE_REGMEM2MEM(16,4,0,treg);
	    break;
	default:
	    cerr << "huh? " << INS_Disassemble(ins) << endl;
	    assert(0);
	}
	break;
    case 8:
	MERGE_REGMEM2MEM(8,8,0,treg);
	break;
    case 16:
	switch(memsize) 
	{
	case 8:
	    MERGE_REGMEM2MEM(8,16,0,treg);
	    break;
	case 16:
	    MERGE_REGMEM2MEM(16,16,0,treg);
	    break;
	default:
	    assert(0);
	}
	break;
    default:
	fprintf(stderr, "[ERROR] instrument_taint_reg2mem: unknown reg size %d\n", regsize);
	assert(0);
	break;
    }
}


void instrument_copy_immval2reg(INS ins, REG dstreg)
{
    int treg = translate_reg((int)dstreg);
    uint32_t regsize = REG_Size(dstreg);

    switch(regsize) { 
    case 1:
	if (REG_is_Lower8(dstreg)) {
	    COPY_IMMVAL2REG(1,0,treg);
	} else if (REG_is_Upper8(dstreg)) {
	    COPY_IMMVAL2REG(1,1,treg);
	}
	break;
    case 2:
	COPY_IMMVAL2REG(2,0,treg);
	break;
    case 4:
	COPY_IMMVAL2REG(4,0,treg);
	break;
    case 8:
	COPY_IMMVAL2REG(8,0,treg);
	break;
    case 16:
	//b/c of xmm stuff
	COPY_IMMVAL2REG(16,0,treg);
	break;
    default:
	fprintf(stderr, "[ERROR] instrument_taint_reg2mem: unknown reg size %d\n", regsize);
	fprintf(stderr, "Instruction %s\n",INS_Disassemble(ins).c_str());
	assert(0);
	break;
    }
}



void instrument_copy_immval2mem(INS ins)
{
    UINT32 memsize = INS_MemoryWriteSize(ins);
    switch(memsize) { 
    case 1:
	COPY_IMMVAL2MEM(1);
	break;
    case 2:
	COPY_IMMVAL2MEM(2);
	break;
    case 4:
	COPY_IMMVAL2MEM(4);
	break;
    case 8:
	COPY_IMMVAL2MEM(8);
	break;
    case 16:
	//b/c of xmm stuff
	COPY_IMMVAL2MEM(16);
	break;
    default:
	fprintf(stderr, "[ERROR] instrument_taint_reg2mem: unknown mem size %d\n", memsize);
	assert(0);
	break;
    }
}

void instrument_merge_memimmval2mem(INS ins)
{
    UINT32 dstsize = INS_MemoryWriteSize(ins);
    UINT32 srcsize = INS_MemoryReadSize(ins);

    assert(dstsize >= srcsize);
    if (dstsize == srcsize){
        switch(dstsize) {
	case 1:
	    MERGE_MEMIMMVAL2MEM(1);
	    break;
	case 2:
	    MERGE_MEMIMMVAL2MEM(2);
	    break;
	case 4:
	    MERGE_MEMIMMVAL2MEM(4);
	    COPY_MEM2MEM(4);
	    break;
	case 8:
	    MERGE_MEMIMMVAL2MEM(8);
	    break;
	    break;
	case 16:
	    MERGE_MEMIMMVAL2MEM(16);
	    break;
	default:
	    assert(0);
	    break;
        }
    } else {
	fprintf(stderr, "don't yet support extend!\n");	
    } 
}

void instrument_merge_regimmval2reg(INS ins, REG dstreg, REG srcreg)
{
    int dsttreg = translate_reg((int)dstreg);
    int srctreg = translate_reg((int)srcreg);
    uint32_t regsize = REG_Size(dstreg);


    switch(regsize) { 
    case 1:
	if (REG_is_Lower8(dstreg) && REG_is_Lower8(srcreg)) {
	    MERGE_REGIMMVAL2REG(1,0,0,dsttreg,srctreg);
	} else if (REG_is_Lower8(dstreg) && REG_is_Upper8(srcreg)) {
	    MERGE_REGIMMVAL2REG(1,0,1,dsttreg,srctreg);
	} else if (REG_is_Upper8(dstreg) && REG_is_Lower8(srcreg)) {
	    MERGE_REGIMMVAL2REG(1,1,0,dsttreg,srctreg);
	} else { 
	    MERGE_REGIMMVAL2REG(1,1,1,dsttreg,srctreg);
	}
	break;
    case 2:
	MERGE_REGIMMVAL2REG(2,0,0,dsttreg,srctreg);
	break;
    case 4:
	MERGE_REGIMMVAL2REG(4,0,0,dsttreg,srctreg);
	break;
    case 8:
	MERGE_REGIMMVAL2REG(8,0,0,dsttreg,srctreg);
	break;
    case 16:
	//b/c of xmm stuff
	MERGE_REGIMMVAL2REG(16,0,0,dsttreg,srctreg);
	break;
    default:
	fprintf(stderr, "[ERROR] instrument_taint_reg2mem: unknown reg size %d\n", regsize);
	fprintf(stderr, "Instruction %s\n",INS_Disassemble(ins).c_str());
	assert(0);
	break;
    }
}





void instrument_move_string(INS ins){}
void instrument_store_string(INS ins){}
void instrument_load_string(INS ins){}

void instrument_xchg (INS ins){}
void instrument_bswap (INS ins) {}

void instrument_mov (INS ins, int extend) 
{
    INC_COUNT(ins);
    REG reg = REG_INVALID();
    REG dstreg = REG_INVALID();

    if(INS_IsMemoryRead(ins)) {
        reg = INS_OperandReg(ins, 0);
        if(!REG_valid(reg) || SPECIAL_REG(reg)) return;

#ifdef LINKAGE_DATA_OFFSET

        REG index_reg = INS_OperandMemoryIndexReg(ins, 1);
        REG base_reg = INS_OperandMemoryBaseReg(ins, 1);

        if (REG_valid(base_reg) && !REG_valid(index_reg)) {
            //(base, memory) -> reg

	    /*
	     * don't have indexing by ebp or esp... 
	     * they aren't semantically important
	     */
	    if (!SPECIAL_REG(base_reg)) 
	    {
		instrument_copy_reg2reg(ins, reg, base_reg, 0);
		instrument_merge_regmem2reg(ins, reg);
	    }
	    else { 
		instrument_copy_mem2reg(ins,reg, extend);
	    }

        } else if (REG_valid(base_reg) && REG_valid(index_reg)) {

	    //(index, base, memory) -> reg 	   
	    instrument_copy_reg2reg(ins, reg, index_reg, 0);
	    instrument_merge_regreg2reg(ins, reg, base_reg);
	    instrument_merge_regmem2reg(ins, reg);
	    
        } else {
            instrument_copy_mem2reg(ins, reg, extend);
        }
#else
	instrument_copy_mem2reg(ins, reg, extend);
#endif

    } else if(INS_IsMemoryWrite(ins)) {
        if(INS_OperandIsReg(ins, 1)) {
            reg = INS_OperandReg(ins, 1);
            if(!REG_valid(reg)) return;

#ifdef LINKAGE_DATA_OFFSET

        REG index_reg = INS_OperandMemoryIndexReg(ins, 0);
        REG base_reg = INS_OperandMemoryBaseReg(ins, 0); //try this??? 

        if (REG_valid(base_reg) && !REG_valid(index_reg)) {
            //(base, reg) -> memory

	    /*
	     * don't have indexing by ebp or esp... 
	     * they aren't semantically important
	     */
	    if (!SPECIAL_REG(base_reg)) 
	    {
		instrument_copy_reg2mem(ins, base_reg, 0);
		instrument_merge_regmem2mem(ins, reg);
	    }
	    else { 
		instrument_copy_reg2mem(ins,reg, extend);
	    }

        } else if (REG_valid(base_reg) && REG_valid(index_reg)) {
	    //(index, base, reg) -> mem
	    instrument_copy_reg2mem(ins, index_reg, 0);
	    instrument_merge_regmem2mem(ins, base_reg);
	    instrument_merge_regmem2mem(ins, reg);
	    
        } else {
	    //both invalid
            instrument_copy_reg2mem(ins, reg, extend);
        }
#else
            instrument_copy_reg2mem(ins, reg, extend);
#endif
        } else {
            if(!INS_OperandIsImmediate(ins, 1)) return;
            instrument_copy_immval2mem(ins);
        }
    } else {
        if(!(INS_OperandIsReg(ins, 0))) return;
        dstreg = INS_OperandReg(ins, 0);
        if(!REG_valid(dstreg) || SPECIAL_REG(dstreg)) return;

        if(INS_OperandIsReg(ins, 1)) {
            reg = INS_OperandReg(ins, 1);
            if(!REG_valid(reg)) return;
            if (REG_is_seg(reg) || REG_is_seg(dstreg)) {
                // ignore segment registers for now
                return;
            }
            instrument_copy_reg2reg(ins, dstreg, reg, extend);
        }
	else {
            if(!INS_OperandIsImmediate(ins, 1)) return;

            //mov immediate value into register
	    instrument_copy_immval2reg(ins, dstreg);
        }
    }
}


void instrument_cmov(INS ins) 
{
}

void instrument_push(INS ins)
{
    INC_COUNT(ins);
    int src_reg = INS_OperandIsReg(ins, 0);
    int src_imm = INS_OperandIsImmediate(ins, 0);

    if (src_imm) {
	instrument_copy_immval2mem(ins);
    } else if (src_reg) {
	REG reg = INS_OperandReg(ins, 0);
	instrument_copy_reg2mem(ins,reg,0);
    }
    else {
	instrument_copy_mem2mem(ins,0);    
    }
} 

void instrument_pop(INS ins)
{
    INC_COUNT(ins);

    if (INS_OperandIsMemory(ins, 0)) {
	instrument_copy_mem2mem(ins,0);
    } else if (INS_OperandIsReg(ins, 0)) {
        REG reg = INS_OperandReg(ins, 0);
	instrument_copy_mem2reg(ins,reg,0);	
    }
}

void instrument_lea(INS ins) {
    INC_COUNT(ins);

    REG dstreg = INS_OperandReg(ins, 0);
    REG base_reg = INS_OperandMemoryBaseReg(ins, 1);
    REG index_reg = INS_OperandMemoryIndexReg(ins, 1);

    if (REG_valid (index_reg) && !REG_valid(base_reg)) {
        // This is a nummeric calculation in disguise
        assert(REG_Size(index_reg) == REG_Size(dstreg));
        switch(REG_Size(dstreg)) {
            case 4:
		COPY_REG2REG(4,0,0,dstreg,index_reg); 
                break;
            default:
                assert(0);
                break;
        }
    } else {
        //loading an effective address adds the depsets of the
        //base/index regs used.
        switch(REG_Size(dstreg)) {
            case 4:
		MERGE_REGREG2REG(4,4,0,0,0,dstreg,index_reg,base_reg);
                break;
            default:
                assert(0);
                break;
        }
    }



}

//weren't ever called by jetstream... may need ot fix
//void instrument_rotate(INS ins) {}

void instrument_shift(INS ins)
{
    int count = INS_OperandCount(ins);
    REG reg, dstreg, midreg; 

    if(count == 2) {

	if (INS_IsMemoryWrite(ins)) 
	{
	    instrument_copy_mem2mem(ins, 0);
	}
	else { 
	    reg = INS_OperandReg(ins, 0);
	    instrument_copy_reg2reg(ins, reg, reg, 0);
	}
    } else if (count == 3) {
	if (INS_IsMemoryWrite(ins)) 
	{
	    if (INS_OperandIsReg(ins, 1)) 
	    {
		reg = INS_OperandReg(ins,1);
		instrument_merge_regmem2mem(ins,reg);
	    }
	    else 
	    {
		instrument_copy_mem2mem(ins, 0);
	    }
	}
	else { 
	    dstreg = INS_OperandReg(ins, 0);
	    if (INS_OperandIsReg(ins, 1)) 
	    {
		reg = INS_OperandReg(ins,1);
		instrument_merge_regreg2reg(ins,dstreg,reg);
	    }
	    else 
	    {
		instrument_copy_reg2reg(ins, dstreg, dstreg, 0); 

	    }	    
	}
    } else if (count == 4) {
	midreg = INS_OperandReg(ins, 1);
	if (INS_IsMemoryWrite(ins)) 
	{	    
	    //always start by merging in the midreg
	    instrument_merge_regmem2mem(ins,midreg);
	    if (INS_OperandIsReg(ins, 2)) 
	    {
		REG reg = INS_OperandReg(ins,1);
		instrument_merge_regmem2mem(ins,reg);
	    }
	    //no need for else, instruction acounted for by regmem2mem
	}
	else { 
	    dstreg = INS_OperandReg(ins, 0);
	    instrument_merge_regreg2reg(ins, dstreg, midreg);

	    if (INS_OperandIsReg(ins, 1)) 
	    {
		reg = INS_OperandReg(ins,1);
		instrument_merge_regreg2reg(ins,dstreg,reg);
	    }
	}


    }
    else { 
	cerr << "huh: " << INS_Disassemble(ins) << endl;
	assert(0);
    }
}

void instrument_addorsub(INS ins)
{
    INC_COUNT(ins);

    int op1mem;
    int op2mem;
    int op1reg;
    int op2reg;
    int op2imm;

    OPCODE opcode;

    opcode = INS_Opcode(ins);

    op1mem = INS_OperandIsMemory(ins, 0);
    op2mem = INS_OperandIsMemory(ins, 1);
    op1reg = INS_OperandIsReg(ins, 0);
    op2reg = INS_OperandIsReg(ins, 1);
    op2imm = INS_OperandIsImmediate(ins, 1);

    if(op1mem && op2reg) {
        REG reg = INS_OperandReg(ins, 1);
        if(!REG_valid(reg)) {
            return;
        }
        instrument_merge_regmem2mem(ins, reg); 

    } else if(op1reg && op2mem) {
        REG reg = INS_OperandReg(ins, 0);
        if(!INS_IsMemoryRead(ins) || !REG_valid(reg)) {
            //we ignore reads from video memory e.g. the gs segment register
            return;
        }
        instrument_merge_regmem2reg(ins, reg);
    } else if(op1reg && op2reg) {
        REG reg;
        REG dstreg;

        dstreg = INS_OperandReg(ins, 0);
        reg = INS_OperandReg(ins, 1);
        if(!REG_valid(dstreg) || !REG_valid(reg)) {
            return;
        } 

        if((opcode == XED_ICLASS_SUB || opcode == XED_ICLASS_XOR ||
	    opcode == XED_ICLASS_PXOR || opcode == XED_ICLASS_XORPS)  
                && (dstreg == reg)) {
	    instrument_copy_immval2reg(ins,dstreg);
        } else {
	    instrument_merge_regreg2reg(ins, dstreg, reg);

        }
    } else if(op1mem && op2imm) {
	instrument_copy_mem2mem(ins, 0);

    } else if(op1reg && op2imm){
        REG reg = INS_OperandReg(ins, 0);	
	instrument_copy_reg2reg(ins, reg, reg, 0);

    } else {
        //if the arithmatic involves an immediate instruction the taint does
        //not propagate...
        string instruction;
        instruction = INS_Disassemble(ins);
        printf("unknown combination of arithmatic ins: %s\n", instruction.c_str());
    }
}

/* Divide has 3 operands.
 *
 *  r/m, AX, AL/H/X <- quotient, AH <- remainder
 * */
//these instructions suck. 
void instrument_div(INS ins) 
{
    if (INS_IsMemoryRead(ins)) {
        UINT32 addrsize;
        // Register translations
        int msb_treg, lsb_treg, dst1_treg, dst2_treg;

        addrsize = INS_MemoryReadSize(ins);

        switch (addrsize) {
	    /*
	     * not using instrument helpers b/c the src and dest
	     * depend on the size of address
	    */
            case 1:
                // mem_loc is Divisor
		//AL, AH = AX / (addr)
                lsb_treg = translate_reg(LEVEL_BASE::REG_AX); // Dividend
                dst1_treg = translate_reg(LEVEL_BASE::REG_AL); // Quotient
                dst2_treg = translate_reg(LEVEL_BASE::REG_AH); // Remainder

		//AX + (addr) -> AL then AL -> AH
		MERGE_REGMEM2REG(1,2,0,0,dst1_treg,lsb_treg);
		COPY_REG2REG(1,1,0,dst2_treg, dst1_treg);
                break;
            case 2:
                // mem_loc is Divisor
                msb_treg = translate_reg(LEVEL_BASE::REG_DX);
                lsb_treg = translate_reg(LEVEL_BASE::REG_AX); // Dividend

		//AX + DX -> AX, AX + (addr) -> AX then AX -> DX
		MERGE_REGREG2REG(2,2,0,0,0,lsb_treg, lsb_treg, msb_treg);
		MERGE_REGMEM2REG(2,2,0,0,lsb_treg, lsb_treg); 
		COPY_REG2REG(2,0,0,msb_treg, lsb_treg);
                break;

            case 4:
                // Dividend is msb_treg:lsb_treg
                // Divisor is src_treg
                msb_treg = translate_reg(LEVEL_BASE::REG_EDX);
                lsb_treg = translate_reg(LEVEL_BASE::REG_EAX);

		//EAX + EDX -> EAX, EAX + (addr) -> EAX then EAX -> EDX
		MERGE_REGREG2REG(4,4,0,0,0,lsb_treg, lsb_treg, msb_treg);
		MERGE_REGMEM2REG(4,4,0,0,lsb_treg, lsb_treg); 
		COPY_REG2REG(4,0,0,msb_treg, lsb_treg);
                break;

            default:

                cerr << "[ERROR] Unsupported div sizes\n"; 
                cerr << "div ins " <<  INS_Disassemble(ins) << endl;
                assert(0);
                break;
		
        }
    } else {
        UINT32 size;
        REG src_reg;

        // Register translations
        int msb_treg, lsb_treg, src_treg, dst1_treg, dst2_treg;

        assert (INS_OperandIsReg(ins, 0));
        src_reg = INS_OperandReg(ins, 0);
        src_treg = translate_reg(src_reg);
        size = REG_Size(src_reg);

        switch (size) {
	    /*
	     * not using instrument helpers b/c the src and dest
	     * depend on the size of address
	     */
	case 1:
	    lsb_treg = translate_reg(LEVEL_BASE::REG_AX); // Dividend
	    dst1_treg = translate_reg(LEVEL_BASE::REG_AL); // Quotient
	    dst2_treg = translate_reg(LEVEL_BASE::REG_AH); // Remainder

	    //AX + src_treg -> AL then AL -> AH
	    if (REG_is_Lower8(src_reg)) {
		MERGE_REGREG2REG(1,2,0,0,0,dst1_treg,lsb_treg, src_treg);
	    } else if (REG_is_Upper8(src_reg)) {
		MERGE_REGREG2REG(1,2,0,0,1,dst1_treg,lsb_treg, src_treg);
	    }

	    COPY_REG2REG(1,1,0,dst2_treg, dst1_treg);
	    break;
	case 2:
	    msb_treg = translate_reg(LEVEL_BASE::REG_DX);
	    lsb_treg = translate_reg(LEVEL_BASE::REG_AX); // Dividend

	    //AX + DX -> AX, AX + src_reg -> AX then AX -> DX
	    MERGE_REGREG2REG(2,2,0,0,0,lsb_treg, lsb_treg, msb_treg);
	    MERGE_REGREG2REG(2,2,0,0,0,lsb_treg, lsb_treg, src_treg); 
	    COPY_REG2REG(2,0,0,msb_treg, lsb_treg);
	    break;

	case 4:
	    msb_treg = translate_reg(LEVEL_BASE::REG_EDX);
	    lsb_treg = translate_reg(LEVEL_BASE::REG_EAX);

	    //EAX + EDX -> EAX, EAX + src_treg -> EAX then EAX -> EDX
	    MERGE_REGREG2REG(4,4,0,0,0,lsb_treg, lsb_treg, msb_treg);
	    MERGE_REGREG2REG(4,4,0,0,0,lsb_treg, lsb_treg, src_treg); 
	    COPY_REG2REG(4,0,0,msb_treg, lsb_treg);
	    break;

	default:
	    cerr << "[ERROR] Unsupported div sizes\n"; 
	    cerr << "div ins " <<  INS_Disassemble(ins) << endl;
	    assert(0);
	    break;
        }
    }
}

void instrument_mul(INS ins) 
{
    if (INS_IsMemoryRead(ins)) {
        int lsb_dst_treg, msb_dst_treg;
        int src_treg;
        UINT32 addrsize;

        addrsize = INS_MemoryReadSize(ins);
        switch (addrsize) {
            case 1:
		// AX = AL * (mem)
                lsb_dst_treg = translate_reg(LEVEL_BASE::REG_AX);
                src_treg = translate_reg(LEVEL_BASE::REG_AL);
		
		MERGE_REGMEM2REG(2,1,0,0,lsb_dst_treg, src_treg);	       
                break;
            case 2:
		// DX:AX = AX * (mem)
                msb_dst_treg = translate_reg(LEVEL_BASE::REG_DX);
                src_treg = translate_reg(LEVEL_BASE::REG_AX); 

		// AX + (mem) -> AX, then AX -> DX
		MERGE_REGMEM2REG(2,2,0,0,src_treg, src_treg);	       
		COPY_REG2REG(2,0,0,msb_dst_treg, src_treg);	       
                break;
            case 4:
		// EDX:EAX = EAX * (mem)		
                msb_dst_treg = translate_reg(LEVEL_BASE::REG_EDX);
                src_treg = translate_reg(LEVEL_BASE::REG_EAX); 

		// EAX + (mem) -> EAX, then EAX -> EDX
		MERGE_REGMEM2REG(4,4,0,0,src_treg, src_treg);	       
		COPY_REG2REG(4,0,0,msb_dst_treg, src_treg);	       
                break;
            default:
                cerr << "[ERROR] Unsupported mul sizes" << endl;
                cerr <<  INS_Disassemble(ins) << endl;
                assert(0);
                break;
        }
    } else if (INS_OperandIsReg(ins, 0)) {
        REG src2_reg;
        int lsb_dst_treg, msb_dst_treg;
        int src_treg, src2_treg;

        assert (INS_OperandIsReg(ins, 0));
        src2_reg = INS_OperandReg(ins, 0);
	src2_treg = translate_reg(src2_reg);

        switch(REG_Size(src2_reg)) {
	case 1:
	    // AX = AL * reg
	    lsb_dst_treg = translate_reg(LEVEL_BASE::REG_AX);
	    src_treg = translate_reg(LEVEL_BASE::REG_AL);

	    // AL + reg -> AX
	    if (REG_is_Lower8(src2_reg)) {
		MERGE_REGREG2REG(2,1,0,0,0,lsb_dst_treg, src_treg, src2_treg);
	    } else {
		MERGE_REGREG2REG(2,1,0,0,1,lsb_dst_treg, src_treg, src2_treg);
	    }	    
	    break;

	case 2:
	    // DX:AX = AX * reg
	    msb_dst_treg = translate_reg(LEVEL_BASE::REG_DX);
	    src_treg = translate_reg(LEVEL_BASE::REG_AX); 
	    
	    // AX + reg -> AX, then AX -> DX
	    MERGE_REGREG2REG(2,2,0,0,0,src_treg, src_treg, src2_treg); 
	    COPY_REG2REG(2,0,0,msb_dst_treg, src_treg);	       
	    break;
	case 4:
	    // EDX:EAX = EAX * (mem)		
	    msb_dst_treg = translate_reg(LEVEL_BASE::REG_EDX);
	    src_treg = translate_reg(LEVEL_BASE::REG_EAX); 
	    
	    // EAX + reg -> EAX, then EAX -> EDX
	    MERGE_REGREG2REG(4,4,0,0,0,src_treg, src_treg, src2_treg);   
	    COPY_REG2REG(4,0,0,msb_dst_treg, src_treg);	       
	    break;
	default:
	    cerr << "[ERROR] Unsupported mul sizes" << endl;
	    cerr << INS_Disassemble(ins) << endl;
	    assert(0);
	    break;
        }
    } else {
        cerr <<  INS_Disassemble(ins) << endl;
        assert(0);
    }
}
void instrument_imul(INS ins) 
{
    int count = INS_OperandCount(ins);
    if (count == 2) {
        // one operand version is same as mul
	instrument_mul(ins); 
    } else if (count == 3) {
	REG dst_reg= INS_OperandReg(ins, 0);
        assert (REG_valid(dst_reg));

        if (INS_IsMemoryRead(ins)) {
	    instrument_merge_regmem2reg(ins, dst_reg);
	}
	else { 
	    //reg
	    REG src_reg = INS_OperandReg(ins, 1);
	    instrument_merge_regreg2reg(ins, dst_reg, src_reg);
	}
    }
    else if (count == 4) {
        // three operand version is taint src to dst

	REG dst_reg;
	int dst_treg;
	assert (INS_OperandIsReg(ins, 0));
	
	dst_reg = INS_OperandReg(ins, 0);
	dst_treg = translate_reg(dst_reg);

	if (INS_IsMemoryRead(ins)) {
	    UINT32 memsize = INS_MemoryWriteSize(ins);
	    switch (memsize) { 
	    case 2:
		COPY_MEM2REG(2,0,dst_treg); 
		break;
	    case 4:
		COPY_MEM2REG(4,0,dst_treg);
		break;
	    default:
		cerr << "[ERROR] imul unsupported sizes" << endl;
		cerr << INS_Disassemble(ins) << endl;
		assert(0);
		break;
	    }
	} else {
	    REG src_reg = INS_OperandReg(ins, 1);
	    assert(REG_valid(src_reg));
	    int src_treg = translate_reg(src_reg);
	    assert (REG_Size(dst_reg) == REG_Size(src_reg));
	    
	    switch (REG_Size(dst_reg)) {
	    case 2:
		COPY_REG2REG(2,0,0,dst_treg, src_treg);
		break;
	    case 4:
		COPY_REG2REG(4,0,0,dst_treg, src_treg);
		break;
	    default:
		cerr << "[ERROR] imul unsupported sizes" << endl;
		cerr << INS_Disassemble(ins) << endl;
		assert(0);
		break;
	    }
	}
    } else {
	// in this case the instruction looks like this:
	//   imul dword ptr [esp+0x88]
	//   which is just like the 2 count version,
	//   it's 4 because of the index register
	instrument_mul(ins);
    }
}


void instrument_palignr(INS ins){}
void instrument_psrldq(INS ins){}
void instrument_pmovmskb(INS ins){}

#ifdef TAINT_DEBUG
void trace_inst(ADDRINT ptr)
{
    taint_debug_inst = ptr;
}
#endif

//extends EAX / AX to EDX:EAX / DX:AX 
void instrument_cdq(INS ins) 
{ 
    REG reg = INS_OperandReg(ins,0);
    uint_fast32_t regsize = REG_Size(reg);
    switch(regsize) { 
    case 2:
	COPY_IMMVAL2REG(2,0,LEVEL_BASE::REG_DX);
	break;
    case 4:
	COPY_IMMVAL2REG(4,0,LEVEL_BASE::REG_EDX);
	break;
    default:
	cerr << "[ERROR] instrument_ddq: unknown reg size" << regsize <<endl;
	assert(0);
	break;
    }      
}

void bw_slice_icount()
{
    full_instcount++;
}

void instruction_instrumentation(INS ins, void *v)
{
    OPCODE opcode;
    UINT32 category;
    int instrumented = 0;

    INS_InsertCall(ins, IPOINT_BEFORE, AFUNPTR(bw_slice_icount),
		   IARG_END);
 
#ifdef TAINT_STATS
    inst_instrumented++;
#endif
    if(INS_IsSyscall(ins)) {
        INS_InsertCall(ins, IPOINT_BEFORE, AFUNPTR(instrument_syscall),
                IARG_SYSCALL_NUMBER, 
                IARG_SYSARG_VALUE, 0, 
                IARG_SYSARG_VALUE, 1,
                IARG_SYSARG_VALUE, 2,
                IARG_SYSARG_VALUE, 3,
                IARG_SYSARG_VALUE, 4,
                IARG_SYSARG_VALUE, 5,
                IARG_END);
    }

    opcode = INS_Opcode(ins);
    category = INS_Category(ins);

    //figure out if this is causing an issue. 
    
    if (slice_ip && slice_ip == INS_Address(ins)) 
	INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)instruction_check_output,
		       IARG_INST_PTR,IARG_END);
    

    
    if (category == XED_CATEGORY_CMOV) {
        // We separate out the tainting of the movement of data with
        //  cf, since we can do so much faster if we don't care about cf
#ifdef CTRL_FLOW
        // TODO
        instrument_cmov_cf(ins);
#else
        instrument_cmov(ins);
#endif
    } else if (category == XED_CATEGORY_SHIFT) {
        instrument_shift(ins);
    } else {
        switch(opcode) {
            // Move and sign/zero extend
	case XED_ICLASS_MOV:
	    instrument_mov(ins,0);
	    break;

	case XED_ICLASS_MOVSX:
	case XED_ICLASS_MOVZX:
	    //flags affected: none
	    instrument_mov(ins, 1);
	    break;
	case XED_ICLASS_MOVD:
	case XED_ICLASS_MOVQ:
	    instrument_mov(ins, 1);
	    break;
	case XED_ICLASS_MOVDQU:
	case XED_ICLASS_MOVDQA:
	case XED_ICLASS_MOVAPS:
	case XED_ICLASS_MOVUPS:
	case XED_ICLASS_MOVLPD:
	case XED_ICLASS_MOVHPD:
	case XED_ICLASS_MOVNTDQA:
	case XED_ICLASS_MOVNTDQ:
	    instrument_mov(ins, 0);
	    break;
	case XED_ICLASS_PALIGNR:
	    instrument_palignr(ins);
	    break;
	case XED_ICLASS_MOVSB:
	case XED_ICLASS_MOVSW:
	case XED_ICLASS_MOVSD:
	case XED_ICLASS_MOVSQ:
	    instrument_move_string(ins);
	    break;
	case XED_ICLASS_STOSB:
	case XED_ICLASS_STOSW:
	case XED_ICLASS_STOSD:
	case XED_ICLASS_STOSQ:
	    instrument_store_string(ins);
	    break;
	case XED_ICLASS_LODSB:
	case XED_ICLASS_LODSW:
	case XED_ICLASS_LODSD:
	case XED_ICLASS_LODSQ:
	    instrument_load_string(ins);
	    break;
	case XED_ICLASS_XCHG:
	    instrument_xchg(ins);
	    break;
	case XED_ICLASS_BSWAP:
	    instrument_bswap(ins);
	    break;
	    /*
	      case XED_ICLASS_CMPXCHG:
	      instrument_cmpxchg(ins);
	      break;
	    */
	case XED_ICLASS_PUSH:
	    instrument_push(ins);
	    break;
	case XED_ICLASS_POP:
	    instrument_pop(ins);
	    break;
	case XED_ICLASS_LEA:
	    instrument_lea(ins);
	    break;
	case XED_ICLASS_XADD:
	    instrument_xchg(ins);
	    instrument_addorsub(ins);
	    break;
	case XED_ICLASS_ADD:
	case XED_ICLASS_SUB:
	case XED_ICLASS_SBB:
	case XED_ICLASS_OR:
	case XED_ICLASS_AND:
	case XED_ICLASS_XOR:
	    instrument_addorsub(ins);
	    break;
	case XED_ICLASS_ADC:
	    instrument_addorsub(ins);
#ifdef CTRL_FLOW
	    // TODO extra taint movement for flags
#endif
	    break;
	case XED_ICLASS_DIV:
	case XED_ICLASS_IDIV:
	    instrument_div(ins);
	    break;
	case XED_ICLASS_MUL:
	    instrument_mul(ins);
	    break;
	case XED_ICLASS_IMUL:
	    instrument_imul(ins);
	    break;

	case XED_ICLASS_CWD:
	case XED_ICLASS_CDQ:
	    instrument_cdq(ins);
	    break;
	    
            // now all of the XMM packed instructions
	case XED_ICLASS_POR:
	case XED_ICLASS_PAND:
	case XED_ICLASS_PANDN:
	case XED_ICLASS_PXOR:
	case XED_ICLASS_PADDB:
	case XED_ICLASS_PADDW:
	case XED_ICLASS_PADDD:
	case XED_ICLASS_PADDQ:
	case XED_ICLASS_PSUBB:
	case XED_ICLASS_PSUBW:
	case XED_ICLASS_PSUBD:
	case XED_ICLASS_PSUBQ:
	case XED_ICLASS_PMADDWD:
	case XED_ICLASS_PMULHUW:
	case XED_ICLASS_PMINUB:
	case XED_ICLASS_PMULLW:
	case XED_ICLASS_PADDUSW:
	case XED_ICLASS_PADDUSB:
	case XED_ICLASS_PACKUSWB:
	case XED_ICLASS_PSHUFHW:
	case XED_ICLASS_PSHUFLW:
	case XED_ICLASS_XORPS:
	case XED_ICLASS_SUBSD:
	case XED_ICLASS_DIVSD:
	    instrument_addorsub(ins);
	    break;
	case XED_ICLASS_PCMPEQB:
	case XED_ICLASS_PCMPEQW:
	case XED_ICLASS_PCMPEQD:
	case XED_ICLASS_PCMPGTB:
	case XED_ICLASS_PCMPGTW:
	case XED_ICLASS_PCMPGTD:
	case XED_ICLASS_PCMPGTQ:
	    instrument_addorsub(ins);
	    break;
	case XED_ICLASS_PSRLDQ:
	    instrument_psrldq(ins);
	    break;
	    /*
	      case XED_ICLASS_PSRLW:
	      case XED_ICLASS_PSRLD:
	      case XED_ICLASS_PSRLQ:
	      assert(0);
	      break;
	    */
	case XED_ICLASS_PMOVMSKB:
	    instrument_pmovmskb(ins);
	    break;
	case XED_ICLASS_PUNPCKHBW:
	case XED_ICLASS_PUNPCKLBW:
	case XED_ICLASS_PUNPCKHWD:
	case XED_ICLASS_PUNPCKLWD:
	case XED_ICLASS_PUNPCKHDQ:
	case XED_ICLASS_PUNPCKLDQ:
	case XED_ICLASS_PUNPCKHQDQ:
	case XED_ICLASS_PUNPCKLQDQ:
	    instrument_addorsub(ins);
	    break;
	    /*
	      case XED_ICLASS_PSHUFD:
	      break;
	    */
	case XED_ICLASS_CALL_NEAR:
	case XED_ICLASS_CALL_FAR:
	case XED_ICLASS_RET_NEAR:
	case XED_ICLASS_RET_FAR:
	    break;
#ifdef CTRL_FLOW
            // TODO
            // case XED_ICLASS_INC:
            // case XED_ICLASS_DEC:
            // case XED_ICLASS_NEG:
	    // flags affected: all but CF
            //    break;
#else
	case XED_ICLASS_INC:
	case XED_ICLASS_DEC:
	case XED_ICLASS_NEG:
	    // flags affected: all but CF
	    break;
#endif
	    /*
	      case XED_ICLASS_ROR:
	      case XED_ICLASS_ROL:
	      case XED_ICLASS_RCL:
	      case XED_ICLASS_RCR:
	      instrument_rotate(ins);
	      // flags affected: CF, OF
	      break;
	    */
#ifdef CTRL_FLOW
	case XED_ICLASS_SETB:
	case XED_ICLASS_SETNB:
	case XED_ICLASS_SETL:
	case XED_ICLASS_SETNL:
	case XED_ICLASS_SETNBE:
	case XED_ICLASS_SETBE:
	case XED_ICLASS_SETLE:
	case XED_ICLASS_SETNLE:
	case XED_ICLASS_SETNO:
	case XED_ICLASS_SETO:
	case XED_ICLASS_SETNP:
	case XED_ICLASS_SETP:
	case XED_ICLASS_SETNS:
	case XED_ICLASS_SETS:
	case XED_ICLASS_SETZ:
	case XED_ICLASS_SETNZ:
	    // TODO
	    break;
#else
	case XED_ICLASS_SETB:
	case XED_ICLASS_SETNB:
	case XED_ICLASS_SETL:
	case XED_ICLASS_SETNL:
	case XED_ICLASS_SETNBE:
	case XED_ICLASS_SETBE:
	case XED_ICLASS_SETLE:
	case XED_ICLASS_SETNLE:
	case XED_ICLASS_SETNO:
	case XED_ICLASS_SETO:
	case XED_ICLASS_SETNP:
	case XED_ICLASS_SETP:
	case XED_ICLASS_SETNS:
	case XED_ICLASS_SETS:
	case XED_ICLASS_SETZ:
	case XED_ICLASS_SETNZ:
	    //instrument_clear_dst(ins);
	    break;
#endif
#ifdef CTRL_FLOW
            // TODO
	    // case XED_ICLASS_BSF:
	    // case XED_ICLASS_BSR:
	    //     break;
#else
	case XED_ICLASS_BSF:
	case XED_ICLASS_BSR:
	    //instrument_clear_dst(ins);
	    break;
#endif
#ifndef CTRL_FLOW
            // these instructions only affect control flow
	case XED_ICLASS_TEST:
	    break;
	case XED_ICLASS_PTEST:
	    break;
	case XED_ICLASS_CMP:
	    break;
	case XED_ICLASS_NOT:
	    break;
	case XED_ICLASS_LEAVE:
	    break;
	case XED_ICLASS_CLD:
	    break;
	case XED_ICLASS_BT:
	    break;
#endif


        case XED_ICLASS_JMP:
        case XED_ICLASS_JB:
        case XED_ICLASS_JNB:
        case XED_ICLASS_JBE:
        case XED_ICLASS_JNBE:
        case XED_ICLASS_JL:
        case XED_ICLASS_JNL:
        case XED_ICLASS_JLE:
        case XED_ICLASS_JNLE:
        case XED_ICLASS_JNO:
        case XED_ICLASS_JO:
        case XED_ICLASS_JNP:
        case XED_ICLASS_JP:
        case XED_ICLASS_JNS:
        case XED_ICLASS_JS:
        case XED_ICLASS_JNZ:
        case XED_ICLASS_JZ:
	    #ifndef DIFT
	    INC_COUNT(ins);
	    INS_InsertCall(ins, IPOINT_BEFORE,			
			   (AFUNPTR)log_jump,		
			   IARG_FAST_ANALYSIS_CALL,		
			   IARG_BRANCH_TARGET_ADDR,		
			   IARG_FALLTHROUGH_ADDR,		
			   IARG_BRANCH_TAKEN,			
			   IARG_INST_PTR,			       
			   IARG_END);

	    #endif
	    break;

	case XED_ICLASS_CPUID:
	    // ignore this instruction
	    break;
	default:

	    if(INS_Address(ins) == 0x804860a)
		fprintf(stderr, "%x: %s, %d\n",INS_Address(ins),INS_Disassemble(ins).c_str(), instrumented);

	    if (INS_IsNop(ins)) {
		INSTRUMENT_PRINT(stderr, "%#x: not instrument noop %s\n",
				 INS_Address(ins), INS_Disassemble(ins).c_str());
		break;
	    }
	    if (INS_IsInterrupt(ins)) {
		INSTRUMENT_PRINT(stderr, "%#x: not instrument an interrupt\n",
				 INS_Address(ins));
		break;
	    }
	    if (INS_IsRDTSC(ins)) {
		INSTRUMENT_PRINT(stderr, "%#x: not instrument an rdtsc\n",
				 INS_Address(ins));
		break;
	    }
	    if (INS_IsSysenter(ins)) {
		INSTRUMENT_PRINT(stderr, "%#x: not instrument a sysenter\n",
				 INS_Address(ins));
		break;
	    }
	    if (instrumented) {
		break;
	    }
	    INSTRUMENT_PRINT(stderr, "[NOOP] ERROR: instruction %s is not instrumented, address: %#x\n",
                        INS_Disassemble(ins).c_str(), (unsigned)INS_Address(ins));
	    // XXX This printing may cause a deadlock in Pin!
	    break;
        }
    }
}

void trace_instrumentation(TRACE trace, void* v)
{
    struct timeval tv_end, tv_start;
    gettimeofday (&tv_start, NULL);
    TRACE_InsertCall(trace, IPOINT_BEFORE, (AFUNPTR) syscall_after_redo, IARG_INST_PTR, IARG_END);

    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {
	for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins)) {
	    instruction_instrumentation (ins, NULL);

	    ip_to_instruction[INS_Address(ins)] = ins; 
	    INS nins = ins;
	    if (INS_Address(ins) == 0x805c6ae)
	    {
		std::cerr << std::hex << INS_Address(ins) << " " 
			  << INS_Disassemble(ins) << std::endl;

		auto iter = ip_to_instruction.find(INS_Address(ins));
		std::cerr << INS_Disassemble(nins) << std::endl;
		std::cerr << INS_Disassemble(iter->second) << std::endl;		
	    }

	}

#ifndef DIFT
	INS tail = BBL_InsTail(bbl);
	if (INS_IsCall(tail)) { 
	    INC_COUNT(tail);
	    if (INS_IsDirectBranchOrCall(tail)) { 
		ADDRINT target = INS_DirectBranchOrCallTargetAddress(tail);
		INS_InsertPredicatedCall(tail, IPOINT_BEFORE,
					 (AFUNPTR)log_call,
					 IARG_FAST_ANALYSIS_CALL,		
					 IARG_INST_PTR,
					 IARG_ADDRINT, target,
					 IARG_REG_VALUE, REG_STACK_PTR,
					 IARG_END);
	    }
	    else { 

		INS_InsertPredicatedCall(tail, IPOINT_BEFORE,
					 (AFUNPTR)log_call,
					 IARG_FAST_ANALYSIS_CALL,		
					 IARG_INST_PTR,
					 IARG_BRANCH_TARGET_ADDR,
					 IARG_REG_VALUE, REG_STACK_PTR,
					 IARG_END);	    
	    }
	}
	else if (INS_IsRet(tail)) { 
	    INC_COUNT(tail);
	    INS_InsertPredicatedCall(tail, IPOINT_BEFORE,
				     (AFUNPTR)log_ret,
				     IARG_FAST_ANALYSIS_CALL,		
				     IARG_REG_VALUE, REG_STACK_PTR,
				     IARG_INST_PTR,
				     IARG_END);
	}
#endif

    }

    gettimeofday (&tv_end, NULL);
    traces_instrumented++;
    instrument_time += tv_end.tv_usec - tv_start.tv_usec + (tv_end.tv_sec - tv_start.tv_sec) * 1000000;
}

void thread_start (THREADID threadid, CONTEXT* ctxt, INT32 flags, VOID* v)
{
    struct thread_data* ptdata;

    // TODO Use slab allocator
    ptdata = (struct thread_data *) malloc (sizeof(struct thread_data));
    if (ptdata == NULL) {
	fprintf (stderr, "Unable to malloc pdata\n");
	assert (0);
    }
    assert(ptdata);
    memset(ptdata, 0, sizeof(struct thread_data));
    ptdata->threadid = threadid;
    ptdata->app_syscall = 0;
    ptdata->record_pid = get_record_pid();
    get_record_group_id(dev_fd, &(ptdata->rg_id));


    int thread_ndx;
    long thread_status = set_pin_addr (dev_fd, (u_long) &(ptdata->app_syscall), ptdata, (void **) &current_thread, &thread_ndx);
    if (!(thread_status&PIN_ATTACH_BLOCKED)) {
	current_thread = ptdata;
    }
    if (thread_status&PIN_ATTACH_REDO) {
	//fprintf (stderr, "Need to redo system call (mmap)\n");
	redo_syscall++;
    }
    PIN_SetThreadData (tls_key, ptdata, threadid);



    if (splice_output && thread_status > 0) {
	initialize_startingReg(thread_ndx, ptdata); 
    }




    if (first_thread) {
#ifdef EXEC_INPUTS
        int acc = 0;
        char** args;
        struct taint_creation_info tci;
#endif
        first_thread = 0;
        if (!ptdata->syscall_cnt) {
            ptdata->syscall_cnt = 1;
        }
#ifdef EXEC_INPUTS
        args = (char **) get_replay_args (dev_fd);
        tci.rg_id = ptdata->rg_id;
        tci.record_pid = ptdata->record_pid;
        tci.syscall_cnt = ptdata->syscall_cnt;
        tci.offset = 0;
        tci.fileno = FILENO_ARGS;
        tci.data = 0;
        while (1) {
            char* arg;
            arg = *args;
            // args ends with a NULL
            if (!arg) {
                break;
            }
            fprintf (stderr, "input arg is %s\n", arg);
            tci.offset = acc;
            create_taints_from_buffer(arg, strlen(arg) + 1, &tci, tokens_fd,
                                                            (char *) "EXEC_ARG");
            acc += strlen(arg) + 1;
            args += 1;
        }
        // Retrieve the location of the env. var from the kernel
        args = (char **) get_env_vars (dev_fd);
        LOG_PRINT ("env. vars are %#lx\n", (unsigned long) args);
        tci.fileno = FILENO_ENVP;
        while (1) {
            char* arg;
            arg = *args;
            // args ends with a NULL
            if (!arg) {
                break;
            }
            fprintf ("input arg is %s\n", arg);
            tci.offset = acc;
            create_taints_from_buffer(arg, strlen(arg) + 1, &tci, tokens_fd,
                                                            (char *) "EXEC_ENV");
            acc += strlen(arg) + 1;
            args += 1;
        }
#endif
    }
//    fprintf(stderr, "%d done 1\n",PIN_GetTid());
    active_threads[ptdata->record_pid] = ptdata;
    fprintf(stderr, "%d done with thread_start\n",PIN_GetTid());
}

void thread_fini (THREADID threadid, const CONTEXT* ctxt, INT32 code, VOID* v)
{
    struct thread_data* tdata = (struct thread_data *) PIN_GetThreadData(tls_key, threadid);
    active_threads.erase(tdata->record_pid);
}

#ifndef NO_FILE_OUTPUT
void init_logs(void)
{

#ifdef VERBOSE    
    char log_name[256];
    if (!log_f.is_open()) {
        snprintf(log_name, 256, "%s/confaid.log.%d",
                group_directory, PIN_GetPid());
        log_f.open(log_name);
    }
#endif
#ifdef TAINT_DEBUG
    {
        char debug_log_name[256];
        if (!debug_f) {
            snprintf(debug_log_name, 256, "%s/debug_taint", group_directory);
	    debug_f = fopen(debug_log_name, "w");
            if (!debug_f) {
                fprintf(stderr, "could not create debug taint log file, errno %d\n", errno);
                exit(0);
            }
        }
w    }
#endif
#ifdef TAINT_STATS
    {
        char stats_log_name[256];
        if (!stats_f) {
            snprintf(stats_log_name, 256, "%s/taint_stats", group_directory);
	    stats_f = fopen(stats_log_name, "w");
            if (!stats_f) {
                fprintf(stderr, "could not create taint stats file, errno %d\n", errno);
                exit(0);
            }
        }
	gettimeofday(&begin_tv, NULL);
    }
#endif
}
#endif

void fini(INT32 code, void* v)
{
    dift_done ();
}

VOID ImageLoad (IMG img, VOID *v)
{
/*
    struct image imm;
    imm.load_offset = IMG_LoadOffset(img);
    imm.low_addr = IMG_LowAddress(img);
    imm.high_addr = IMG_HighAddress(img);
    sprintf(imm.name, "%s",IMG_Name(img).c_str());

    int rc = write(images_fd, &imm, sizeof(imm));
    if (rc != sizeof(imm)) { 
	printf("huh? %d %d, fd: %d",rc, errno, images_fd);
    }
    assert(rc == sizeof(imm));
*/	    
//    fprintf(stderr, "[IMG] Loading image name %s with load offset %#lx, (%#lx, %#lx)\n",
//            IMG_Name(img).c_str(), imm.load_offset, imm.low_addr, imm.high_addr);
}


int get_open_file_descriptors ()
{
    struct open_fd ofds[4096];
    long rc = get_open_fds (dev_fd, ofds, 4096);
#ifdef TAINT_DEBUG      
	fprintf (debug_f, "get_open_file_desciptors returns %ld\n", rc);
#endif
    
    if (rc < 0) {
	fprintf (stderr, "get_open_file_desciptors returns %ld\n", rc);
	return rc;
    }

    for (long i = 0; i < rc; i++) {
#ifdef TAINT_DEBUG
	int fd = -1;
#endif
	if (ofds[i].type == OPEN_FD_TYPE_FILE) {
	    struct open_info* oi = (struct open_info *) malloc (sizeof(struct open_info));
	    strcpy (oi->name, ofds[i].channel);
	    oi->flags = 0;
	    oi->fileno = 0;
	    monitor_add_fd(open_fds, ofds[i].fd, oi);
#ifdef TAINT_DEBUG	    
	    fd = ofds[i].fd;
#endif
	} else if (ofds[i].type == OPEN_FD_TYPE_SOCKET) {
	    struct socket_info* si = (struct socket_info *) malloc (sizeof(struct socket_info));
	    si->domain = ofds[i].data;
	    si->type = -1;
	    si->protocol = -1;
	    si->fileno = -1; 
	    si->ci = NULL;
	    monitor_add_fd(open_socks, ofds[i].fd, si);
#ifdef TAINT_DEBUG
	    fd = ofds[i].fd;
#endif
	}
#ifdef TAINT_DEBUG
	fprintf (debug_f, "get_open_fds %d\n",fd);
#endif	
	
    }
    return 0;
}

vector<REG> get_regs(INS ins)
{
    vector<REG> regs; 
    if(INS_IsMemoryRead(ins)) {
        REG index_reg = INS_OperandMemoryIndexReg(ins, 1);
        REG base_reg = INS_OperandMemoryBaseReg(ins, 1);
	if (REG_valid(index_reg))
	    regs.push_back(index_reg);
	if (REG_valid(base_reg))
	    regs.push_back(base_reg);

    } else if(INS_IsMemoryWrite(ins)) {
        REG index_reg = INS_OperandMemoryIndexReg(ins, 0);
        REG base_reg = INS_OperandMemoryBaseReg(ins, 0); //try this??? 
	if (REG_valid(index_reg))
	    regs.push_back(index_reg);
	if (REG_valid(base_reg))
	    regs.push_back(base_reg);
    } 
    else 
    {
	std::cerr << "what's the alternative?" << std::endl;
    }
    return regs;
}


bool signal_handler(THREADID tid, INT32 sig, CONTEXT *ctx, BOOL hasHandler, 
		    const EXCEPTION_INFO *pExceptionInfo, VOID *v)
{
    std::cerr << "got a signal" << sig << std::endl;
    std::cerr << "exception " << PIN_ExceptionToString(pExceptionInfo) << std::endl;

    ADDRINT inst = PIN_GetExceptionAddress(pExceptionInfo);
    std::cerr << "failed @ " << std::hex << inst << std::endl;

    auto iter = ip_to_instruction.find(inst);
    if (iter == ip_to_instruction.end())
    {
	assert(false);

    }
    INS ins = iter->second;
    std::cerr << "[" << INS_Disassemble(ins) << "]" << std::endl;
    
    vector<REG> accessedRegs = get_regs(ins);
    for (auto &r : accessedRegs) {
	std::cerr << "slicing on " << r << " @ " << inst << std::endl;
	instruction_output(inst, (void *)r, 4);
    }

    return true;
}

int main(int argc, char** argv) 
{    
    int rc;

    // This is a very specific trick to figure out if we're a child or not
    if (!strcmp(argv[4], "--")) { // pin injected into forked process
        child = 1;
    } else { // pin attached to replay process
        child = 0;
    }

    PIN_InitSymbols();
    if (PIN_Init(argc, argv)) {
        fprintf(stderr, "ERROR: could not initialize Pin?\n");
        exit(-1);
    }

    tls_key = PIN_CreateThreadDataKey(0);

    // Intialize the replay device
    rc = devspec_init (&dev_fd);
    if (rc < 0) return rc;
    global_syscall_cnt = 0;

    /* Create a directory for logs etc for this replay group*/
    snprintf(group_directory, 256, "/tmp/%d", PIN_GetPid());
#ifndef NO_FILE_OUTPUT
    if (mkdir(group_directory, 0755)) {
        if (errno == EEXIST) {
            fprintf(stderr, "directory already exists, using it: %s\n", group_directory);
        } else {
            fprintf(stderr, "could not make directory %s\n", group_directory);
            exit(-1);
        }
    }
#endif

    // Read in command line args
    segment_length = KnobSegmentLength.Value();
    splice_output = KnobSpliceOutput.Value();
    all_output = KnobAllOutput.Value();
    fork_flags = KnobForkFlags.Value().c_str();         


#ifdef RETAINT
    retaint = KnobRetaintEpochs.Value().c_str();
    retaint_str = (char *) retaint;
    char* p;
    for (p = retaint_str; *p != '\0' && *p != ','; p++);
    *p = '\0';
    retaint_next_clock = strtoul(retaint_str, NULL, 10);
    fprintf (stderr, "Next epoch to retaint: %lu\n", retaint_next_clock);
    retaint_str = p+1;
#endif      
    
#ifdef RECORD_TRACE_INFO
    record_trace_info = KnobRecordTraceInfo.Value();
#endif
    fork_flags_index = 0;   

    /*
     * if there are fork_flags (non-null and len > 0), 
     * and there is a 1 in the fork_flags, we shouldn't start 
     * producing output right away (until we get to the point where
     * we stop following forks). 
     */

    if (fork_flags && strlen(fork_flags) &&
	strstr(fork_flags, "1")) { 

	produce_output = false;
    }

    if (!open_fds) {
        open_fds = new_xray_monitor();
    }
    if (!open_socks) {
        open_socks = new_xray_monitor();
    }

#ifndef NO_FILE_OUTPUT
    init_logs();
#endif

    // Determine open file descriptors for filters
    if (splice_output) get_open_file_descriptors();

    init_graph(group_directory);
    init_input_output(group_directory);

    // Try to map the log clock for this epoch
    ppthread_log_clock = map_shared_clock(dev_fd);
    //printf ("Log clock is %p, value is %ld\n", ppthread_log_clock, *ppthread_log_clock);

    if (!child) {
        // input filters
	const char *input_filename = KnobFilterInputFile.Value().c_str();

	FILE* file = fopen(input_filename, "r");
	if (file) {
	    add_input_filters(file);
	}

	file = fopen(KnobFilterDiftFile.Value().c_str(), "r");	
	if (file)
	{ 
	    cerr << getpid() << ": importing dift file " << KnobFilterDiftFile.Value() << endl;
	    add_taint_num_filters(file);
	    fclose(file);
	}
	dump_input_filters();	


	file = fopen(KnobStartingAS.Value().c_str(), "r"); 
	if (file)
	{
	    cerr << getpid() <<" importing starting address space " << KnobStartingAS.Value() << endl;
	    initialize_starting_nodeids(file);
	    fclose(file);
	}

	file = fopen(KnobEndingAS.Value().c_str(), "r"); 
	if (file)
	{
	    cerr << getpid() <<" importing ending address space " << KnobEndingAS.Value() << endl;
	    initialize_endingAS(file);
	    fclose(file);
	}


        // output filters
	u_long filter_outputs_before = KnobFilterOutputsBefore.Value();
	if (filter_outputs_before) {
	    add_output_filters(filter_outputs_before);
	    fprintf (stderr, "Filtering to outputs on or after %lu\n", filter_outputs_before);
	}

	slice_clock = KnobSliceClock.Value();
	slice_ip = KnobSliceIP.Value();
	slice_location = KnobSliceLocation.Value();
	slice_size = KnobSliceSize.Value();
	
	if (slice_ip) {
	    cerr << dec << getpid() << std::hex << ": slicing on ip "<< slice_ip << ", clock " << slice_clock;
	} else { 
	    if (all_output)
		cerr << std::dec << getpid() << ": intermediate epoch";
	    else
	    {
		slice_on_errors = true;
	    }
	}

	if (splice_output) { 
	    cerr << " starting in middle finishes at " << dec << segment_length << endl;
	}
	else 
	    cerr << " starting at the beginning finishes at " << dec << segment_length << endl;
    }
    
    const char* hostname = KnobNWHostname.Value().c_str();
    int port = KnobNWPort.Value();
    struct hostent* hp = gethostbyname (hostname);

    if (hp != NULL) { 
	s = socket (AF_INET, SOCK_STREAM, 0);
	if (s < 0) {
	    fprintf (stderr, "Cannot create socket, errno=%d\n", errno);
	    return -1;
	}
    
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	memcpy (&addr.sin_addr, hp->h_addr, hp->h_length);
    
	int tries = 0;
	do {
	    rc = connect (s, (struct sockaddr *) &addr, sizeof(addr));
	    if (rc < 0) {
		tries++;
		usleep(1000);
	    }
	} while (rc < 0 && tries <=100); //whats the punishment for increasing tries here? 
    
	if (rc < 0) {
	    fprintf (stderr, "Cannot connect to socket (host %s, port %d), errno=%d\n", hostname, port, errno);
	    return -1;
	}
    }

    PIN_AddThreadStartFunction(thread_start, 0);
    PIN_AddThreadFiniFunction(thread_fini, 0);
    PIN_AddFiniFunction(fini, 0);

    main_prev_argv = argv;

    TRACE_AddInstrumentFunction (trace_instrumentation, 0);
    IMG_AddInstrumentFunction (ImageLoad, 0);    
    PIN_AddSyscallExitFunction(instrument_syscall_ret, 0);

    if (slice_on_errors)
	PIN_InterceptSignal(SIGSEGV, signal_handler, 0);
    PIN_StartProgram();

    return 0;

}
