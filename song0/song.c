#include "dfg.h"


DEF_CURVE1(dingdong, 420+x+c0)

BUS song(void)
{
	set_bpm(125);

	const char* seq = "3.33.3.3.3.33.3.3.33.3.3.3.33.3.3.33.3.3.3.33.3.4.44.4.5.5.55.5.1.11.1.1.1.11.1.1.11.1.1.1.11.1.1.11.1.1.1.11.1.2.22.2.2.2.22.2.";




	const int n_beats = 4*4;
	CURVE gain_curve = NULL;
	CURVE freq_curve = NULL;
	for (int repeat = 0; repeat < 4; repeat++) {
		for (int beat = 0; beat < n_beats; beat++) {
			#if 0
			curve_add1(&gain_curve, 0,
				(beat&3) == 0 ? 1.0 : 0.5,
				(beat&3) == 0 ? -1.0*4 : -0.5*4);
			curve_add0(&freq_curve, 0, (beat&3) == 0 ? (repeat >= 2 ? 180 : 200) : 600);
			curve_zramp(&gain_curve, 0, (beat&3) == 0 ? 1.0 : 0.5,

				(beat&3) == 0 ? -1.0*4 : -0.5*4);
			#endif

			curve_dingdong(&gain_curve, 0, 42);

			adv(0.25);
		}
	}

	BUS freq = curvegen(freq_curve);
	scope(freq, "freq.wav");
	BUS reflect = constant(1);
	BUS peak_time = constant(0);
	BUS half_height = constant(0);
	BUS zero_wait = constant(0);
	BUS hw = hexwave(freq,reflect,peak_time,half_height,zero_wait);
	BUS cu = curvegen(gain_curve);
	cu = mul(mul(cu,cu), cu);
	scope(cu, "curve.wav");
	BUS a = mul(hw,cu);
	a = mul(a,constant(0.5));

	return a;
}
