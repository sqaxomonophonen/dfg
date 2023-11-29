#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>
#include <stdarg.h>

#include "idfg.h"

#define PTHREADED

#ifdef PTHREADED
#include <pthread.h>
#endif

struct seq global_seq;
double global_sample_rate;
int global_buffer_length;

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
			opcode_fn fn;
			void* usr;
			int n_inputs;
			BUS* inputs;
			int n_dependencies;
			int n_dependees;
			BUS* yield_arr;
			int order;
		} opcode;
		struct {
			BUS bus;
			int offset;
		} slice;
		struct {
			SIGNAL value;
			int index;
		} constant;
	};
	int tag;
};

struct bus* busses;

static BUS new_bus(void)
{
	BUS i = arrlen(busses);
	struct bus* b = arraddnptr(busses, 1);
	memset(b, 0, sizeof *b);
	return i;
}

static inline struct bus* get_bus(BUS bi)
{
	assert((bi >= 0) && "null bus");
	assert((0 <= bi && bi < arrlen(busses)) && "bus out of range");
	return &busses[bi];
}

int bus_width(BUS i)
{
	return get_bus(i)->width;
}

BUS constant(SIGNAL v)
{
	BUS i = new_bus();
	struct bus* b = get_bus(i);
	b->width = 1;
	b->type = BUS_CONSTANT;
	b->constant.value = v;
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

BUS opcode_arr(int output_width, opcode_fn fn, void* usr, int n_inputs, BUS* inputs)
{
	BUS bi = new_bus();
	struct bus* b = get_bus(bi);
	b->width = output_width;
	b->type = BUS_OPCODE;
	b->opcode.fn = fn;
	b->opcode.usr = usr;
	b->opcode.n_inputs = n_inputs;
	b->opcode.inputs = calloc(n_inputs, sizeof *inputs);
	memcpy(b->opcode.inputs, inputs, n_inputs * sizeof(*inputs));
	for (int ii = 0; ii < n_inputs; ii++) {
		struct bus* inp = get_bus(inputs[ii]);
		for (;;) {
			if (inp->type == BUS_SLICE) {
				// follow slice chain
				inp = get_bus(inp->slice.bus);
				continue;
			} else if (inp->type == BUS_CONSTANT) {
				// constants are not dependencies
			} else if (inp->type == BUS_OPCODE) {
				int found = 0;
				const int n = arrlen(inp->opcode.yield_arr);
				for (int i = 0; i < n; i++) {
					if (inp->opcode.yield_arr[i] == bi) {
						found = 1;
						break;
					}
				}
				if (!found) {
					arrput(inp->opcode.yield_arr, bi);
					b->opcode.n_dependencies++;
					inp->opcode.n_dependees++;
					arrput(b->opcode.yield_arr, ii);
				}
			} else {
				assert(!"unhandled type");
			}
			break;
		}
	}
	return bi;
}

BUS opcode(int output_width, opcode_fn fn, void* usr, ...)
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
		for (int i = 0; i < n_frames; i++) {
			const SIGNAL t = t0+(SIGNAL)i;
			curw(&out)[0] =
				x0->c0         +
				x0->c1 * t     +
				x0->c2 * t*t   +
				x0->c3 * t*t*t ;
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

static void trace_bus(BUS bi)
{
	struct bus* b = get_bus(bi);
	assert((b->tag == 0) && "already traced/tagged");
	b->tag = 1;
	if (b->type == BUS_SLICE) {
		trace_bus(b->slice.bus);
	} else if (b->type == BUS_CONSTANT) {
		// ok
	} else if (b->type == BUS_OPCODE) {
		const int n = b->opcode.n_inputs;
		for (int i = 0; i < n; i++) {
			trace_bus(b->opcode.inputs[i]);
		}
	} else {
		assert(!"unhandled type");
	}
}

struct todo {
	int iteration;
	int index;
};

static int todo_compar(const void* va, const void* vb)
{
	const struct todo* a = va;
	const struct todo* b = vb;
	const int d0 = b->iteration - a->iteration;
	if (d0 != 0) return d0;
	const int d1 = b->index - a->index;
	return d1;
}

struct exec {
	opcode_fn fn;
	struct opcode_context context;
	#ifdef PTHREADED
	pthread_mutex_t counter_mutex;
	#endif
	int counter;
	int counter_reset;
	int n_yields;
	int* yields;
	int iteration;
	int is_main_output;
};

struct gig {
	int n_frames;
	struct exec* execs;
	#ifdef PTHREADED
	pthread_mutex_t sync_mutex;
	pthread_cond_t sync_cond;
	#endif
	int n_execs_active;
	struct todo* todos;
	SIGNAL* constants;
	int n_channels;
	FILE* wav;
} gig;

static void buf_init(struct buf* buf, int width, int n_frames)
{
	memset(buf, 0, sizeof *buf);
	buf->width = width;
	buf->data = calloc(width*n_frames, sizeof *buf->data);
}


void band_bus(BUS bi)
{
	gig.n_channels = bus_width(bi);

	trace_bus(bi);

	const int n_busses = arrlen(busses);
	int n_orphans = 0;
	BUS* opcode_busses = NULL;
	for (BUS i = 0; i < n_busses; i++) {
		struct bus* bus = get_bus(i);
		if (!bus->tag) {
			n_orphans++;
			continue;
		}
		if (bus->type == BUS_OPCODE) {
			arrput(opcode_busses, i);
		} else if (bus->type == BUS_CONSTANT) {
			bus->constant.index = arrlen(gig.constants);
			arrput(gig.constants, bus->constant.value);
		}
	}
	if (n_orphans > 0) printf("[WARNING] oprhan bus count: %d\n", n_orphans);

	BUS* order = NULL;
	arrsetlen(gig.todos, 0);
	for (int i = 0; i < arrlen(opcode_busses); i++) {
		BUS oi = opcode_busses[i];
		struct bus* bus = get_bus(oi);
		assert(bus->type == BUS_OPCODE);
		bus->tag = bus->opcode.n_dependencies;
		if (bus->opcode.n_dependencies == 0) {
			arrput(gig.todos, ((struct todo) { .index = arrlen(order) }));
			arrput(order, oi);
		}
	}

	int c = 0;
	while (c < arrlen(order)) {
		BUS oi = order[c++];
		struct bus* bus = get_bus(oi);
		assert(bus->type == BUS_OPCODE);
		for (int i = 0; i < arrlen(bus->opcode.yield_arr); i++) {
			BUS yi = bus->opcode.yield_arr[i];
			if (yi < oi) continue;
			struct bus* yb = get_bus(yi);
			assert(yb->tag > 0);
			yb->tag--;
			if (yb->tag == 0) {
				arrput(order, yi);
			}
		}
	}

	const int n_execs = arrlen(order);
	assert(c == n_execs);
	assert(n_execs == arrlen(opcode_busses));

	for (int i0 = 0; i0 < n_execs; i0++) {
		struct bus* bus = get_bus(order[i0]);
		assert(bus->type == BUS_OPCODE);
		bus->opcode.order = i0;
	}

	#ifdef PTHREADED
	pthread_mutex_init(&gig.sync_mutex, NULL);
	pthread_cond_init(&gig.sync_cond, NULL);
	#endif
	arrsetlen(gig.execs, n_execs);

	for (int i0 = 0; i0 < n_execs; i0++) {
		struct bus* bus = get_bus(order[i0]);
		struct exec* x = &gig.execs[i0];
		memset(x, 0, sizeof *x);
		x->is_main_output = order[i0] == bi;
		x->fn = bus->opcode.fn;
		x->context.usr = bus->opcode.usr;
		buf_init(&x->context.out, bus->width, global_buffer_length);
		const int n_inputs = bus->opcode.n_inputs;
		x->context.n_in = n_inputs;
		x->context.in = calloc(n_inputs, sizeof *x->context.in);
		for (int i1 = 0; i1 < n_inputs; i1++) {
			struct bus* bo = get_bus(bus->opcode.inputs[i1]);
			int offset = 0;
			struct buf inbuf = {
				.width = bo->width,
			};
			for (;;) {
				if (bo->type == BUS_CONSTANT) {
					assert((offset == 0) && "cannot slice constant (width=1)");
					assert((inbuf.width == 1) && "what's going on");
					assert((bo->width == 1) && "dude no");
					inbuf.data = &gig.constants[bo->constant.index];
					inbuf.stride = -1;
					break;
				} else if (bo->type == BUS_OPCODE) {
					struct exec* xo = &gig.execs[bo->opcode.order];
					struct buf* bfo = &xo->context.out;
					assert(bfo->data != NULL);
					inbuf.data = bfo->data + offset;
					inbuf.stride = bfo->width - inbuf.width;
					break;
				} else if (bo->type == BUS_SLICE) {
					const int w = bo->width;
					const int o = bo->slice.offset;
					offset += o;
					bo = get_bus(bo->slice.bus);
					assert(bo->width >= (o+w));

				} else {
					assert(!"unhandled type");
				}
			}
			x->context.in[i1] = inbuf;
		}

		#ifdef PTHREADED
		pthread_mutex_init(&x->counter_mutex, NULL);
		#endif
		x->counter = bus->opcode.n_dependencies;
		x->counter_reset = x->counter + bus->opcode.n_dependees;
		x->n_yields = arrlen(bus->opcode.yield_arr);
		x->yields = calloc(x->n_yields, sizeof *x->yields);
		for (int i1 = 0; i1 < x->n_yields; i1++) {
			x->yields[i1] = get_bus(bus->opcode.yield_arr[i1])->opcode.order;
		}
	}
}

static inline void wav__u8(FILE* file, uint8_t v)
{
	assert(fputc(v, file) == v);
}

static inline void wav__u16(FILE* file, uint16_t v)
{
	for (int shift = 0; shift < 16; shift += 8) wav__u8(file, (v >> shift) & 0xff);
}

static inline void wav__i16(FILE* file, int16_t v)
{
	wav__u16(file, v);
}

static inline void wav__u32(FILE* file, uint32_t v)
{
	for (int shift = 0; shift < 32; shift += 16) wav__u16(file, (v >> shift) & 0xffff);
}

static inline void wav__str(FILE* file, const char* str)
{
	size_t n = strlen(str);
	assert(fwrite(str, sizeof *str, n, file) == n);
}

static inline void wav_header(FILE* file, int sample_rate, int n_channels, int n_frames)
{
	wav__str(file, "RIFF");
	const int data_length = 2 * n_channels * n_frames;
	wav__u32(file, data_length + 36);
	wav__str(file, "WAVEfmt ");
	wav__u32(file, 16);
	wav__u16(file, 1);
	wav__u16(file, n_channels);
	wav__u32(file, sample_rate);
	wav__u32(file, sample_rate * n_channels * 2);
	wav__u16(file, n_channels * 2);
	wav__u16(file, 16); // bits per sample
	wav__str(file, "data");
	wav__u32(file, data_length);

}

static inline FILE* wav_begin(const char* path, int sample_rate, int n_channels, int n_frames)
{
	FILE* file = fopen(path, "wb+");
	wav_header(file, sample_rate, n_channels, n_frames);
	return file;
}

static inline void wav_end(FILE* file)
{
	fclose(file);
}

/* emit a single sample (NOTE: 1 mono frame contains 1 sample; 1 stereo frame
 * contains 2 samples) */
static inline void wav_sample(FILE* file, float sample)
{
	wav__i16(file, (int16_t)(fminf(1.0f, fmaxf(-1.0f, sample)) * 32767.0f));
}

static inline void wav_samples(FILE* file, float* samples, int n_samples)
{
	for (int i = 0; i < n_samples; i++) wav_sample(file, samples[i]);
}

static void write_output(struct buf* buf, int n_frames)
{
	if (gig.wav != NULL) {
		struct cur cur = bufcur(buf);
		const int w = buf->width;
		for (int i0 = 0; i0 < n_frames; i0++) {
			SIGNAL* x = curw(&cur);
			for (int i1 = 0; i1 < w; i1++) {
				wav_sample(gig.wav, x[i1]);
			}
		}
	}
}

static void work_it_out(void)
{
	#ifdef PTHREADED
	pthread_mutex_lock(&gig.sync_mutex);
	#endif
	for (;;) {
		struct todo top;
		if (gig.n_execs_active == 0) {
			#ifdef PTHREADED
			pthread_mutex_unlock(&gig.sync_mutex);
			#endif
			break;
		}
		int n = arrlen(gig.todos);
		if (n == 0) {
			#ifdef PTHREADED
			struct timespec ts;
			clock_gettime(CLOCK_REALTIME, &ts);
			ts.tv_nsec += 50 * 1000000;
			if (ts.tv_nsec >= 1000000000) {
				ts.tv_nsec -= 1000000000;
				ts.tv_sec++;
			}
			pthread_cond_timedwait(&gig.sync_cond, &gig.sync_mutex, &ts);
			continue;
			#else
			assert(!"UNREACHABLE");
			#endif
		}

		assert(n > 0);
		top = gig.todos[n-1];
		arrsetlen(gig.todos, n-1);
		#ifdef PTHREADED
		pthread_mutex_unlock(&gig.sync_mutex);
		#endif

		assert(0 <= top.index && top.index < arrlen(gig.execs));
		struct exec* x = &gig.execs[top.index];
		assert(x->counter == 0);

		int n_frames = global_buffer_length;
		const int last_frame = (x->iteration+1) * global_buffer_length;
		int is_last = 0;
		if (last_frame >= gig.n_frames) {
			n_frames = gig.n_frames - x->iteration * global_buffer_length;
			is_last = 1;
		}

		assert(n_frames > 0);

		x->iteration++;
		x->counter = x->counter_reset;
		x->context.n_frames = n_frames;

		x->fn(&x->context);
		if (x->is_main_output) {
			write_output(&x->context.out, n_frames);
		}

		int n_new_todos = 0;
		struct todo new_todos[1<<10];
		for (int i = 0; i < x->n_yields; i++) {
			int y = x->yields[i];
			if (is_last && y < top.index) {
				// this is the last execution; don't retrigger earlier
				// opcodes
				continue;
			}
			struct exec* o = &gig.execs[y];
			#ifdef PTHREADED
			pthread_mutex_lock(&o->counter_mutex);
			#endif
			assert(o->counter > 0);
			o->counter--;
			if (o->counter == 0) {
				assert(n_new_todos < ARRAY_LENGTH(new_todos));
				new_todos[n_new_todos++] = (struct todo){
					.iteration = o->iteration,
					.index = y,
				};
			}
			#ifdef PTHREADED
			pthread_mutex_unlock(&o->counter_mutex);
			#endif
		}

		#ifdef PTHREADED
		pthread_mutex_lock(&gig.sync_mutex);
		#endif
		if (n_new_todos > 0 || is_last) {
			if (n_new_todos > 0) {
				struct todo* dst = arraddnptr(gig.todos, n_new_todos);
				memcpy(dst, new_todos, n_new_todos * sizeof(*dst));
				qsort(gig.todos, arrlen(gig.todos), sizeof(*gig.todos), todo_compar);
				#ifdef PTHREADED
				if (n_new_todos >= 2) {
					pthread_cond_broadcast(&gig.sync_cond);
				} else {
					pthread_cond_signal(&gig.sync_cond);
				}
				#endif
			}
			if (is_last) {
				assert(gig.n_execs_active > 0);
				gig.n_execs_active--;
			}
		}
	}
}

#ifdef PTHREADED
static void* worker_thread(void* _arg)
{
	work_it_out();
	return NULL;
}
#endif

void render(int n_frames, int n_threads)
{
	gig.n_frames = n_frames;
	gig.n_execs_active = arrlen(gig.execs);
	gig.wav = wav_begin("out.wav", global_sample_rate, gig.n_channels, n_frames);

	#ifdef PTHREADED
	const int n_spawn = n_threads - 1;
	pthread_t* threads = calloc(n_spawn, sizeof *threads);
	for (int i = 0; i < n_spawn; i++) {
		assert(0 == pthread_create(&threads[i], NULL, worker_thread, NULL));
	}
	#endif

	work_it_out();

	#ifdef PTHREADED
	for (int i = 0; i < n_spawn; i++) {
		assert(0 == pthread_join(threads[i], NULL));
	}
	#endif

	wav_end(gig.wav);
}
