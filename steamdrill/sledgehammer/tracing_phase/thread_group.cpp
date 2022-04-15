//#define _LARGEFILE64_SOURCE
#include <algorithm>
#include <iostream>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <sstream>
#include <memory>

#include <sys/types.h>
#include <sys/ptrace.h>
#include <sys/reg.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <assert.h>
#include <fcntl.h>
#include <dirent.h>
#include <byteswap.h>

#include "inst_point.hpp"
#include "util.h"
#include "configuration.hpp"
#include "globals.hpp"
#include "log.hpp"
#include "mem_region.hpp"

#include "task.hpp"
#include "thread_group.hpp"

// bad bad bad bad man
#include "../../utils/utils.hpp"

//should put these into the globals datastructure!
using std::copy;
using std::function;
using std::make_unique;
using std::string;
using std::shared_ptr;
using std::unique_ptr;
using std::unordered_set;
using std::vector;

using Configuration::TracerConfiguration;
using Tracer::Gadget;
using Tracer::AddressSet;

// this needs to be relative to the end location of the binary (which happens to be this folder)
#define LIBDL_PATH "../../../eglibc-2.15/prefix/lib/libdl.so.2"
//#define INT3_TEST_MODE

namespace ThreadGroup {
#ifdef PARANOID
static RegionSet newRegions;
static int mmaps = 0;
#endif
bool debugTracer = false;

std::unique_ptr<Syscall> Syscall::getSyscall(regs_struct &regs) {
  unique_ptr<Syscall> sp(new Syscall(regs.orig_eax));

  switch (sp->syscallNum) {
    case NR_MMAP2:
      sp->mmap.len = regs.ecx;
      sp->mmap.prot = regs.edx;
      break;

    case NR_MPROTECT:
      sp->mprotect.addr = regs.ebx;
      sp->mprotect.len = regs.ecx;
      sp->mprotect.prot = regs.edx;
      break;

    case NR_BRK:
      sp->brk.addr = (uintptr_t)regs.ebx;
  }

  return sp;
}

void Syscall::updateSyscall(regs_struct &regs) {
  if (syscallNum != regs.orig_eax) {
    NDEBUG("yikes! difference in opinion {} eax {} {}",
           syscallNum, regs.orig_eax, ENOSYS);
    THROW("syscall issues.");
  }

  switch (syscallNum) {
    case NR_MMAP2:
      mmap.addr = (uintptr_t)regs.eax;
      break;
    case NR_BRK:
      // the brk systemcall returns the new break (or old on failure)
      brk.addr = (uintptr_t)regs.eax;
      break;
  }
}

ThreadGroup::ThreadGroup(pid_t pid, pid_t tgid) {
  auto g = Globals::Globals::getInstance();
  DIR *dir;
  struct dirent *ent;
  char dirname[256];

  _sharedObject = make_unique<TracerObject>();

  _nextRegion = _endRegionBlock = 0;
  _regionsAllocator = NULL;
  _threadStartTracer = nullptr;
  _forkExitTracer= nullptr;
  _stackLow = 0;
  _stackHigh = 0;

  // first attach to the master boiii:
  NDEBUG("attaching to {} tgid {}", pid, tgid);
  _currTask = new Task(pid, tgid == pid);
  _currTask->attach();
  _tasks.emplace(pid, _currTask);

  sprintf(dirname, "/proc/%d/task", pid);
  dir = opendir( dirname);
  if (dir == NULL) {
    ERROR("can't open {}", dirname);
    THROW("cannot find thread in proc");
  }

  while ((ent = readdir(dir)) != NULL) {
    if (atoi(ent->d_name)) {
      pid_t cPid = atoi(ent->d_name);
      if (cPid != pid) {
        auto t = new Task(cPid, tgid == cPid);
        t->attach();
        _tasks.emplace(cPid, t);
        INFO("found pid {}, accumulated {} tasks!", cPid, _tasks.size());
      }
    }
  }

  NDEBUG("finished with attach!");

  NDEBUG("prepare to trace")
  STEP();

  struct user_regs_struct regs;
  _currTask->getregs(&regs);
  NDEBUG("prep regs: {}", Log::parseRegs(regs));

  g->startLoadTime();
  mapPoints();
  g->stopLoadTime();
  _currTask->setregs(&regs);
  _currTask = NULL;
}

ThreadGroup::ThreadGroup(pid_t pid, pid_t tgid, const ThreadGroup *parent) {
  DIR *dir;
  struct dirent *ent;
  char dirname[256];

  sprintf(dirname, "/proc/%d/task", pid);
  dir = opendir(dirname);
  if (dir == NULL) {
    ERROR("can't open {}", dirname);
    THROW("cannot find thread in proc");
  }

  while ((ent = readdir(dir)) != NULL) {
    if (atoi(ent->d_name)) {
      pid_t cPid = atoi(ent->d_name);
      auto t = new Task(cPid, tgid == cPid);
      // you don't need to attach here, you have a parent!
      // t->attach();
      _tasks.emplace(cPid, t);
      INFO("{} added {}, {}", (void*)this, cPid, t->getPid());
    }
  }

  mp = parent->mp;
  _tracerIndex = parent->_tracerIndex;
  _ctracers = parent->_ctracers;
  _strings = parent->_strings;
  _instrPoints = parent->_instrPoints;

  _sharedObject = make_unique<TracerObject>(parent->_sharedObject.get());
  _exitTracer = parent->_exitTracer;
  _forkExitTracer = parent->_forkExitTracer;
  _threadStartTracer = parent->_threadStartTracer;

  _segfaultGadget = parent->_segfaultGadget;

  _regionsAllocator = parent->_regionsAllocator;

  _stackLow = parent->_stackLow;
  _stackHigh = parent->_stackHigh;
  _nextRegion = parent->_nextRegion;
  _endRegionBlock = parent->_endRegionBlock;

  INFO("{} has stack of [{:x} - {:x}]", (void*)this, _stackLow, _stackHigh);
}

void ThreadGroup::addThread(pid_t pid) {
  _tasks.insert(std::make_pair(pid, new Task(pid, false)));
}



void ThreadGroup::detach() {
  for (auto t: _tasks) {
    t.second->detach();
  }
  NDEBUG("finished detaching");
}

void ThreadGroup::restart(pid_t pid) {
  Task *mainTask = nullptr;
  unordered_set<pid_t> otherPids;
  int status;
  struct user_regs_struct regs;
  auto g = Globals::Globals::getInstance();

  INFO("restarting {} tasks at time {}", _tasks.size(), g->getPthreadLogClock());
  for (auto t: _tasks) {
    if (t.second->getPid() == pid) {
      mainTask = t.second;
    }
    else {
      // initialize the task, then restart it's sytemcall
      t.second->getregs(&regs);
      initTaskStructure(t.second);
      t.second->setregs(&regs);
      t.second->restartSyscall();
      t.second->cont(PTRACE_SYSCALL);
      g->waitFor(&status, t.second->getPid());
    }
  }
  assert (mainTask);

  mainTask->getregs(&regs);
  initTaskStructure(mainTask);
  mainTask->setregs(&regs);
  mainTask->restartSyscall();
  mainTask->cont(PTRACE_SYSCALL);
  g->waitFor(&status, pid);
  INFO("restart completed, clock {}", g->getPthreadLogClock());
}

void ThreadGroup::cont(pid_t pid, int signalRecd) {
  if (pid)
    _tasks[pid]->cont(PTRACE_SYSCALL, signalRecd);

  else
    for (auto t : _tasks) {
      t.second->cont(PTRACE_SYSCALL);
    }
}

void ThreadGroup::step(pid_t pid, int signalRecd) {
  assert (pid != 0);
  _tasks[pid]->step(signalRecd);
}


std::vector<pid_t> ThreadGroup::getPids() {
  std::vector<pid_t> rtn;
  for (auto t : _tasks)
    rtn.push_back(t.first);

  return rtn;
}

//TODO: add caching to this function
uintptr_t ThreadGroup::resolveAddress(Configuration::Address *addr) {
  assert (_currTask);
  u_long lib = 0;
  TRACE("resolveAddress for {:x}", (uintptr_t)addr);
  if (addr->getLibrary().size() > 0) {
    _currTask->findFirstRegion(addr->getLibrary(), addr->getFlags(), &lib);

    // lib == -1 means the region couldn't be found. pass to caller
    if ((long)lib == -1) {
      return (u_long)-1;
    }
  }

  TRACE("done, found {:x}", lib + addr->getOffset());
  return lib + addr->getOffset();
}

void ThreadGroup::closeTracerLibs(pid_t pid) {
  assert (_dlsymGadget);

  if (pid) {
    auto ctIter = _tasks.find(pid);
    if (ctIter == _tasks.end()) {
      ERROR("pid {} not a member of this thread group!", pid);
      assert (false);
    }
    _currTask = ctIter->second;
  }
  INFO("done with {}", _currTask->getPid());
  auto g = Globals::Globals::getInstance();


  u_long destroyReturn = (int) executeGadget<0>(_sharedObject->destroyFxnGadget.get());
  INFO("done, destroy return {}", destroyReturn);


  assert (!destroyReturn);

  // clear out inst points in case any happen (after) exit
  for (auto &ip : _instrPoints) {
    unsetInstPoint(ip.second);
  }

  if (pid)
    _currTask = nullptr; // this is err... problematic!
}

void ThreadGroup::initTaskStructure(Task *t) {
  u_long gadgetLoc, initReturn;
  uintptr_t threadStackLow, threadStackHigh;
  struct user_regs_struct regs;
  auto g = Globals::Globals::getInstance();

  if (t) _currTask = t;
  if (_currTask->getInit()) {
    return;
  }
  INFO("{}: initTaskStructre start", _currTask->getPid());
  ptrace_syscall_begin(g->getDevFd(), _currTask->getPid());
  _currTask->getStackBounds(threadStackLow, threadStackHigh);

  // the taskInitgadget only modifies tracer structures.  But, we have
  // to isolate the stack. So, we assign to the tracing stack before
  // executing the gadget.  luckily, the caller of this function
  // restores register values (so non need to ckpt esp)

  initReturn = (int) executeGadget(_sharedObject->taskInitGadget.get(), threadStackLow, threadStackHigh);
  _currTask->setInit();
  assert (!initReturn);
  ptrace_syscall_end(g->getDevFd(), _currTask->getPid(), nullptr, nullptr);

  INFO("{}: initTaskStructre finished", _currTask->getPid());
}

Tracer::Tracer* ThreadGroup::buildTracer(TracerConfiguration *tc) {

  Tracer::Tracer *newTracer = new Tracer::Tracer(tc);
  tc->forEachBreakpoint(
      [newTracer, this](const Configuration::RegionIter r,
                        const Configuration::TracerConfiguration &tc) {
        char *insts;
        u_long len;
        uintptr_t start = resolveAddress(r.first), end = resolveAddress(r.second);
        assert (start != (uintptr_t)-1 && end != (uintptr_t)-1);
        TRACE("int3 gadget with 0x{:x} bytes for 0x{:x}-0x{:x}",
              (char*)end - (char*)start, start, end);

        auto int3 = Tracer::makeInt3Gadget((char*)end - (char*)start);

        // grab instructions originally in breakpoint
        len = _currTask->getdata(start, int3->getLen(),  &insts);
        assert(len >= int3->getLen());

        // copy into bpRegion. Shove int3 at start to set bp.

        // trickly little garbage at the end for char[] delete
        std::shared_ptr<char> pinsts(new char[len], std::default_delete<char[]>());
        std::shared_ptr<char> pbreakpoint(new char[len], std::default_delete<char[]>());

        memcpy(pinsts.get(), insts, len);
        memcpy(pbreakpoint.get(), pinsts.get(), len);
        memcpy(pbreakpoint.get(), int3->getBuffer(), int3->getLen());

        newTracer->addBreakpointRegion( Tracer::BreakpointRegion(start, end, pinsts, pbreakpoint, len));

        delete int3;
        delete[] insts;
      });

  NDEBUG("buildTracer {}", newTracer->toString());
  return newTracer;
}

void ThreadGroup::rebuildTracer(Tracer::Tracer *tracer) {
  assert (_currTask);

  for (auto br : *tracer) {
    auto int3 = Tracer::makeInt3Gadget((char*)br->getEnd() - (char*)br->getStart());

    // grab instructions originally in breakpoint
    auto len = _currTask->getdata(br->getStart(), br->getRegionLen(), &br->getRegion());
    assert(len == br->getRegionLen());

    // copy into bpRegion. Shove int3 at start to set bp.
    memcpy(br->getBreakpoint().get(), br->getRegion().get(), len);
    memcpy(br->getBreakpoint().get(), int3->getBuffer(), int3->getLen());
    delete int3;
  }
}

void ThreadGroup::populateInstPoint(Tracer::InstPoint *ip) {
  // not sure what tracer is, tbh

  // make jump if supported, int3 if not:
#ifdef INT3_TEST_MODE
  auto gadget = Tracer::makeInt3Gadget(1); // just shove this old dawg in there
#else
  auto gadget = ip->isJump() ? Tracer::makeJmpGadget(ip->getBPLoc(), ip->getResolvedLink()) : Tracer::makeInt3Gadget(1);
#endif

  // grab instructions originally in breakpoint
  char *ogInsts;
  auto len = _currTask->getdata(ip->getBPLoc(), gadget->getLen(),  &ogInsts);

  assert(len >= gadget->getLen());

  // check with our existing instruction points, are any of them in the slot that we're looking for?
  // (could have the instrumentation engine give us a heads up?)
  // note: this requires no duplicate instPoints. I think everything breaks in that case anwyays.
#ifndef INT3_TEST_MODE
  if (!ip->isJump()) {
#endif  
  for (int i = 1; i < len; ++i) {
    auto it = _instrPoints.find(i + ip->getBPLoc()) ;
    if (it != _instrPoints.end()) {

      assert (i >= 5);
      // this requires that the other instrPoints are already (somewhat) setup
      auto otherIP = it->second;

      memcpy(ogInsts + i,
             otherIP->getBPRegion()->getBreakpoint().get(),
             std::min((unsigned )(len - i), otherIP->getGadgetLen()));
    }
    if (false) { //i + ip->getBPLoc() == 0xb75d111d) {
      ERROR("fail! {} overlaps something that seems IMPORTANT", ip->toString() );
      THROW("what the fuck");
    }
  }
#ifndef INT3_TEST_MODE
}
#endif

  // trickly little garbage at the end for char[] delete
  std::shared_ptr<char> pinsts(new char[len], std::default_delete<char[]>());
  std::shared_ptr<char> pbreakpoint(new char[len], std::default_delete<char[]>());

  memcpy(pinsts.get(), ogInsts, len);
  delete[] ogInsts;
  ogInsts = nullptr;

  memcpy(pbreakpoint.get(), pinsts.get(), len);
  memcpy(pbreakpoint.get(), gadget->getBuffer(), gadget->getLen());

  ip->setBreakpoint(pinsts, pbreakpoint, len, gadget->getLen());

  setInstPoint(ip);
  //  INFO("{} orig {} bp {}", ip->toString(),
  //Log::parseLocal(pinsts, len), Log::parseLocal(pbreakpoint, len));

  delete gadget;
}

Tracer::InstPoint* ThreadGroup::buildInstPoint(InstConfiguration *inst, uintptr_t tracer) {
  assert (_currTask);

  auto newIP = new Tracer::InstPoint(inst, inst->getBP().first->getOffset());
  newIP->setResolvedLink(tracer);
  populateInstPoint(newIP);
  //setInstPoint(newIP);
  return newIP;
}


void ThreadGroup::setInstPoint(Tracer::InstPoint *ip) {
  auto &bpr = ip->getBPRegion();
  _currTask->setdata(ip->getBPLoc(), (*bpr).getRegionLen(), (*bpr).getBreakpoint());
}

void ThreadGroup::unsetInstPoint(Tracer::InstPoint *ip) {
  auto &bpr = ip->getBPRegion();
  _currTask->setdata(ip->getBPLoc(), (*bpr).getRegionLen(), (*bpr).getRegion());
}



void ThreadGroup::setTracer(Tracer::Tracer *t) {
  t->forEachBreakpoint([this](Tracer::BreakpointRegion &r) {
      TRACE("SET: putting {} into {:x}-{:x}",
             Log::parseLocal(r.getBreakpoint(), r.getRegionLen()),
             (unsigned)r.getStart(), (unsigned)(((char *)r.getStart()) + r.getRegionLen()));

      _currTask->setdata(r.getStart(), r.getRegionLen(), r.getBreakpoint());
    });
}

void ThreadGroup::unsetTracer(Tracer::Tracer *t) {
  t->forEachBreakpoint([this](Tracer::BreakpointRegion &r) {
      TRACE("UNSET: putting {} into {:x}-{:x}",
             Log::parseLocal(r.getRegion(), r.getRegionLen()),
             (unsigned)r.getStart(), (unsigned)(((char *)r.getStart()) + r.getRegionLen()));

      _currTask->setdata(r.getStart(), r.getRegionLen(), r.getRegion().get());
    });
}


void ThreadGroup::checkpointRegion(RegionSet &regions, const std::string regionId) {
  assert(_currTask);
  _currTask->findAllRegions(regionId, {}, regions);

  for (auto &mr : regions) {
    u_long len = _currTask->getdata(mr.getStart(),
                                    mr.getLen(),
                                    mr.getRegionLoc());
    assert(len == mr.getLen());
  }
}



void ThreadGroup::checkpointRegions(RegionSet &regions, const MemRegionMatcher &matcher) {
  assert(_currTask);
  _currTask->findAllRegions(matcher, regions);

  for (auto &mr : regions) {
    u_long len = _currTask->getdata(mr.getStart(), mr.getLen(), mr.getRegionLoc());
    assert(len == mr.getLen());
  }
}

void ThreadGroup::restoreRegions(RegionSet &regions) {
  assert(_currTask);

  for (auto &r : regions) {
    _currTask->setdata(r.getStart(), r.getLen(), *r.getRegionLoc());
    delete[] *r.getRegionLoc();
  }
}

void ThreadGroup::unsetTracers() {
  for (auto titer : _ctracers) {
    unsetTracer(titer);
  }
}

void ThreadGroup::setTracers() {
  for (auto titer : _ctracers) {
    setTracer(titer);
  }
}


// probably should be void, no?
void ThreadGroup::loadTracerObject() {
  auto c =  Configuration::Configuration::getInstance();
  // auto g = Globals::Globals::getInstance();
  auto tracerObjectLoc = c->getTracerObj();
  uintptr_t datalow = 0, datahigh = 0;
  Elf32_Addr addr;

  // load the tracerObject locally and remotely for ease of use:
  {
    Elf32_Ehdr ehdr;
    const Elf32_Phdr *phdr;
    const Elf32_Phdr *ph;
    size_t maplength;

    // a bunch of things:
    Elf32_Dyn *ld, *dynIter;
    Elf32_Half phnum, ldnum;
    Elf32_Addr map_start, map_end;

    auto openG = Tracer::makeOpenGadget();
    auto mmapG = Tracer::makeMmapGadget();
    auto mprotectG = Tracer::makeMprotectGadget();

    
    uintptr_t remoteStr = getStringLocation(tracerObjectLoc);
    u_int remotefd = executeGadget(openG, remoteStr, (u_long)(O_RDONLY | O_CLOEXEC));

    int localfd = open(tracerObjectLoc.c_str(), O_RDONLY | O_CLOEXEC);
    ssize_t len = read(localfd, (void*)&ehdr, sizeof(ehdr));
    assert (len == sizeof(ehdr));

    if (ehdr.e_type != ET_DYN) {
      ERROR("oh no, this isn't a dynamic elf! {}", ehdr.e_type);
      THROW("load problem.");
    }
    phnum = ehdr.e_phnum;

    maplength = ehdr.e_phnum * sizeof (Elf32_Phdr);
    if (ehdr.e_phoff + maplength <= (size_t) sizeof(ehdr))
      phdr = (Elf32_Phdr*) (((char*)&ehdr) + ehdr.e_phoff);
    else {
      phdr = (Elf32_Phdr*) malloc (maplength);
      lseek (localfd, ehdr.e_phoff, SEEK_SET);
      if ((size_t) read (localfd, (void *) phdr, maplength) != maplength) {
        ERROR("cannot read file data");
        THROW("load problem.");
      }
    }

    /* Scan the program header table, collecting its load commands.  */
    struct loadcmd {
      Elf32_Addr mapstart, mapend, dataend, allocend;
      off_t mapoff;
      int prot;
    } loadcmds[phnum], *c;
    size_t nloadcmds = 0;

    for (ph = phdr; ph < &phdr[phnum]; ++ph) {
      switch (ph->p_type) {
        /* These entries tell us where to find things once the file's
           segments are mapped in.  We record the addresses it says
           verbatim, and later correct for the run-time load address.  */
        case PT_DYNAMIC:
          ld = (Elf32_Dyn*) ph->p_vaddr;
          ldnum = ph->p_memsz / sizeof (Elf32_Dyn);
          break;

        case PT_LOAD:
          /* A load command tells us to map in part of the file.
             We record the load commands and process them all later.  */
          c = &loadcmds[nloadcmds++];

          c->mapstart = ph->p_vaddr & ~(PAGE_SIZE - 1);
          c->mapend = ((ph->p_vaddr + ph->p_filesz + PAGE_SIZE - 1)  & ~(PAGE_SIZE - 1));

          c->dataend = ph->p_vaddr + ph->p_filesz;
          c->allocend = ph->p_vaddr + ph->p_memsz;

          c->mapoff = (ph->p_offset >> PAGE_SHIFT);

          c->prot = 0;
          if (ph->p_flags & PF_R)
            c->prot |= PROT_READ;
          if (ph->p_flags & PF_W)
            c->prot |= PROT_WRITE;
          if (ph->p_flags & PF_X)
            c->prot |= PROT_EXEC;

          if (c->prot & (PROT_READ |PROT_WRITE) == (PROT_READ | PROT_WRITE)) {
            datalow = c->mapstart + addr;
            datahigh = c->mapend + addr;
          }
          break;

        case PT_TLS:
          ERROR("OH NO! we don't have any support for thread local storage!");
          THROW("load failed.");

        case PT_GNU_STACK:
          INFO("gnu stack: {}", ph->p_align);
          _sharedObject->setAlign(ph->p_align);
          break;
        case PT_GNU_EH_FRAME:
        case PT_GNU_RELRO:
          // no support, but I don't care about these
          break;
        default:
          INFO("unhandled program_header {}", ph->p_type);
      }
    }
    assert (nloadcmds);
    /* Now process the load commands and map segments into memory.  */
    c = loadcmds;

    /* Length of the sections to be loaded.  */
    maplength = loadcmds[nloadcmds - 1].allocend - c->mapstart;

    /* We can let the kernel map it anywhere it likes, but we must have
       space for all the segments in their specified positions relative
       to the first.  So we map the first segment without MAP_FIXED, but
       with its extent increased to cover all the segments.  Then we
       remove access from excess portion, and there is known sufficient
       space there to remap from the later segments.
    */
    map_start = (Elf32_Addr) executeGadget(mmapG,
                                           NULL,
                                           maplength,
                                           PROT_NONE,
                                           MAP_PRIVATE,
                                           remotefd,
                                           c->mapoff);
    if ((void *) map_start == MAP_FAILED) {
      ERROR("failed to map segment from shared object");
      THROW("load failed.");
    }

    map_end = map_start + maplength;
    addr = map_start - c->mapstart;

    while (c < &loadcmds[nloadcmds]) {
      if (c->mapend > c->mapstart) {
        // Map the segment contents from the file.
        NDEBUG("mmaping {:x} into region [{:x}-{:x}]",
              c->mapoff * PAGE_SIZE, addr + c->mapstart, c->mapend - c->mapstart);

        if ( (void*) executeGadget(mmapG,
                                   addr + c->mapstart,
                                   c->mapend - c->mapstart,
                                   PROT_READ|PROT_WRITE,
                                   MAP_FIXED|MAP_PRIVATE,
                                   remotefd,
                                   c->mapoff) == MAP_FAILED)
          THROW("load failed.");
      }

      if (c->allocend > c->dataend) {
        /* Extra zero pages should appear at the end of this segment,
           after the data mapped from the file.   */
        Elf32_Addr zero, zeroend, zeropage;

        zero = addr + c->dataend;
        zeroend = addr + c->allocend;
        zeropage = ((zero + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1));

        if (zeroend < zeropage)
          /* All the extra data is in the last page of the segment.   We can just zero it.  */
          zeropage = zeroend;

        if (zeropage > zero) {
          //  Zero the final part of the last page of the segment.
          unique_ptr<char[]> ptr(new char[zeropage - zero]);
          memset(ptr.get(), '\0', zeropage - zero);
          _currTask->setdata(zero, zeropage - zero, ptr.get());
        }

        if (zeroend > zeropage) {
          /* Map the remaining zero pages in from the zero fill FD.  */
          void* mapat = (void*)executeGadget(mmapG,
                                             zeropage,
                                             zeroend - zeropage,
                                             PROT_READ|PROT_WRITE,
                                             MAP_ANON|MAP_PRIVATE|MAP_FIXED,
                                             -1,
                                             0);
          if (mapat == MAP_FAILED) {
            ERROR("cannot map zero-fill pages");
            THROW("load failed.");
          }
        }
      }
      ++c;
    }

    // first grab the dynamic section as a block, then iterate through the block
    Elf32_Addr symtab = 0, relStart = 0, strtab = 0, initarray = 0;
    uintptr_t relsize = 0, strsize, initsize = 0;
    {
      shared_ptr<char> dynSection;
      _currTask->getdata((Elf32_Addr)ld + addr, ldnum * sizeof(Elf32_Dyn), &dynSection);

      dynIter = (Elf32_Dyn*) dynSection.get();
      while (dynIter->d_tag != DT_NULL) {
        switch (dynIter->d_tag) {
          case DT_REL:
            relStart = (Elf32_Addr)(dynIter->d_un.d_ptr + addr);
            break;
          case DT_RELSZ:
            relsize = dynIter->d_un.d_val;
            break;
          case DT_RELENT:
            assert (dynIter->d_un.dval == sizeof(Elf32_Rel));
            break;
          case DT_STRTAB:
            strtab = dynIter->d_un.d_val + addr;
            break;
          case DT_STRSZ:
            strsize = dynIter->d_un.d_val;
            break;
          case DT_SYMTAB:
            symtab = (Elf32_Addr)(dynIter->d_un.d_ptr + addr);
            break;
          case DT_SYMENT:
            assert (dynIter->d_un.d_val == sizeof(Elf32_Sym));
            break;
          case DT_INIT_ARRAY:
            initarray = (Elf32_Addr)(dynIter->d_un.d_ptr + addr);
            break;
          case DT_INIT_ARRAYSZ:
            initsize = dynIter->d_un.d_val;
            break;
          case DT_TEXTREL:
            // we intentionally ignore these.
            break; 
          default:
            ERROR("unhandled tag {} -> {:x} ({:x})",
                  dynIter->d_tag, dynIter->d_un.d_val, dynIter->d_un.d_ptr);
        }
        ++dynIter;
      }
    }
    {
      // load start into my own memory
      NDEBUG("got relSection from [{} -{}]", relStart, relsize);
      shared_ptr<char> relSection;
      _currTask->getdata(relStart, relsize, &relSection);
      Elf32_Rel *start = (Elf32_Rel*)relSection.get();
      Elf32_Rel *end = (Elf32_Rel*)((char*)relSection.get() + relsize);


      NDEBUG("getting symSection [{:x}-{}]", symtab, strtab - symtab);
      shared_ptr<char> symSection;
      _currTask->getdata(symtab, strtab - symtab , &symSection);
      Elf32_Sym *symStart = (Elf32_Sym*)symSection.get();
      Elf32_Sym *symEnd = (Elf32_Sym*)(((char*)symSection.get()) + (strtab - symtab));

      NDEBUG("getting strsection [{}-{}]", strtab, strsize);
      shared_ptr<char> strSection;
      _currTask->getdata(strtab, strsize, &strSection);

      // assumes that strtab always after symtab (I guess libc does this too?)

      INFO("relocating {} items", end - start);
      for (; start < end; ++start) {
        Elf32_Addr dest_addr = addr + start->r_offset;
        Elf32_Sym  *sym_value =  symStart + ELF32_R_SYM(start->r_info);

        // get dest address using getdata
        shared_ptr<char> dest_holder;
        _currTask->getdata(dest_addr, sizeof(Elf32_Addr), &dest_holder);
        Elf32_Addr *dest_value = (Elf32_Addr*)dest_holder.get();

        shared_ptr<char> checker;
        Elf32_Addr *checkData = nullptr;
        switch (ELF32_R_TYPE(start->r_info)) {
          case R_386_RELATIVE:
            *dest_value += addr;
            _currTask->setdata(dest_addr, sizeof(Elf32_Addr), (char*)dest_value);
            _currTask->getdata(dest_addr, sizeof(Elf32_Addr), &checker);
            checkData = (Elf32_Addr*)checker.get();
                        break;
          case R_386_PC32:
            *dest_value += addr + sym_value->st_value - (Elf32_Addr)dest_addr;
            _currTask->setdata(dest_addr, sizeof(Elf32_Addr), (char*)dest_value);
            break;
          case R_386_32:
            if (sym_value->st_value == 0) {
              ERROR("WHOAH, NELLY no value for {}", strSection.get() + sym_value->st_name);
            }
            *dest_value += addr + sym_value->st_value;
            _currTask->setdata(dest_addr, sizeof(Elf32_Addr), (char*)dest_value);
            break;
          default:
            fprintf(stderr, "offset: %x, info: %x, type: %x\n", start->r_offset, start->r_info, ELF32_R_TYPE(start->r_info));
            break;
        }
      }

      for (; symStart != symEnd; ++symStart) {
        if (ELF32_ST_TYPE(symStart->st_info) == STT_FUNC ||
            ELF32_ST_TYPE(symStart->st_info) == STT_NOTYPE) {
          assert (symStart->st_name < strsize);

          string name = strSection.get() + symStart->st_name;
          _sharedObject->addSymbol(name, symStart->st_value + addr);
        }
      }
    }

    TRACE("fixup mapping permissions");
    // fixup permissions after relocations:
    c = loadcmds;
    while (c < &loadcmds[nloadcmds]) {
      if (c->mapend > c->mapstart) {
        executeGadget(mprotectG, (addr + c->mapstart), c->mapend - c->mapstart, c->prot);
      }
      ++c;
    }

    {
      TRACE("calling init fxns:  [{:x}-{}]", initarray, initsize);
      shared_ptr<char> initSection;
      _currTask->getdata(initarray, initsize, &initSection);
      u_long *inits = (u_long*)initSection.get();

      for (int i = 0; i < initsize; i += 4) {
        NDEBUG("initializer: {:x}", inits[i]);
        auto initfxn = Tracer::makeFxnCallGadget<0>(inits[i], _sharedObject->getAlign());
        executeGadget(initfxn);
      }
    }
  }

  // did wark?
  INFO("done mapping");
  // _currTask->debugAllRegions();
  STEP();


  // Get shared mem region ready:
  std::string sharedMemName = tracerObjectLoc;
  std::replace(sharedMemName.begin(), sharedMemName.end(), '/','.');
  if (sharedMemName[0] == '.') {
    sharedMemName = sharedMemName.substr(1); // skip first item
  }
  std::stringstream sharedNameStream;
  sharedNameStream << "/dev/shm/" << sharedMemName << "." << _currTask->getPid();


  NDEBUG("create shared state {} with model", sharedNameStream.str());
  int fd = open(sharedNameStream.str().c_str(), O_RDWR | O_CREAT, 0777);
  int rc = ftruncate(fd, SHARED_STATE_SIZE);
  assert(!rc);
  _sharedObject->state = (struct sharedState *) mmap(NULL, 0x1000, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  close(fd);

  //Get initialization params and call tracer obj file init
  _sharedObject->state->tracerStackLow = (u_long *)_stackLow;
  _sharedObject->state->tracerStackHigh = (u_long *)_stackHigh;
  _sharedObject->state->tracerDataLow = (u_long*)datalow;
  _sharedObject->state->tracerDataHigh = (u_long*)datahigh;

  // prepare standard gadgets
  _sharedObject->taskInitGadget.reset(Tracer::makeFxnCallGadget<2>(_sharedObject->getSymbol(TASK_INIT_FXN),
                                                                   _sharedObject->getAlign()));

  _sharedObject->destroyFxnGadget.reset(Tracer::makeFxnCallGadget<0>(_sharedObject->getSymbol(TRACER_DESTROY_FXN),
                                                                     _sharedObject->getAlign()));

  //initialize the object
  auto sharedName = getStringLocation(sharedNameStream.str());
  auto init = Tracer::makeFxnCallGadget<2>(_sharedObject->getSymbol(TRACER_INIT_FXN),
                                           _sharedObject->getAlign());

  NDEBUG("executing init gadget!");
  STEP();

  // debugTracer = true;
  auto initReturn = (int) executeGadget(init, sharedName, (u_long)c->getJC());
  // open the logs:
  auto addLog = Tracer::makeFxnCallGadget<1>(_sharedObject->getSymbol(TRACER_ADDLOG_FXN),
                                             _sharedObject->getAlign());  // das need getalign?
  for (auto log : c->getTracerLogs()) {
    // todo: somehow free getStrings!
    auto logLoc = getStringLocation(log);
    (int) executeGadget(addLog, logLoc);
  }
  NDEBUG("done!");
  STEP();
}


void ThreadGroup::mapInstPoints() {
  auto conf = Configuration::Configuration::getInstance();
  NDEBUG("mapping Inst Points with {}", (void *)_sharedObject.get());
  for (auto &tc : *conf) {
    NDEBUG("tracing config {}", tc->toString());
    auto tracer = _sharedObject->getSymbol(tc->getLink());
    if (!tracer) {
      ERROR("Cannot find {} ", tc->getLink());
      THROW("tracer not found");
    }
    auto *ip = buildInstPoint(tc, tracer);
    _instrPoints.emplace(ip->getBPLoc(), ip);
    NDEBUG("mapInstPoint {}", ip->toString());
  }
}


void ThreadGroup::mapCTPoints(void) {
  auto conf = Configuration::Configuration::getInstance();
  for (auto tc = conf->tracerBegin(), end = conf->tracerEnd(); tc != end; ++tc) {

    auto iter = buildTracer(*tc);
    _ctracers.emplace_back(iter);

    // load the shared object for this tracer
    auto outputFoo = _sharedObject->getSymbol((*tc)->getOutputFunction());
    auto filterFoo = _sharedObject->getSymbol((*tc)->getFilter()->getOutputFunction());
    auto beginFoo = _sharedObject->getSymbol(TRACER_BEGIN_FXN);
    auto endFoo = _sharedObject->getSymbol(TRACER_END_FXN);

    iter->setGadget(Tracer::makeTracerCall(outputFoo, beginFoo, endFoo, _sharedObject->getAlign()));
    iter->setFilterGadget(Tracer::makeTracerCall(filterFoo, beginFoo, endFoo, _sharedObject->getAlign()));
  }
}

void ThreadGroup::mapPoints() {
  assert(_currTask);

  auto conf = Configuration::Configuration::getInstance();

  // set a "tracer" (breakpoint) for thread start
  auto tc = conf->getThreadStartTracerConfig();
  assert (tc);
  _threadStartTracer = buildTracer(tc);
  setTracer(_threadStartTracer);
  NDEBUG("inserted threadStartTracer");

  // also set the "tracer" (breakpoint) for exit
  auto exitConfig = conf->getExitTracerConfig();
  assert (exitConfig);
  _exitTracer = buildTracer(exitConfig);
  setTracer(_exitTracer);
  NDEBUG("inserted exitTracer");

  // set a "tracer" (breakpoint) for fork_exit points
  auto forkTC = conf->getForkExitTracerConfig();
  assert (forkTC);
  _forkExitTracer = buildTracer(forkTC);
  setTracer(_forkExitTracer);
  NDEBUG("inserted forkExitTracer");

  // now map them into the replay's address space:
  NDEBUG("Map all instrumenatiton points");
  STEP();

  // prevent changes to shared variables from breaking replay
  auto g = Globals::Globals::getInstance();
  ptrace_syscall_begin(g->getDevFd(), _currTask->getPid());
  g->startLoadTracerTime();
  loadTracerObject();
  g->stopLoadTracerTime();
  ptrace_syscall_end(g->getDevFd(), _currTask->getPid(), nullptr, nullptr);
  NDEBUG("initmainTask");

  initTaskStructure(_currTask);
  NDEBUG("map all inst points");
  mapInstPoints();
  NDEBUG("map all ctpoints");
  mapCTPoints();

  _segfaultGadget = Tracer::makeSegfaultGadget(_sharedObject->getSymbol(TRACER_END_FXN));

  // set all tracepoints before yielding control to the traced program
/*
  // these are set inside of mapInstPoints... I hope they're set correctly!
  INFO("setting {} points", _instrPoints.size())
  for (auto ip : _instrPoints) {
    setInstPoint(ip.second);
    NDEBUG("{} placed {}", ip.second->toString(),
           Log::parseRemote(_currTask, ip.second->getBPLoc(), 0x8));
  }
*/
  // execute all CTracers before yielding control to the traced program
  // todo: what happens when we aren't the first epoch!!
  if (_ctracers.size() > 0) {
    struct user_regs_struct regs;
    _currTask->getregs(&regs);
    
    INFO("Call ctracers at attach!");
    for (auto t : _ctracers) {
      bool execute = callCTracer(t, regs, CSYS);
      if (execute) {
        _activeTracers.insert(t);
      }
    }
  }

  NDEBUG("done mapping");
  STEP();
}

u_long ThreadGroup::getStringLocation(std::string s) {
  auto stringIter = _strings.find(s);
  //add one b/c of null byte
  u_long storeLen = s.size() + 1;
  u_long returnLoc;

  if (stringIter == _strings.end()) {
    if (_nextRegion + storeLen > _endRegionBlock) {
      NDEBUG("getStringLocation getting a new region");
      getNewRegion(&_nextRegion, &_endRegionBlock);
    }

    // place string. Add one to len b/c of
    returnLoc = _nextRegion;
    _nextRegion += storeLen;

    // and insert string
    _currTask->setdata(returnLoc, storeLen, s.c_str());
    _strings.insert(std::make_pair(s, returnLoc));
    return returnLoc;
  }
  return stringIter->second;
}

void ThreadGroup::getNewRegion(u_long *start, u_long *end, u_long len, u_long prot) {
  u_long newRegion, endRegion;
  assert(_currTask);
  TRACE("getting new Region");
  if (!_regionsAllocator) {
    // first build a region for the region allocator gadget
    char *tempData = NULL;
    u_long tempLen = 0, tempGadgetLoc;

    // get and checkpoint stack
    uintptr_t stackLow, stackHigh;
    shared_ptr<char> stack;
    _currTask->getStackBounds(stackLow, stackHigh);
    _currTask->getdata(stackLow, stackHigh - stackLow, &stack);

    _currTask->findFirstRegion("", "x", &tempGadgetLoc);
    _regionsAllocator = Tracer::makeMmapGadget();

    // Put the allocator into a temp location
    tempLen = _currTask->getdata(tempGadgetLoc, _regionsAllocator->getLen(), &tempData);
    _currTask->setdata(tempGadgetLoc, _regionsAllocator->getLen(), _regionsAllocator->getBuffer());
    _regionsAllocator->setLocation(tempGadgetLoc);

    // now execute the allocator
    TRACE("execute allocator");
    // debugTracer = true;
    auto allocRegion  = executeGadget(_regionsAllocator,
                                      0,
                                      0x1000,
                                      PROT_EXEC|PROT_READ,
                                      MAP_PRIVATE | MAP_ANONYMOUS,
                                      -1,
                                      0);
    assert ((long)newRegion != -1);

    // fixup the temp location
    _currTask->setdata(tempGadgetLoc, tempLen, tempData);
    delete []tempData;
    NDEBUG("allocator {:x}-{:x}", allocRegion, allocRegion + _regionsAllocator->getLen());

    // place allocator in perminent home
    _currTask->setdata(allocRegion, _regionsAllocator->getLen(), _regionsAllocator->getBuffer());
    _regionsAllocator->setLocation(allocRegion);

    // build new stack
    _stackLow = executeGadget(_regionsAllocator,
                              0,
                              STACK_LEN,
                              PROT_READ|PROT_WRITE,
                              MAP_PRIVATE|MAP_ANON,
                              -1,
                              0);
    _stackHigh = _stackLow + STACK_LEN;
    INFO("{} stack is [{:x} - {:x}]", (void*)this, _stackLow, _stackHigh);

    // undo changes to the original stack:
    _currTask->setdata(stackLow, stackHigh - stackLow, stack);
  }

  newRegion = executeGadget(_regionsAllocator,
                            0,
                            0x1000,
                            PROT_EXEC|PROT_READ,
                            MAP_PRIVATE | MAP_ANONYMOUS,
                            -1,
                            0);
  endRegion = newRegion + len;
  *start = newRegion;
  *end   = endRegion;
}

#ifdef PARANOID
static void checkMemory(RegionSet prStart, RegionSet prEnd)
{
  // changed so that won't be defined

  if (state->memStates > mmaps)
  {
    for (int i = mmaps; i < state->memStates; ++i)
    {
      for (auto region : prEnd)
      {
        // assume that if the starts match that the whole thing
        // is from the tracer Malloc (does this *seem* reasonable?)
        if (region.getStart() == (u_long)state->mem[i].start)
        {
          newRegions.insert(region);
        }
      }
    }
    mmaps = state->memStates;
  }
  bool differ = false;
  // DIFF 'EM
  for (auto s : prStart)
  {
    auto e = prEnd.find(s);
    if (e == prEnd.end())
    {
      ERROR("cannot find ({:x},{:x}) in end",
            s.getStart(), s.getLen());

      // THROW("PARANOID");
      continue;
    }
    // skip regions that only exist b/c of tracers
    if (newRegions.find(s) != newRegions.end())
    {
      continue;
    }
    bool rdiffer = false;
    int incrs = 0;
    u_long *sStart = (u_long *)(*s.getRegionLoc()),
        *sPtr = (u_long *)(*s.getRegionLoc()),
        *sEnd = (u_long *)((*s.getRegionLoc()) + s.getLen()),
        *ePtr = (u_long *)(*e->getRegionLoc()),
        *eEnd = (u_long *)((*e->getRegionLoc()) + e->getLen());

    if (s.getLen() % sizeof(long))
      NDEBUG("yikes, skipping some comparison data!");
    while (sPtr < sEnd)
    {
      u_long sword = *(sPtr), eword = *(ePtr);
      if (sword != eword)
        ERROR("{:x}: {:x} vs. {:x} ",
              (incrs * sizeof(u_long)) + s.getStart(),
              sword,
              eword);

      incrs++;
      sPtr++;
      ePtr++;
      rdiffer = rdiffer || (sword != eword);
    }
    if (rdiffer)
      ERROR("{} has difference", s.toString());

    differ = differ || rdiffer;
    DEBUG_ALLOC("free {}", (void*)*s.getRegionLoc());
    DEBUG_ALLOC("free {}", (void*)*e->getRegionLoc());
  }
  if (differ) {
    _currTask->debugAllRegions();
    THROW("PARANOID");
  }
  cleanupRegions(prStart);
  cleanupRegions(prEnd);
}
#endif

void ThreadGroup::updateAddresses(struct sharedState *state, AddressSet &as) {

  // We have to convert accesslog to pointers in our address space:
  u_long *currPtr = (u_long*)(((char*)state) + AL_OFFSET);
  u_long *endPtr = currPtr + (state->acclogPtr - state->acclogStart);

  INFO("update addresses: {}-{}", (void*)currPtr, (void*)endPtr);

  as.incTimeStep();
  while (currPtr < endPtr) {
    INFO("I'm seeing: {:x}", *currPtr);
    if (as.addAddr(*currPtr)) {
      mp.protectOne(_currTask, *currPtr);
    }
    currPtr++;
  }

  INFO("memProtector state {}", mp.debugString());
}

// Todo: when we're setting up the addressSet and we aren't the first epoch?
bool ThreadGroup::callCTracer(Tracer::Tracer *tracer, struct user_regs_struct &regs, CTracerCause cause) {
  int rc;
  uintptr_t entryEip = regs.eip, gadgetLoc;

  Globals::timePoint tracerStart, tracerEnd;
  Globals::Globals *g = Globals::Globals::getInstance();
  bool filReturn = false;
  bool wasInRegion = tracer->getInRegion(); // tells us if we were already looking at the function
  bool inRegion = tracer->isInRegion(g->getPthreadLogClock());

  assert(_currTask);

  if (cause == CTracerCause::CSYS && wasInRegion && !inRegion) {
    INFO("Tear this fucker down! (but we don't know how yet)");
    assert(false);
  }
  else if (inRegion && (cause != CTracerCause::CSYS || (!wasInRegion && CTracerCause::CSYS))) {
    INFO("calling {} cause {} chalk {} eip {:x}", tracer->toString(), cause, g->getPthreadLogClock(), entryEip);

    assert (tracer->getFilterGadget());
    if (cause != CTracerCause::CSTEP) {
      tracerStart = Globals::myClock::now();
      filReturn = executeGadget(true, tracer->getFilterGadget());
      tracerEnd = Globals::myClock::now();

      NDEBUG("filter gadget returns {} at {}", filReturn, g->getPthreadLogClock());
      STEP();

      // I think sharedObject->state isn't coppied correctly or something?
      NDEBUG("shared obj ptr {}", (void*)_sharedObject->state);
      updateAddresses(_sharedObject->state, tracer->as);
    }
    else {
      filReturn = true;
    }
    if (filReturn) {
      //call the tracer!
      // debugTracer = true;
      tracerStart = Globals::myClock::now();
      executeGadget(tracer->getGadget());
      tracerEnd = Globals::myClock::now();
      TRACE("pid {} tracer execute done @ {:x}",  _currTask->getPid(),  (unsigned)regs.eip);
    }
    else {
      TRACE("pid {} filtering gadget", _currTask->getPid());
    }
    tracer->setActive(filReturn);
    INFO("we out.");
  }//wat
  return tracer->getActive();
}



static int ipCount = 0;

void ThreadGroup::instPointJmp(Tracer::InstPoint *instp, struct user_regs_struct &regs) {
  Globals::Globals *g = Globals::Globals::getInstance();
  int status = 0;
  uintptr_t orig_eip = regs.eip;

  assert(_currTask);

  ipCount += 1;
  regs.eip = instp->getResolvedLink();
  _currTask->setregs(&regs);

  //  if (g->getPthreadLogClock() == 0xa47 ) {
  //INFO("regs={}", Log::parseRegs(regs));
  //}
  if (false) {
  //  if (g->getPthreadLogClock() == 14757 && (orig_eip == 0x805691f  || orig_eip == 0x8056920 )) {
    NDEBUG("entering super debug on task {}", _currTask->getPid());
    _currTask->debugAllRegions(); // this might not work b/c of deadness?
    do {
      _currTask->getregs(&regs);
      INFO("{}: {} {}",  _currTask->getPid(), Log::parseRegs(regs), Log::parseRegsExt(regs));
      INFO("Inst={}", Log::parseInstruction(_currTask, regs.eip)); //
      //      INFO("{:x}={}", 0xb6ad50c8 - 44, Log::parseRemote(_currTask, 0xb6ad50c8 - 44, 0x4));

      uintptr_t ptr = regs.eip - 1;
      auto iter = _instrPoints.find(ptr);
      if (iter != _instrPoints.end()) {
        INFO("nxt inst jump");
        g->addBreakpointCount();
        regs.eip = iter->second->getResolvedLink();
        _currTask->setregs(&regs);

      }
      else {
        int steprc = _currTask->step();
        if (steprc) {
          ERROR("problem whilst steppin");
          break;
        }
        g->wait(&status);
      }
    } while (WIFSTOPPED(status) && WSTOPSIG(status) != SIGSEGV
             && g->getPthreadLogClock() == 14757);

    ERROR("segfault why debugging! {}", _currTask->getFaultAddr());
  }
}


string getInteresting(Task *t) {
  std::stringstream stream;
  //auto mem = 0xb5b00000 + 10972;
  //auto mem = 0xb4b00000 + 8385146;
  auto mem = 0xb5306aa8;

  stream << std::hex << Log::parseRemotePlain(t, mem, 0x4);
  return stream.str();
}

void dumpInteresting(Task *t) {
  std::ofstream dump("mem_dump", std::ios_base::app);
  auto mem = 0xb5b00000;
  for (u_long offset = 0; offset < 0x23000; offset += 0x1000) {
    dump << std::hex << Log::parseRemotePlain(t, mem + offset, 0x1000);
  }
  dump << std::endl;
}

void ThreadGroup::syscallLoop() {
  struct user_regs_struct regs;
  Globals::Globals *g = Globals::Globals::getInstance();
  int status = 0, cnt = 0;
  assert(_currTask);


  NDEBUG("entering super debug on task {}", _currTask->getPid());
  _currTask->debugAllRegions(); // this might not work b/c of deadness?
  do {
    _currTask->getregs(&regs);

    INFO("{}: {}: {}", g->getPthreadLogClock(), Log::parseRegs(regs), getInteresting(_currTask));

    int steprc = _currTask->step();
    if (steprc) {
      NDEBUG("problem whilst steppin");
      break;
    }
    g->wait(&status);
    cnt ++;

    } while (WIFSTOPPED(status) && WSTOPSIG(status) != SIGSEGV &&
             g->getPthreadLogClock() <= 128935 &&
             cnt <= 100000);
}


void ThreadGroup::watchLoop(u_int endclock) {
  struct user_regs_struct regs;
  Globals::Globals *g = Globals::Globals::getInstance();
  int status = 0;
  assert(_currTask);

  string inst = getInteresting(_currTask);
  NDEBUG("entering watch debug on task {} with {}", _currTask->getPid(), inst);
  do {
    string check = getInteresting(_currTask);
    _currTask->getregs(&regs);

    if (inst.compare(check) || regs.eip == 0xb75159c9) {
      inst = check;
      INFO("{}: {}: {}", g->getPthreadLogClock(), Log::parseRegs(regs), check);
    }
    int steprc = _currTask->step();
    if (steprc) {
      NDEBUG("problem whilst steppin");
      break;
    }
    g->wait(&status);

    } while (WIFSTOPPED(status) && WSTOPSIG(status) != SIGSEGV &&
             g->getPthreadLogClock() <= endclock);
}

// #define MEM_DEBUG 0xb4b00000
bool once = true;
TGStatus ThreadGroup::dispatch(int status, pid_t caughtPid, int &signalToSend) {
  struct user_regs_struct regs;
  struct user_fpxregs_struct fpu;
  int signalRecd;
  uintptr_t signalInst;
  pid_t wpid;
  TGStatus tgstatus = TGStatus::ALIVE;
  Globals::Globals *g = Globals::Globals::getInstance();


    // get the current task
  auto ctaskIter = _tasks.find(caughtPid);
  if (ctaskIter == _tasks.end()) {
    ERROR("{} pid {} not a member of this thread group", (void*)this, caughtPid);
    for (auto task : _tasks) {
      ERROR("task : {}, {}", task.first, task.second->getPid());
    }
    assert(false);
  } // if
  _currTask = ctaskIter->second;


  // figure out what this dispatch is all about
  signalToSend = 0;
  if (WIFEXITED(status)) {
    STATUS("pid {} dead with status {}", caughtPid, WEXITSTATUS(status));
    // return TGStatus::DEAD;
    return TGStatus::DYING;
  } //if

  else if (WIFSIGNALED(status)) {
    STATUS("pid {} dead from signal {}", caughtPid, WTERMSIG(status));
    //return TGStatus::DEAD;
    return TGStatus::DYING;
  } // if

  assert(WIFSTOPPED(status));
  signalRecd = WSTOPSIG(status);
  _currTask->getregs(&regs);
  _currTask->getfpu(&fpu);

  signalInst = regs.eip - 1;

  // siginf
  NDEBUG("dispatch with {}", Log::parseRegs(regs));

  switch (signalRecd) {
    case SIGSEGV: {
      // signalInst is incorrect in these cases, it was actually signalled on the
      // current eip
      u_long addr = _currTask->getFaultAddr();
      TRACE("{} segfault on {:x} @ {:x} clk {}",  caughtPid, addr, (unsigned)regs.eip, g->getPthreadLogClock());

      if (mp.inSet(addr)) {
        // unset the protection and step the execution
        mp.unprotectOne(_currTask, addr);
        TRACE("{}: seg on {:x} @ {}", _currTask->getPid(), addr, Log::parseRegs(regs));
        _currTask->setregs(&regs);
        _currTask->step();
        g->waitFor(&status, _currTask->getPid());

        // there's a world in which we double fault... but correctly
        // EEK (!) What if it's because an access crosses page
        // boundaries? should we spin here until there's no more faults?

        // maybe I shouldn't automagically protect this? not so sure
        mp.protectOne(_currTask, addr);

        // check to see if any of my cTracers are watching this address:
        _currTask->getregs(&regs);
        for (auto t : _ctracers) {
          if (t->as.contains(addr)) {
            INFO("calling a tracer on {} b/c {:x} clk {}", _currTask->getPid(), addr, g->getPthreadLogClock());
            ERROR("has {}", t->as.debugString());
            bool execute = callCTracer(t, regs);
            if (execute) {
              ERROR("would add it to the active set");
              _activeTracers.insert(t); // does has work?
            }
          }
        }
        if (_activeTracers.size() > 0) {
          enableSingleStep(regs);
        }
        _currTask->setregs(&regs);
        _currTask->setfpu(&fpu);
      }
      else {
        ERROR("{}:{} regs {} addr {:x}", _currTask->getPid(), g->getPthreadLogClock(), Log::parseRegs(regs), addr);
        ERROR("Inst={}", Log::parseInstruction(_currTask, regs.eip));
	ERROR("Might be the program, might be our fault, but unhandlable segfault");
        _currTask->debugAllRegions(); // seems... problematic?
	return TGStatus::DYING; // this *really* should be a dead. but, we can close the pipe, so I made it dying.?
      }
      break;
    }
    case (0x80 | SIGTRAP): {
#ifdef MEM_DEBUG
      auto clock = g->getPthreadLogClock();
      if (clock >= 124712 && clock <= 128935) {

        RegionSet rs;
        checkpointRegions(rs, MemRegionMatcher("", 0, MEM_DEBUG));
        int fd = open("mem_dump", O_APPEND|O_WRONLY|O_CREAT|O_LARGEFILE, 0777);
        if (fd < 0) {
          ERROR("couldn't open mem_dump {} {}", fd, errno);
        }

        int rc = write(fd, (void*)&clock, sizeof(clock));
        if (rc <= 0) {
          ERROR("couldn't write size...? {} {}", rc, errno);
        }

        for (auto r : rs) {
          r.dumpRaw(fd);
          delete [] *r.getRegionLoc();
        }
        close(fd);
      }
#endif

      unique_ptr<Syscall> sp = Syscall::getSyscall(regs);
      auto syscall = _syscallInProgress.find(_currTask);
      if (syscall == _syscallInProgress.end() ||
          syscall->second->syscallNum != sp->syscallNum) {
        NDEBUG("pid {} @ syscall {} start, clock {}",
             caughtPid, sp->syscallNum, g->getPthreadLogClock());
        _syscallInProgress[_currTask] = std::move(sp);
      }
      else {
        unique_ptr<Syscall> &sp = syscall->second;
        int num =  sp->syscallNum;
        NDEBUG("pid {} @ syscall {} end, clock={}",
               caughtPid, sp->syscallNum, g->getPthreadLogClock());

        sp->updateSyscall(regs);

        switch(sp->syscallNum) {
          case NR_MMAP2:
            for (auto it : _instrPoints) {
              if (sp->mmap.addr <= it.first && it.first < sp->mmap.addr + sp->mmap.len) {
                TRACE("EEK! {}:found an overlap (!) mmap {:x}-{:x}, tracer is {:x}",
                     _currTask->getPid(), sp->mmap.addr, sp->mmap.addr + sp->mmap.len, it.first);

                // The underlying memory probably changed. so REBUILD is necessary:
                populateInstPoint(it.second);
                //setInstPoint(it.second);
                STEP();
              }
            }
            for (auto ip : *_threadStartTracer) {
              if (sp->mmap.addr <= ip->getStart() && ip->getStart() < sp->mmap.addr + sp->mmap.len) {
                rebuildTracer(_threadStartTracer);
                setTracer(_threadStartTracer);
                break;
              }
            }
            for (auto ip : *_exitTracer) {
              if (sp->mmap.addr <= ip->getStart() && ip->getStart() < sp->mmap.addr + sp->mmap.len) {
                rebuildTracer(_exitTracer);
                setTracer(_exitTracer);
                break;
              }
            }
            for (auto ip : *_forkExitTracer) {
              if (sp->mmap.addr <= ip->getStart() && ip->getStart() < sp->mmap.addr + sp->mmap.len) {
                rebuildTracer(_forkExitTracer);
                setTracer(_forkExitTracer);
                break;
              }
            }
            // check our memProtector
              // INFO("MMAP: {} -{}",(void*)sp->mmap.addr, (void*)(sp->mmap.len + sp->mmap.addr))
            mp.updateRange(_currTask, sp->mmap.addr, sp->mmap.len, sp->mmap.prot);

            // check and call any CTracers that need it:
            if (_ctracers.size() > 0) {
              // might want a mechanism to not execute that CTracer if active tracers
              // because o/w/ we might have the weird single-stepping through the program
              for (auto t : _ctracers) {
                bool call = false;
                for (auto addr : t->as) {
                  if (sp->mmap.addr <= addr && addr < sp->mmap.addr + sp->mmap.len)
                    call = true;
                }
                if (call) {
                  INFO("Call continuous tracers because of syscall (end) {} by {} @ {}",
                       regs.orig_eax, _currTask->getPid(), g->getPthreadLogClock());
                  bool execute = callCTracer(t, regs, CSYS);
                  if (execute) {
                    _activeTracers.insert(t);
                  }
                }
              }
              if (_activeTracers.size() > 0) {
                ERROR("I'm not sure what to do here");
              }
              _currTask->setregs(&regs);
              _currTask->setfpu(&fpu);
            }

            break;
          case NR_MPROTECT:
            // INFO("MPROTECT: {} -{}",(void*)sp->mprotect.addr, (void*)(sp->mprotect.len + sp->mprotect.addr));
            mp.updateRange(_currTask, sp->mprotect.addr, sp->mprotect.len, sp->mprotect.prot);
            // TRACE("WARNING! {}: possible weirdness with continuous tracers!", _currTask->getPid());
            break;
          case 252:
            tgstatus = TGStatus::DYING;
            break;
        }
        _syscallInProgress.erase(syscall);

        // #define SPECIAL_TEST
#ifdef SPECIAL_TEST
        if (g->getPthreadLogClock() == 0xa47) {
          INFO("entering super debug on task {}", _currTask->getPid());
          _currTask->debugAllRegions(); // this might not work b/c of deadness?
          do {
            //std::stringstream stream;
            _currTask->getregs(&regs);
            INFO("{}: {} {}",  _currTask->getPid(), Log::parseRegs(regs), Log::parseRegsExt(regs));
            INFO("{}={}", 0xb6ad50c8 - 44, Log::parseRemote(_currTask, 0xb6ad50c8 - 44, 0x4));
            INFO("Inst={}", Log::parseInstruction(_currTask, regs.eip)); //
            // INFO("Other Inst={}", Log::parseInstruction(_currTask, 0xb764a456));
            //INFO("{}", stream.str());

            uintptr_t ptr = regs.eip; // - 1;
            auto iter = _instrPoints.find(ptr);
            bool skip = false;
            if (iter != _instrPoints.end()) {
              INFO("nxt inst jump");
#ifdef INT3_TEST_MODE
              if (true) {
#else
                if (!iter->second->isJump()) {
#endif
                skip = true;
                g->addBreakpointCount();
                regs.eip = iter->second->getResolvedLink();
                _currTask->setregs(&regs);
              }
            }
            if (!skip) {
              int steprc = _currTask->step();
              if (steprc) {
                NDEBUG("problem whilst steppin");
                break;
              }
              g->wait(&status);
            }

          } while (WIFSTOPPED(status) && WSTOPSIG(status) != SIGSEGV);
        }
#endif
      }
      break;
    }
    case SIGTRAP: {
      u_long clock = g->getPthreadLogClock();
      if (clock >= 2459578 && clock <= 2459580)
        INFO("pid={} clock={} eip={:x}", _currTask->getPid(), clock, signalInst);
      auto iter = _instrPoints.find(signalInst);

      if (_threadStartTracer->hasRegion(signalInst)) {
        //g->startLoadTime();
        // remove the loaded tracer's breakpoint
        INFO("{}: thread trappin' on thread_start ({:x})",
             _currTask->getPid(),
             signalInst);

        // step around the breakpoint
        regs.eip = signalInst;
        _currTask->setregs(&regs);
        unsetTracer(_threadStartTracer);
        _currTask->step();
        g->waitFor(&status, _currTask->getPid());
        setTracer(_threadStartTracer);

        // create the task structure for the thread
        _currTask->getregs(&regs);
        INFO("{}: init task structure {}", _currTask->getPid(),
             Log::parseRegs(regs));
        //debugTracer = true;
        initTaskStructure();
        //debugTracer = false;
        INFO("{}: after task structrue", _currTask->getPid());

        // your life just got a lot more like hell (!)
        if (_activeTracers.size() > 0) {
          enableSingleStep(regs);
        }
        _currTask->setregs(&regs);
        STEP();
      }

      else if (_forkExitTracer->hasRegion(signalInst)) {
        ERROR("{}: thread trappin' on fork_exit @ {:x}", _currTask->getPid(), signalInst);

        // step around the breakpoint
        regs.eip = signalInst;
        _currTask->setregs(&regs);
        unsetTracer(_forkExitTracer);
        _currTask->step();
        g->waitFor(&status, _currTask->getPid());
        setTracer(_forkExitTracer);

        _currTask->getregs(&regs);
        // will only operate if the task is not yet init'd
        initTaskStructure();
        _currTask->setregs(&regs);
        STEP();
        // g->stopLoadTime();
      }

      else if (_exitTracer->hasRegion(signalInst)) {
        ERROR("{}: HIT exit!!", _currTask->getPid());

        // step around the breakpoint
        regs.eip = signalInst;
        _currTask->setregs(&regs);
        unsetTracer(_exitTracer); // not sure.....?
        _currTask->step();
        g->waitFor(&status, _currTask->getPid());
        setTracer(_exitTracer); // do I need to reset this shit...?

        _currTask->getregs(&regs);

        // close the tracer library:
        closeTracerLibs();
        STEP();
        _currTask->setregs(&regs);
        // g->stopLoadTime();
      }

      // A tracer was reached
      else if (iter != _instrPoints.end()) {
        auto &ip = iter->second;

        assert (!ip->isJump()); /// we've massively messed up otherwise
        g->addBreakpointCount();

        ip->addCount();
        if (!(ip->getCount() % 10000)) {
          NDEBUG("{} has been called {} times", ip->toString(), ip->getCount());
        }

        /*        if (g->getPthreadLogClock() < 14768) {
          ERROR("clock {} @ bp {:x} mem {}",
                //_currTask->getPid(),
                g->getPthreadLogClock(),
                //                g->getBreakpointCount(),
                (unsigned)signalInst,
                Log::parseRemote(_currTask, 0xb46c7098 - 0x24, 0x4));
        }
        */
        STEP();
        instPointJmp(ip, regs);
        //set the registers (back?)
      }

      else {
        // we cannot execute tracers on other thread groups.. but that's a weird semantic anyway
        // Only consider this thread groups active tracers here:
        //ERROR("trace: {} {} {} {}", _currTask->getPid(), Log::parseRegs(regs), getInteresting(_currTask), Log::parseFPU(fp));
        // this is what you will toggle!
        //if (false) {
        if (_activeTracers.size() > 0) {
          // disable single-stepping through the gadget itself:
          disableSingleStep(regs);
          _currTask->setregs(&regs);

          auto it = _activeTracers.begin();
          while (it != _activeTracers.end()) {
            callCTracer(*it, regs, CSTEP);
            ++it;
          }
            enableSingleStep(regs);
          _currTask->setregs(&regs);
          _currTask->setfpu(&fpu);
        }
        // this can happen because of having multiple threads and system calls and whatnot
        else {
          disableSingleStep(regs);
          _currTask->setregs(&regs);
        }
      }
      break;
    }
    case SIGSTOP:
      INFO("pid {} sigstop @  clock {}. Regs {}?",
           caughtPid, g->getPthreadLogClock(), Log::parseRegs(regs));
      break;
    case SIGILL:
      ERROR("{}:{} regs {}", _currTask->getPid(), g->getPthreadLogClock(), Log::parseRegs(regs));
      ERROR("Inst={}", Log::parseInstruction(_currTask, regs.eip));
      ERROR("Might be the program, might be our fault, but unhandlable segfault");
      _currTask->debugAllRegions(); // seems... problematic?
      return TGStatus::DEAD; // this *really* should be a dead. but, we can close the pipe, so I made it dying.?

    default:
      INFO("pid {} caught because of sig {} @ clock {}",
             caughtPid,  signalRecd, g->getPthreadLogClock());
      signalToSend = signalRecd;

      break;
  }
  // NDEBUG("done, returning execution back to {}", Log::parseRegs(regs));

  _currTask = NULL;

  return tgstatus == TGStatus::DYING ? tgstatus :
      _activeTracers.size() > 0 ? TGStatus::STEP : TGStatus::ALIVE;
}
}
