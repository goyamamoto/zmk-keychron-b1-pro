/* SPDX-License-Identifier: MIT */
#include <kb1/ghost_filter.h>

static int bit_count(uint32_t v) { return __builtin_popcount(v); }

bool gf_row_ambiguous(const uint32_t *raw, uint8_t rows, uint8_t row) {
    if (row >= rows || bit_count(raw[row]) < 2) {
        return false;
    }
    for (uint8_t other = 0; other < rows; other++) {
        if (other != row && bit_count(raw[row] & raw[other]) >= 2) {
            return true;
        }
    }
    return false;
}

void gf_mask(const uint32_t *raw, const uint32_t *pressed, uint32_t *out, uint8_t rows) {
    for (uint8_t row = 0; row < rows; row++) {
        out[row] = gf_row_ambiguous(raw, rows, row) ? (raw[row] & pressed[row]) : raw[row];
    }
}
