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
	int width;
	int stride;
};

struct cur {
	SIGNAL* p;
	int inc;
};

static inline struct cur bufcur(struct buf* buf)
{
	return (struct cur) {
		.p = buf->data,
		.inc = buf->width + buf->stride,
	};
}

static inline struct cur bufcurw(int width, struct buf* buf)
{
	assert(buf->width == width);
	return bufcur(buf);
}

static inline struct cur bufcur1(struct buf* buf)
{
	return bufcurw(1, buf);
}

static inline SIGNAL* curw(struct cur* cur)
{
	SIGNAL* p = cur->p;
	cur->p += cur->inc;
	return p;
}

static inline SIGNAL cur1read(struct cur* cur)
{
	return curw(cur)[0];
}

struct opcode_context {
	struct buf* in;
	struct buf out;
	void* usr;
	int n_in;
	int n_frames;
};
typedef void (*opcode_fn)(struct opcode_context*);

BUS opcode_arr(int output_width, opcode_fn, void* usr, int n_inputs, BUS* inputs);
BUS opcode(int output_width, opcode_fn, void* usr, ...);

BUS song(void); // XXX call something else?

BUS constant(SIGNAL v);
BUS add(BUS b0, BUS b1);
BUS sub(BUS b0, BUS b1);
BUS mul(BUS b0, BUS b1);
BUS divide(BUS b0, BUS b1);
BUS vadd(BUS b0, ...);

BUS hexwave(BUS freq, BUS reflect, BUS peak_time, BUS half_height, BUS zero_wait);

int bus_width(BUS bus);
BUS bus_slice(BUS bus, int offset, int width);
//BUS bus_concat(BUS b0, BUS b1);

static inline BUS B1(BUS b) { assert(bus_width(b) == 1); return b; }

struct seq {
	double position;
	double seconds_per_beat;
};

extern struct seq global_seq;
extern double     global_sample_rate;
extern int        global_buffer_length;

static inline void seq_set_bpm(struct seq* seq, double beats_per_minute)
{
	seq->seconds_per_beat = 60.0 / beats_per_minute;
}

static inline void seq_warp(struct seq* seq, double n_seconds)
{
	seq->position += n_seconds;
}

static inline void seq_adv(struct seq* seq, double n_beats)
{
	seq_warp(seq, seq->seconds_per_beat * n_beats);
}

static inline double seq_beatlen(struct seq* seq, double sample_rate, double beats)
{
	return sample_rate * seq->seconds_per_beat * beats;
}

static inline double seq_relpos(struct seq* seq, double sample_rate, double beats)
{
	return sample_rate * (seq->position + seq->seconds_per_beat * beats);
}

static inline void set_bpm(double beats_per_minute)
{
	seq_set_bpm(&global_seq, beats_per_minute);
}

static inline void adv(double n_beats)
{
	seq_adv(&global_seq, n_beats);
}

static inline void warp(double n_seconds)
{
	seq_warp(&global_seq, n_seconds);
}

static inline double beatlen(double beats)
{
	return seq_beatlen(&global_seq, global_sample_rate, beats);
}

static inline double relpos(double beats)
{
	return seq_relpos(&global_seq, global_sample_rate, beats);
}

struct curve_segment {
	int p;
	SIGNAL c0, c1, c2, c3;
};

typedef struct curve_segment* CURVE;

static inline void curve_add_segment(struct curve_segment** xs, struct curve_segment x)
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

static inline void curve_addb(struct curve_segment** xs, double pos, double c0, double c1, double c2, double c3)
{
	const double s = 1.0 / beatlen(1.0);
	curve_add_segment(xs, ((struct curve_segment) {
		.p = relpos(pos),
		.c0 = c0,
		.c1 = c1*s,
		.c2 = c2*s*s,
		.c3 = c3*s*s*s,
	}));
}

static inline void curve_add0(struct curve_segment** xs, double pos, double c0)
{
	curve_addb(xs, pos, c0, 0, 0, 0);
}

static inline void curve_add1(struct curve_segment** xs, double pos, double c0, double c1)
{
	curve_addb(xs, pos, c0, c1, 0, 0);
}

static inline void curve_add2(struct curve_segment** xs, double pos, double c0, double c1, double c2)
{
	curve_addb(xs, pos, c0, c1, c2, 0);
}

static inline void curve_add3(struct curve_segment** xs, double pos, double c0, double c1, double c2, double c3)
{
	curve_addb(xs, pos, c0, c1, c2, c3);
}

BUS curvegen(struct curve_segment* xs);


#define DFG_H
#endif
