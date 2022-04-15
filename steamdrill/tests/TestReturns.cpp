#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <malloc.h>

#include <vector>

struct test {
  int one;
  int two;
};

int global = 12;
std::vector<int> globalv;

struct test loopIter(int i)
{
  struct test t;
  t.one = 1;
  t.two = 2;
  u_long blah = (u_long)malloc(0x10);
  printf("loopIter %d, %lx\n", i, blah);

  return t;
}

int main()
{
  int *bleh = new int;
  int *bleh2 = (int *)malloc(sizeof(int));
  int v;
  for (v = 0; v < 10; v++) {
    globalv.push_back(v);
    struct test t = loopIter(v);
    printf("test: %d %p %p\n", t.one, bleh, bleh2);
    global += v;
  }
  return 0;
}
