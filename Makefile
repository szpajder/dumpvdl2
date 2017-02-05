export DEBUG ?= 1
export USE_STATSD ?= 0
export WITH_RTLSDR ?= 1
export WITH_MIRISDR ?= 0
CC = gcc
CFLAGS = -std=c99 -g -Wall -O3 -ffast-math -D_XOPEN_SOURCE=500 -DDEBUG=$(DEBUG)
CFLAGS += -DUSE_STATSD=$(USE_STATSD) -DWITH_RTLSDR=$(WITH_RTLSDR) -DWITH_MIRISDR=$(WITH_MIRISDR)
LDLIBS = -lfec -lm
LDFLAGS = -Llibfec
SUBDIRS = libfec
CLEANDIRS = $(SUBDIRS:%=clean-%)
BIN = dumpvdl2
OBJ =	acars.o \
	avlc.o \
	bitstream.o \
	clnp.o \
	crc.o \
	decode.o \
	demod.o \
	esis.o \
	idrp.o \
	output.o \
	rs.o \
	dumpvdl2.o \
	tlv.o \
	x25.o \
	xid.o \
	util.o

FEC = libfec/libfec.a
DEPS = $(OBJ) $(FEC)
ifeq ($(USE_STATSD), 1)
  DEPS += statsd.o
  LDLIBS += -lstatsdclient
endif
ifeq ($(WITH_RTLSDR), 1)
  DEPS += rtl.o
  LDLIBS += -lrtlsdr
endif
ifeq ($(WITH_MIRISDR), 1)
  DEPS += mirisdr.o
  LDLIBS += -lmirisdr
endif

.PHONY: all clean $(SUBDIRS) $(CLEANDIRS)

all: $(BIN)

$(BIN): $(DEPS)

$(FEC): libfec ;

clnp.o: dumpvdl2.h clnp.h idrp.h

decode.o: dumpvdl2.h

demod.o: dumpvdl2.h

bitstream.o: dumpvdl2.h

esis.o: dumpvdl2.h esis.h tlv.h

idrp.o: dumpvdl2.h idrp.h tlv.h

rs.o: dumpvdl2.h fec.h

dumpvdl2.o: dumpvdl2.h rtl.h mirisdr.h

avlc.o: dumpvdl2.h avlc.h

acars.o: dumpvdl2.h acars.h

mirisdr.o: dumpvdl2.h mirisdr.h

output.o: avlc.h acars.h

rtl.o: rtl.h

tlv.o: tlv.h dumpvdl2.h

xid.o: dumpvdl2.h tlv.h

x25.o: dumpvdl2.h clnp.h esis.h tlv.h x25.h

$(SUBDIRS):
	$(MAKE) -C $@

$(CLEANDIRS):
	$(MAKE) -C $(@:clean-%=%) clean

clean: $(CLEANDIRS)
	rm -f *.o $(BIN)
