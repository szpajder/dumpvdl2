#include <stdint.h>
#include "x25.h"

// CLNP header dissection is not implemented yet
#define CLNP_MIN_LEN 2
#define CLNP_COMPRESSED_INIT_MIN_LEN 4

typedef struct {
	uint8_t proto;
	uint8_t data_valid;
	void *data;
	uint32_t datalen;
} clnp_pdu_t;

// clnp.c
clnp_pdu_t *parse_clnp_pdu(uint8_t *buf, uint32_t len);
clnp_pdu_t *parse_clnp_compressed_init_pdu(uint8_t *buf, uint32_t len);
void output_clnp(clnp_pdu_t *pdu);
void output_clnp_compressed(clnp_pdu_t *pdu);
