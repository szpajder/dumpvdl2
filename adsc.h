/*
 *  dumpvdl2 - a VDL Mode 2 message decoder and protocol analyzer
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
#include <stdint.h>	// uint8_t, uint32_t
#include <glib.h>	// gpointer

#define ADSC_CRC_LEN 2

#define CAST_PTR(x, t, y) t x = (t)(y)
#define ADSC_PARSER_PROTOTYPE(x) static int x(void *dest, uint8_t *buf, uint32_t len)
#define ADSC_FORMATTER_PROTOTYPE(x) static char *x(char const * const label, void const * const data)

typedef enum {
	ADSC_MSG_UNKNOWN = 0,
	ADSC_MSG_ADS,
	ADSC_MSG_DIS
} adsc_msgid_t;

typedef struct {
	char const * const label;
	int (*parse)(void *dest, uint8_t *buf, uint32_t len);
	char *(*format)(char const * const, void const * const);
	void (*destroy)(void *data);
} type_descriptor_t;

// ADS-C message
typedef struct {
	uint8_t err;
	adsc_msgid_t id;
	GSList *tag_list;
} adsc_msg_t;

// generic tag structure
typedef struct {
	uint8_t tag;
	type_descriptor_t *type;
	void *data;
} adsc_tag_t;

// Downlink tag structures

// negative acknowledgement (tag 4)
typedef struct {
	uint8_t contract_req_num;
	uint8_t reason;
	uint8_t ext_data;
} adsc_nack_t;
#define ADSC_NACK_MAX_REASON_CODE 13

// description of a single non-compliant message group contained in the above notification
typedef struct {
	uint8_t noncomp_tag;			// non-compliant tag value
	uint8_t is_unrecognized;		// 1 - group unrecognized, 0 - group unavailable
	uint8_t is_whole_group_unavail;		// 1 - entire group is unavailable;
						// 0 - one or more group params is unavailable
	uint8_t param_cnt;			// number of unavailable params
						// (used when is_whole_group_noncompliant==0)
	uint8_t params[15];			// a table of non-compliant parameter numbers
} adsc_noncomp_group_t;

// noncompliance notification (tag 5)
typedef struct {
	uint8_t contract_req_num;		// contract request number
	uint8_t group_cnt;			// number of non-compliant groups
	adsc_noncomp_group_t *groups;		// a table of non-compliant groups
} adsc_noncomp_notify_t;

// basic ADS group (downlink tags: 7, 9, 10, 18, 19, 20)
typedef struct {
	double lat, lon;
	double timestamp;
	int alt;
	uint8_t redundancy, accuracy, tcas_health;
} adsc_basic_report_t;

// flight ID group (tag 12)
typedef struct {
	char id[9];
} adsc_flight_id_t;

// predicted route group (tag 13)
typedef struct {
	double lat_next, lon_next;
	double lat_next_next, lon_next_next;
	int alt_next, alt_next_next;
	int eta_next;
} adsc_predicted_route_t;

// earth or air reference group (tags: 14, 15)
typedef struct {
	double heading;
	double speed;
	int vert_speed;
	uint8_t heading_invalid;
} adsc_earth_air_ref_t;

// meteorological group (tag 16)
typedef struct {
	double wind_speed;
	double wind_dir;
	double temp;
	uint8_t wind_dir_invalid;
} adsc_meteo_t;

// airframe ID group (tag 17)
typedef struct {
	uint8_t icao_hex[3];
} adsc_airframe_id_t;

// intermediate projected intent group (tag 22)
typedef struct {
	double distance;
	double track;
	int alt;
	int eta;
	uint8_t track_invalid;
} adsc_intermediate_projection_t;

// fixed projected intent group (tag 23)
typedef struct {
	double lat, lon;
	int alt;
	int eta;
} adsc_fixed_projection_t;

// Uplink tag structures

// periodic and event contract requests (tags: 7, 8, 9)
typedef struct {
	uint8_t contract_num;	// contract number
	GSList *req_tag_list;	// list of adsc_tag_t's describing requested report groups
} adsc_req_t;

// lateral deviation change (uplink tag 10)
typedef struct {
	double lat_dev_threshold;
} adsc_lat_dev_chg_event_t;

// reporting interval (uplink tag 11)
typedef struct {
	uint8_t scaling_factor;
	uint8_t rate;
} adsc_report_interval_req_t;

// vertical speed change threshold (uplink tag 18)
typedef struct {
	int vspd_threshold;
} adsc_vspd_chg_event_t;

// altitude range change event (uplink tag 19)
typedef struct {
	int ceiling_alt, floor_alt;
} adsc_alt_range_event_t;

// aircraft intent group (uplink tag 21)
typedef struct {
	uint8_t modulus;
	uint8_t acft_intent_projection_time;
} adsc_acft_intent_group_req_t;

// adsc.h
adsc_msg_t *adsc_parse_msg(adsc_msgid_t msgid, uint8_t *buf, uint32_t len, uint32_t *msg_type);
void adsc_output_msg(adsc_msg_t *msg);
