#include <stdint.h>
#define TLV_MIN_PARAMLEN 2	// type (1) + len (1) + zero-length value

typedef struct {
	void *next;
	uint8_t type;
	uint8_t len;
	uint8_t *val;
} tlv_list_t;

// tlv.c
void tlv_list_free(tlv_list_t *p);
void tlv_list_append(tlv_list_t **head, uint8_t type, uint8_t len, uint8_t *value);
tlv_list_t *tlv_list_search(tlv_list_t *ptr, uint8_t type);
tlv_list_t *tlv_deserialize(uint8_t *buf, uint16_t len);
