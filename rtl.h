#include <stdint.h>
#include "dumpvdl2.h"
#define RTL_BUFSIZE 320000
#define RTL_BUFCNT 15
#define RTL_OVERSAMPLE 10
#define RTL_RATE (SYMBOL_RATE * SPS * RTL_OVERSAMPLE)
// rtl.c
void rtl_init(vdl2_state_t *ctx, uint32_t device, int freq, int gain, int correction);
void rtl_cancel();
