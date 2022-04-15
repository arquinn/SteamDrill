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

#include <iostream>
#include <fstream>

#include <set>
#include <map>
#include <unordered_map>


using namespace std;
#include "util.h"
#include "bw_slice.h"
#include "CallStack.H"
#include "syscall_names.H"


#define PIN_NORMAL         0
#define PIN_ATTACH_RUNNING 1
#define PIN_ATTACH_BLOCKED 2
#define PIN_ATTACH_REDO    4


uint64_t full_instcount = 0;

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



//#define MMAP_INPUTS
//#define EXEC_INPUTS

/* Global state */

TLS_KEY tls_key; // Key for accessing TLS. 
int dev_fd; // File descriptor for the replay device
int get_record_pid(void);
struct thread_data* current_thread; // Always points to thread-local data (changed by kernel on context switch)
int first_thread = 1;
int child = 0;
char** main_prev_argv = 0;

#ifdef VERBOSE
ofstream log_f; 
#endif

unsigned long global_syscall_cnt = 0;
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


//added for multi-process replay
const char* fork_flags = NULL;
u_int fork_flags_index = 0;
bool produce_output = true; 



KNOB<string> KnobOutfile(KNOB_MODE_WRITEONCE,
			  "pintool", "outfile", "",
			  "where to output results");

KNOB<string> KnobAddr(KNOB_MODE_WRITEONCE,
			  "pintool", "addrfile", "",
			  "addresses on which to run callstack");


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

///////////////////////// Prototypes //////////////////////////////////////////

const string& Target2RtnName(ADDRINT target);
const string& Target2LibName(ADDRINT target);

///////////////////////// Global Variables ////////////////////////////////////

ostream *Output;

CallStack callStack(Target2RtnName, Target2LibName);

bool main_entry_seen = false;
bool prevIpDoesPush = FALSE;

set<ADDRINT> addrsToDump;
set<ADDRINT> pushIps;

///////////////////////// Utility functions ///////////////////////////////////
void RecordPush (INS ins)
{
    pushIps.insert(INS_Address (ins));
}

bool IpDoesPush (ADDRINT ip)
{
    return (pushIps.find(ip) != pushIps.end());
}

const string& Target2RtnName(ADDRINT target)
{
  const string & name = RTN_FindNameByAddress(target);

  if (name == "")
      return *new string("[Unknown routine]");
  else
      return *new string(name);
}

const string& Target2LibName(ADDRINT target)
{
    PIN_LockClient();
    
    const RTN rtn = RTN_FindByAddress(target);
    static const string _invalid_rtn("[Unknown image]");

    string name;
    
    if( RTN_Valid(rtn) ) {
        name = IMG_Name(SEC_Img(RTN_Sec(rtn)));
    } else {
        name = _invalid_rtn;
    }

    PIN_UnlockClient();

    return *new string(name);
}

///////////////////////// Analysis Functions //////////////////////////////////
void ProcessInst (ADDRINT ip)
{
    prevIpDoesPush = IpDoesPush (ip);  
}

void A_ProcessDirectCall(ADDRINT ip, ADDRINT target, ADDRINT sp)
{
  //cout << "Direct call: " << Target2String(target) << endl;
  callStack.ProcessCall(sp, target);
}

void A_ProcessIndirectCall(ADDRINT ip, ADDRINT target, ADDRINT sp)
{
  //cout << "Indirect call: " << Target2String(target) << endl;
  callStack.ProcessCall(sp, target);
}

static void 
A_ProcessStub(ADDRINT ip, ADDRINT target, ADDRINT sp) 
{
  //cout << "Instrumenting stub: " << Target2String(target) << endl;
  //cout << "STUB: ";
  //cout << Target2RtnName(target) << endl;
  callStack.ProcessCall(sp, target);
}

static void 
A_ProcessReturn(ADDRINT ip, ADDRINT sp) {
  callStack.ProcessReturn(sp, prevIpDoesPush);
}

static void
A_EnterMainImage(ADDRINT ip, ADDRINT target, ADDRINT sp)
{

  //assert(current_pc == main_entry_addr);
  //cout << "main" << endl;
  main_entry_seen = true;
  callStack.ProcessMainEntry(sp, target);
}

static void 
A_DoMem(bool isStore, ADDRINT pc)
{
  string filename;
  int lineno;
  PIN_LockClient();

  PIN_GetSourceLocation(pc, NULL, &lineno, &filename);
  
  PIN_UnlockClient();
  
  *Output << (isStore ? "store" : "load") 
	  << " pc=" << (void*)pc 
	  << endl;
  if( filename != "") {
      *Output << filename << ":" << lineno;
  } else {
      *Output << "UNKNOWN:0";
  }
  *Output << endl;
  callStack.DumpStack(Output);
  *Output << endl;
}

///////////////////////// Instrumentation functions ///////////////////////////

static BOOL IsPLT(TRACE trace)
{
    RTN rtn = TRACE_Rtn(trace);

    // All .plt thunks have a valid RTN
    if (!RTN_Valid(rtn))
        return FALSE;

    if (".plt" == SEC_Name(RTN_Sec(rtn)))
        return TRUE;
    return FALSE;
}

