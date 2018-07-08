CC=gcc
AR=ar
RANLIB=ranlib
LIBS=   decode_rs_char.o \
	init_rs_char.o \

CFLAGS += -g -O3 -fno-omit-frame-pointer -fPIC -I. -Wall
LDFLAGS=

LIB=libfec.a

all: $(LIB)

libfec.a: $(LIBS)
	$(AR) rv $@ $^
	$(RANLIB) libfec.a

decode_rs_char.o: decode_rs_char.c char.h rs-common.h

init_rs_char.o: init_rs_char.c char.h rs-common.h

clean:
	rm -f *.o $(LIB)

