export DEBUG ?= 0
export USE_STATSD ?= 0
export WITH_RTLSDR ?= 1
export WITH_MIRISDR ?= 0
export WITH_SDRPLAY ?= 0
CC = gcc
CFLAGS += -std=c99 -g -Wall -O3 -fno-omit-frame-pointer -ffast-math -D_XOPEN_SOURCE=500 -DDEBUG=$(DEBUG)
DUMPVDL2_VERSION:=\"$(shell git describe --always --tags --dirty)\"
ifneq ($(DUMPVDL2_VERSION), \"\")
  CFLAGS+=-DDUMPVDL2_VERSION=$(DUMPVDL2_VERSION)
endif

CFLAGS += -Iasn1
CFLAGS += -DUSE_STATSD=$(USE_STATSD) -DWITH_RTLSDR=$(WITH_RTLSDR) -DWITH_SDRPLAY=$(WITH_SDRPLAY) -DWITH_MIRISDR=$(WITH_MIRISDR)
CFLAGS += `pkg-config --cflags glib-2.0`
LDLIBS = -lm
LDLIBS += `pkg-config --libs glib-2.0`
SUBDIRS = libfec asn1
CLEANDIRS = $(SUBDIRS:%=clean-%)
BIN = dumpvdl2
OBJ =	acars.o \
	avlc.o \
	bitstream.o \
	clnp.o \
	cotp.o \
	crc.o \
	decode.o \
	demod.o \
	esis.o \
	icao.o \
	idrp.o \
	output.o \
	rs.o \
	dumpvdl2.o \
	tlv.o \
	x25.o \
	xid.o \
	util.o

FEC = libfec/libfec.a
ASN1 = asn1/asn1.a

DEPS = $(OBJ) $(FEC) $(ASN1)
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
ifeq ($(WITH_SDRPLAY), 1)
  DEPS += sdrplay.o
  LDLIBS += -lmirsdrapi-rsp
endif

.PHONY: all clean $(SUBDIRS) $(CLEANDIRS)

all: $(BIN)

$(BIN): $(DEPS)

$(FEC): libfec ;

$(ASN1): asn1 ;

clnp.o: dumpvdl2.h clnp.h idrp.h cotp.h

cotp.o: dumpvdl2.h tlv.h cotp.h icao.h

decode.o: dumpvdl2.h

demod.o: dumpvdl2.h

bitstream.o: dumpvdl2.h

esis.o: dumpvdl2.h esis.h tlv.h

icao.o: dumpvdl2.h icao.h

idrp.o: dumpvdl2.h idrp.h tlv.h

rs.o: dumpvdl2.h fec.h

dumpvdl2.o: dumpvdl2.h rtl.h mirisdr.h

avlc.o: dumpvdl2.h avlc.h xid.h acars.h x25.h

acars.o: dumpvdl2.h acars.h

mirisdr.o: dumpvdl2.h mirisdr.h

sdrplay.o: dumpvdl2.h sdrplay.h

output.o: dumpvdl2.h

rtl.o: dumpvdl2.h rtl.h

statsd.o: dumpvdl2.h

tlv.o: tlv.h dumpvdl2.h

util.o: dumpvdl2.h tlv.h

xid.o: dumpvdl2.h tlv.h xid.h

x25.o: dumpvdl2.h clnp.h esis.h tlv.h x25.h

$(SUBDIRS):
	$(MAKE) -C $@

$(CLEANDIRS):
	$(MAKE) -C $(@:clean-%=%) clean

clean: $(CLEANDIRS)
	rm -f *.o $(BIN)
