/* SPDX-License-Identifier: MIT
 * Host tests of src/ghost_filter.c. Build and run: bash scripts/run-host-tests.sh
 *
 * The raw matrices are what a matrix without diodes reads with floating
 * columns and pulled-down rows: a key reads pressed when its row and column
 * are linked through pressed keys (read_matrix() below computes that from the
 * physically pressed keys, independently of the code under test). */
#include <stdio.h>
#include <string.h>

#include <kb1/ghost_filter.h>

static int failures;
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        if (!(c)) {                                                                                \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                    \
            failures++;                                                                            \
        }                                                                                          \
    } while (0)

#define ROWS 8

/* Readings of a diode-less matrix: (r, c) reads pressed if row r and column c
 * are connected in the bipartite graph of rows and columns whose edges are
 * the physically pressed keys. */
static void read_matrix(const uint32_t *phys, uint32_t *raw) {
    for (int r = 0; r < ROWS; r++) {
        uint32_t rows_reached = 1u << r, cols = 0, prev_rows = 0, prev_cols = ~0u;
        while (rows_reached != prev_rows || cols != prev_cols) {
            prev_rows = rows_reached;
            prev_cols = cols;
            for (int rr = 0; rr < ROWS; rr++) {
                if (rows_reached & (1u << rr)) {
                    cols |= phys[rr];
                }
            }
            for (int rr = 0; rr < ROWS; rr++) {
                if (phys[rr] & cols) {
                    rows_reached |= 1u << rr;
                }
            }
        }
        raw[r] = cols;
    }
}

/* One scan pass: physical keys -> readings -> mask with the pressed state,
 * then treat the masked readings as the new pressed state (debounce aside). */
static void pass(const uint32_t *phys, uint32_t *pressed) {
    uint32_t raw[ROWS], out[ROWS];
    read_matrix(phys, raw);
    gf_mask(raw, pressed, out, ROWS);
    memcpy(pressed, out, sizeof(out));
}

static void two_keys_on_a_row(void) {
    /* Shift+T: two keys on one row read correctly and both press. */
    uint32_t phys[ROWS] = {0}, pressed[ROWS] = {0};
    phys[1] = (1u << 15) | (1u << 4);
    pass(phys, pressed);
    CHECK(pressed[1] == ((1u << 15) | (1u << 4)));
}

static void rectangle_ghost_is_held(void) {
    uint32_t phys[ROWS] = {0}, pressed[ROWS] = {0};
    /* A=(0,1) and B=(0,2) pressed first. */
    phys[0] = (1u << 1) | (1u << 2);
    pass(phys, pressed);
    CHECK(pressed[0] == ((1u << 1) | (1u << 2)));
    /* C=(1,1): the ghost D=(1,2) reads pressed too. Neither C nor D may
     * press: row 1 is ambiguous. A and B stay pressed. */
    phys[1] = 1u << 1;
    uint32_t raw[ROWS];
    read_matrix(phys, raw);
    CHECK(raw[1] == ((1u << 1) | (1u << 2))); /* the ghost is really read */
    pass(phys, pressed);
    CHECK(pressed[1] == 0);
    CHECK(pressed[0] == ((1u << 1) | (1u << 2)));
    /* Releasing B ends the block; C comes through, D never does. */
    phys[0] = 1u << 1;
    pass(phys, pressed);
    CHECK(pressed[0] == (1u << 1));
    CHECK(pressed[1] == (1u << 1));
}

static void release_in_row_that_stays_ambiguous(void) {
    uint32_t phys[ROWS] = {0}, pressed[ROWS] = {0};
    /* (0,3) pressed alone first. */
    phys[0] = 1u << 3;
    pass(phys, pressed);
    CHECK(pressed[0] == (1u << 3));
    /* Rows 0 and 1 form a block on columns 1 and 2 (plus the ghosts), and
     * (0,3) links row 1 to column 3 as well. */
    phys[0] |= (1u << 1) | (1u << 2);
    phys[1] = 1u << 1;
    pass(phys, pressed);
    uint32_t raw[ROWS];
    read_matrix(phys, raw);
    CHECK(gf_row_ambiguous(raw, ROWS, 0));
    CHECK(pressed[0] == (1u << 3)); /* no new press in the ambiguous row */
    /* Releasing (0,3) must get through although row 0 stays ambiguous. */
    phys[0] &= ~(1u << 3);
    read_matrix(phys, raw);
    CHECK(gf_row_ambiguous(raw, ROWS, 0));
    pass(phys, pressed);
    CHECK((pressed[0] & (1u << 3)) == 0);
}

static void chain_across_three_rows(void) {
    /* Keys (0,0), (1,0), (1,1), (2,1): every row reads columns 0 and 1, so the
     * ghosts (0,1) and (2,0) appear. No new press may leak from the block. */
    uint32_t phys[ROWS] = {0}, pressed[ROWS] = {0};
    phys[0] = 1u << 0;
    phys[1] = (1u << 0) | (1u << 1);
    phys[2] = 1u << 1;
    pass(phys, pressed);
    CHECK((pressed[0] & (1u << 1)) == 0);
    CHECK((pressed[2] & (1u << 0)) == 0);
}

int main(void) {
    two_keys_on_a_row();
    rectangle_ghost_is_held();
    release_in_row_that_stays_ambiguous();
    chain_across_three_rows();
    printf("%s: %d failure(s)\n", __FILE__, failures);
    return failures ? 1 : 0;
}
