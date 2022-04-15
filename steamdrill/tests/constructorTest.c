#include <stdio.h>

#include "soTest.h"

__attribute__((constructor))
static void mainC()
{
  printf("main constructor\n");
}

int main()
{
  printf("in main");
  soFunction();
}
