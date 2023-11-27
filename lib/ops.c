#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "idfg.h"

struct seq global_seq;
double global_sample_rate;

enum bus_type {
	BUS_OPCODE = 1,
	BUS_SLICE,
	BUS_CONSTANT,
};

struct bus {
	int width;
	enum bus_type type;
	union {
		struct {
			int n_inputs;
			BUS* inputs;
			struct op_context context;
		} opcode;
		struct {
			BUS bus;
			int offset;
		} slice;
		SIGNAL constant;
	};
};

struct bus* busses;

static BUS new_bus(void)
{
	BUS bus = arrlen(busses);
	arrput(busses, ((struct bus){0}));
	return bus;
}

#define ASSERT_VALID_BUS(_b) { \
	assert((_b >= 0) && "null bus"); \
	assert((0 <= _b && _b < arrlen(busses)) && "bus out of range"); \
}

static inline struct bus* get_bus(BUS i)
{
	ASSERT_VALID_BUS(i)
	return &busses[i];
}

int bus_width(BUS i)
{
	return get_bus(i)->width;
}

#if 0
BUS op_new(
	int n_in,
	int out_width,
	op_process process_fn,
	void* usr,
	struct op_bus* in_bus)
{
	struct op_context* ctx = calloc(1, sizeof *ctx);
	ctx->usr = usr;
	ctx->n_in = n_in;
	ctx->n_out = n_out;
	ctx->in = calloc(n_in, sizeof *ctx->in);
	for (int i = 0; i < n_in; i++) {
		struct op_bus* b = &in_bus[i];
		if (b->assert_width != 0){
			assert((bus_width(b->bus) == b->assert_width) && "unexpected bus width");
		}
	}

	//ctx->out = calloc(n_out, sizeof *ctx->out);
	//BUS out = bus_new(n_out);
	//for (int i = 0; i < n_out; i++) {
	//}
	return out;
}
#endif


BUS constant(SIGNAL v)
{
	BUS i = new_bus();
	struct bus* b = get_bus(i);
	b->width = 1;
	b->type = BUS_CONSTANT;
	b->constant = v;
	return i;
}

BUS bus_slice(BUS i0, int offset, int width)
{
	assert(offset >= 0);
	assert(width > 0);
	struct bus* b0 = get_bus(i0);
	assert(offset+width <= b0->width);
	BUS i1 = new_bus();
	struct bus* b1 = get_bus(i1);
	b1->width = width;
	b1->type = BUS_SLICE;
	b1->slice.bus = i0;
	b1->slice.offset = offset;
	return i1;
}

BUS opcode_arr(int output_width, op_process fn, void* usr, int n_inputs, BUS* inputs)
{
	BUS i = new_bus();
	struct bus* b = get_bus(i);
	b->width = output_width;
	b->type = BUS_OPCODE;
	b->opcode.n_inputs = n_inputs;
	b->opcode.inputs = calloc(n_inputs, sizeof *inputs);
	memcpy(b->opcode.inputs, inputs, n_inputs * sizeof(*inputs));
	// XXX TODO allocate buffer/context? execution plan?
	return i;
}

#include <stdarg.h>
BUS opcode(int output_width, op_process fn, void* usr, ...)
{
	BUS busses[1<<10];
	va_list ap;
	va_start(ap, usr);
	BUS* wp = busses;
	for (;;) {
		BUS bus_index = va_arg(ap, BUS);
		if (bus_index == NILBUS) break;
		assert(bus_index >= 0);
		assert((wp - busses) < ARRAY_LENGTH(busses));
		*(wp++) = bus_index;
	}
	va_end(ap);
	return opcode_arr(output_width, fn, usr, wp - busses, busses);
}

static void add_process(struct op_context* ctx)
{
	assert(!"TODO");
}

BUS add(BUS b0, BUS b1)
{
	assert((bus_width(b0) == bus_width(b1)) && "cannot add busses of different widths");
	//int* wp = calloc(1, sizeof *wp);
	//*wp = bus_width(b0);
	//return op_new(2, 1, add_process, wp,
	const int w = bus_width(b0);
	return opcode(w, add_process, NULL, b0, b1, NILBUS);
}

struct curvegen_state {
	int n;
	struct curve_point* xs;
};

static void curvegen_process(struct op_context* ctx)
{
	assert(!"TODO");
}

BUS curvegen(struct curve_point* xs)
{
	struct curvegen_state* cs = calloc(1, sizeof *cs);
	const int n = arrlen(xs);
	cs->n = n;
	const size_t sz = sizeof(*cs->xs);
	cs->xs = calloc(n, sz);
	memcpy(cs->xs, xs, n * sz);
	return opcode(1, curvegen_process, cs, NILBUS);
}
