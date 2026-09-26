/*
 * Copyright (c) 2026 zmk-kb1 contributors
 *
 * SPDX-License-Identifier: MIT
 *
 * Turning off: soft off with the right wake sources, idle sleep, and the
 * low-battery cutoff.
 *
 * Every way of turning off goes through ZMK's zmk_pm_soft_off(), which
 * suspends all devices and then resumes the wake sources listed in the
 * board's zmk,soft-off-wakeup-sources: the key waker (only for idle sleep)
 * and the switch waker (always). Plugging in USB wakes the nRF52840 through
 * its VBUS detection (POWER RESETREAS.VBUS).
 *
 * Idle sleep replaces ZMK's CONFIG_ZMK_SLEEP, whose path suspends devices
 * without resuming wake sources afterwards, so it cannot re-arm the switch
 * inputs that the direct kscan releases on suspend.
 */

#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/fatal.h>
#include <zephyr/init.h>
#include <zephyr/sys/reboot.h>

#include <zmk/activity.h>
#include <zmk/event_manager.h>
#include <zmk/events/activity_state_changed.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/pm.h>
#include <zmk/usb.h>

#include <kb1/leds.h>
#include <kb1/power.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

static bool idle_sleep;

bool kb1_idle_sleep_requested(void) { return idle_sleep; }

void kb1_turn_off(enum kb1_off_reason reason) {
    LOG_INF("Turning off (reason %d)", reason);
    idle_sleep = reason == KB1_OFF_IDLE;
    /* GPIO outputs keep their level in System OFF: turn the LEDs off first. */
    kb1_leds_power_off();
    int err = zmk_pm_soft_off();
    /* Only reached on failure. Some devices may be left suspended (keys dead,
     * radio on); a reboot restores a working state, and the switch position is
     * applied again at boot. */
    LOG_ERR("Soft off failed (%d); rebooting", err);
    idle_sleep = false;
    sys_reboot(SYS_REBOOT_COLD);
}

/* ---------------------------------------------------------------- idle sleep */

static void idle_sleep_work_handler(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(idle_sleep_work, idle_sleep_work_handler);

/* Matrix keys held down (positions below the switch and charger inputs). A
 * key held at idle sleep would keep the matrix scanning instead of arming it,
 * so no key could wake the keyboard: sleep only with all keys up, as the stock
 * firmware does. */
static int keys_down;

static int position_listener(const zmk_event_t *eh) {
    const struct zmk_position_state_changed *ev = as_zmk_position_state_changed(eh);

    if (ev != NULL && ev->position < CONFIG_KB1_FIRST_SWITCH_POSITION) {
        keys_down += ev->state ? 1 : -1;
        if (keys_down < 0) {
            keys_down = 0;
        }
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(kb1_power_position, position_listener);
ZMK_SUBSCRIPTION(kb1_power_position, zmk_position_state_changed);

static void idle_sleep_work_handler(struct k_work *work) {
    if (zmk_activity_get_state() == ZMK_ACTIVITY_ACTIVE) {
        return;
    }
    if (zmk_usb_is_powered() || keys_down > 0) {
        /* Still idle, but on USB power or with a key held: check again later. */
        k_work_reschedule(&idle_sleep_work, K_MINUTES(10));
        return;
    }
    kb1_turn_off(KB1_OFF_IDLE);
}

static int activity_listener(const zmk_event_t *eh) {
    const struct zmk_activity_state_changed *ev = as_zmk_activity_state_changed(eh);

    if (ev == NULL) {
        return ZMK_EV_EVENT_BUBBLE;
    }
    if (ev->state == ZMK_ACTIVITY_IDLE) {
        k_work_reschedule(&idle_sleep_work, K_MSEC(CONFIG_KB1_IDLE_SLEEP_MS));
    } else if (ev->state == ZMK_ACTIVITY_ACTIVE) {
        k_work_cancel_delayable(&idle_sleep_work);
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(kb1_power_activity, activity_listener);
ZMK_SUBSCRIPTION(kb1_power_activity, zmk_activity_state_changed);

/* --------------------------------------------------------- low-battery cutoff */

#if IS_ENABLED(CONFIG_KB1_BATTERY_CUTOFF)

/* ZMK's battery reporting samples the divider every
 * CONFIG_ZMK_BATTERY_REPORT_INTERVAL seconds (on its low-priority work queue)
 * and raises an event only when the percentage changes, which stays at 0 below
 * 3450 mV. So check the last sampled voltage on a timer instead, without
 * sampling again (no second user of the ADC). */
static const struct device *const battery = DEVICE_DT_GET(DT_CHOSEN(zmk_battery));

static void battery_check(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(battery_check_work, battery_check);
static int low_readings;

static void battery_check(struct k_work *work) {
    struct sensor_value voltage;

    k_work_schedule(&battery_check_work, K_SECONDS(CONFIG_ZMK_BATTERY_REPORT_INTERVAL));

    if (zmk_usb_is_powered()) {
        low_readings = 0;
        kb1_leds_low_battery(false);
        return;
    }
    /* ZMK samples the battery only while the keyboard is active. While it is
     * idle, sample here (ZMK does not use the ADC then; the ADC driver
     * serializes reads anyway), so the cutoff also works on an idle keyboard. */
    if (zmk_activity_get_state() != ZMK_ACTIVITY_ACTIVE &&
        sensor_sample_fetch_chan(battery, SENSOR_CHAN_GAUGE_VOLTAGE) != 0) {
        return;
    }
    if (sensor_channel_get(battery, SENSOR_CHAN_GAUGE_VOLTAGE, &voltage) != 0) {
        return;
    }
    int mv = voltage.val1 * 1000 + voltage.val2 / 1000;
    /* 0 before the first sample; below 2 V the reading is not a battery. */
    bool valid = mv > 2000;

    /* The stock firmware's low-battery warning threshold; the LED warns only
     * while the keyboard is in use. */
    kb1_leds_low_battery(valid && mv < CONFIG_KB1_BATTERY_LOW_MV &&
                         zmk_activity_get_state() == ZMK_ACTIVITY_ACTIVE);

    /* A single sample can sag under load: require several in a row, as the
     * stock firmware does. */
    if (valid && mv < CONFIG_KB1_BATTERY_CUTOFF_MV) {
        low_readings++;
        LOG_WRN("Battery at %d mV, below %d mV (%d)", mv, CONFIG_KB1_BATTERY_CUTOFF_MV,
                low_readings);
        if (low_readings >= CONFIG_KB1_BATTERY_CUTOFF_READINGS) {
            kb1_turn_off(KB1_OFF_LOW_BATTERY);
        }
    } else {
        low_readings = 0;
    }
}

static int battery_check_init(void) {
    k_work_schedule(&battery_check_work, K_SECONDS(CONFIG_ZMK_BATTERY_REPORT_INTERVAL));
    return 0;
}

SYS_INIT(battery_check_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

#endif /* CONFIG_KB1_BATTERY_CUTOFF */

/* --------------------------------------------------------------- fatal error */

/* Zephyr halts on a fatal error by default: the CPU stays on and only the reset
 * switch recovers the keyboard. Reboot instead. The UF2 bootloader runs first
 * on every reset, so the reset-switch DFU path stays available even if the
 * fault repeats at every boot. */
void k_sys_fatal_error_handler(unsigned int reason, const struct arch_esf *esf) {
    ARG_UNUSED(reason);
    ARG_UNUSED(esf);
    sys_reboot(SYS_REBOOT_COLD);
    CODE_UNREACHABLE;
}
