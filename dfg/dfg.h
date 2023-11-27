#ifndef DFG_H

#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>

#include "stb_ds.h"

typedef int BUS;
typedef BUS B;
#define NILBUS ((BUS)-1)

typedef float SIGNAL;

#define ARRAY_LENGTH(xs) (sizeof(xs)/sizeof((xs)[0]))

void argscan(const char* fmt, ...);

struct buf {
	SIGNAL* data;
	int n;
	int width;
	int width_stride;
	int index_stride;
};

struct cur {
	SIGNAL* p;
	int inc;
};

static inline struct cur bufcur1(struct buf* buf)
{
	assert(buf->width == 1);
	return (struct cur) {
		.p = buf->data,
		.inc = buf->width_stride + buf->index_stride,
	};
}

static inline SIGNAL cur1read(struct cur* cur)
{
	SIGNAL v = *(cur->p);
	cur->p += cur->inc;
	return v;
}

struct op_context {
	struct buf* in;
	struct buf out;
	void* usr;
	int n_in;
};
typedef void (*op_process)(struct op_context*);

BUS opcode_arr(int output_width, op_process, void* usr, int n_inputs, BUS* inputs);
BUS opcode(int output_width, op_process, void* usr, ...);

BUS song(void); // XXX call something else?

BUS constant(SIGNAL v);
BUS add(BUS b0, BUS b1);
//BUS sub(BUS b0, BUS b1);
//BUS mul(BUS b0, BUS b1);

BUS hexwave(BUS freq, BUS reflect, BUS peak_time, BUS half_height, BUS zero_wait);

int bus_width(BUS bus);
BUS bus_slice(BUS bus, int offset, int width);
//BUS bus_concat(BUS b0, BUS b1);

static inline BUS B1(BUS b) { assert(bus_width(b) == 1); return b; }



#define SEQ_SECBITS_LOG2 (48) // allows up to 1<<(64-48)=65536 seconds of output
#define SEQ_SEC2STEP(s) round(s * (double)(1LL << SEQ_SECBITS_LOG2))
#define SEQ_SECS(acc) (((double)(acc)) * (1.0 / (double)(1LL << SEQ_SECBITS_LOG2)))

struct seq {
	int64_t acc;
	int64_t step;
};

extern struct seq global_seq;
extern double     global_sample_rate;

static inline void seq_set_bpm(struct seq* seq, double beats_per_minute)
{
	const double seconds_per_beat = 60.0 / beats_per_minute;
	seq->step = SEQ_SEC2STEP(seconds_per_beat);
}

static inline void seq_adv(struct seq* seq, int n_beats)
{
	seq->acc += seq->step * n_beats;
}

static inline void seq_warp(struct seq* seq, double n_seconds)
{
	seq->acc += SEQ_SEC2STEP(n_seconds);
}

static inline int seq_relpos(struct seq* seq, double sample_rate, double beats)
{
	const int64_t a = round((double)seq->step * beats);
	return sample_rate * SEQ_SECS(seq->acc + a);
}

static inline void set_bpm(double beats_per_minute)
{
	seq_set_bpm(&global_seq, beats_per_minute);
}

static inline void adv(int n_beats)
{
	seq_adv(&global_seq, n_beats);
}

static inline void warp(double n_seconds)
{
	seq_warp(&global_seq, n_seconds);
}

static inline int relpos(double beats)
{
	return seq_relpos(&global_seq, global_sample_rate, beats);
}

struct curve_point {
	int p;
	SIGNAL c0, c1, c2, c3;
};

static inline void curve_add(struct curve_point** xs, struct curve_point x)
{
	int left = 0;
	int right = arrlen(*xs);
	while (left < right) {
		const int mid = (left + right) >> 1;
		if ((*xs)[mid].p < x.p) {
			left = mid + 1;
		} else {
			right = mid;
		}
	}
	arrsetlen(*xs, left);
	arrput(*xs, x);
}

static inline void curve_add0(struct curve_point** xs, int p, SIGNAL c0)
{
	curve_add(xs, ((struct curve_point) {
		.p = p,
		.c0 = c0,
	}));
}

static inline void curve_add1(struct curve_point** xs, int p, SIGNAL c0, SIGNAL c1)
{
	curve_add(xs, ((struct curve_point) {
		.p = p,
		.c0 = c0,
		.c1 = c1,
	}));
}

static inline void curve_add2(struct curve_point** xs, int p, SIGNAL c0, SIGNAL c1, SIGNAL c2)
{
	curve_add(xs, ((struct curve_point) {
		.p = p,
		.c0 = c0,
		.c1 = c1,
		.c2 = c2,
	}));
}

static inline void curve_add3(struct curve_point** xs, int p, SIGNAL c0, SIGNAL c1, SIGNAL c2, SIGNAL c3)
{
	curve_add(xs, ((struct curve_point) {
		.p = p,
		.c0 = c0,
		.c1 = c1,
		.c2 = c2,
		.c3 = c3,
	}));
}

BUS curvegen(struct curve_point* xs);


#define DFG_H
#endif
