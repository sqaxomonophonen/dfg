#include "dfg.h"

BUS song(void)
{
	set_bpm(120);
	const int n_beats = 4*4;
	CURVE gain_curve = NULL;
	for (int beat = 0; beat < n_beats; beat++) {
		curve_add1(&gain_curve, 0, 1.0, -1.0);
		adv(1);
	}

	BUS freq = constant(0.004);
	BUS reflect = constant(1);
	BUS peak_time = constant(0);
	BUS half_height = constant(0);
	BUS zero_wait = constant(0);
	BUS hw = hexwave(freq,reflect,peak_time,half_height,zero_wait);
	BUS a = mul(hw,curvegen(gain_curve));
	a = mul(a,constant(0.04));

	return a;
}
