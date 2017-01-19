#ifndef _TLV_H
#define _TLV_H 1
#include <stdint.h>
#define TLV_MIN_PARAMLEN 2	// type (1) + len (1) + zero-length value

typedef struct {
	void *next;
	uint8_t *val;
	uint16_t len;
	uint8_t type;
} tlv_list_t;

typedef struct {
	uint8_t id;
	void *val;
} dict;

typedef struct {
	uint8_t id;
	char *(*stringify)(uint8_t *, uint16_t);
	char *description;
} tlv_dict;

// tlv.c
void tlv_list_free(tlv_list_t *p);
void tlv_list_append(tlv_list_t **head, uint8_t type, uint16_t len, uint8_t *value);
void output_tlv(tlv_list_t *list, const tlv_dict *d);
dict *dict_search(const dict *list, uint8_t id);
tlv_list_t *tlv_list_search(tlv_list_t *ptr, uint8_t type);
tlv_list_t *tlv_deserialize(uint8_t *buf, uint16_t len);
#endif // !_TLV_H
