#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "idfg.h"

int main(int argc, char** argv)
{
	global_sample_rate = 48000;
	int s = song();
	printf("song -> %d (argc=%d) -> %d smps\n", s, argc, relpos(0));
	return EXIT_SUCCESS;
}
