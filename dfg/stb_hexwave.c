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

	struct cur c_freq        = bufcur1(&ctx->in[0]);
	struct cur c_reflect     = bufcur1(&ctx->in[1]);
	struct cur c_peak_time   = bufcur1(&ctx->in[2]);
	struct cur c_half_height = bufcur1(&ctx->in[3]);
	struct cur c_zero_wait   = bufcur1(&ctx->in[4]);

	int remaining = ctx->n_frames;

	assert((sizeof(*ctx->out.data) == sizeof(float)) && "hexwave uses float");
	assert(ctx->out.width == 1);

	HexWaveParameters* last_params = &hexwav->last_params;
	HexWave* hw = &hexwav->hw;
	int todo = 0;
	float f0 = 0;
	while (remaining > 0) {
		int update = 0;
		float lastfreq = 0;
		while (todo < remaining) {
			HexWaveParameters params = {
				.reflect = cur1read(&c_reflect) != 0,
				.peak_time = cur1read(&c_peak_time),
				.half_height = cur1read(&c_half_height),
				.zero_wait = cur1read(&c_zero_wait),
			};

			if (memcmp(&params, last_params, sizeof params) != 0) {
				memcpy(last_params, &params, sizeof params);
				hexwave_change(
					hw,
					params.reflect,
					params.peak_time,
					params.half_height,
					params.zero_wait);
				update = 1;
			}

			SIGNAL freq = cur1read(&c_freq);
			if (todo == 0) {
				f0 = freq;
			} else if (freq != f0) {
				update = 1;
			}
			lastfreq = freq;

			if (update) break;
			todo++;
		}

		hexwave_generate_samples(
			ctx->out.data,
			todo,
			hw,
			f0);
		remaining -= todo;
		f0 = lastfreq;
		todo = 1;
	}
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
