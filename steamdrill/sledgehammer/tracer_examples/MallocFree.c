#include <string>

#include <stdio.h>
#include <unistd.h>

void mallocStart(long ignore_rtn, int size)
{
    fprintf(stderr, "ms, %d, %lx, %x\n", getpid(), ignore_rtn, size);
}

void mallocEnd(void *addr)
{
    fprintf(stderr, "me, %d, %p\n", getpid(), addr);
}

void freeStart(long ignore_rtn, void *addr)
{
    fprintf(stderr, "fs, %d, %lx, %p\n", getpid(), ignore_rtn, addr);
}


std::string rtnString()
{
    std::string myStr("test");

    return myStr;
}
