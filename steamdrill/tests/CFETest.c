#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

int globalVar = 10;

int main(int argc, char **argv)
{
  int counter = 10;
  if (argc > 1)
    counter = atoi(argv[1]);
  printf("using %d counts\n", counter);

  int localVariable = 9, v;
  for (v = 0; v < counter; v++) {
    localVariable++;
    globalVar += 1;
  }

  printf("hello world! %d\n", localVariable);
  return 0;
}
