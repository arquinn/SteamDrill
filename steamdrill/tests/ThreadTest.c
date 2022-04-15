#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>


void* startThread (void * parm)
{
  printf("new thread!");
  sleep (3);
}

int main()
{
  pthread_t thread;
  void * retval;

  pthread_create( &thread, NULL, &startThread, NULL);


  sleep (3);
  pthread_join(thread, &retval);


  return 0;
}
