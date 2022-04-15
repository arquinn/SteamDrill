
#ifndef _POSIX_SOURCE
#define _POSIX_SOURCE
#endif /* _POSIX_SOURCE */

#include <signal.h>

#include <sys/ptrace.h>
#include <sys/reg.h>
#include <sys/user.h>
#include <sys/types.h>

#include <iostream>
#include <memory>
#include <vector>

using std::shared_ptr;

#ifndef __LOGGER_H_
#define __LOGGER_H_

#include "../log/log.hpp"
#include "globals.hpp"

//#define DEBUG_ALLOC(...) NDEBUG(__VA_ARGS__)
#define DEBUG_ALLOC(x,...)
#define STEP() if (::Log::doSteps) {std::cerr << "enter char to continue...\n"; getchar();};
#define THROW(x) ::Log::throwException(x);


// forward decl of this
namespace ThreadGroup {class Task;}

namespace Log{

using std::vector;

void logStack(int numArgs, pid_t pid);
void logLocalVars(int numVars, pid_t pid);

std::string parseRegs(struct user_regs_struct &regs);
std::string parseRegsExt(struct user_regs_struct &regs);
std::string parseFPU(struct user_fpxregs_struct &regs);
std::string parseLocal(const char *gadget, long gadgetLen);
std::string parseLocal(const shared_ptr<char> gadget, long gadgetLen);

std::string parseLocalPlain(const char *gadget, long gadgetLen);
std::string parseLocalPlain(const shared_ptr<char> gadget, long gadgetLen);
std::string parseStatus(int status, struct user_regs_struct *regs = nullptr);
std::string parseSiginfo(siginfo_t sig);
std::string parseInstruction(ThreadGroup::Task *task, long addr);

void logRemoteLocation(long location, long length, pid_t pid);
void logRemoteLocationPlain(long loc, long length, pid_t pid);


int debugAll(ThreadGroup::Task *task, uintptr_t end, vector<u_long> addrs = {}, int len = 0);
void stepAndCheck(ThreadGroup::Task *task, long addr = 0, int len = 0);
std::string parseRemote(ThreadGroup::Task *task, long addr, int len);
std::string parseRemotePlain(ThreadGroup::Task *task, long addr, int len);


void inline throwException(const std::string s) {
  Globals::Globals::getInstance()->cleanupOnError();
  throw std::logic_error(s);
}

// extern bool verbose;
 extern bool doSteps;
};

#endif /*__LOGGER_H_*/
