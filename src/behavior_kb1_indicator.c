/*
 * Copyright (c) 2026 zmk-kb1 contributors
 *
 * SPDX-License-Identifier: MIT
 *
 * &kb1_ind KB1_IND_BATTERY: while held, the RGB LED shows the battery level
 * (green at 70 % or more, blue at 30 % or more, red below), as Fn+B does on
 * the stock firmware.
 */

#define DT_DRV_COMPAT zmk_behavior_kb1_indicator

#include <zephyr/device.h>
#include <drivers/behavior.h>

#include <zmk/behavior.h>
#include <dt-bindings/kb1/indicator.h>

#include <kb1/leds.h>

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

static int on_pressed(struct zmk_behavior_binding *binding,
                      struct zmk_behavior_binding_event event) {
    if (binding->param1 != KB1_IND_BATTERY) {
        return -ENOTSUP;
    }
    kb1_leds_battery_display(true);
    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_released(struct zmk_behavior_binding *binding,
                       struct zmk_behavior_binding_event event) {
    if (binding->param1 != KB1_IND_BATTERY) {
        return -ENOTSUP;
    }
    kb1_leds_battery_display(false);
    return ZMK_BEHAVIOR_OPAQUE;
}

#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
static const struct behavior_parameter_value_metadata values[] = {
    {
        .display_name = "Battery level",
        .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE,
        .value = KB1_IND_BATTERY,
    },
};

static const struct behavior_parameter_metadata_set set = {
    .param1_values = values,
    .param1_values_len = ARRAY_SIZE(values),
};

static const struct behavior_parameter_metadata metadata = {
    .sets_len = 1,
    .sets = &set,
};
#endif

static const struct behavior_driver_api kb1_indicator_driver_api = {
    .binding_pressed = on_pressed,
    .binding_released = on_released,
    .locality = BEHAVIOR_LOCALITY_CENTRAL,
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
    .parameter_metadata = &metadata,
#endif
};

BEHAVIOR_DT_INST_DEFINE(0, NULL, NULL, NULL, NULL, POST_KERNEL,
                        CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &kb1_indicator_driver_api);

#endif
