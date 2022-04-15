#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <sys/types.h>

//return the square of a number

int max(int *a, int size) 
{
    int i, max = -1;
    for (i = 1; i < size; ++i) 
    {
	if (max < a[i])
	{
	    max = a[i];
	}
    }
    return max;
}

int main() { 

    int array[] = {10,2,3,4,5};
    int add = 12;
    int i, m = -1;

    
    int a = 6, b = 12, c; 
    u_int d = a;  
    //alu instructions
    c = a + b; 
    c = a - b; 
    c = a / b;
    c = a * b;

    c = a << 12; 
    d <<= 12;


    //off by one error
    for (i = 1; i < 5; ++i) 
    {
	array[i] = array[i] + add;
    }
    m = max(array, 5);
    
    printf("so long, and thanks for all the fish!\n");
    return -1;
}
