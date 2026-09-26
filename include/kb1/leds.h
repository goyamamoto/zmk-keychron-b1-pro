/* SPDX-License-Identifier: MIT */
#pragma once

#include <stdbool.h>

#if IS_ENABLED(CONFIG_KB1_LEDS)

/* Something the LEDs show may have changed: render again. */
void kb1_leds_refresh(void);
/* Battery display (the indicator behavior) on or off. */
void kb1_leds_battery_display(bool on);
/* Show the low-battery warning (three red flashes) now. */
void kb1_leds_low_battery(bool low);
/* Turn every LED off for power-off; no further updates until reset. GPIO
 * outputs keep their level in System OFF, so a lit LED would stay lit. */
void kb1_leds_power_off(void);

#else

static inline void kb1_leds_refresh(void) {}
static inline void kb1_leds_battery_display(bool on) { (void)on; }
static inline void kb1_leds_low_battery(bool low) { (void)low; }
static inline void kb1_leds_power_off(void) {}

#endif
