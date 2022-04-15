#include "PtraceLibrary.h"

void mallocStart(long ignore_rtn, int size)
{
    int rc = tracerLog("ms, %d, %lx, %x\n", ptraceGetpid(), ignore_rtn, size);
    if (rc < 0)
    {
	ptraceFprintf(1, "huh, tl failed? %d\n", rc);
    }
}

void mallocEnd(void *addr)
{
    int rc = tracerLog("me, %d, %p\n", ptraceGetpid(), addr);
    if (rc < 0)
    {
	ptraceFprintf(1, "huh, tl failed? %d\n", rc);
    }
}

void freeStart(long ignore_rtn, void *addr)
{
    int rc = tracerLog("fs, %d, %lx, %p\n", ptraceGetpid(), ignore_rtn, addr);
    if (rc < 0)
    {
	ptraceFprintf(1, "huh, tl failed? %d\n", rc);
    }
}
