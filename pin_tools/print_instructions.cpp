#include "pin.H"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <assert.h>
#include <sys/types.h>
#include <syscall.h>
#include "util.h"
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/uio.h>

#include <linux/unistd.h>
#include <asm/ldt.h>

#include <glib-2.0/glib.h>
#include <iostream>
#include <sstream>

//#define PLUS_TWO
//#define TIMING_ON

#define INTERESTING(x) (x == 0xb76c8cf0)// (x == 0xb53feb54 || x == 0xb5e1c870 || x == 0xb5e1c874 || x == 0xbfb561cc)

struct thread_data* current_thread; // Always points to thread-local data (changed by kernel on context switch)

long print_limit = 10;
KNOB<string> KnobPrintLimit(KNOB_MODE_WRITEONCE, "pintool", "p", "10000000", "syscall print limit");
long print_stop = 10;
KNOB<string> KnobPrintStop(KNOB_MODE_WRITEONCE, "pintool", "s", "10000000", "syscall print stop");

// #define DEBUG_FUNCTIONS
#ifdef DEBUG_FUNCTIONS
long function_print_limit = 10;
long function_print_stop = 10;
KNOB<string> KnobFunctionPrintLimit(KNOB_MODE_WRITEONCE, "pintool", "f", "10000000", "function print limit");
KNOB<string> KnobFunctionPrintStop(KNOB_MODE_WRITEONCE, "pintool", "g", "10000000", "function print stop");
#endif

u_long* ppthread_log_clock = NULL;

long global_syscall_cnt = 0;
/* Toggle between which syscall count to use */
#define SYSCALL_CNT tdata->syscall_cnt
// #define SYSCALL_CNT global_syscall_cnt

// Use a Pin virtual register to store the TLS pointer
#define USE_TLS_SCRATCH
#ifdef USE_TLS_SCRATCH
REG tls_reg;
#endif

struct thread_data {
    u_long app_syscall; // Per thread address for specifying pin vs. non-pin system calls
    int record_pid; 	// per thread record pid
    int syscall_cnt;	// per thread count of syscalls
    int sysnum;		// current syscall number
    u_long ignore_flag;
  u_int *ptr_val; //special garbage
};

ADDRINT array[10000];
int child = 0;

int fd; // File descriptor for the replay device
TLS_KEY tls_key; // Key for accessing TLS. 

GHashTable* sysexit_addr_table; 

int get_record_pid(void);


ADDRINT find_static_address(ADDRINT ip)
{
	PIN_LockClient();
	IMG img = IMG_FindByAddress(ip);
	if (!IMG_Valid(img)) return ip;
	ADDRINT offset = IMG_LoadOffset(img);
	PIN_UnlockClient();
	return ip - offset;
}


inline void increment_syscall_cnt (struct thread_data* ptdata, int syscall_num)
{
	// ignore pthread syscalls, or deterministic system calls that we don't log (e.g. 123, 186, 243, 244)
    if (!(syscall_num == 17 || syscall_num == 31 || syscall_num == 32 || 
	  syscall_num == 35 || syscall_num == 44 || syscall_num == 53 || 
	  syscall_num == 56 || syscall_num == 58 || syscall_num == 98 || 
	  syscall_num == 119 || syscall_num == 123 || syscall_num == 127 ||
	  syscall_num == 186 || syscall_num == 243 || syscall_num == 244)) {
	if (ptdata->ignore_flag) {
	    if (!(*(int *)(ptdata->ignore_flag))) {
		global_syscall_cnt++;
		ptdata->syscall_cnt++;
	    }
	} else {
	    global_syscall_cnt++;
	    ptdata->syscall_cnt++;
	}
    }
}


