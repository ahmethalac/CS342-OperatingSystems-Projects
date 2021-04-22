
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include "sbmem.h"
#include <math.h>

int main(int argc, char* argv[])
{
    int i, ret; 
    char *p;  	

    ret = sbmem_open(); 
    if (ret == -1)
	exit (1); 

    // Experiment
    srand(time(NULL));
    int size = rand() % 3968 + 128;
    FILE *output = fopen("output.txt", "a");
    fprintf(output, "ALlocating %d bytes\n------\n", size);

    p = sbmem_alloc (size); // allocate space to hold 1024 characters
    for (i = 0; i < 256; ++i)
	p[i] = 'a'; // init all chars to ‘a’

	if (argc > 1)
    sbmem_free (p);

    sbmem_close(); 
    
    return (0); 
}
