#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>

#include "idfg.h"

int main(int argc, char** argv)
{
	global_buffer_length = 1<<14;
	global_sample_rate = 48000;
	BUS out = song();
	const int n_frames = ceil(relpos(0));
	band_bus(out);
	const int n_threads = sysconf(_SC_NPROCESSORS_ONLN);
	render(n_frames, n_threads);
	return EXIT_SUCCESS;
}