void inst_syscall_end(THREADID thread_id, CONTEXT* ctxt, SYSCALL_STANDARD std, VOID* v)
{
#ifdef USE_TLS_SCRATCH
    struct thread_data* tdata = (struct thread_data *) PIN_GetContextReg(ctxt, tls_reg);
#else
    struct thread_data* tdata = (struct thread_data *) PIN_GetThreadData(tls_key, PIN_ThreadId());
#endif
    if (tdata) {
	if (tdata->app_syscall != 999) tdata->app_syscall = 0;
    } else {
	fprintf (stderr, "inst_syscall_end: NULL current_thread\n");
    }

    increment_syscall_cnt(tdata, tdata->sysnum);
    // reset the syscall number after returning from system call

    tdata->sysnum = 0;
    increment_syscall_cnt(tdata, tdata->sysnum);
}

// called before every application system call
#ifdef USE_TLS_SCRATCH
#ifdef PLUS_TWO
void set_address_one(ADDRINT syscall_num, ADDRINT ebx_value, ADDRINT tls_ptr, ADDRINT syscallarg0, ADDRINT syscallarg1, ADDRINT syscallarg2, ADDRINT ip)
#else
void set_address_one(ADDRINT syscall_num, ADDRINT ebx_value, ADDRINT tls_ptr, ADDRINT syscallarg0, ADDRINT syscallarg1, ADDRINT syscallarg2)
#endif
#else
void set_address_one(ADDRINT syscall_num, ADDRINT ebx_value, ADDRINT syscallarg0, ADDRINT syscallarg1, ADDRINT syscallarg2)
#endif
{
#ifdef USE_TLS_SCRATCH
    struct thread_data* tdata = (struct thread_data *) tls_ptr;
#else
    struct thread_data* tdata = (struct thread_data *) PIN_GetThreadData(tls_key, PIN_ThreadId());
#endif
    if (tdata) {
	int sysnum = (int) syscall_num;
        if (sysnum == 90)
          sysnum = 192;

	fprintf (stderr, "Record pid %d, %d: syscall num is %d clock %lu\n",
                 tdata->record_pid, tdata->syscall_cnt, (int) syscall_num, *ppthread_log_clock);
	if (sysnum == 31) {
	    tdata->ignore_flag = (u_long) syscallarg1;
	}

	if (sysnum == SYS_write) {
	    fprintf(stderr,"write %d %s\n", syscallarg0, (char *) syscallarg1);
	}

#ifdef PLUS_TWO
	    g_hash_table_add(sysexit_addr_table, GINT_TO_POINTER(ip+2));
	    printf ("Add address %x\n", ip+2);
	    g_hash_table_add(sysexit_addr_table, GINT_TO_POINTER(ip+11));
	    printf ("Add address %x\n", ip+11);
#endif
	if (sysnum == 45 || sysnum == 90 || sysnum == 91 || sysnum == 120 || sysnum == 125 ||
            sysnum == 174 || sysnum == 175 || sysnum == 190 || sysnum == 192) {
	    check_clock_before_syscall (fd, (int) sysnum);
	}
	tdata->app_syscall = sysnum;
	tdata->sysnum = sysnum;
    } else {
	fprintf (stderr, "set_address_one: NULL current_thread\n");
    }
}

#ifdef USE_TLS_SCRATCH
void syscall_after (ADDRINT ip, ADDRINT tls_ptr)
#else
void syscall_after (ADDRINT ip)
#endif
{
#ifdef USE_TLS_SCRATCH
    struct thread_data* tdata = (struct thread_data *) tls_ptr;
#else
    struct thread_data* tdata = (struct thread_data *) PIN_GetThreadData(tls_key, PIN_ThreadId());
#endif
    if (tdata) {
	if (tdata->app_syscall == 999) {
	    if (check_clock_after_syscall (fd) == 0) {
	    } else {
		fprintf (stderr, "Check clock failed\n");
	    }
	    tdata->app_syscall = 0;
	}
    } else {
	fprintf (stderr, "syscall_after: NULL current_thread\n");
    }
    if (tdata->sysnum == 243)
      fprintf(stderr,"sysnum?? %d, ptr %d\n", tdata->sysnum, *tdata->ptr_val);
}

