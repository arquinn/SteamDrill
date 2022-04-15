#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

int global = 12;

int loopIter(int i)
{
  char *blah = (char *)malloc(0x10);
  sprintf(blah, "%d", i);
  free(blah);
  printf("loopIter %d, %s (%p)\n", i, blah, (void *)blah);

  return i;
}

int main()
{
  int v;
  for (v = 0; v < 10; v++) {
    loopIter(v);
    global += v;
  }
  return 0;
}
