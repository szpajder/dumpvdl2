export DEBUG ?= 0
CC = gcc
AR = ar
RANLIB = ranlib
CFLAGS += -std=c11 -g -Wall -O3 -fno-omit-frame-pointer -DEMIT_ASN_DEBUG=$(DEBUG) -I. -D_XOPEN_SOURCE=600
include Makefile.inc

LIB=asn1.a
OBJS=${ASN_MODULE_SOURCES:.c=.o}

all: $(LIB)

$(LIB): $(OBJS)
	$(AR) rv $@ $^
	$(RANLIB) $@

clean:
	rm -f *.o $(LIB)

srcclean:
	rm -f *.c *.h Makefile.am.sample

regen:
	asn1c -fcompound-names -gen-PER atn-cm.asn1 atn-cpdlc.asn1 atn-ulcs.asn1
# disable missing SET_OF_encode_uper function
	patch -p0 < patches/disable_missing_set_of_encode_uper.diff
# enable printing of CHOICE names by asn_print()
	patch -p0 < patches/print_choice_names.diff
# silence compiler warnings on missing _DEFAULT_SOURCE
	patch -p0 < patches/asn_system_h_default_source.diff
# make asn_fprint accept indentation level as a parameter
	patch -p0 < patches/asn_fprint_parameterized_indentation.diff
# expose _fetch_present_idx from constr_CHOICE.c
	patch -p0 < patches/CHOICE_expose__fetch_present_idx.diff
