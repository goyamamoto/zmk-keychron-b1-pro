/*
 * Copyright (c) 2026 zmk-kb1 contributors
 *
 * SPDX-License-Identifier: MIT
 *
 * Connection switch of the Keychron B1 Pro: BT / cable / 2.4G, left to right.
 *
 * The switch has an input for the BT and for the 2.4G position; the cable
 * position is the one where neither is active. They are keys of the direct
 * kscan, bound in the keymap as:
 *
 *   &conn_sw CONN_SW_BT    the BT position (released: the cable position)
 *   &conn_sw CONN_SW_OFF   the 2.4G position, which is the off position
 *                          (released: the cable position)
 *
 * The output follows kb1_decide() (conn_policy.c): the switch alone decides,
 * never ZMK's automatic fallback between USB and Bluetooth. The decision is
 * applied again whenever an input to it changes: the switch, the USB state,
 * the Bluetooth connection, and the end of settings loading, which would
 * otherwise restore a stored preference over the switch at boot.
 */

#define DT_DRV_COMPAT zmk_behavior_kb1_conn_switch

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <drivers/behavior.h>

#include <zmk/behavior.h>
#include <zmk/ble.h>
#include <zmk/endpoints.h>
#include <zmk/event_manager.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/usb.h>
#include <dt-bindings/kb1/conn_switch.h>

#include <kb1/conn_policy.h>
#include <kb1/leds.h>
#include <kb1/power.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

/* The state changes run on the system work queue (the kscan's work item,
 * ZMK's event handling, the work items below), except the settings commit,
 * which runs in the main thread during settings_load(); apply() only reads the
 * position there and sets the preferred transport, which is idempotent. */
static enum kb1_position position = KB1_POS_UNKNOWN;

enum kb1_position kb1_conn_position(void) { return position; }

/* HID ready is what ZMK's endpoint selection requires for USB (configured by
 * the host; ZMK counts a suspended bus as ready too); preferring USB before
 * that would let ZMK fall back to Bluetooth. */
static enum kb1_usb usb_state(void) {
    if (zmk_usb_is_hid_ready()) {
        return KB1_USB_HID;
    }
    return zmk_usb_is_powered() ? KB1_USB_POWERED : KB1_USB_NONE;
}

static void cable_off(struct k_work *work) {
    struct kb1_decision d = kb1_decide(position, usb_state(), false);
    if (d.cable_off) {
        kb1_turn_off(KB1_OFF_CABLE_NO_USB);
    }
}

static K_WORK_DELAYABLE_DEFINE(cable_off_work, cable_off);

static void switch_off(struct k_work *work) {
    if (position == KB1_POS_OFF) {
        kb1_turn_off(KB1_OFF_SWITCH);
    }
}

static K_WORK_DELAYABLE_DEFINE(switch_off_work, switch_off);

static void apply(void) {
    struct kb1_decision d =
        kb1_decide(position, usb_state(), zmk_ble_active_profile_is_connected());
    enum zmk_transport transport = d.output == KB1_OUT_USB   ? ZMK_TRANSPORT_USB
                                   : d.output == KB1_OUT_BLE ? ZMK_TRANSPORT_BLE
                                                             : ZMK_TRANSPORT_NONE;

    if (zmk_endpoint_get_preferred_transport() != transport) {
        LOG_INF("Connection switch: position %d, output %d", position, d.output);
        zmk_endpoint_set_preferred_transport(transport);
    }

    if (d.cable_off) {
        /* Keep an already running countdown; start one otherwise. */
        if (!k_work_delayable_is_pending(&cable_off_work)) {
            k_work_schedule(&cable_off_work, K_MSEC(CONFIG_KB1_CABLE_OFF_DELAY_MS));
        }
    } else {
        k_work_cancel_delayable(&cable_off_work);
    }

    if (d.turn_off) {
        /* The switch must stay in the off position for the hold time; a
         * contact glitch or a quick pass does not turn the keyboard off. */
        if (!k_work_delayable_is_pending(&switch_off_work)) {
            k_work_schedule(&switch_off_work, K_MSEC(CONFIG_KB1_OFF_HOLD_MS));
        }
    } else {
        k_work_cancel_delayable(&switch_off_work);
    }

    kb1_leds_refresh();
}

/* After boot, the direct kscan has reported a switch held in BT or 2.4G. If
 * neither was reported, the switch is in the cable position. */
static void boot_check(struct k_work *work) {
    if (position == KB1_POS_UNKNOWN) {
        position = KB1_POS_CABLE;
    }
    apply();
}

static K_WORK_DELAYABLE_DEFINE(boot_check_work, boot_check);

static int on_pressed(struct zmk_behavior_binding *binding,
                      struct zmk_behavior_binding_event event) {
    switch (binding->param1) {
    case CONN_SW_BT:
        position = KB1_POS_BT;
        break;
    case CONN_SW_OFF:
        position = KB1_POS_OFF;
        break;
    default:
        return -ENOTSUP;
    }
    apply();
    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_released(struct zmk_behavior_binding *binding,
                       struct zmk_behavior_binding_event event) {
    switch (binding->param1) {
    case CONN_SW_BT:
    case CONN_SW_OFF:
        /* Both BT and 2.4G are next to the cable position. */
        position = KB1_POS_CABLE;
        apply();
        return ZMK_BEHAVIOR_OPAQUE;
    default:
        return -ENOTSUP;
    }
}

static int state_listener(const zmk_event_t *eh) {
    if (as_zmk_usb_conn_state_changed(eh) != NULL ||
        as_zmk_ble_active_profile_changed(eh) != NULL) {
        if (position != KB1_POS_UNKNOWN) {
            apply();
        }
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(kb1_conn_switch, state_listener);
ZMK_SUBSCRIPTION(kb1_conn_switch, zmk_usb_conn_state_changed);
ZMK_SUBSCRIPTION(kb1_conn_switch, zmk_ble_active_profile_changed);

/* settings_load() runs in main(), after the kscan has reported the switch,
 * and restores ZMK's stored preferred transport. Commit handlers run after
 * all settings are loaded: apply the switch again. */
static int conn_switch_settings_commit(void) {
    if (position != KB1_POS_UNKNOWN) {
        apply();
    }
    return 0;
}

SETTINGS_STATIC_HANDLER_DEFINE(kb1_conn_switch, "kb1", NULL, NULL, conn_switch_settings_commit, NULL);

static int conn_switch_init(const struct device *dev) {
    k_work_schedule(&boot_check_work, K_MSEC(CONFIG_KB1_CONN_SWITCH_BOOT_CHECK_MS));
    return 0;
}

#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
static const struct behavior_parameter_value_metadata position_values[] = {
    {
        .display_name = "BT position",
        .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE,
        .value = CONN_SW_BT,
    },
    {
        .display_name = "Off position (2.4G)",
        .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE,
        .value = CONN_SW_OFF,
    },
};

static const struct behavior_parameter_metadata_set position_set = {
    .param1_values = position_values,
    .param1_values_len = ARRAY_SIZE(position_values),
};

static const struct behavior_parameter_metadata metadata = {
    .sets_len = 1,
    .sets = &position_set,
};
#endif

static const struct behavior_driver_api conn_switch_driver_api = {
    .binding_pressed = on_pressed,
    .binding_released = on_released,
    .locality = BEHAVIOR_LOCALITY_CENTRAL,
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
    .parameter_metadata = &metadata,
#endif
};

BEHAVIOR_DT_INST_DEFINE(0, conn_switch_init, NULL, NULL, NULL, POST_KERNEL,
                        CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &conn_switch_driver_api);

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */
