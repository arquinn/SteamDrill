#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

int global = 12;

int loopIter(int i)
{
  u_long blah = (u_long)malloc(0x10);
  printf("loopIter %d, %lx\n", i, blah);
  return i;
}

int main()
{
  int v;
  for (v = 0; v < 1000000; v++) {
    loopIter(v);
    global += v;
  }
  return 0;
}
