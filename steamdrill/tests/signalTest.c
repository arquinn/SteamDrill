#define _GNU_SOURCE

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ucontext.h>
#include <unistd.h>

#include <sys/mman.h>


void sigact_handler(int number, siginfo_t *si, void* v)
{
  ucontext_t *u = (ucontext_t *)v;

  if (number == SIGSEGV) {
    printf("received SIGSEGV at %x on %p\n",
           u->uc_mcontext.gregs[REG_EIP], si->si_addr);
    exit (-1);
  }
}

int main(int argc, char **argv) {
  struct sigaction sa;

  sa.sa_flags = SA_SIGINFO;
  sigemptyset(&sa.sa_mask);
  sa.sa_sigaction = sigact_handler;

  if (sigaction(SIGSEGV, &sa, NULL) < 0)
    printf("cannot catch sigsegv's...?");

  // dereference garbage:
  *(u_long*)(0x1000) = 7;

  printf("hello world\n");
  return 0;
}
