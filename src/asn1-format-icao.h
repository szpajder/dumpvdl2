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

#ifndef _ASN1_FORMAT_ICAO_H
#define _ASN1_FORMAT_ICAO_H

#include "asn1-util.h"                          // asn_formatter_t
#include "dumpvdl2.h"                           // dict

// asn1-format-icao-text.c
extern dict const Associate_result_labels[];
extern dict const Release_request_reason_labels[];
extern dict const Release_response_reason_labels[];
extern dict const ABRT_source_labels[];
extern dict const ATCUplinkMsgElementId_labels[];
extern dict const VerticalType_bit_labels[];
extern dict const ReportTypeNotSupported_bit_labels[];
extern dict const EPPLimitations_bit_labels[];
extern dict const EventTypeNotSupported_bit_labels[];
extern dict const EmergencyUrgencyStatus_bit_labels[];
extern dict const ATCDownlinkMsgElementId_labels[];
extern asn_formatter_t const asn1_icao_formatter_table_text[];
extern size_t asn1_icao_formatter_table_text_len;
extern asn_formatter_t const asn1_acse_formatter_table_text[];
extern size_t asn1_acse_formatter_table_text_len;

// asn1-format-icao-json.c
extern asn_formatter_t const asn1_icao_formatter_table_json[];
extern size_t asn1_icao_formatter_table_json_len;
extern asn_formatter_t const asn1_acse_formatter_table_json[];
extern size_t asn1_acse_formatter_table_json_len;

#endif // !_ASN1_FORMAT_ICAO_H
