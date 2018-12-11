export DEBUG ?= 0
export USE_STATSD ?= 0
export WITH_RTLSDR ?= 1
export WITH_MIRISDR ?= 0
export WITH_SDRPLAY ?= 0
export WITH_SOAPYSDR ?= 0
PREFIX = /usr/local
BINDIR = $(PREFIX)/bin
INSTALL_USER = root
INSTALL_GROUP = root
CC = gcc
CFLAGS += -std=c11 -g -Wall -O3 -fno-omit-frame-pointer -ffast-math -pthread -D_XOPEN_SOURCE=600 -D_FILE_OFFSET_BITS=64 -DDEBUG=$(DEBUG)

ifeq ($(PLATFORM), rpiv1)
  CFLAGS += -march=armv6zk -mtune=arm1176jzf-s -mfpu=vfp -mfloat-abi=hard
else ifeq ($(PLATFORM), rpiv2)
  CFLAGS += -march=armv7-a -mtune=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard
else ifeq ($(PLATFORM), rpiv3)
  CFLAGS += -march=armv8-a -mtune=cortex-a53 -mfpu=neon-vfpv4 -mfloat-abi=hard
endif

DUMPVDL2_VERSION:=\"$(shell git describe --always --tags --dirty)\"
ifneq ($(DUMPVDL2_VERSION), \"\")
  CFLAGS+=-DDUMPVDL2_VERSION=$(DUMPVDL2_VERSION)
endif

GLIBERROR = 0
GLIBCFLAGS:=$(shell pkg-config --cflags glib-2.0)
GLIBLDLIBS:=$(shell pkg-config --libs glib-2.0)
ifeq ($(strip $(GLIBCFLAGS)),)
  GLIBERROR = 1
else ifeq ($(strip $(GLIBLDLIBS)),)
  GLIBERROR = 1
endif

LIBACARSERROR = 0
LIBACARSCFLAGS:=$(shell pkg-config --cflags libacars)
LIBACARSLDLIBS:=$(shell pkg-config --libs libacars)
ifeq ($(strip $(LIBACARSCFLAGS)),)
  LIBACARSERROR = 1
else ifeq ($(strip $(LIBACARSLDLIBS)),)
  LIBACARSERROR = 1
endif

CFLAGS += -Iasn1 $(GLIBCFLAGS) $(LIBACARSCFLAGS)
CFLAGS += -DUSE_STATSD=$(USE_STATSD) -DWITH_RTLSDR=$(WITH_RTLSDR) -DWITH_SDRPLAY=$(WITH_SDRPLAY) -DWITH_MIRISDR=$(WITH_MIRISDR) -DWITH_SOAPYSDR=$(WITH_SOAPYSDR)
LDLIBS = -lm -lpthread $(GLIBLDLIBS) $(LIBACARSLDLIBS)
SUBDIRS = libfec asn1
CLEANDIRS = $(SUBDIRS:%=clean-%)
BIN = dumpvdl2
OBJ =	acars.o \
	asn1-format-common.o \
	asn1-format-icao.o \
	asn1-util.o \
	avlc.o \
	bitstream.o \
	chebyshev.o \
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

ifeq ($(WITH_SOAPYSDR), 1)
  DEPS += soapysdr.o
  LDLIBS += -lSoapySDR
endif

.PHONY: all clean install check_glib check_libacars $(SUBDIRS) $(CLEANDIRS)

all: check_glib check_libacars $(BIN)

$(BIN): $(DEPS)

$(FEC): libfec ;

$(ASN1): asn1 ;

check_glib:
	@if test $(GLIBERROR) -ne 0; then \
		printf "ERROR: failed to find glib package configuration with pkgconfig.\n"; \
		printf "Verify if pkgconfig and glib are installed correctly.\n"; \
		false; \
	fi;

check_libacars:
	@if test $(LIBACARSERROR) -ne 0; then \
		printf "ERROR: failed to find libacars library configuration with pkgconfig.\n"; \
		printf "Verify if pkgconfig and libacars are installed correctly.\n"; \
		false; \
	fi;

asn1-format-common.o: asn1-util.h tlv.h

asn1-format-icao.o: tlv.h dumpvdl2.h asn1-util.h asn1-format-common.h

asn1-util.o: dumpvdl2.h asn1-util.h

clnp.o: dumpvdl2.h clnp.h idrp.h cotp.h

cotp.o: dumpvdl2.h tlv.h cotp.h icao.h

decode.o: dumpvdl2.h avlc.h

demod.o: dumpvdl2.h chebyshev.h

bitstream.o: dumpvdl2.h

chebyshev.o: dumpvdl2.h chebyshev.h

esis.o: dumpvdl2.h esis.h tlv.h

icao.o: dumpvdl2.h icao.h asn1-util.h asn1-format-icao.h

idrp.o: dumpvdl2.h idrp.h tlv.h

rs.o: dumpvdl2.h fec.h

dumpvdl2.o: dumpvdl2.h avlc.h rtl.h mirisdr.h sdrplay.h soapysdr.h

avlc.o: dumpvdl2.h avlc.h xid.h acars.h x25.h

acars.o: dumpvdl2.h acars.h

mirisdr.o: dumpvdl2.h mirisdr.h

sdrplay.o: dumpvdl2.h sdrplay.h

soapysdr.o: dumpvdl2.h soapysdr.h

output.o: dumpvdl2.h

rtl.o: dumpvdl2.h rtl.h

statsd.o: dumpvdl2.h

tlv.o: tlv.h dumpvdl2.h

util.o: dumpvdl2.h tlv.h

xid.o: dumpvdl2.h tlv.h xid.h avlc.h

x25.o: dumpvdl2.h clnp.h esis.h tlv.h x25.h

$(SUBDIRS):
	$(MAKE) -C $@

$(CLEANDIRS):
	$(MAKE) -C $(@:clean-%=%) clean

clean: $(CLEANDIRS)
	rm -f *.o $(BIN)

install: $(BIN)
	install -d -o $(INSTALL_USER) -g $(INSTALL_GROUP) $(BINDIR)
	install -o $(INSTALL_USER) -g $(INSTALL_GROUP) -m 755 $(BIN) $(BINDIR)