void AfterForkInChild(THREADID threadid, const CONTEXT* ctxt, VOID* arg)
{
#ifdef USE_TLS_SCRATCH
    struct thread_data* tdata = (struct thread_data *) PIN_GetContextReg(ctxt, tls_reg);
#else
    struct thread_data* tdata = (struct thread_data *) PIN_GetThreadData(tls_key, PIN_ThreadId());
#endif
    int record_pid;
    printf ("AfterForkInChild\n");
    record_pid = get_record_pid();
    printf ("get record id %d\n", record_pid);
    tdata->record_pid = record_pid;

    // reset syscall index for thread
    tdata->syscall_cnt = 0;
}

typedef struct token_s {
    char *value;
    size_t length;
} token_t;

void print_instruction(ADDRINT ip, string extras) {
  PIN_LockClient();
  string img = "", func = "";
  ADDRINT saddr = ip;
  if (IMG_Valid(IMG_FindByAddress(ip))) {
    img = IMG_Name(IMG_FindByAddress(ip));
    func = RTN_FindNameByAddress(ip);
    // saddr = find_static_address(ip);
  }
  PIN_UnlockClient();
  fprintf(stderr,"[INST] Pid %d - %#x (%ld) (%s %s %#x) %s\n", get_record_pid(), ip, *ppthread_log_clock, img.c_str(), func.c_str(), saddr, extras.c_str());
}

void instrument_inst_print (ADDRINT ip)
{
  if (*ppthread_log_clock >= (u_long)print_limit && *ppthread_log_clock < (u_long)print_stop) {
    print_instruction(ip, "");
  }
}

void instrument_mem(ADDRINT ip, ADDRINT *addr, bool read)
{
  stringstream s;
  s << std::hex << (read ? "r" : "w")  << addr << " -> " << *addr;

  if ((*ppthread_log_clock >= (u_long)print_limit && *ppthread_log_clock < (u_long)print_stop) ||
      INTERESTING((ADDRINT)addr)) {
    print_instruction(ip, s.str()); //fprintf(stderr, "%d, %x: %p, has value %x\n", PIN_GetTid(), ip, addr, *addr);
  }
}

void track_bbl(TRACE trace, void* data)
{
  //if (print_limit != print_stop) {
  for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {
    for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins)) {
      /*BBL_InsertCall(bbl, IPOINT_BEFORE, (AFUNPTR)instrument_inst_print, IARG_INST_PTR,
                     IARG_REG_VALUE, LEVEL_BASE::REG_EBP,
                     IARG_REG_VALUE, LEVEL_BASE::REG_ESP,
                     IARG_END);
      */
      if(INS_IsMemoryRead(ins)) {
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)instrument_mem, IARG_INST_PTR,
                       IARG_MEMORYREAD_EA,
                       IARG_BOOL, true,
                       IARG_END);
      } else if (INS_IsMemoryWrite(ins)) {
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)instrument_mem, IARG_INST_PTR,
                       IARG_MEMORYWRITE_EA,
                       IARG_BOOL, false,
                       IARG_END);
      } else {
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)instrument_inst_print, IARG_INST_PTR,
                       IARG_END);
      }
    }
  }
}

void track_inst(INS ins, void* data)
{

#ifdef USE_TLS_SCRATCH
    if(INS_IsSyscall(ins)) {
	    INS_InsertCall(ins, IPOINT_BEFORE, AFUNPTR(set_address_one), IARG_SYSCALL_NUMBER,
			   IARG_REG_VALUE, LEVEL_BASE::REG_EBX, 
			   IARG_REG_VALUE, tls_reg,
			   IARG_SYSARG_VALUE, 0, 
			   IARG_SYSARG_VALUE, 1,
			   IARG_SYSARG_VALUE, 2,
#ifdef PLUS_TWO
			   IARG_INST_PTR,
#endif
			   IARG_END);
    }
#else
    if(INS_IsSyscall(ins)) {
	    INS_InsertCall(ins, IPOINT_BEFORE, AFUNPTR(set_address_one), IARG_SYSCALL_NUMBER,
                    IARG_REG_VALUE, LEVEL_BASE::REG_EBX,
		    IARG_SYSARG_VALUE, 0,
		    IARG_SYSARG_VALUE, 1,
		    IARG_SYSARG_VALUE, 2,
		    IARG_END);
    }
#endif
}

