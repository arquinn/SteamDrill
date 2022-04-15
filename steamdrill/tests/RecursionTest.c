#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

int global = 12;

int recurse(int i) {

  if (i == 0)  {
    return i;
  }

  return recurse(i - 1) + i;
}

int main()
{
  recurse(5);
  return 0;
}
