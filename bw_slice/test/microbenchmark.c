#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

//#include <math.h>
#include <sys/types.h>

#define ROUNDS 10000000

int main()
{ 
    int rounds = ROUNDS;
    int a = 10, b = 20, tmp; 
    int i; 

    //copies. 
//    for (i = 0; i < rounds; ++i)
//    {
//	tmp = a;
//    }

    //merges. 

    for (i = 1; i < rounds + 1; ++i)
    {
	tmp += a;

    }
    
//    printf("so long, and thanks for all the fish!\n");
    return -1;
}
