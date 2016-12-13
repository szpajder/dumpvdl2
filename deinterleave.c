#include <stdint.h>
#include "rtlvdl2.h"

int deinterleave(uint8_t *in, uint32_t len, uint32_t rows, uint32_t cols, uint8_t out[][cols], uint32_t fillwidth, uint32_t offset) {
	if(rows == 0 || cols == 0 || fillwidth == 0)
		return -1;
	uint32_t last_row_len = len % fillwidth;
	if(fillwidth + offset > cols)					// fillwidth or offset too large
		return -2;
	if(len > rows * fillwidth)					// result won't fit
		return -3;
	if(rows > 1 && len - last_row_len < (rows - 1) * fillwidth)	// not enough data to fill requested width
		return -4;
	if(last_row_len == 0 && len / fillwidth < rows)			// not enough data to fill requested number of rows
		return -5;
	uint32_t row = 0, col = offset;
	last_row_len += offset;
	for(uint32_t i = 0; i < len; i++) {
		if(row == rows - 1 && col >= last_row_len) {
			out[row][col] = 0x00;
			row = 0;
			col++;
		}
		out[row++][col] = in[i];
		if(row == rows) {
			row = 0;
			col++;
		}
	}
	return 0;
}
