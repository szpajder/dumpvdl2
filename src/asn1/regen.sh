#!/bin/bash
# This file is a part of dumpvdl2
#
# Copyright (c) 2018 Tomasz Lemiech <szpajder@gmail.com>

ASN_MODULES="atn-cm.asn1 atn-cpdlc.asn1 atn-ulcs.asn1"
set -e

rm -f *.c *.h CMakeLists.inc

asn1c -fcompound-names -gen-PER $ASN_MODULES

./am2cmake.pl Makefile.am.sample ASN_MODULE_SOURCES >CMakeLists.inc
rm -f Makefile.am.sample converter-sample.c
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
