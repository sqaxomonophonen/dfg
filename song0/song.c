#include "dfg.h"

BUS song(void)
{
	set_bpm(120);
	const int n_beats = 4*4;
	for (int beat = 0; beat < n_beats; beat++) {
		//printf("beat %d -> %.0f\n", beat, relpos(0));
		adv(1);
	}
	//adv(n_beats);

	BUS freq = constant(0.004);
	BUS reflect = constant(1);
	BUS peak_time = constant(0);
	BUS half_height = constant(0);
	BUS zero_wait = constant(0);
	BUS hw = hexwave(freq,reflect,peak_time,half_height,zero_wait);

	BUS a = mul(hw,constant(0.02));

	return a;
}
