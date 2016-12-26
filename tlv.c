#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include "tlv.h"
#include "rtlvdl2.h"

void tlv_list_free(tlv_list_t *p) {
	if(p == NULL) return;
	if(p->next != NULL)
		tlv_list_free(p->next);
	free(p);
}

void tlv_list_append(tlv_list_t **head, uint8_t type, uint8_t len, uint8_t *value) {
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

tlv_list_t *tlv_deserialize(uint8_t *buf, uint16_t len) {
	assert(buf);
	tlv_list_t *head = NULL;
	uint8_t *ptr = buf;
	while(len >= TLV_MIN_PARAMLEN) {
		uint8_t pid = *ptr;
		ptr++; len--;
		uint8_t paramlen = *ptr;
		ptr++; len--;
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
