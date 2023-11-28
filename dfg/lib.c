#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "idfg.h"

int main(int argc, char** argv)
{
	global_sample_rate = 48000;
	BUS out = song();
	const int n_samples = ceil(relpos(0));
	printf("%d smps\n", n_samples);
	band_bus(out);
	return EXIT_SUCCESS;
}
