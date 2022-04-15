#include <stddef.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

int global = 12;

int loopIter(int i)
{
  unsigned long blah = (unsigned long)malloc(0x10);
  printf("loopIter %d, %lx\n", i, blah);
  return i;
}

/*
int main()
{
  int v;
  char * unused = malloc(1);

  for (v = 0; v < 10; v++) {
    loopIter(v);
    global += v;
  }
  return 0;
}
*/
