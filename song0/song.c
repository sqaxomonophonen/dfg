#include "dfg.h"

BUS song(void)
{
	set_bpm(120);
	const int n_beats = 4*4;
	for (int beat = 0; beat < n_beats; beat++) {
		printf("beat %d -> %d\n", beat, relpos(0));
		adv(1);
	}
	//adv(n_beats);

	BUS freq = constant(0.1);
	BUS x1 = constant(0);
	BUS x2 = constant(0);
	BUS x3 = constant(0);
	BUS x4 = constant(0);
	BUS hw = hexwave(freq,x1,x2,x3,x4);
	return hw;
}
