/* SPDX-License-Identifier: MIT */
#pragma once

#include <stdbool.h>

enum kb1_off_reason {
    KB1_OFF_SWITCH,      /* the 2.4G position of the connection switch */
    KB1_OFF_CABLE_NO_USB, /* the cable position without USB power */
    KB1_OFF_LOW_BATTERY,
    KB1_OFF_IDLE,        /* idle sleep: a key press also wakes */
};

/* Turn the keyboard off (nRF52840 System OFF through ZMK's soft off). Does not
 * return on success; on failure the keyboard reboots. */
void kb1_turn_off(enum kb1_off_reason reason);

/* True while turning off for idle sleep (used by key_waker.c). */
bool kb1_idle_sleep_requested(void);
