#include <sys/user.h>
#include <stdio.h>

#include "tracer_library.h"
void Dispatcher()
{
  struct user_regs_struct regs = tracerGetregs();
  tracerlogFprintf("%lx %lx %lx ",  regs.eax, regs.esp, regs.eip);
}


u_long fooParams(u_long esp)
{
  return *((u_long*)esp + 1);
}

/*
void fooParams(u_long esp)
{
  tracerlogFprintf("%lx ", *((u_long*)esp + 1));
}
*/
