/* SPDX-License-Identifier: MIT
 * Host tests of src/conn_policy.c. Build and run: bash scripts/run-host-tests.sh */
#include <stdio.h>

#include <kb1/conn_policy.h>

static int failures;
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        if (!(c)) {                                                                                \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                    \
            failures++;                                                                            \
        }                                                                                          \
    } while (0)

int main(void) {
    struct kb1_decision d;

    /* BT: Bluetooth only while connected; never USB, even with USB ready. */
    d = kb1_decide(KB1_POS_BT, KB1_USB_HID, true);
    CHECK(d.output == KB1_OUT_BLE && !d.cable_off && !d.turn_off);
    d = kb1_decide(KB1_POS_BT, KB1_USB_HID, false);
    CHECK(d.output == KB1_OUT_NONE && !d.cable_off && !d.turn_off);
    d = kb1_decide(KB1_POS_BT, KB1_USB_NONE, false);
    CHECK(d.output == KB1_OUT_NONE && !d.cable_off);

    /* Cable: USB only once HID is ready; never Bluetooth. */
    d = kb1_decide(KB1_POS_CABLE, KB1_USB_HID, true);
    CHECK(d.output == KB1_OUT_USB && !d.cable_off);
    d = kb1_decide(KB1_POS_CABLE, KB1_USB_POWERED, true);
    CHECK(d.output == KB1_OUT_NONE && !d.cable_off);
    d = kb1_decide(KB1_POS_CABLE, KB1_USB_NONE, true);
    CHECK(d.output == KB1_OUT_NONE && d.cable_off && !d.turn_off);

    /* 2.4G: off, whatever else. */
    d = kb1_decide(KB1_POS_OFF, KB1_USB_HID, true);
    CHECK(d.turn_off && d.output == KB1_OUT_NONE);

    /* Unknown (before the switch is known): nothing, no turn-off. */
    d = kb1_decide(KB1_POS_UNKNOWN, KB1_USB_NONE, false);
    CHECK(d.output == KB1_OUT_NONE && !d.cable_off && !d.turn_off);

    printf("%s: %d failure(s)\n", __FILE__, failures);
    return failures ? 1 : 0;
}
