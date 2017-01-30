#include <stdint.h>
#define RTL_BUFSIZE 320000
#define RTL_BUFCNT 15
#define RTL_OVERSAMPLE 10
#define RTL_RATE (SYMBOL_RATE * SPS * RTL_OVERSAMPLE)
#define RTL_AUTO_GAIN -100
// rtl.c
void rtl_init(void *ctx, uint32_t device, int freq, int gain, int correction);
void rtl_cancel();
