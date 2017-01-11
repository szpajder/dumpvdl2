export DEBUG ?= 1
export USE_STATSD ?= 0
CC = gcc
# TODO: -O3, -ffast-math?
CFLAGS = -std=c99 -g -Wall -D_XOPEN_SOURCE=500 -DDEBUG=$(DEBUG) -DUSE_STATSD=$(USE_STATSD)
LDLIBS = -lfec -lm -lrtlsdr
BIN = rtlvdl2
DEPS = crc.o decode.o bitstream.o deinterleave.o rs.o avlc.o xid.o acars.o x25.o output.o util.o tlv.o rtlvdl2.o

ifeq ($(USE_STATSD), 1)
  DEPS += statsd.o
  LDLIBS += -lstatsdclient
endif

.PHONY = all clean

all: $(BIN)

$(BIN): $(DEPS)

decode.o: rtlvdl2.h

bitstream.o: rtlvdl2.h

deinterleave.o: rtlvdl2.h

rs.o: rtlvdl2.h

rtlvdl2.o: rtlvdl2.h

avlc.o: rtlvdl2.h avlc.h

acars.o: rtlvdl2.h acars.h

output.o: avlc.h acars.h

tlv.o: tlv.h rtlvdl2.h

xid.o: rtlvdl2.h tlv.h

x25.o: rtlvdl2.h tlv.h x25.h

clean:
	rm -f *.o $(BIN)
