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

void scope(BUS, const char* path);

BUS opcode_arr(int output_width, opcode_fn, void* usr, int n_inputs, BUS* inputs);
BUS opcode(int output_width, opcode_fn, void* usr, ...);

BUS song(void); // XXX call something else?

BUS constant(SIGNAL v);
BUS add(BUS,BUS);
BUS sub(BUS,BUS);
BUS mul(BUS,BUS);
BUS divide(BUS,BUS);
BUS vadd(BUS,...);
BUS msin(BUS);
BUS mcos(BUS);
BUS mpow(BUS,BUS);

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
extern SIGNAL     global_sample_rate;
extern SIGNAL     global_inverse_sample_rate;
extern int        global_buffer_length;

static inline void set_sample_rate(SIGNAL sample_rate)
{
	global_sample_rate = sample_rate;
	global_inverse_sample_rate = 1.0f / sample_rate;
}

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

typedef void (*curvefn)(SIGNAL* out, int n, SIGNAL x0, SIGNAL dx, SIGNAL c0, SIGNAL c1, SIGNAL c2, SIGNAL c3);

struct curve_segment {
	int p;
	curvefn fn;
	SIGNAL c0, c1, c2, c3;
};
typedef struct curve_segment* CURVE;

void curve_add_segment(CURVE* xs, struct curve_segment x);

static inline void curve_addfn(CURVE* xs, double pos, curvefn fn, SIGNAL c0, SIGNAL c1, SIGNAL c2, SIGNAL c3)
{
	curve_add_segment(xs, ((struct curve_segment) {
		.p = relpos(pos),
		.fn = fn,
		.c0 = c0,
		.c1 = c1,
		.c2 = c2,
		.c3 = c3,
	}));
}

void curve_ramp(CURVE* xs, double pos, double len, SIGNAL val0, SIGNAL val1);
static inline void curve_zramp(CURVE* xs, double pos, double len, SIGNAL val)
{
	curve_ramp(xs, pos, len, val, 0);
}

BUS curvegen(struct curve_segment* xs);

#define DEF_CURVEFN(NAME,EXPR) \
static void _curvefn_ ## NAME(SIGNAL* out, int n, SIGNAL x0, SIGNAL dx, SIGNAL c0, SIGNAL c1, SIGNAL c2, SIGNAL c3) \
{ \
	SIGNAL tmp0=0, tmp1=0; \
	(void)tmp0; (void)tmp1; \
	SIGNAL* wp = out; \
	SIGNAL fi = 0.0; \
	for (int i = 0; i < n; i++, fi+=1.0) { \
		const SIGNAL x = x0 + fi*dx; \
		(void)x; \
		*(wp++) = (EXPR); \
	} \
}

#define DEF_CURVE0(NAME,EXPR) \
DEF_CURVEFN(NAME,EXPR) \
void curve_ ## NAME(struct curve_segment** xs, double pos) { \
	curve_addfn(xs, pos, _curvefn_ ## NAME, 0, 0, 0, 0); \
}

#define DEF_CURVE1(NAME,EXPR) \
DEF_CURVEFN(NAME,EXPR) \
void curve_ ## NAME(struct curve_segment** xs, double pos, SIGNAL c0) { \
	curve_addfn(xs, pos, _curvefn_ ## NAME, c0, 0, 0, 0); \
}

#define DEF_CURVE2(NAME,EXPR) \
DEF_CURVEFN(NAME,EXPR) \
void curve_ ## NAME(struct curve_segment** xs, double pos, SIGNAL c0, SIGNAL c1) { \
	curve_addfn(xs, pos, _curvefn_ ## NAME, c0, c1, 0, 0); \
}

#define DEF_CURVE3(NAME,EXPR) \
DEF_CURVEFN(NAME,EXPR) \
void curve_ ## NAME(struct curve_segment** xs, double pos, SIGNAL c0, SIGNAL c1, SIGNAL c2) { \
	curve_addfn(xs, pos, _curvefn_ ## NAME, c0, c1, c2, 0); \
}

#define DEF_CURVE4(NAME,EXPR) \
DEF_CURVEFN(NAME,EXPR) \
void curve_ ## NAME(struct curve_segment** xs, double pos, SIGNAL c0, SIGNAL c1, SIGNAL c2, SIGNAL c3) { \
	curve_addfn(xs, pos, _curvefn_ ## NAME, c0, c1, c2, c3); \
}

#define DFG_H
#endif