void track_trace(TRACE trace, void* data)
{

#ifdef USE_TLS_SCRATCH
    TRACE_InsertCall(trace, IPOINT_BEFORE, (AFUNPTR) syscall_after, IARG_INST_PTR, IARG_REG_VALUE, tls_reg, IARG_END);
#else
    TRACE_InsertCall(trace, IPOINT_BEFORE, (AFUNPTR) syscall_after, IARG_INST_PTR, IARG_END);
#endif
}

BOOL follow_child(CHILD_PROCESS child, void* data)
{
    char** argv;
    char** prev_argv = (char**)data;
    int index = 0;

    printf ("following child...\n");

    /* the format of pin command would be:
     * pin_binary -follow_execv -t pin_tool new_addr*/
    int new_argc = 5;
    argv = (char**)malloc(sizeof(char*) * new_argc);

    argv[0] = prev_argv[index++];
    argv[1] = (char *) "-follow_execv";
    while(strcmp(prev_argv[index], "-t")) index++;
    argv[2] = prev_argv[index++];
    argv[3] = prev_argv[index++];
    argv[4] = (char *) "--";

    CHILD_PROCESS_SetPinCommandLine(child, new_argc, argv);

    printf("returning from follow child\n");
    printf("pin my pid is %d\n", PIN_GetPid());
    printf("%d is application thread\n", PIN_IsApplicationThread());

    return TRUE;
}

int get_record_pid() {
    //calling kernel for this replay thread's record log
    int record_log_id;

    record_log_id = get_log_id (fd, 0);
    if (record_log_id == -1) {
        int pid = PIN_GetPid();
        fprintf(stderr, "Could not get the record pid from kernel, pid is %d\n", pid);
        return pid;
    }
    return record_log_id;
}

void thread_start (THREADID threadid, CONTEXT* ctxt, INT32 flags, VOID* v)
{
    struct thread_data* ptdata;

    ptdata = (struct thread_data *) malloc (sizeof(struct thread_data));
    assert (ptdata);
//    getppid();
//    fprintf (stderr, "Start of threadid %d ptdata %p\n", (int) threadid, ptdata);

    ptdata->app_syscall = 0;
    ptdata->record_pid = get_record_pid();
    //   get_record_group_id(dev_fd, &(ptdata->rg_id));

#ifdef USE_TLS_SCRATCH
    // set the TLS in the virutal register
    PIN_SetContextReg(ctxt, tls_reg, (ADDRINT) ptdata);
#else
    PIN_SetThreadData (tls_key, ptdata, threadid);
#endif

    int thread_ndx;
    long thread_status = set_pin_addr (fd, (u_long) &(ptdata->app_syscall), ptdata, (void **) &current_thread, &thread_ndx);
    /*
     * DON'T PUT SYSCALLS ABOVE THIS POINT!
     */

    if (thread_status < 2) {
	current_thread = ptdata;
    }
    std::cerr << "Thread " << ptdata->record_pid << "gets rc " << thread_status
	      << "ndx " << thread_ndx << "from set_pin_addr\n";
}

void thread_fini (THREADID threadid, const CONTEXT* ctxt, INT32 code, VOID* v)
{
    struct thread_data* ptdata;
    ptdata = (struct thread_data *) malloc (sizeof(struct thread_data));
    printf("Pid %d (recpid %d, tid %d) thread fini\n", PIN_GetPid(), ptdata->record_pid, PIN_GetTid());
}

