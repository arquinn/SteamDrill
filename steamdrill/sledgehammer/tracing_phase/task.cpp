#define _LARGEFILE64_SOURCE
#ifndef _POSIX_SOURCE
#define _POSIX_SOURCE
#endif 
#include <signal.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ptrace.h>

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <cstring>
#include <cassert>
#include <cstdio>
#include <sstream>
#include <iostream>
#include <fstream>

#include "util.h"
#include "log.hpp"
#include "thread_group.hpp"
#include "task.hpp"
#include "globals.hpp"

using std::string;

namespace ThreadGroup {
void Task::resetFstream(std::ifstream &fs)
{
  if (fs.eof())
    fs.clear();
  fs.seekg(0, std::ios::beg);
  if (!fs.good())
  {
    ERROR("what the fuck?! {} {} {}", fs.eof(), fs.fail(), fs.bad());
  }
  assert(fs.good());
}

void Task::initExe(void) {
  ssize_t size;
  char exe[PATH_MAX], filename[128];

  sprintf(filename, "/proc/%d/exe", _pid);
  size = readlink(filename, exe, PATH_MAX);
  assert (!size);
  // hack: chop down the size to remove replay added timestamps:
  // /replay_cache/800001_886c56
  size = std::min(size, 27);
  exe[size] = '\0';
  _exe = exe;
  TRACE("{}: exec is {}", _pid, _exe);
}

void Task::initMemFd(void) const {
  if (_memFd < 0 ) {
    char filename[PATH_MAX];
    sprintf(filename, "/proc/%d/mem", _pid);
    _memFd = open(filename, O_RDWR | O_LARGEFILE);
    assert(_memFd != 0);
  }
}

void Task::initRegionFd(void) {
  std::stringstream filename;
  filename << "/proc/" << _pid << "/maps";
  _regionFile.open(filename.str(), std::ifstream::in);
  assert(_regionFile.good());
}

void Task::findFirstRegion(const std::string library,
                           const std::string flags,
                           u_long *start,
                           u_long *end) const
{
  string line;
  resetFstream(_regionFile);

  *start = 0;
  if (end) *end = 0;

  // trying to find the region @ the beginning of the program
  if (library == _exe && flags == "x")  return;

  while (!_regionFile.eof())
  {
    std::getline(_regionFile, line);
    if (line.find(library) != std::string::npos &&
        line.find(flags) != std::string::npos)
    {
      *start = (long) strtoul(line.c_str(), NULL, 16);
      if (end) *end = (long) strtoul(line.c_str() + 9, NULL, 16);
      break;
    }
  }
  if (*start == 0)
    *start = (u_long)-1;
}

void Task::findAllRegions(const std::string match,
                          const SkipSet &skips,
                          RegionSet &regions) const
{
  string line;
  resetFstream(_regionFile);
  while (!_regionFile.eof())
  {
    std::getline(_regionFile, line);
    if (line.size() <= 0)
      continue;
    if (line.find(match) != std::string::npos)
    {

      bool toSkip = false;
      std::for_each(skips.begin(), skips.end(),
                    [&toSkip, &line] (std::string elem) {
                      toSkip |= (line.find(elem) != std::string::npos);
                    });
      if (!toSkip)
      {
        // prot lines have format:
        // <start>-<end> <prot> 00000000 08:01 6947699 <name>",
        #define FORMAT "%lx-%lx %s %x %x:%x %d %s"
        u_long start, end;
        int unused;
        char prot[5], filename[128];

        int read = sscanf(line.c_str(), FORMAT,
                          &start, &end, prot,
                          &unused, &unused, &unused, &unused, filename);

        if (read < 8)
        {
          regions.emplace(start, end, prot, "");
        }
        else {
          regions.emplace(start, end, prot, filename);
        }
      }
    }
  }
}

void Task::findAllRegions(const MemRegionMatcher &matcher, RegionSet &regions) const {
  string line;
  resetFstream(_regionFile);
  while (!_regionFile.eof())
  {
    std::getline(_regionFile, line);
    if (line.size() <= 0)
      continue;

    // proc lines have format:
    // <start>-<end> <prot> 00000000 08:01 6947699 <name>",
    u_long start, end;
    int unused;
    char prot[5], filename[128] = "";

    int read = sscanf(line.c_str(), FORMAT,
                      &start, &end, prot,
                      &unused, &unused, &unused, &unused, filename);

    assert (read >= 7);

    MemRegion r(start, end, prot, filename);
    if(matcher.matches(r))
      regions.insert(std::move(r));

    /*
    regions.move(r);
    if (read < 8) {
      regions.emplace(start, end, prot, "");
    }
    else {
      regions.emplace(start, end, prot, filename);
    }
    */
  }
}

void Task::findAllRegions(std::function<bool(MemRegion &&)> foo) const {
  string line;
  resetFstream(_regionFile);
  while (!_regionFile.eof()) {
    std::getline(_regionFile, line);
    if (line.size() <= 0)
      continue;

    // proc lines have format:
    // <start>-<end> <prot> 00000000 08:01 6947699 <name>",
    u_long start, end;
    int unused;
    char prot[5], filename[128] = "";
    int read = sscanf(line.c_str(), FORMAT, &start, &end, prot,
                      &unused, &unused, &unused, &unused, filename);
    assert (read >= 7);
    if (foo(MemRegion(start, end, prot, filename))) {
      break;
    }
  }
}


void Task::initStackBounds(bool threadLeader) {
  RegionSet stack;
  char stackname[64];

  if (threadLeader) {
    sprintf(stackname, "[stack]");
  }
  else {
    sprintf(stackname,"[stack:%d]", _pid);
  }

  findAllRegions(stackname, {}, stack);

  if (stack.size() > 1) {
    for (auto it : stack) {
      INFO("stack {}", it.toString());
    }
  }

  assert (stack.size() == 1);
  _stackLow = stack.begin()->getStart();
  _stackHigh = stack.begin()->getEnd();
  INFO("{} stack is [{:x}-{:x}]", _pid, _stackLow, _stackHigh);
}

void Task::getStackBounds(uintptr_t &stackLow, uintptr_t &stackHigh) const {
  stackLow = _stackLow;
  stackHigh = _stackHigh;
}


void Task::debugAllRegions(void) const {
  std::string line;

  resetFstream(_regionFile);
  while (!_regionFile.eof()) {
    std::getline(_regionFile, line);
    if (line.size() <= 0)
      continue;
    ERROR("region {}", line);
  }
  ERROR("finished");
}

// let's umm... go ahead and see if we can make some
int Task::getdata(const long addr, const int len, shared_ptr<char> *str) const
{
  char *buff = nullptr;
  int rtnVal = getdata(addr, len, &buff);

  str->reset(buff, std::default_delete<char[]>()); // does this work?
  return rtnVal;
}


//assumes that str has extra space for len % sizeof(long)
int Task::getdata(const long addr, const int len, char **str) const
{
  char *dest;
  int i =  0, end = len / sizeof(long);

  if (len > 0x8) {
    return getdataLarge(addr, len, str);
  }

  // allocate the string, making it word-aligned.
  if (len % sizeof(long)) {
    end++;
  }
  *str = new char[end * sizeof(long)];

  DEBUG_ALLOC("new {:x} rtn {}", end * sizeof(long), (void*)*str);

  while(i < end) {
    dest = *str + (i * sizeof(long));
     *(long *)dest = ptrace(PTRACE_PEEKDATA, _pid,
                           addr + i * sizeof(long),
                           NULL);

    if ((long)dest == -1 && errno != 0) {
      ERROR("getdata from pid {}, addr {x}, len {}, errno {}",  _pid, addr, len, errno);
      return -1;
    }
    ++i;
  }
  return end * sizeof(long);
}


int Task::getdataLarge(const unsigned long addr, const int len,  char **str) const {
  off64_t rc, tmpLen;
  // allocate the string! (don't wory about alignment)
  *str = new char[len];

  tmpLen = 0;
  while (tmpLen < len) {
    off64_t offset = addr + tmpLen;
    rc = pread64(_memFd, (*str) + tmpLen, len - tmpLen, offset);
    if (rc < 0)  {
      ERROR("getdataLarge({:x}, {}, {}) errno {}", (unsigned)addr, (void*)*str, len, errno);
      checkGets();
      debugAllRegions();

      THROW("cannot get data.");
      break;
    }
    tmpLen += rc;
  }
  return len;
}

int Task::setdata(const long addr, const int len, const shared_ptr<char> str) const {
  return setdata(addr, len, str.get());
}

int Task::setdata(const long addr, const int len, const char *str) const {
  int rc;
  int i = 0, end = len / sizeof(long);
  int leftover = len % sizeof(long);

  if (len > 0x8) {
    return setdataLarge(addr, len, str);
  }

  while(i < end) {
    long *src = (long*)(str + (i * sizeof(long)));
    rc = ptrace(PTRACE_POKEDATA, _pid, addr + i * sizeof(long), *src);

    if (rc < 0) {
      ERROR("putdata from addr ({:x}), len {}, errno {}", (unsigned) (addr + i *sizeof(long)), len, errno);
      //debugAllRegions();
    }
    ++i;
  }

  // if we have lefotvers, fill in the extra space with '0's.
  // (I don't think this jives with the rest of this program..?)
  if (leftover) {
    long word = 0;
    long *src = (long*)(str + (i * sizeof(long)));

    memcpy(&word, src, leftover);
    rc = ptrace(PTRACE_POKEDATA, _pid, addr + i * sizeof(long), word);
    if (rc < 0) {
      ERROR("putdata from pid {}, addr {:x}, errno {}",
            _pid, addr + i * 4, errno);
    }
  }

  //word-aligned ceiling of size
  return len + sizeof(long) - leftover;
}

int Task::setdataLarge(const unsigned long addr, const int len, const char *str) const {
  int rc, tmpLen;

  tmpLen = 0;
  while (tmpLen < len) {
    off64_t offset = addr + tmpLen;
    rc = pwrite64(_memFd, str + tmpLen, len - tmpLen, offset);

    if (rc < 0) {
      ERROR("memfd {}, spot {}, len {} off {:x}",
            _memFd, (void*)(str + tmpLen), len - tmpLen, (uint64_t)offset);

      ERROR("setdataLarge({:x}, {}, {:x}) file {} errno {} ({})",
            (unsigned)addr, (void*)str, len, _memFd, errno, strerror(errno));
      checkGets();
      debugAllRegions();

      THROW("cannot set data.");
      break;
    }
    tmpLen += rc;
  }
  return 0;
}

void Task::getSiginfo(siginfo_t *input) const {
  int rc = ptrace(PTRACE_GETSIGINFO, _pid, 0, input);
  if (rc) {
    ERROR("can't getsiginfo for {}, errno {}", _pid, errno);
  }
}

void Task::checkGets() const {
  ERROR("checkGets for {}", _pid);
  char unused[0x1000];
  uintptr_t startRegion = 0x80000000;
  bool work = false;
  int rc;
  off64_t page;

  for (page = 0xB7530000; page < 0xb7547000; page += 0x1000) {
    // let's try pread?
    rc =  pread64(_memFd, unused, 0x1000, page);
    assert (rc > 0 || errno == 5);
    if ((rc < 0 && work) || (rc > 0 && !work)) {
      if (rc < 0) // when we transition to NOT getting a value, then we are golden
        INFO("[{:x}-{:x}]", startRegion, page);
      startRegion = page;
      work = (rc > 0);
    }
  }
  if (work) {
    INFO("[{:x}-{:x}]", startRegion, page);
  }
}



void Task::attach(void) {
  int rc, status;

  rc = ptrace(PTRACE_ATTACH, _pid, NULL, NULL);
  if (rc) {
    ERROR("can't attach to {}, errno {}", _pid, errno);
    THROW("ptrace problem");
  }

  // this is kinda a hack. Special odd semantics in globals so that w can break a cycle:
  Globals::Globals::getInstance()->waitFor(&status, _pid);
  assert(rc == _pid);
  rc = ptrace(PTRACE_SETOPTIONS, _pid, NULL,
              PTRACE_O_TRACECLONE | PTRACE_O_TRACEFORK | PTRACE_O_TRACEVFORK |
              PTRACE_O_TRACESYSGOOD);
  if (rc) {
    ERROR("can't setops on {}, error {}", _pid, strerror(errno));
    THROW("setops problem");
  }

  // you do in fact need to redo these (at least memFd):
  close(_memFd);
  _memFd = -1;
  initMemFd();
  initStackBounds(_isTgid);
  initExe();
}

void Task::detach(void) {
  int rc;

  NDEBUG("{} called dettach for {}", getpid(), _pid);
  rc = ptrace(PTRACE_DETACH, _pid, NULL, NULL);
  if (rc)
  {
    ERROR("can't detach to {}, errno {}", _pid, errno);
  }
}


void Task::restartSyscall() {
  struct user_regs_struct regs;

  //assume we're 2 off of the int 0x80 (this worked in the past)
  if (!getregs(&regs)) {
    ERROR("restart can't getregs on {}, errno {}", _pid, errno);
    THROW("restart exception");
    return;
  }

  regs.eip -= 2;
  regs.eax = _syscallNumber;

  NDEBUG("restarting {} syscall {} eip {:x}", _pid, _syscallNumber, (unsigned)regs.eip);

  if (!setregs(&regs)) {
    ERROR("restart can't setregs on {}, errno {}", _pid, errno);
    THROW("restart exception");
  }
}

u_long Task::getSyscall() const {
  char filename[1024];
  u_long syscallNumber = -1;

  sprintf(filename, "/proc/%d/syscall", _pid);
  std::ifstream syscallFile(filename);
  if (syscallFile.good()) {
    syscallFile >> syscallNumber;
  }
  syscallFile.close();
  return syscallNumber;
}



bool Task::getregs(struct user_regs_struct *regs) const
{
  int retval = ptrace(PTRACE_GETREGS, _pid, NULL, regs);
  if (retval)
  {
    ERROR("can't getregs for {}, errno {}", _pid, errno);
    return false;
  }
  return true;;
}

bool Task::setregs(struct user_regs_struct *regs) const
{
  int retval = ptrace(PTRACE_SETREGS, _pid, NULL, regs);
  if (retval)
  {
    ERROR("can't setregs for {}, errno {}", _pid, errno);
    return false;
  }
  return true;
}

bool Task::getfpu(struct user_fpxregs_struct *regs) const {
  int retval = ptrace(PTRACE_GETFPXREGS, _pid, NULL, regs);
  if (retval) {
    ERROR("can't getregs for {}, errno {}", _pid, errno);
    return false;
  }
  return true;;

}


bool Task::setfpu(struct user_fpxregs_struct *regs) const {
  int retval = ptrace(PTRACE_SETFPXREGS, _pid, NULL, regs);
  if (retval) {
    ERROR("can't getregs for {}, errno {}", _pid, errno);
    return false;
  }
  return true;;

}



void Task::cont(__ptrace_request ptraceType, int signalToSend) {
  int rc = ptrace(ptraceType, _pid, NULL, signalToSend);
  if (rc) {
    ERROR("can't continue on {}, errno {}", _pid, errno);
    THROW("continue pid");
  }
}

int Task::step(int signalToSend)
{
  int rc = ptrace(PTRACE_SINGLESTEP, _pid, NULL, signalToSend);
  if (rc)
    ERROR("can't step on {}, errno {}", _pid, errno);
  return rc;
}


/***********************************/
/* For managing memory permissions */
/***********************************/
void Task::changeProt(const u_long start,
                      const u_long length,
                      const u_long prot) const
{
  int rc;
  int dev = Globals::Globals::getInstance()->getDevFd();

  //TRACE("changePageProt({},{:x},{:x},{:x},{})", dev, start, length, prot, getPid());
  rc = mprotect_other(dev, start, length, prot, getPid());
  if (rc)
    ERROR("mprotect other returns {} errno {}", rc, errno);
  assert (!rc);
}


u_long Task::getFaultAddr(void) const
{
  int dev = Globals::Globals::getInstance()->getDevFd();
  return get_fault_address(dev, getPid());
}


} /*Namespace ThreadGroup*/

