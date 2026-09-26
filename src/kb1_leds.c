/*
 * Copyright (c) 2026 zmk-kb1 contributors
 *
 * SPDX-License-Identifier: MIT
 *
 * Drives the B1 Pro's status LEDs from the keyboard state, as described in
 * led_logic.h (which decides what is shown). Renders on every change and,
 * while a timed pattern runs, every 50 ms; nothing runs when the LEDs are
 * still.
 */

#include <zephyr/device.h>
#include <zephyr/drivers/led.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <dt-bindings/zmk/hid_indicators.h>
#include <zmk/battery.h>
#include <zmk/ble.h>
#include <zmk/event_manager.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/events/hid_indicators_changed.h>
#include <zmk/events/position_state_changed.h>

#include <kb1/conn_policy.h>
#include <kb1/led_logic.h>
#include <kb1/leds.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define LED_IDX(label) DT_NODE_CHILD_IDX(DT_NODELABEL(label))

static const struct device *const leds = DEVICE_DT_GET(DT_NODELABEL(status_leds));

static const uint32_t all_leds[] = {LED_IDX(led_bt),    LED_IDX(led_num_lock), LED_IDX(led_caps_lock),
                                    LED_IDX(led_24g),   LED_IDX(led_red),      LED_IDX(led_green),
                                    LED_IDX(led_blue)};

/* All state is changed on the system work queue (ZMK events, the render work
 * item, behavior callbacks), except kb1_leds_power_off(), which only sets a
 * flag, cancels the work and turns the LEDs off. */
static struct kb1_led_inputs in;
static int bt_profile = -1;
static bool charging_input, charge_done_input;
static struct kb1_led_outputs shown;
static bool shown_valid;
static bool powered_off;

static void render(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(render_work, render);

static void set(uint32_t idx, bool on) {
    int err = on ? led_on(leds, idx) : led_off(leds, idx);
    if (err) {
        LOG_ERR("LED %u: %d", idx, err);
    }
}

static enum kb1_bt_led bt_led_state(void) {
    if (kb1_conn_position() != KB1_POS_BT) {
        return KB1_BT_LED_OFF;
    }
    if (zmk_ble_active_profile_is_connected()) {
        return KB1_BT_LED_CONNECTED;
    }
    return zmk_ble_active_profile_is_open() ? KB1_BT_LED_PAIRING : KB1_BT_LED_RECONNECTING;
}

static void render(struct k_work *work) {
    if (powered_off) {
        return;
    }
    int64_t uptime = k_uptime_get();
    in.now_ms = (uint32_t)uptime;
    in.power_on_test = uptime < KB1_LED_POWER_ON_MS;
    enum kb1_bt_led bt = bt_led_state();
    int profile = zmk_ble_active_profile_index();
    /* A new state, or the same state on another profile, restarts it. */
    if (bt != in.bt || profile != bt_profile) {
        in.bt = bt;
        in.bt_since_ms = in.now_ms;
        bt_profile = profile;
    }

    struct kb1_led_outputs out = kb1_led_render(&in);
#define UPDATE(field, label)                                                                       \
    if (!shown_valid || out.field != shown.field) {                                                \
        set(LED_IDX(label), out.field);                                                            \
    }
    UPDATE(bt, led_bt)
    UPDATE(caps_lock, led_caps_lock)
    UPDATE(red, led_red)
    UPDATE(green, led_green)
    UPDATE(blue, led_blue)
    UPDATE(led_24g, led_24g)
#undef UPDATE
    if (!shown_valid) {
        set(LED_IDX(led_num_lock), false); /* not used on the B1 Pro */
    }
    shown = out;
    shown_valid = true;

    if (out.animating) {
        k_work_schedule(&render_work, K_MSEC(50));
    }
}

void kb1_leds_refresh(void) {
    if (!powered_off) {
        k_work_reschedule(&render_work, K_NO_WAIT);
    }
}

void kb1_leds_battery_display(bool on) {
    in.battery_display = on;
    in.battery_percent = zmk_battery_state_of_charge();
    kb1_leds_refresh();
}

void kb1_leds_low_battery(bool low) {
    if (low) {
        in.low_battery_since_ms = k_uptime_get_32();
    }
    in.low_battery = low;
    kb1_leds_refresh();
}

void kb1_leds_power_off(void) {
    powered_off = true;
    k_work_cancel_delayable(&render_work);
    for (size_t i = 0; i < ARRAY_SIZE(all_leds); i++) {
        set(all_leds[i], false);
    }
}

static int state_listener(const zmk_event_t *eh) {
    const struct zmk_hid_indicators_changed *ind = as_zmk_hid_indicators_changed(eh);
    const struct zmk_position_state_changed *pos = as_zmk_position_state_changed(eh);

    if (ind != NULL) {
        in.caps_lock = (ind->indicators & HID_INDICATOR_CAPS_LOCK) != 0;
    } else if (pos != NULL) {
        if (pos->position == CONFIG_KB1_CHARGING_POSITION) {
            charging_input = pos->state;
        } else if (pos->position == CONFIG_KB1_CHARGE_DONE_POSITION) {
            charge_done_input = pos->state;
        } else {
            return ZMK_EV_EVENT_BUBBLE;
        }
        /* Done wins; with done gone, a charging signal still shows. */
        in.charge = charge_done_input ? KB1_CHARGE_DONE
                    : charging_input  ? KB1_CHARGE_CHARGING
                                      : KB1_CHARGE_NONE;
    }
    kb1_leds_refresh();
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(kb1_leds, state_listener);
ZMK_SUBSCRIPTION(kb1_leds, zmk_ble_active_profile_changed);
ZMK_SUBSCRIPTION(kb1_leds, zmk_hid_indicators_changed);
ZMK_SUBSCRIPTION(kb1_leds, zmk_position_state_changed);

static int kb1_leds_init(void) {
    if (!device_is_ready(leds)) {
        LOG_ERR("Status LEDs not ready");
        return -ENODEV;
    }
    kb1_leds_refresh(); /* power-on pattern */
    return 0;
}

SYS_INIT(kb1_leds_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
