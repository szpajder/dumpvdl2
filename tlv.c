#include <stdio.h>
#include <stdlib.h>
#include "tlv.h"
#include "dumpvdl2.h"

void tlv_list_free(tlv_list_t *p) {
	if(p == NULL) return;
	if(p->next != NULL)
		tlv_list_free(p->next);
	free(p);
}

void tlv_list_append(tlv_list_t **head, uint8_t type, uint16_t len, uint8_t *value) {
	tlv_list_t *ptr;
	if(*head == NULL) {	// new list
		*head = ptr = XCALLOC(1, sizeof(tlv_list_t));
	} else {		// existing list
		for(ptr = *head; ptr->next != NULL; ptr = ptr->next)
			;
		ptr->next = XCALLOC(1, sizeof(tlv_list_t));
		ptr = ptr->next;
	}
	ptr->type = type;
	ptr->len = len;
	ptr->val = value;
}

tlv_list_t *tlv_list_search(tlv_list_t *ptr, uint8_t type) {
	while(ptr != NULL) {
		if(ptr->type == type) break;
		ptr = ptr->next;
	}
	return ptr;
}

tlv_list_t *tlv_deserialize(uint8_t *buf, uint16_t len, uint8_t len_octets) {
	tlv_list_t *head = NULL;
	uint8_t *ptr = buf;
	uint8_t tlv_min_paramlen = 1 + len_octets;	/* code + <len_octets> length field + empty data field */
	uint16_t paramlen;
	while(len >= tlv_min_paramlen) {
		uint8_t pid = *ptr;
		ptr++; len--;

		paramlen = *ptr;
		if(len_octets == 2)
			paramlen = (paramlen << 8) | (uint16_t)ptr[1];

		ptr += len_octets; len -= len_octets;
		if(paramlen > len) {
			fprintf(stderr, "TLV param %02x truncated: paramlen=%u buflen=%u\n", pid, paramlen, len);
			return NULL;
		}
		tlv_list_append(&head, pid, paramlen, ptr);
		ptr += paramlen; len -= paramlen;
	}
	if(len > 0)
		fprintf(stderr, "Warning: %u unparsed octets left at end of TLV list\n", len);
	return head;
}

dict *dict_search(const dict *list, uint8_t id) {
	if(list == NULL) return NULL;
	dict *ptr;
	for(ptr = (dict *)list; ; ptr++) {
		if(ptr->val == NULL) return NULL;
		if(ptr->id == id) return ptr->val;
	}
}

tlv_dict *tlv_dict_search(const tlv_dict *list, uint8_t id) {
	if(list == NULL) return NULL;
	tlv_dict *ptr;
	for(ptr = (tlv_dict *)list; ; ptr++) {
		if(ptr->description == NULL) return NULL;
		if(ptr->id == id) return ptr;
	}
}

void output_tlv(FILE *f, tlv_list_t *list, const tlv_dict *d) {
	if(list == NULL || d == NULL) return;
	for(tlv_list_t *p = list; p != NULL; p = p->next) {
		tlv_dict *entry = tlv_dict_search(d, p->type);
		char *str = NULL;
		if(entry != NULL) {
			str = (*(entry->stringify))(p->val, p->len);
			fprintf(f, " %s: %s\n", entry->description, str);
		} else {
			str = fmt_hexstring(p->val, p->len);
			fprintf(f, " (Unknown code 0x%02x): %s\n", p->type, str);
		}
		if(str)
			free(str);
	}
}
