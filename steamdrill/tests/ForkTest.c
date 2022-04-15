#include <stdio.h>
#include <unistd.h>
#include <asm/prctl.h>
#include <sys/prctl.h>



void doprint(const char* msg) {
  printf("%d: %s", getpid(), msg);
}

int main() {

  printf ("Hello World\n");
  if (fork() == 0) {
    doprint ("childy\n");
    return 1;
  }
  doprint("parenty\n");
  return 0;
}
