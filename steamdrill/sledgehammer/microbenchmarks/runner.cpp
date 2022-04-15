// Shell program for testing some ptrace overheads

#include <sys/ptrace.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/wait.h>


#include <assert.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <chrono>
#include <fstream>
#include <iostream>
#include <iomanip>

#include "args/args.hxx"
#include "util.h"

using std::string;
using std::ifstream;

// stuff for statistics
typedef std::chrono::high_resolution_clock myClock;
typedef std::chrono::nanoseconds duration;
typedef std::chrono::time_point<myClock> timePoint;
#define msDurationCast std::chrono::duration_cast<std::chrono::milliseconds>

asm(
".global int3start\n"
".global int3end\n"
"int3start:\n"
"int $3\n"
"int3end:\n"
".skip 8\n");

extern char int3start;// asm("int3start");
extern char int3end; //asm("int3end");

struct breakpoint
{
  char *origCode;
  char *bpCode;
  int size;
};


bool isStep = false;
bool isSkipStep = false;
bool isEnable = false;
bool isKernel = false;

uintptr_t lowPC, highPC;
struct breakpoint stepBP;
struct breakpoint ssBP;

int devFd;

std::ostream& operator<<(std::ostream &out, duration dur)
{
  return out << msDurationCast(dur).count() << " ms";
}

string parseStatus(int status)
{
  std::stringstream ss;
  //ss << status << " ";

  if (WIFEXITED(status))
    ss << "exited " << WEXITSTATUS(status);
  if (WIFSIGNALED(status))
    ss << "signaled " << strsignal(WTERMSIG(status));
  if (WIFSTOPPED(status)) {
    ss << "stopped ";

    if (WSTOPSIG(status) & 0x80)
      ss << strsignal(WSTOPSIG(status) & ~0x80) << " syscall";
    else
      ss << strsignal(WSTOPSIG(status));
  }
  return ss.str();
}

string parseLocal(const char *gadget, long gadgetLen)
{
  std::stringstream ss;
  for (int i = 0; i < gadgetLen; ++i)
  {
    ss << std::hex <<  std::setfill('0') << std::setw(2)
       << (u_int)(u_char)gadget[i] << "(" << gadget[i] << ") ";
  }
  return ss.str();
}




int setMem(const long addr, const int len, const char *str, pid_t pid)
{
  int rc;
  int i = 0, end = len / sizeof(uintptr_t);
  assert (!(len % sizeof(uintptr_t)));

  while(i < end)
  {
    long *src = (long*)(str + (i * sizeof(long)));
    rc = ptrace(PTRACE_POKEDATA, pid, addr + i * sizeof(long), *src);

    if (rc < 0)
    {
      std::cerr << "setMem failed " << strerror(errno) << std::endl;
    }
    ++i;
  }
}

int getMem(const long addr, const int len, char *str, pid_t pid)
{
  char *dest;
  int i =  0, end = len / sizeof(uintptr_t);
  while(i < end) {
    dest = str + (i * sizeof(long));
     *(long *)dest = ptrace(PTRACE_PEEKDATA, pid,
                           addr + i * sizeof(long),
                           NULL);

    if ((long)dest == -1 && errno != 0)
    {
      std::cerr << "getdata  failed " << strerror(errno) << std::endl;
      return -1;
    }
    ++i;
  }
  return end * sizeof(long);
}

bool parseConfig(ifstream &config)
{
  config >> std::hex;
  config >> lowPC;
  config >> highPC;

  std::cout << std::hex
            << "steping " << lowPC << " to " << highPC << std::endl;
  std::cout << std::dec;
  return true;
}

void setupChild(pid_t pid)
{
  stepBP.size = &int3end - &int3start;

  if (stepBP.size % sizeof(uintptr_t))
    stepBP.size += (sizeof(uintptr_t) -
                    (stepBP.size % sizeof(uintptr_t)));

  stepBP.origCode = new char[stepBP.size];
  stepBP.bpCode = new char[stepBP.size];

  getMem(lowPC, stepBP.size, stepBP.origCode, pid);
  memcpy(stepBP.bpCode, (void*)&int3start, &int3end - &int3start);
  memcpy(stepBP.bpCode + (&int3end - &int3start),
         stepBP.origCode + (&int3end - &int3start),
         stepBP.size - (&int3end - &int3start));

  setMem(lowPC, stepBP.size, stepBP.bpCode, pid);

  if (isSkipStep)
  {
    ssBP.size = stepBP.size;
    ssBP.origCode = new char[ssBP.size];
    ssBP.bpCode = new char[ssBP.size];

    getMem(highPC, ssBP.size, ssBP.origCode, pid);
    memcpy(ssBP.bpCode, (void*)&int3start, &int3end - &int3start);
    memcpy(ssBP.bpCode + (&int3end - &int3start),
           ssBP.origCode + (&int3end - &int3start),
           ssBP.size - (&int3end - &int3start));

    setMem(highPC, ssBP.size, ssBP.bpCode, pid);
  }
}

void teardownChild(void)
{
  delete [] stepBP.origCode;
  delete [] stepBP.bpCode;
  if (isSkipStep)
  {
    delete [] ssBP.origCode;
    delete [] ssBP.bpCode;
  }
}

