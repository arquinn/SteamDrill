#include <stdio.h>
#include <stdlib.h>

#define LABEL_SYMBOL(name) asm volatile ("labelsym_" name ": .global labelsym_" name);

int main()
{
  int i;
  // dummy loop:
  LABEL_SYMBOL("before_loop")
  for (i = 0; i < 1000000; ++i)
  {
    rand();
  }

  LABEL_SYMBOL("after_loop")

  return 0;
}
