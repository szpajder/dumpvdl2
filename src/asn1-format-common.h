/*
 *  This file is a part of dumpvdl2
 *
 *  Copyright (c) 2017-2018 Tomasz Lemiech <szpajder@gmail.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <stdio.h>			// FILE
#include "asn1/asn_application.h"	// asn_TYPE_descriptor_t
#include "asn1-util.h"			// ASN1_FORMATTER_PROTOTYPE

char const *value2enum(asn_TYPE_descriptor_t *td, long const value);
void _format_INTEGER_with_unit(FILE *stream, char const * const label, asn_TYPE_descriptor_t *td,
	void const *sptr, int indent, char const * const unit, double multiplier, int decimal_places);
void _format_CHOICE(FILE *stream, char const * const label, dict const * const choice_labels,
	asn1_output_fun_t cb, asn_TYPE_descriptor_t *td, void const *sptr, int indent);
void _format_SEQUENCE(FILE *stream, char const * const label, asn1_output_fun_t cb,
	asn_TYPE_descriptor_t *td, void const *sptr, int indent);
void _format_SEQUENCE_OF(FILE *stream, char const * const label, asn1_output_fun_t cb,
	asn_TYPE_descriptor_t *td, void const *sptr, int indent);
ASN1_FORMATTER_PROTOTYPE(asn1_format_any);
ASN1_FORMATTER_PROTOTYPE(asn1_format_NULL);
ASN1_FORMATTER_PROTOTYPE(asn1_format_ENUM);
ASN1_FORMATTER_PROTOTYPE(asn1_format_Deg);
