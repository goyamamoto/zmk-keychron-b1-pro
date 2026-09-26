/* SPDX-License-Identifier: MIT
 *
 * What the B1 Pro's status LEDs show (pure logic, no Zephyr). What each LED
 * means follows the stock Keychron firmware; the timings are this firmware's
 * own (the stock pairing blink is 1 s on / 1 s off for 3 min, reconnecting is
 * three flashes, the low-battery warning five flashes):
 *
 *   power-on   every LED on for 2.5 s after boot
 *   Bluetooth  (blue LED, BT position only) pairing: slow blink (1 s period)
 *              for up to 60 s; reconnecting: fast blink (0.4 s period) for up
 *              to 30 s; connected: on for 3 s, then off
 *   Caps Lock  on while the host's Caps Lock is on
 *   RGB        battery display (while its key is held): green at 70 % or
 *              more, blue at 30 % or more, red below; else a low-battery
 *              warning: three red flashes; else charging: red, charged: green
 *
 * kb1_led_render() gives the LED states at a moment from the inputs, and
 * whether a timed pattern is still running (the caller then renders again
 * soon).
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

enum kb1_bt_led {
    KB1_BT_LED_OFF,
    KB1_BT_LED_PAIRING,
    KB1_BT_LED_RECONNECTING,
    KB1_BT_LED_CONNECTED,
};

enum kb1_charge {
    KB1_CHARGE_NONE,
    KB1_CHARGE_CHARGING,
    KB1_CHARGE_DONE,
};

#define KB1_LED_POWER_ON_MS 2500
#define KB1_LED_CONNECTED_MS 3000
#define KB1_LED_PAIRING_MS 60000
#define KB1_LED_PAIRING_PERIOD_MS 1000
#define KB1_LED_RECONNECTING_MS 30000
#define KB1_LED_RECONNECTING_PERIOD_MS 400
#define KB1_LED_LOW_BATTERY_FLASHES 3
#define KB1_LED_LOW_BATTERY_PERIOD_MS 400

struct kb1_led_inputs {
    bool power_on_test;       /* within KB1_LED_POWER_ON_MS of boot */
    uint32_t now_ms;          /* time since boot, wrapping (differences only) */
    enum kb1_bt_led bt;
    uint32_t bt_since_ms;     /* when bt got its current value */
    bool caps_lock;
    enum kb1_charge charge;
    bool battery_display;
    uint8_t battery_percent;
    bool low_battery;
    uint32_t low_battery_since_ms; /* start of the last warning */
};

struct kb1_led_outputs {
    bool bt;
    bool caps_lock;
    bool red;
    bool green;
    bool blue;
    bool led_24g;
    bool animating; /* a timed pattern is running: render again soon */
};

struct kb1_led_outputs kb1_led_render(const struct kb1_led_inputs *in);
