/* SPDX-License-Identifier: MIT */
#include <kb1/led_logic.h>

/* On for the first half of each period, for `duration` after `since`. */
static bool blink(uint32_t now, uint32_t since, uint32_t period, uint32_t duration,
                  bool *animating) {
    uint32_t elapsed = now - since;
    if (elapsed >= duration) {
        return false;
    }
    *animating = true;
    return (elapsed % period) < period / 2;
}

struct kb1_led_outputs kb1_led_render(const struct kb1_led_inputs *in) {
    struct kb1_led_outputs out = {0};

    if (in->power_on_test) {
        out.bt = out.caps_lock = out.red = out.green = out.blue = out.led_24g = true;
        out.animating = true;
        return out;
    }

    uint32_t bt_elapsed = in->now_ms - in->bt_since_ms;
    switch (in->bt) {
    case KB1_BT_LED_CONNECTED:
        if (bt_elapsed < KB1_LED_CONNECTED_MS) {
            out.bt = true;
            out.animating = true;
        }
        break;
    case KB1_BT_LED_PAIRING:
        out.bt = blink(in->now_ms, in->bt_since_ms, KB1_LED_PAIRING_PERIOD_MS, KB1_LED_PAIRING_MS,
                       &out.animating);
        break;
    case KB1_BT_LED_RECONNECTING:
        out.bt = blink(in->now_ms, in->bt_since_ms, KB1_LED_RECONNECTING_PERIOD_MS,
                       KB1_LED_RECONNECTING_MS, &out.animating);
        break;
    case KB1_BT_LED_OFF:
    default:
        break;
    }

    out.caps_lock = in->caps_lock;

    if (in->battery_display) {
        if (in->battery_percent >= 70) {
            out.green = true;
        } else if (in->battery_percent >= 30) {
            out.blue = true;
        } else {
            out.red = true;
        }
    } else if (in->low_battery &&
               in->now_ms - in->low_battery_since_ms <
                   KB1_LED_LOW_BATTERY_FLASHES * KB1_LED_LOW_BATTERY_PERIOD_MS) {
        out.red = blink(in->now_ms, in->low_battery_since_ms, KB1_LED_LOW_BATTERY_PERIOD_MS,
                        KB1_LED_LOW_BATTERY_FLASHES * KB1_LED_LOW_BATTERY_PERIOD_MS,
                        &out.animating);
    } else if (in->charge == KB1_CHARGE_CHARGING) {
        out.red = true;
    } else if (in->charge == KB1_CHARGE_DONE) {
        out.green = true;
    }

    return out;
}