static void I_Trace(TRACE trace, void *v)
{
    for(BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {
        INS tail = BBL_InsTail(bbl);

        // All memory reads/writes
        for( INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins) ) {
	    ADDRINT pc = INS_Address(ins); 
            if(  addrsToDump.find(pc) != addrsToDump.end() ) 
	    {
                INS_InsertCall(ins, IPOINT_BEFORE,
                               (AFUNPTR)A_DoMem,
                               IARG_BOOL, INS_IsMemoryWrite(ins),
                               IARG_INST_PTR,
                               IARG_END);
            }
        }


        
	if( INS_IsCall(tail) ) {
	    if( INS_IsDirectBranchOrCall(tail) ) {
		ADDRINT target = INS_DirectBranchOrCallTargetAddress(tail);
		INS_InsertPredicatedCall(tail, IPOINT_BEFORE,
					 (AFUNPTR)A_ProcessDirectCall,
					 IARG_INST_PTR,
					 IARG_ADDRINT, target,
					 IARG_REG_VALUE, REG_STACK_PTR,
					 IARG_END);
	    } else if( !IsPLT(trace) ) {
		INS_InsertPredicatedCall(tail, IPOINT_BEFORE,
					 (AFUNPTR)A_ProcessIndirectCall,
					 IARG_INST_PTR,
					 IARG_BRANCH_TARGET_ADDR,
					 IARG_REG_VALUE, REG_STACK_PTR,
					 IARG_END);
	    }
	}
	if( IsPLT(trace) ) {
	    INS_InsertCall(tail, IPOINT_BEFORE, 
			   (AFUNPTR)A_ProcessStub,
			   IARG_INST_PTR,
			   IARG_BRANCH_TARGET_ADDR,
			   IARG_REG_VALUE, REG_STACK_PTR,
			   IARG_END);
	}
	if( INS_IsRet(tail) ) {
	    INS_InsertPredicatedCall(tail, IPOINT_BEFORE,
				     (AFUNPTR)A_ProcessReturn,
				     IARG_INST_PTR,
				     IARG_REG_VALUE, REG_STACK_PTR,
				     IARG_END);
	
	}
    }
}

static void 
I_ImageLoad(IMG img, void *v) 
{
  static bool main_rtn_instrumented = false;

  if( !main_rtn_instrumented ) {
    RTN rtn = RTN_FindByName(img, "main");
    if( rtn == RTN_Invalid() ) {
      rtn = RTN_FindByName(img, "__libc_start_main");      
    }
    // Instrument main
    if( rtn != RTN_Invalid() ) {
      main_rtn_instrumented = true;
      RTN_Open(rtn);
      RTN_InsertCall(rtn, IPOINT_BEFORE,
		     (AFUNPTR)A_EnterMainImage,
		     IARG_INST_PTR,
		     IARG_ADDRINT, RTN_Address(rtn),
		     IARG_REG_VALUE, REG_STACK_PTR,
		     IARG_END);
      RTN_Close(rtn);
    }
  }
}




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
    tdata->app_syscall = syscall_num;
}



static void syscall_after_redo (ADDRINT ip)
{
    if (current_thread->app_syscall == 999) {
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



void instruction_instrumentation(INS ins, void *v)
{
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
}

void trace_instrumentation(TRACE trace, void* v)
{
    struct timeval tv_end, tv_start;

    gettimeofday (&tv_start, NULL);
    TRACE_InsertCall(trace, IPOINT_BEFORE, (AFUNPTR) syscall_after_redo, IARG_INST_PTR, IARG_END);
    //TRACE_InsertCall(trace, IPOINT_BEFORE, (AFUNPTR) syscall_after, IARG_INST_PTR, IARG_END);

    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {
	for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins)) {
	    instruction_instrumentation (ins, NULL);
	}
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
    PIN_SetThreadData (tls_key, ptdata, threadid);



    if (first_thread) {
        first_thread = 0;
        if (!ptdata->syscall_cnt) {
            ptdata->syscall_cnt = 1;
        }
    }
    active_threads[ptdata->record_pid] = ptdata;
}

void thread_fini (THREADID threadid, const CONTEXT* ctxt, INT32 code, VOID* v)
{
    struct thread_data* tdata = (struct thread_data *) PIN_GetThreadData(tls_key, threadid);
    active_threads.erase(tdata->record_pid);
}

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
    }
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

void fini(INT32 code, void* v)
{
    dift_done ();
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

    Output = &cout;
    

    init_logs();



    string s = KnobAddr.Value();
    ifstream infile(s);
    if( !infile ) {
	cerr << "give me an addr file!!!" << endl;
	perror(s.c_str());
	exit(1);
    }
    infile >> hex;
    while( !infile.eof() ) {
      static ADDRINT addr;
      infile >> addr;
      addrsToDump.insert(addr);
      cout << addr << endl;
    }


    // Try to map the log clock for this epoch
    ppthread_log_clock = map_shared_clock(dev_fd);
  

    PIN_AddThreadStartFunction(thread_start, 0);
    PIN_AddThreadFiniFunction(thread_fini, 0);
    PIN_AddFiniFunction(fini, 0);

    main_prev_argv = argv;

    TRACE_AddInstrumentFunction (trace_instrumentation, 0);
    PIN_AddSyscallExitFunction(instrument_syscall_ret, 0);

    IMG_AddInstrumentFunction(I_ImageLoad, 0);
    TRACE_AddInstrumentFunction(I_Trace, 0);

    PIN_StartProgram();

    return 0;

}