/*
void ThreadGroup::ThreadGroup::beforeTracer(pid_t pid)
{
    if (!getregs(&_regs, pid))
    {
	ERROR("beforeTracer " << pid <<
	      " errno " << errno << std::endl);
	THROW("beforeTracer");
    }
}

void ThreadGroup::ThreadGroup::afterTracer(pid_t pid)
{
    if (!setregs(&_regs, pid))
    {
	ERROR("afterTracer " << pid
	      << " errno " << errno << std::endl);
	THROW("afterTracer");
    }
}

long findRemoteRegion(int length, pid_t pid)
{
    return findRegion("",pid);
}


int findAllLibraries(const char *library,
		     std::unordered_map<u_long, u_long> &lib,
		     pid_t pid, const char *skip)
{
    char filename[1024], buffer[1024];
    FILE *fp = NULL;
    u_long address = 0;
    u_long endAddr = 0;

    u_long skipLen = strlen(skip);
    sprintf( filename, "/proc/%d/maps", pid);
    fp = fopen( filename, "rt" );
    if(fp == NULL)
    {
	perror("fopen");
	return 0;
    }
    while(fgets(buffer, sizeof(buffer), fp))
    {
	if(strstr(buffer, library) && 
	   (!skipLen || !strstr(buffer, skip)))
	{
	    address = (long) strtoul(buffer, NULL, 16);
	    endAddr = (long) strtoul(buffer + 9, NULL, 16);

	    lib[address] =  endAddr;

	    //std::cout << std::hex << endAddr - address << " " << buffer;
	}
    }

    fclose(fp);
    return address;
}

void findLibraryName(u_long startAddr, char *libName, pid_t pid)
{
    char filename[1024], buffer[1024]; 
    FILE *fp = NULL;
    char search[32];

    sprintf(search, "%lx-",startAddr);
    sprintf( filename, "/proc/%d/maps", pid);

    fp = fopen( filename, "rt" );
    if(fp == NULL)
    {
	perror("fopen");
	return;
    }

    while(fgets(buffer, sizeof(buffer), fp))
    {
	if(strstr(buffer, search))
	{
	    sprintf(libName, "%s",buffer);
	    break;
	}
    }

    fclose(fp);    
}

u_long findPageProtection(u_long startAddr, pid_t pid)
{
    char filename[1024], buffer[1024]; 
    FILE *fp = NULL;
    u_long sAddr = 0 , eAddr = 0, prot = 0;
    sprintf( filename, "/proc/%d/maps", pid);
    fp = fopen( filename, "rt" );
    if(fp == NULL)
    {
	perror("fopen");	
	return 0; 
    }

    while(fgets(buffer, sizeof(buffer), fp))
    {
	sAddr = (long) strtoul(buffer, NULL, 16);
	eAddr = (long) strtoul(buffer + 9, NULL, 16);

	if (sAddr <= startAddr && eAddr > startAddr)
	{
	    char *protStr = strchr(buffer, ' '); 

	    if ((strstr(protStr, "r") - protStr) < 4)
	    {
		prot |= PROT_READ;
	    }
	    if ((strstr(protStr, "w") - protStr) < 4)
	    {
		prot |= PROT_WRITE;
	    }
	    if ((strstr(protStr, "x") - protStr) < 4)
	    {
		prot |= PROT_EXEC;
	    }
	    break;
	}
    }

    fclose(fp);
    return prot;

}

u_long findRegion(const char *library, pid_t pid)
{
    char filename[1024], buffer[1024];
    FILE *fp = NULL;
    u_long address = 0;

    sprintf( filename, "/proc/%d/maps", pid);
    fp = fopen( filename, "rt" );
    if(fp == NULL)
    {
	perror("fopen");
	return 0; 
    }
    while(fgets(buffer, sizeof(buffer), fp))
    {
	if(strstr(buffer, library))
	{
	    address = (long) strtoul(buffer, NULL, 16);
	    break;
	}
    }
    fclose(fp);
    return address;
}

u_long findRegionEnd(const char *library, pid_t pid) 
{
    char filename[1024], buffer[1024]; 
    FILE *fp = NULL;
    u_long address = 0;

    sprintf( filename, "/proc/%d/maps", pid);
    fp = fopen( filename, "rt" );
    if(fp == NULL)
    {
	perror("fopen");	
	return 0; 
    }
    while(fgets(buffer, sizeof(buffer), fp))
    {
	if(strstr(buffer, library))
	{
	    address = (long) strtoul(buffer + 9, NULL, 16);
	    break;
	}
    }
    fclose(fp);
    return address;
}


u_long findDataRegion(const char *library, pid_t pid) 
{
    char filename[1024], buffer[1024]; 
    FILE *fp = NULL;
    u_long address = 0;

    sprintf( filename, "/proc/%d/maps", pid);
    fp = fopen( filename, "rt" );
    if(fp == NULL)
    {
	perror("fopen");	
	return 0; 
    }
    while(fgets(buffer, sizeof(buffer), fp))
    {
	if(strstr(buffer, "rw") && strstr(buffer, library))
	{
	    address = (long) strtoul(buffer, NULL, 16);
	    break;
	}
    }
    fclose(fp);
    return address;
}

u_long findDataRegionEnd(const char *library, pid_t pid)
{
    char filename[1024], buffer[1024];
    FILE *fp = NULL;
    u_long address = 0;

    sprintf( filename, "/proc/%d/maps", pid);
    fp = fopen( filename, "rt" );
    if(fp == NULL)
    {
	perror("fopen");
	return 0;
    }
    while(fgets(buffer, sizeof(buffer), fp))
    {
	if(strstr(buffer, "rw") && strstr(buffer, library))
	{
	    address = (long) strtoul(buffer + 9, NULL, 16);
	    break;
	}
    }
    fclose(fp);
    return address;
}

extern void* int3AsmStart asm("_int3AsmStart");
extern void* int3AsmEnd asm("_int3AsmEnd");

void callint3Asm()
{
    asm(
	".global _int3AsmStart\n"
	".global _int3AsmEnd\n"
	"_int3AsmStart: int $3\n"
	"_int3AsmEnd: ret"
	);
}

//shoudln't need that external gadget thing ^^
long setBreakpoint(long trap, char *savedRegion, pid_t pid)
{
    char gadget[8];
    long gadgetLen;

    gadgetLen = (long)(&int3AsmEnd) - (long)(&int3AsmStart);

    // put data into the gadget
    memcpy(gadget, &int3AsmStart, gadgetLen);
    getdata(trap, savedRegion, gadgetLen, pid);
    putdata(trap, gadget, gadgetLen, pid);

    return gadgetLen;
}


long resetBreakpoint(long trap, pid_t pid)
{
    long gadgetLen;
    gadgetLen = (long)(&int3AsmEnd) - (long)(&int3AsmStart);
    putdata(trap,(char *)&int3AsmStart, gadgetLen, pid);
    return gadgetLen;
}


void fixBreakpoint(long trap, char *savedRegion, long savedRegionLen)
{
    // breakpoints are generally so small that this seems to be faster
    putdata(trap, savedRegion, savedRegionLen, pid);
}

void setInstructionPointer(long trap, pid_t pid)
{
    TracerGlobals::savedRegs.eip = trap;
    if (!setregs(&TracerGlobals::savedRegs, pid))
    {
	ERROR("failed setInstructionPointer" << std::endl);
    }
}





int ThreadGroup::ThreadGroup::attachOne(pid_t pid)
{
    int rc, status;

    rc = ptrace(PTRACE_ATTACH, pid, NULL, NULL);
    if (rc)
    {
	ERROR("attach cannot ptrace errno " << errno << std::endl);
	THROW("ptrace problem");
    }

    waitForEvent(&status);
    rc = ptrace(PTRACE_SETOPTIONS, pid, NULL,
		PTRACE_O_TRACECLONE | PTRACE_O_TRACEFORK |
		PTRACE_O_TRACEVFORK | PTRACE_O_TRACESYSGOOD);

    if (rc)
    {
	ERROR("attach cannot setopts errno " << errno << std::endl);
	THROW("setops problem");
    }
}


*/
