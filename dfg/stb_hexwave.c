#include <assert.h>
#include <string.h>

#define STB_HEXWAVE_IMPLEMENTATION
#include "stb_hexwave.h"

#include "idfg.h"

struct hexwav {
	HexWave hw;
	HexWaveParameters last_params;
};


static void process(struct opcode_context* ctx)
{
	struct hexwav* hexwav = ctx->usr;
	HexWaveParameters* last_params = &hexwav->last_params;
	HexWave* hw = &hexwav->hw;

	struct cur c_freq        = bufcur1(&ctx->in[0]);
	struct cur c_reflect     = bufcur1(&ctx->in[1]);
	struct cur c_peak_time   = bufcur1(&ctx->in[2]);
	struct cur c_half_height = bufcur1(&ctx->in[3]);
	struct cur c_zero_wait   = bufcur1(&ctx->in[4]);

	assert((sizeof(*ctx->out.data) == sizeof(float)) && "hexwave uses float");
	float* wp = ctx->out.data;
	assert(ctx->out.width == 1);

	const int n_frames = ctx->n_frames;
	SIGNAL f0 = 0;
	int i0 = 0;
	int n_total = 0;
	for (int i = 0; i < n_frames; i++) {
		const int is_first = (i == 0);
		const int is_last  = (i == (n_frames-1));

		SIGNAL freq = cur1read(&c_freq);
		HexWaveParameters params = {
			.reflect     = cur1read(&c_reflect) != 0,
			.peak_time   = cur1read(&c_peak_time),
			.half_height = cur1read(&c_half_height),
			.zero_wait   = cur1read(&c_zero_wait),
		};

		if (is_first) f0 = freq;

		int run = 0;
		if (memcmp(&params, last_params, sizeof params) != 0) {
			memcpy(last_params, &params, sizeof params);
			hexwave_change(
				hw,
				params.reflect,
				params.peak_time,
				params.half_height,
				params.zero_wait);
			run = 1;
		}
		if (freq != f0 || is_last) run = 1;

		if (run) {
			const int todo = (i-i0)+1;
			hexwave_generate_samples(wp, todo, hw, f0);
			wp += todo;
			f0 = freq;
			i0 = i+1;
			n_total += todo;
		}
	}
	assert(n_total == n_frames);
}

static int did_init;

BUS hexwave(BUS freq, BUS reflect, BUS peak_time, BUS half_height, BUS zero_wait)
{
	if (!did_init) {
		hexwave_init(32, 16, NULL);
		did_init = 1;
	}

	struct hexwav* h = calloc(1, sizeof *h);
	hexwave_create(&h->hw, 0, 0, 0, 0);
	return opcode(
		1, // output width
		process, h,
		B1(freq),
		B1(reflect),
		B1(peak_time),
		B1(half_height),
		B1(zero_wait),
		NILBUS
	);
}
