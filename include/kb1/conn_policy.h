/* SPDX-License-Identifier: MIT
 *
 * Output policy of the B1 Pro connection switch (pure logic, no Zephyr).
 * The switch position alone decides the output; ZMK's automatic fallback
 * between USB and Bluetooth is never used:
 *   BT     Bluetooth while the active profile is connected, else nothing.
 *   cable  USB once the host has configured the HID interface, else
 *          nothing; turn off after a delay when USB is not powered at all.
 *   2.4G   off.
 */
#pragma once

#include <stdbool.h>

enum kb1_position { KB1_POS_UNKNOWN, KB1_POS_CABLE, KB1_POS_BT, KB1_POS_OFF };
enum kb1_usb { KB1_USB_NONE, KB1_USB_POWERED, KB1_USB_HID };
enum kb1_output { KB1_OUT_NONE, KB1_OUT_USB, KB1_OUT_BLE };

struct kb1_decision {
    enum kb1_output output;
    bool cable_off; /* start (or keep) the delayed turn-off of the cable position */
    bool turn_off;  /* the off position: turn off (after its hold time) */
};

struct kb1_decision kb1_decide(enum kb1_position position, enum kb1_usb usb, bool ble_connected);

/* The current switch position (behavior_conn_switch.c). */
enum kb1_position kb1_conn_position(void);
