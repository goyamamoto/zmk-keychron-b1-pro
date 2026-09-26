/*
 * Copyright (c) 2026 zmk-kb1 contributors
 *
 * SPDX-License-Identifier: MIT
 *
 * Optional wake from System OFF on a key press, for idle sleep only.
 *
 * ZMK's soft off suspends every device, then resumes the devices listed as
 * wakeup sources, then suspends again every device that is not a wakeup
 * source. This device is listed; when it is resumed and idle sleep was
 * requested (kb1_power.c), it marks the matrix kscan as a wakeup source and
 * resumes it: the matrix drives its columns and arms its rows (GPIO SENSE),
 * so that a key press wakes the keyboard. For the
 * other ways of turning off (the 2.4G position, the cable position without
 * USB, low battery) it does nothing, and only the switch and USB wake it.
 */

#define DT_DRV_COMPAT zmk_kb1_key_waker

#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <zephyr/pm/device.h>

#include <kb1/power.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

struct key_waker_config {
    const struct device *kscan;
};

static int key_waker_pm_action(const struct device *dev, enum pm_device_action action) {
    const struct key_waker_config *config = dev->config;

    switch (action) {
    case PM_DEVICE_ACTION_RESUME:
        if (!kb1_idle_sleep_requested()) {
            return 0;
        }
        LOG_DBG("Key waker: arming the matrix");
        /* zmk_pm_soft_off() suspends every device that is not a wakeup
         * source after resuming the wakers: mark the matrix as one, so that
         * it stays armed (all columns driven, rows sensing) through power-off. */
        if (!pm_device_wakeup_enable(config->kscan, true)) {
            LOG_ERR("Key waker: the matrix cannot be a wakeup source");
            return -ENOTSUP;
        }
        return pm_device_action_run(config->kscan, PM_DEVICE_ACTION_RESUME);
    case PM_DEVICE_ACTION_SUSPEND:
        return 0;
    default:
        return -ENOTSUP;
    }
}

static int key_waker_init(const struct device *dev) {
    pm_device_init_suspended(dev);
    pm_device_wakeup_enable(dev, true);
    return 0;
}

#define KEY_WAKER_INST(n)                                                                          \
    static const struct key_waker_config key_waker_config_##n = {                                  \
        .kscan = DEVICE_DT_GET(DT_INST_PHANDLE(n, kscan)),                                         \
    };                                                                                             \
    PM_DEVICE_DT_INST_DEFINE(n, key_waker_pm_action);                                              \
    DEVICE_DT_INST_DEFINE(n, key_waker_init, PM_DEVICE_DT_INST_GET(n), NULL,                       \
                          &key_waker_config_##n, POST_KERNEL,                                      \
                          CONFIG_KB1_KEY_WAKER_INIT_PRIORITY, NULL);

DT_INST_FOREACH_STATUS_OKAY(KEY_WAKER_INST)
