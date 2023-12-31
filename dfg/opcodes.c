#include <assert.h>
#include <stdarg.h>
#include <math.h>

#include "idfg.h"

#define DEF_X(NAME,EXPR) \
static void NAME ## _process(struct opcode_context* ctx) \
{ \
	const int w = ctx->out.width; \
	struct cur out = bufcurw(w, &ctx->out); \
	struct cur in = bufcurw(w, &ctx->in[0]); \
	const int n_frames = ctx->n_frames; \
	for (int i0 = 0; i0 < n_frames; i0++) { \
		SIGNAL* outp = curw(&out); \
		SIGNAL* inp = curw(&in); \
		for (int i1 = 0; i1 < w; i1++) { \
			const SIGNAL x = *(inp++); \
			const SIGNAL r = (EXPR); \
			*(outp++) = r; \
		} \
	} \
} \
  \
BUS NAME(BUS b) \
{ \
	return opcode(bus_width(b), NAME ## _process, NULL, b, NILBUS); \
}

#define DEF_XY(NAME,EXPR) \
static void NAME ## _process(struct opcode_context* ctx) \
{ \
	const int w = ctx->out.width; \
	struct cur out = bufcurw(w, &ctx->out); \
	struct cur in0 = bufcurw(w, &ctx->in[0]); \
	struct cur in1 = bufcurw(w, &ctx->in[1]); \
	const int n_frames = ctx->n_frames; \
	for (int i0 = 0; i0 < n_frames; i0++) { \
		SIGNAL* outp = curw(&out); \
		SIGNAL* in0p = curw(&in0); \
		SIGNAL* in1p = curw(&in1); \
		for (int i1 = 0; i1 < w; i1++) { \
			const SIGNAL x = *(in0p++); \
			const SIGNAL y = *(in1p++); \
			const SIGNAL r = (EXPR); \
			*(outp++) = r; \
		} \
	} \
} \
  \
BUS NAME(BUS b0, BUS b1) \
{ \
	assert((bus_width(b0) == bus_width(b1)) && "cannot " #NAME " busses of different widths"); \
	return opcode(bus_width(b0), NAME ## _process, NULL, b0, b1, NILBUS); \
}

DEF_XY(mul,    (x*y))
DEF_XY(add,    (x+y))
DEF_XY(sub,    (x-y))
DEF_XY(divide, (x/y))
DEF_X(msin,    sinf(x));
DEF_X(mcos,    cosf(x));
DEF_XY(mpow,   powf(x,y));

BUS vadd(BUS bus0, ...)
{
	va_list ap;
	va_start(ap, bus0);
	BUS bus = bus0;
	for (;;) {
		BUS busx = va_arg(ap, BUS);
		if (busx == NILBUS) break;
		assert(busx >= 0);
		bus = add(bus, busx);
	}
	va_end(ap);
	return bus;
}

#if 0
struct curvegen_state {
	int n;
	struct curve_segment* xs;
	int index;
	int position;
	int p0;
};

static void curvegen_process(struct opcode_context* ctx)
{
	struct curvegen_state* st = ctx->usr;
	struct cur out = bufcur1(&ctx->out);

	int remaining = ctx->n_frames;
	while (remaining > 0) {
		assert(0 <= st->index && st->index < st->n);
		struct curve_segment* x0 = &st->xs[st->index];
		struct curve_segment* x1 = (st->index+1) < st->n ? &st->xs[st->index+1] : NULL;
		int n_frames = remaining;
		const int do_incr = x1 != NULL && (st->position + n_frames) >= x1->p;
		if (do_incr) {
			n_frames = x1->p - st->position;
		}
		const SIGNAL t0 = st->position - st->p0;
		const SIGNAL c0 = x0->c0;
		const SIGNAL c1 = x0->c1;
		const SIGNAL c2 = x0->c2;
		const SIGNAL c3 = x0->c3;
		for (int i = 0; i < n_frames; i++) {
			const SIGNAL t = t0+(SIGNAL)i;
			curw(&out)[0] =
				c0         +
				c1 * t     +
				c2 * t*t   +
				c3 * t*t*t ;
		}

		st->position += n_frames;
		remaining -= n_frames;

		if (do_incr) {
			st->index++;
			st->p0 = st->position;
		}
	}
}

BUS curvegen(struct curve_segment* xs)
{
	struct curvegen_state* cs = calloc(1, sizeof *cs);
	const int n = arrlen(xs);
	cs->n = n;
	const size_t sz = sizeof(*cs->xs);
	cs->xs = calloc(n, sz);
	memcpy(cs->xs, xs, n * sz);
	return opcode(1, curvegen_process, cs, NILBUS);
}
#endif


void curve_add_segment(CURVE* xs, struct curve_segment x)
{
	// Maintain the curve using "musical drop sort":
	//  - Curve must consist of unique positions in ascending order.
	//  - Any segment in the curve with a position later or equal to the
	//    one being appended is removed first.
	const int n = arrlen(*xs);
	if (n > 0 && x.p <= (*xs)[n-1].p) {
		int left = 0;
		int right = n;
		while (left < right) {
			const int mid = (left + right) >> 1;
			if ((*xs)[mid].p < x.p) {
				left = mid + 1;
			} else {
				right = mid;
			}
		}
		arrsetlen(*xs, left);
	}
	arrput(*xs, x);
}


void curve_ramp(CURVE* xs, double pos, double len, SIGNAL val0, SIGNAL val1)
{
}

#if 0
static inline void curve_ramp(struct curve_segment** xs, double pos, double len, double val0, double val1)
{
	if (len <= 0) return;
	curve_add1(xs, pos, val0, (val1-val0) / len);
	curve_add0(xs, pos+len, val1);
}

#endif

