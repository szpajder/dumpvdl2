CC = gcc
# TODO: -O3, -ffast-math?
CFLAGS = -std=c11 -g -Wall -march=native -DDEBUG=1
LDFLAGS = -lfec -lm -lrtlsdr
.PHONY = all clean

all: rtlvdl2

rtlvdl2: crc.o decode.o bitstream.o deinterleave.o rs.o avlc.o output.o rtlvdl2.o

decode.o: rtlvdl2.h

bitstream.o: rtlvdl2.h

deinterleave.o: rtlvdl2.h

rs.o: rtlvdl2.h

rtlvdl2.o: rtlvdl2.h

avlc.o: rtlvdl2.h avlc.h

output.o: avlc.h

clean:
	rm -f *.o rtlvdl2
