/* SPDX-License-Identifier: MIT
 * Host tests of src/led_logic.c. Build and run: bash scripts/run-host-tests.sh
 * The expectations restate the stock firmware's LED meanings (led_logic.h). */
#include <stdio.h>

#include <kb1/led_logic.h>

static int failures;
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        if (!(c)) {                                                                                \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                    \
            failures++;                                                                            \
        }                                                                                          \
    } while (0)

static struct kb1_led_outputs at(struct kb1_led_inputs in, uint32_t now) {
    in.now_ms = now;
    in.power_on_test = now < KB1_LED_POWER_ON_MS; /* the caller's rule */
    return kb1_led_render(&in);
}

int main(void) {
    struct kb1_led_inputs in = {0};
    struct kb1_led_outputs o;

    /* Power-on: all on for 2.5 s, whatever else. */
    o = at(in, 0);
    CHECK(o.bt && o.caps_lock && o.red && o.green && o.blue && o.led_24g && o.animating);
    o = at(in, 2499);
    CHECK(o.bt && o.red);
    o = at(in, 2500);
    CHECK(!o.bt && !o.caps_lock && !o.red && !o.green && !o.blue && !o.led_24g && !o.animating);

    /* Connected: on for 3 s from the connection, then off and still. */
    in.bt = KB1_BT_LED_CONNECTED;
    in.bt_since_ms = 10000;
    CHECK(at(in, 10000).bt && at(in, 12999).bt && at(in, 12999).animating);
    CHECK(!at(in, 13000).bt && !at(in, 13000).animating);

    /* Pairing: 1 s period for 60 s. */
    in.bt = KB1_BT_LED_PAIRING;
    in.bt_since_ms = 20000;
    CHECK(at(in, 20000).bt && at(in, 20499).bt && !at(in, 20500).bt && at(in, 21000).bt);
    CHECK(!at(in, 80000).bt && !at(in, 80000).animating);

    /* Reconnecting: faster than pairing, 30 s. */
    in.bt = KB1_BT_LED_RECONNECTING;
    in.bt_since_ms = 20000;
    CHECK(at(in, 20000).bt && !at(in, 20200).bt && at(in, 20400).bt);
    CHECK(!at(in, 50000).animating);

    /* Off outside the BT position. */
    in.bt = KB1_BT_LED_OFF;
    CHECK(!at(in, 20000).bt);

    /* Caps Lock follows the host after power-on. */
    in.caps_lock = true;
    CHECK(at(in, 5000).caps_lock);

    /* Charging red, charged green. */
    in.charge = KB1_CHARGE_CHARGING;
    o = at(in, 5000);
    CHECK(o.red && !o.green && !o.blue);
    in.charge = KB1_CHARGE_DONE;
    o = at(in, 5000);
    CHECK(o.green && !o.red && !o.blue);

    /* Battery display wins over charging: >=70 green, >=30 blue, else red. */
    in.battery_display = true;
    in.battery_percent = 70;
    o = at(in, 5000);
    CHECK(o.green && !o.red && !o.blue);
    in.battery_percent = 69;
    o = at(in, 5000);
    CHECK(o.blue && !o.green && !o.red);
    in.battery_percent = 30;
    CHECK(at(in, 5000).blue);
    in.battery_percent = 29;
    o = at(in, 5000);
    CHECK(o.red && !o.green && !o.blue);
    in.battery_display = false;

    /* The 32-bit clock wraps after 49.7 days: a small now_ms long after boot
     * is not the power-on test (the caller decides that from 64-bit uptime),
     * and patterns use differences only. */
    {
        struct kb1_led_inputs w = in;
        w.low_battery = false;
        w.charge = KB1_CHARGE_NONE;
        w.caps_lock = false;
        w.bt = KB1_BT_LED_CONNECTED;
        w.bt_since_ms = 0xFFFFFF00u;
        w.now_ms = 0x00000100u; /* 512 ms after the connection, across the wrap */
        w.power_on_test = false;
        o = kb1_led_render(&w);
        CHECK(o.bt && o.animating);
        CHECK(!o.red && !o.green && !o.blue && !o.led_24g); /* no power-on test */
    }

    /* Low battery: three red flashes over charging, then back to charging. */
    in.charge = KB1_CHARGE_DONE;
    in.low_battery = true;
    in.low_battery_since_ms = 40000;
    o = at(in, 40000);
    CHECK(o.red && !o.green && o.animating);
    CHECK(!at(in, 40200).red);
    CHECK(at(in, 40800).red);
    o = at(in, 41200);
    CHECK(!o.red && o.green); /* warning over */

    printf("%s: %d failure(s)\n", __FILE__, failures);
    return failures ? 1 : 0;
}