#ifdef DEBUG_FUNCTIONS
void before_function_call(ADDRINT name, ADDRINT rtn_addr)
{
    if (global_syscall_cnt >= function_print_limit && global_syscall_cnt < function_print_stop) {
        printf("Before call to %s (%#x)\n", (char *) name, rtn_addr);
    }
}

void after_function_call(ADDRINT name, ADDRINT rtn_addr)
{
    if (global_syscall_cnt >= function_print_limit && global_syscall_cnt < function_print_stop) {
        printf("After call to %s (%#x)\n", (char *) name, rtn_addr);
    }
}

void routine (RTN rtn, VOID *v)
{
    const char *name;

    name = RTN_Name(rtn).c_str();

    RTN_Open(rtn);

    if (!strstr(name, "get_pc_thunk")) {
        RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)before_function_call,
                IARG_PTR, name, IARG_ADDRINT, RTN_Address(rtn), IARG_END);
        RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)after_function_call,
			    IARG_PTR, name, IARG_ADDRINT, RTN_Address(rtn), IARG_END);
    }

    RTN_Close(rtn);
}
#endif

/*VOID ImageLoad (IMG img, VOID *v)
{
	uint32_t id = IMG_Id (img);

	ADDRINT load_offset = IMG_LoadOffset(img);
	printf ("[IMG] Loading image id %d, name %s with load offset %#x\n",
		id, IMG_Name(img).c_str(), load_offset);
                }*/

void print_addr (gpointer key, gpointer value, gpointer data)
{
	FILE* fp = (FILE*) data;
	fprintf (fp, "syscall addr: %p\n", key);
}

void fini(INT32 code, void* v) {
    printf ("process is done\n");
#ifdef TIMING_ON
    {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        printf("Pid %d start %ld secs %ld usecs\n", PIN_GetPid(), tv.tv_sec, tv.tv_usec);
    }
#endif
}

int main(int argc, char** argv)
{
  int rc;

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

    // Intialize the replay device
    rc = devspec_init (&fd);
    if (rc < 0) return rc;


    ppthread_log_clock = map_shared_clock(fd);

#ifdef USE_TLS_SCRATCH
    // Claim a Pin virtual register to store the pointer to a thread's TLS
    tls_reg = PIN_ClaimToolRegister();
#else
    // Obtain a key for TLS storage
    tls_key = PIN_CreateThreadDataKey(0);
#endif

    fprintf(stderr, "getting printlimint and such\n");

    print_limit = atoi(KnobPrintLimit.Value().c_str());
    print_stop = atoi(KnobPrintStop.Value().c_str());
#ifdef DEBUG_FUNCTIONS
    function_print_limit = atoi(KnobFunctionPrintLimit.Value().c_str());
    function_print_stop = atoi(KnobFunctionPrintStop.Value().c_str());
#endif

    fprintf(stderr, "pl (%ld, %ld)\n", print_limit, print_stop);

    PIN_AddThreadStartFunction(thread_start, 0);
    PIN_AddThreadFiniFunction(thread_fini, 0);
    PIN_AddFiniFunction(fini, 0);

    //PIN_AddFollowChildProcessFunction(follow_child, argv);
    INS_AddInstrumentFunction(track_inst, 0);
    TRACE_AddInstrumentFunction(track_bbl, 0);

    // Register a notification handler that is called when the application
    // forks a new process
    PIN_AddForkFunction(FPOINT_AFTER_IN_CHILD, AfterForkInChild, 0);

    TRACE_AddInstrumentFunction (track_trace, 0);
#ifdef DEBUG_FUNCTIONS
    RTN_AddInstrumentFunction (routine, 0);
#endif

    PIN_AddSyscallExitFunction(inst_syscall_end, 0);
#ifdef TIMING_ON
    {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        printf ("Pid %d start %ld secs %ld usecs\n", PIN_GetPid(), tv.tv_sec, tv.tv_usec);
    }
#endif

    fprintf(stderr, "starting program\n");
    PIN_StartProgram();

    return 0;
}
