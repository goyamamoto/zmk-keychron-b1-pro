/* SPDX-License-Identifier: MIT
 *
 * Ghost masking for a key matrix without diodes (pure logic, no Zephyr).
 *
 * With floating columns and pulled-down rows, a key reads as pressed whenever
 * its row and column are linked through a chain of pressed keys. Three keys
 * at three corners of a rectangle therefore make the fourth read as pressed,
 * and in general the readings form blocks of rows x columns. A row is
 * ambiguous when it has two or more keys reading pressed and shares two or
 * more of those columns with another row: it is part of such a block.
 *
 * The matrix driver calls gf_mask() once per scan pass on the raw readings of
 * the whole pass, before debouncing: in an ambiguous row, keys that are not
 * already pressed read as released, so no new press starts there until the
 * block is gone. Keys already pressed keep their raw reading, so a key is
 * released as soon as it reads released (a ghost can only add readings). A
 * key whose row and column stay linked through other pressed keys keeps
 * reading pressed after it is released; that is the matrix, not the mask.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#define GF_MAX_ROWS 16
#define GF_MAX_COLS 32

/* raw[r]: bit c set if (r, c) reads pressed in this scan pass. */
bool gf_row_ambiguous(const uint32_t *raw, uint8_t rows, uint8_t row);

/* out[r] = raw[r] for rows that are not ambiguous, raw[r] & pressed[r] for
 * ambiguous ones. pressed[r]: bit c set if (r, c) is currently pressed after
 * debouncing. out may not alias raw or pressed. */
void gf_mask(const uint32_t *raw, const uint32_t *pressed, uint32_t *out, uint8_t rows);
