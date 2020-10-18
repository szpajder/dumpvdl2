/*
 *  This file is a part of dumpvdl2
 *
 *  Copyright (c) 2017-2020 Tomasz Lemiech <szpajder@gmail.com>
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
#include <stdio.h>                  // FILE
#include <libacars/vstring.h>       // la_vstring
#include "asn1/asn_application.h"   // asn_TYPE_descriptor_t
#include "asn1-util.h"              // ASN1_FORMATTER_FUN_T, asn1_formatter_param_t
#include "dumpvdl2.h"               // dict

char const *value2enum(asn_TYPE_descriptor_t *td, long value);

// Text formatters
void format_INTEGER_with_unit_as_text(asn1_formatter_param_t p,
		char const *unit, double multiplier, int decimal_places);
void format_INTEGER_as_ENUM_as_text(asn1_formatter_param_t p, dict const *value_labels);
void format_CHOICE_as_text(asn1_formatter_param_t p, dict const *choice_labels,
		asn1_formatter_fun_t cb);
void format_SEQUENCE_as_text(asn1_formatter_param_t p, asn1_formatter_fun_t cb);
void format_SEQUENCE_OF_as_text(asn1_formatter_param_t p, asn1_formatter_fun_t cb);
void format_BIT_STRING_as_text(asn1_formatter_param_t p, dict const *bit_labels);
ASN1_FORMATTER_FUN_T(asn1_format_any_as_text);
ASN1_FORMATTER_FUN_T(asn1_format_label_only_as_text);
ASN1_FORMATTER_FUN_T(asn1_format_ENUM_as_text);

// JSON formatters
void format_INTEGER_with_unit_as_json(asn1_formatter_param_t p,
		char const *unit, double multiplier);
void format_INTEGER_as_ENUM_as_json(asn1_formatter_param_t p, dict const *value_labels);
void format_CHOICE_as_json(asn1_formatter_param_t p, dict const *choice_labels,
		asn1_formatter_fun_t cb);
void format_SEQUENCE_as_json(asn1_formatter_param_t p, asn1_formatter_fun_t cb);
void format_SEQUENCE_OF_as_json(asn1_formatter_param_t p, asn1_formatter_fun_t cb);
void format_BIT_STRING_as_json(asn1_formatter_param_t p, dict const *bit_labels);
ASN1_FORMATTER_FUN_T(asn1_format_any_as_string_as_json);
ASN1_FORMATTER_FUN_T(asn1_format_long_as_json);
ASN1_FORMATTER_FUN_T(asn1_format_bool_as_json);
ASN1_FORMATTER_FUN_T(asn1_format_label_only_as_json);
ASN1_FORMATTER_FUN_T(asn1_format_ENUM_as_json);
ASN1_FORMATTER_FUN_T(asn1_format_OCTET_STRING_as_json);
