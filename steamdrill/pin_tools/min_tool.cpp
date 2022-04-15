#include "pin.H"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syscall.h>
#include <unistd.h>


#include <sys/types.h>

#include "util.h"
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/uio.h>

#include <glib-2.0/glib.h>

#include <unordered_set>
// #include <iostream>

#define PIN_NORMAL         0
#define PIN_ATTACH_RUNNING 1
#define PIN_ATTACH_BLOCKED 2
#define PIN_ATTACH_REDO    4

//#define TIMING_ON

struct thread_data* current_thread; // Always points to thread-local data (changed by kernel on context switch)
TLS_KEY tls_key; // needed for thread restart to work correctly. 
u_long* ppthread_log_clock = NULL;
u_int syscalls_to_redo = 0;

KNOB<unsigned int> KNOBend(KNOB_MODE_WRITEONCE, "pintool", "s", "", "end clock");
std::unordered_set<pid_t> aftered;

struct thread_data {
  u_long app_syscall; // Per thread address for specifying pin vs. non-pin system calls
  pid_t record_pid; 	// per thread record pid
  int syscall_cnt;	// per thread count of syscalls
  int sysnum;		// current syscall number
  u_long ignore_flag;
};

ADDRINT array[10000];
int child = 0;
u_long segment_length = 0;

int fd; // File descriptor for the replay device

int get_record_pid(void);

// called before every application system call
void syscall_before(ADDRINT syscall_num, ADDRINT ebx_value, ADDRINT syscallarg0, ADDRINT syscallarg1, ADDRINT syscallarg2)
{

  // holy shit, this is a rough bug to trace down:
  // Because of PIN restart issues, this function MUST use PIN thread-specific data
  struct thread_data *tdata =
      (struct thread_data *) PIN_GetThreadData(tls_key, PIN_ThreadId());

  tdata->sysnum = syscall_num;

  if (syscall_num == 45 || syscall_num == 91 || syscall_num == 120 ||
      syscall_num == 125 || syscall_num == 174 || syscall_num == 175 ||
      syscall_num == 190 || syscall_num == 192)
    check_clock_before_syscall(fd, (int) syscall_num);

  tdata->app_syscall = syscall_num;

  if (segment_length && *ppthread_log_clock >= segment_length) {
    try_to_exit(fd, PIN_GetPid());
    PIN_ExitApplication(0);
  }
}

void inst_syscall_end(THREADID thread_id, CONTEXT* ctxt, SYSCALL_STANDARD std, VOID* v)
{
  // why can't I just not do that syscall_after thing?
  if (current_thread) {
    if (current_thread->app_syscall != 999)
      current_thread->app_syscall = 0;

    if (current_thread->sysnum == SYS_gettid)
      PIN_SetContextReg(ctxt, LEVEL_BASE::REG_EAX, current_thread->record_pid);


  } else {
    fprintf (stderr, "inst_syscall_end: NULL current_thread\n");
  }

  // reset the syscall number after returning from system call
  current_thread->sysnum = 0;
}

void syscall_after (void)
{
  if (syscalls_to_redo) {
    long retval;
    u_long rc, len;
    int syscall = check_for_redo(fd);

    if (syscall == 192) {
      syscalls_to_redo--;
      //fprintf (stderr, "Instruction %x redo mmap please %d\n", ip, redo_syscall);
      retval = redo_mmap (fd, &rc, &len);
      if (retval) fprintf (stderr, "redo_mmap failed, rc=%ld\n", retval);
      current_thread->app_syscall = 0;
    }
    else if(syscall == 91) {
      syscalls_to_redo--;
      retval = redo_munmap (fd);
      // fprintf(stderr, "running the redo_munmap!\n");
      if (retval) fprintf (stderr, "redo_mmap failed, rc=%ld\n", retval);
      current_thread->app_syscall = 0;
    }
    else {
      syscalls_to_redo--;
      // fprintf(stderr, "%d redoing systemcall %d\n", get_record_pid(), syscall);
      retval = redo_syscall(fd);
      if (retval) fprintf(stderr, "syscall failed, rc=%ld, errmsg %sn",
                          retval, strerror(errno));
    }
  } else if (current_thread->app_syscall == 999) {
    check_clock_after_syscall (fd);
    current_thread->app_syscall = 0;
  }
}

void AfterForkInChild(THREADID threadid, const CONTEXT* ctxt, VOID* arg)
{
  printf ("AfterForkInChild\n");
  current_thread->record_pid = get_record_pid();

  // reset syscall index for thread
  current_thread->syscall_cnt = 0;
}


void track_trace(TRACE trace, void* data)
{
  TRACE_InsertCall(trace,
                   IPOINT_BEFORE, (AFUNPTR) syscall_after, IARG_END);

  for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl))
  {
    for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins))
    {
      if(INS_IsSyscall(ins)) {
        INS_InsertCall(ins, IPOINT_BEFORE, AFUNPTR(syscall_before), IARG_SYSCALL_NUMBER,
                       IARG_REG_VALUE, LEVEL_BASE::REG_EBX,
                       IARG_SYSARG_VALUE, 0,
                       IARG_SYSARG_VALUE, 1,
                       IARG_SYSARG_VALUE, 2,
                       IARG_END);
      }
    }
  }
}

int get_record_pid()
{
  //calling kernel for this replay thread's record log
  int record_log_id;

  record_log_id = get_log_id (fd);
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
  ptdata->app_syscall = 0;
  ptdata->record_pid = get_record_pid();

  int thread_ndx;
  long thread_status = set_pin_addr (fd,
                                     (u_long) &(ptdata->app_syscall),
                                     ptdata,
                                     (void **) &current_thread,
                                     &thread_ndx);

  /*
   * DON'T PUT SYSCALLS ABOVE THIS POINT!
   */
  if (thread_status & PIN_ATTACH_REDO) {
    syscalls_to_redo++;
    // fprintf (stderr, "%d Need to redo system call!\n", get_record_pid());
  }
  if (!(thread_status & PIN_ATTACH_BLOCKED)) {
    current_thread = ptdata;
  }

  // needed because of PIN restart issues... (current_thread is incorrect when attaching)
  PIN_SetThreadData(tls_key, ptdata, threadid);
}


void fini(INT32 code, void* v) {}

int main(int argc, char** argv)
{
  int rc;

  // fprintf(stderr, "started pin\n");
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

  segment_length = KNOBend.Value();

  // Inialize the replay device
  rc = devspec_init (&fd);
  if (rc < 0) return rc;

  ppthread_log_clock = map_shared_clock(fd);
  tls_key = PIN_CreateThreadDataKey(0);


  PIN_AddThreadStartFunction(thread_start, 0);
  // Register a notification handler that is called when the application
  // forks a new process
  PIN_AddForkFunction(FPOINT_AFTER_IN_CHILD, AfterForkInChild, 0);
  PIN_AddFiniFunction(fini, 0);

  TRACE_AddInstrumentFunction(track_trace, 0);
  PIN_AddSyscallExitFunction(inst_syscall_end, 0);

  PIN_StartProgram();
  // never returns

  return 0;
}
