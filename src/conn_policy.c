/* SPDX-License-Identifier: MIT */
#include <kb1/conn_policy.h>

struct kb1_decision kb1_decide(enum kb1_position position, enum kb1_usb usb, bool ble_connected) {
    struct kb1_decision d = {.output = KB1_OUT_NONE, .cable_off = false, .turn_off = false};

    switch (position) {
    case KB1_POS_BT:
        d.output = ble_connected ? KB1_OUT_BLE : KB1_OUT_NONE;
        break;
    case KB1_POS_CABLE:
        d.output = usb == KB1_USB_HID ? KB1_OUT_USB : KB1_OUT_NONE;
        d.cable_off = usb == KB1_USB_NONE;
        break;
    case KB1_POS_OFF:
        d.turn_off = true;
        break;
    case KB1_POS_UNKNOWN:
    default:
        break;
    }
    return d;
}