bool getRegs(pid_t pid, struct user_regs_struct &regs)
{
  int retval = ptrace(PTRACE_GETREGS, pid, NULL, &regs);
  if (retval)
  {
    std::cerr << "getregs fail " << strerror(errno) << std::endl;
    return false;
  }
  return true;
}

void setRegs(pid_t pid, struct user_regs_struct &regs)
{
  int retval = ptrace(PTRACE_SETREGS, pid, NULL, &regs);
  if (retval)
  {
    std::cerr << "setregs fail " << strerror(errno) << std::endl;
    assert (false);
  }
}

int doPtrace()
{
  pid_t wpid, child;
  int status, signalRecd, step = 0;
  uintptr_t eip;
  bool set = false;
  struct user_regs_struct regs;

  child = fork();
  if(child == 0) {
    ptrace(PTRACE_TRACEME, 0, 0, 0);
    execl("test", "test", NULL);
    std::cerr << "cannot execl? " << strerror(errno) << std::endl;
    _exit(0);
  }
  do
  {
    wpid = waitpid(-1, &status, __WALL);
    if (WIFEXITED(status))
        break;
    getRegs(wpid, regs);

    if (!set)
    {
      setupChild(child);
      set = true;
    }
    else if ((regs.eip - 1) == lowPC)
    {
      // patch orig code

      setMem(lowPC, stepBP.size, stepBP.origCode, wpid);
      regs.eip -= 1;
      setRegs(wpid, regs);
      std::cerr << std::hex << "start @ " << regs.eip << std::endl;
      timePoint start = myClock::now();

      if (isStep)
      {
        while (regs.eip != highPC)
        {
          ptrace(PTRACE_SINGLESTEP, wpid, NULL, NULL);
          wpid = waitpid(-1, &status, __WALL);
          step++;
          if (!getRegs(wpid, regs))
            break;
        }
      }
      else if (isSkipStep)
      {
        ptrace(PTRACE_CONT, wpid, NULL, NULL);
        wpid = waitpid(-1, &status, __WALL);
        getRegs(wpid, regs);
        assert (regs.eip == highPC + 1);
      }

      else if (isEnable)
      {
        regs.eflags = regs.eflags | (1 << 8);
        setRegs(wpid, regs);
        //enableTF(regs, wpid);
        while (regs.eip != highPC)
        {
          ptrace(PTRACE_CONT, wpid, NULL, NULL);
          wpid = waitpid(-1, &status, __WALL);
          assert (wpid == child);
          step++;
          if (!getRegs(wpid, regs))
            break;
        }
      }

      timePoint stop = myClock::now();
      if (isStep)
        std::cout << "step ";
      else if (isSkipStep)
        std::cout << "skip ";
      else if (isEnable)
        std::cout << "enable ";
      else if (isKernel)
        std::cout << "kernel ";
      std::cout << "(steps, " << step << "); (time, "
                << stop - start <<")"  << std::endl;

      if (isEnable || isKernel)
      {
        regs.eflags = regs.eflags & ~(1 << 8);
        setRegs(wpid, regs);
      }
      if (isSkipStep)
      {
        setMem(highPC, ssBP.size, ssBP.origCode, wpid);
        regs.eip -= 1;
        setRegs(wpid, regs);
      }
    }

    signalRecd = WSTOPSIG(status) == SIGTRAP ? 0 : WSTOPSIG(status);
    std::cerr << std::hex << "wait @ " << regs.eip
              << " " << parseStatus(status) << std::endl;
    ptrace(PTRACE_CONT, wpid, NULL, signalRecd);

  } while (wpid > 0);

  teardownChild();
}

int main (int argc, char* argv[])
{
  char llevel = 'i';
  args::ArgumentParser parser("run microbenchmarks", "A.R.Q.");
  args::Flag
      verbose(parser, "verbose", "verbose output.", {'v', "verbose"});
  args::HelpFlag
      help(parser, "help", "Display help menu", {'h', "help"});

  args::Positional<string>
      configFile(parser, "config", "the configFile");

  try
  {
    parser.ParseCLI(argc, argv);
  }
  catch (args::Help)
  {
    std::cout << parser;
    return 0;
  }
  catch (args::ParseError e)
  {
    std::cerr << e.what() << std::endl;
    std::cerr << parser;
    return -1;
  }
  catch (args::ValidationError e)
  {
    std::cerr << e.what() << std::endl;
    std::cerr << parser;
    return -2;
  }

  ifstream config(args::get(configFile));
  if (config.bad())
  {
    std::cerr << "cannot open "<< configFile << std::endl;
    return -2;
  };

  parseConfig(config);

  int rc = devspec_init (&devFd);
  assert (!rc);

  /*
  isSkipStep = true;
  for (int i = 0; i < 1; ++i)
  {
    doPtrace ();
  }
  isSkipStep = false;
  */

  isKernel = true;
  for (int i = 0; i < 10; ++i)
  {
    doPtrace ();
  }

  /*  isEnable = true;
  for (int i = 0; i < 1; ++i)
  {
    doPtrace ();
    }*/


  std::cerr << "done\n";
  return 0;
}
