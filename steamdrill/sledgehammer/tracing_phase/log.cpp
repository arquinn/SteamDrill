
#ifndef _POSIX_SOURCE
#define _POSIX_SOURCE
#endif 

#include <signal.h>

#include <sys/types.h>
#include <sys/ptrace.h>
#include <sys/reg.h>
#include <sys/user.h>

#include <string.h>

#include <sstream>
#include <iomanip>

#include "log.hpp"
#include "task.hpp"
#include "thread_group.hpp"
#include "globals.hpp"

bool Log::doSteps = false;

std::string Log::parseStatus(int status, user_regs_struct *regs) {
  std::stringstream ss;
  ss << status << " ";

  if (WIFEXITED(status))
    ss << "exited " << WEXITSTATUS(status);
  if (WIFSIGNALED(status))
    ss << "signaled " << strsignal(WTERMSIG(status));
  if (WIFSTOPPED(status)) {
    ss << "stopped " << WSTOPSIG(status) << " ";

    if (WSTOPSIG(status) & 0x80) {
      ss << strsignal(WSTOPSIG(status) & ~0x80) << " syscall";
      if (regs) {
        auto sp = ThreadGroup::Syscall::getSyscall(*regs);
        ss << " " << sp->syscallNum;
      }
    }
    else
      ss << strsignal(WSTOPSIG(status));
  }
  return ss.str();
}

std::string Log::parseLocal(const shared_ptr<char> gadget, long gadgetLen) {
  return parseLocal(gadget.get(), gadgetLen);
}
std::string Log::parseLocal(const char *gadget, long gadgetLen) {
  assert(gadget != nullptr);

  std::stringstream ss;
  for (int i = 0; i < gadgetLen; ++i) {
    ss << std::hex <<  std::setfill('0') << std::setw(2)
       << (u_int)(u_char)gadget[i] << "(" << gadget[i] << ") ";
  }
  return ss.str();
}
std::string Log::parseLocalPlain(const shared_ptr<char> gadget, long gadgetLen) {
  return parseLocalPlain(gadget.get(), gadgetLen);
}

std::string Log::parseLocalPlain(const char *gadget, long gadgetLen) {
  std::stringstream ss;
  ss << std::hex << std::setfill('0') << std::setw(2);
  for (int i = 0; i < gadgetLen; ++i) {
    ss << std::hex << (u_int)(u_char)gadget[i];
  }
  return ss.str();
}


std::string Log::parseRemote(ThreadGroup::Task *task, long addr, int len) {
  char *data;

  task->getdata(addr, len, &data);
  std::string rtnString = parseLocal(data, len);
  delete[] data;
  return rtnString;
}

std::string Log::parseRemotePlain(ThreadGroup::Task *task, long addr, int len) {
  char *data;

  task->getdata(addr, len, &data);
  std::string rtnString = parseLocalPlain(data, len);
  delete data;
  return rtnString;
}

std::string Log::parseInstruction(ThreadGroup::Task *task, long addr) {
  char rtnString[1024];
  shared_ptr<char> data;

  task->getdata(addr, 15, &data);
  char *blob = data.get();

  xed_decoded_inst_t xedd;
  xed_decoded_inst_zero(&xedd);
  xed_decoded_inst_set_mode(&xedd, XED_MACHINE_MODE_LEGACY_32, XED_ADDRESS_WIDTH_32b);
  xed_error_enum_t xed_error = xed_decode(&xedd,
                                          XED_STATIC_CAST(const xed_uint8_t*, blob),
                                          15);
  xed_decoded_inst_dump_xed_format(&xedd, rtnString, 1024, addr);

  return rtnString;
}



std::string Log::parseRegs(struct user_regs_struct &regs) {
  std::stringstream ss;
  ss << std::hex
     << "[eip]:"<< regs.eip
     << " [eax]:" << regs.eax
     << " [ebx]:" << regs.ebx
     << " [ecx]:" << regs.ecx
     << " [edx]:" << regs.edx
     << " [esi]:" << regs.esi
     << " [edi]:" << regs.edi
     << " [ebp]:" << regs.ebp
     << " [esp]:" << regs.esp
     << " [ss]:" << regs.xss;
  return ss.str();
}

