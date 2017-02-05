#include <stdint.h>
#include "dumpvdl2.h"
#define MIRISDR_BUFSIZE 320000
#define MIRISDR_BUFCNT 32
#define MIRISDR_OVERSAMPLE 13
#define MIRISDR_RATE (SYMBOL_RATE * SPS * MIRISDR_OVERSAMPLE)
// mirisdr.c
void mirisdr_init(vdl2_state_t *ctx, uint32_t device, int flavour, uint32_t freq, int gain, int freq_offset);
void mirisdr_cancel();
