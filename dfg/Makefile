CFLAGS+=-std=gnu11 -Wall
#CFLAGS+=-O0 -g
CFLAGS+=-O2

all: libdfg.a

O=\
	stb_ds.o stb_hexwave.o stb_image.o stb_image_write.o stb_perlin.o \
	lib.o ops.o

libdfg.a: ${O}
	ar rcs $@ $^

clean:
	rm -f *.o *.a