std::string Log::parseSiginfo(siginfo_t sig) {
  std::stringstream ss;
  ss << "si_signo:" << sig.si_signo
     << " si_code:" << sig.si_code
     << "(aln:" << (sig.si_code == BUS_ADRALN)
     << " err:" << (sig.si_code == BUS_ADRERR)
     << " obj:" << (sig.si_code == BUS_OBJERR)
     << ")"
     << " si_errno:" << sig.si_errno
     << " si_pid:" << sig.si_pid
     << " si_uid:" << sig.si_uid
     << " si_addr:" << sig.si_addr
     << " si_status:" << sig.si_status
     << " si_band:" << sig.si_band;

  return ss.str();
}

std::string Log::parseRegsExt(struct user_regs_struct &regs) {
  std::stringstream ss;
  ss << std::hex
     << "[xgs]:"<< regs.xgs
     << " [eflags]:" << regs.eflags;
  return ss.str();
}

std::string Log::parseFPU(struct user_fpxregs_struct &regs) {
  std::stringstream ss;
  ss << std::hex
     << "[xmmo]:" << regs.xmm_space[0] << regs.xmm_space[1] << regs.xmm_space[2] << regs.xmm_space[3]
     << " [xmm1]: " << regs.xmm_space[4] << regs.xmm_space[5] << regs.xmm_space[6] << regs.xmm_space[7]
     << " [xmm2]: " << regs.xmm_space[8] << regs.xmm_space[9] << regs.xmm_space[10] << regs.xmm_space[11]
     << " [xmm3]: " << regs.xmm_space[12] << regs.xmm_space[13] << regs.xmm_space[14] << regs.xmm_space[15];
  return ss.str();
}





void Log::stepAndCheck(ThreadGroup::Task *task, long addr, int len) {
  int status;
  struct user_regs_struct regs;
  char nextChar = 0;
  Globals::Globals *g = Globals::Globals::getInstance();
  do {
    task->getregs(&regs);

    if (addr != 0) {
      NDEBUG("{} {}", parseRegs(regs), parseRemote(task, addr, len));
    }
    else {
      NDEBUG("{}", parseRegs(regs));
    }
    nextChar = getchar();
    task->step();
    g->wait(&status);
  }
  while (nextChar != 'e');
}

int Log::debugAll(ThreadGroup::Task *task, uintptr_t end, vector<u_long> addrs, int len ) {
  int status, oldEip = 0;
  struct user_regs_struct regs;
  Globals::Globals *g = Globals::Globals::getInstance();

  do {
    task->getregs(&regs);
    // THIS BREAKS ON REPT INSTRUCTIONS! (but there must be some problems it catches?)
    //if (oldEip == regs.eip) {
    //return 2;
    //}
    oldEip = regs.eip;
    if (regs.eip == end)
      return 0;

    std::stringstream stream;
    for (auto addr : addrs) {
      stream << std::hex << addr << ": " << parseRemote(task,addr,len);
    }

    stream << parseInstruction(task, regs.eip);
    //if (regs.edx)
    // stream << "[stack]: " << parseRemotePlain(task, regs.esp-0x4, 0x8); // here's to the onese that never remember
    //stream << " [BPPTR]:" << parseRemotePlain(task, 0xb73c8000 - 0x1c, 0x4);
    //stream << " [WPPTR]:" << parseRemotePlain(task, 0xb73c8000 - 0x18, 0x4);
    //stream << " [BFPTR]:" << parseRemotePlain(task, 0xb771fd40 + 0x1a8, 0x4);
    // stream << "[edx]:" << parseRemote(task, 0xb7467008, 0x60);
    // stream << " [eax]:" << parseRemote(task, 0xb77b7d9c, 0x60);
    INFO("{}: {} {}", g->getPthreadLogClock(), parseRegs(regs),  stream.str());

    task->step();
    g->waitFor(&status, task->getPid());
    //g->wait(&status);
  }
  while (WIFSTOPPED(status) &&
         (WSTOPSIG(status) != SIGSEGV && WSTOPSIG(status) != SIGBUS));
  return status;
}
