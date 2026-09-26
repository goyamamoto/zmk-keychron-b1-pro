/*
 * Copyright (c) 2026 zmk-kb1 contributors
 *
 * SPDX-License-Identifier: MIT
 *
 * Wake from System OFF when the connection switch moves.
 *
 * The switch inputs are levels, and so is the nRF52840's wake from System OFF
 * (GPIO SENSE -> DETECT). When this device is resumed just before power-off,
 * each input is armed for the level opposite to the one it has then, so any
 * movement of the switch wakes the keyboard, whichever position it was turned
 * off in.
 *
 * SENSE is written directly with the nrf_gpio HAL instead of through the
 * Zephyr GPIO interrupt API, and the GPIOTE PORT interrupt is disabled first.
 * If the switch moved between arming and power-off, the PORT interrupt would
 * run nrfx's handler (which flips SENSE of latched pins) and the direct
 * kscan's callbacks on the same pins. With the interrupt off nothing touches
 * the pins. A pin already at its sensed level makes DETECT high, so the
 * nRF52840 is expected to wake as soon as it is off (not yet checked on this
 * hardware): it reboots and reads the new position.
 * Waking from System OFF does not need the interrupt, only DETECT. This runs
 * just before power-off (ZMK's soft off resumes wake sources last), so the
 * PORT interrupt stays off until the reset.
 */

#define DT_DRV_COMPAT zmk_kb1_switch_waker

#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <zephyr/pm/device.h>
#include <zephyr/kernel.h>
#include <hal/nrf_gpio.h>
#include <hal/nrf_gpiote.h>
#include <soc_nrf_common.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

struct switch_waker_config {
    size_t count;
    uint32_t pins[];
};

static int switch_waker_arm(const struct device *dev) {
    const struct switch_waker_config *config = dev->config;

    nrf_gpiote_int_disable(NRF_GPIOTE, NRF_GPIOTE_INT_PORT_MASK);

    for (size_t i = 0; i < config->count; i++) {
        /* The board pulls these inputs up; no internal pull. */
        nrf_gpio_cfg_input(config->pins[i], NRF_GPIO_PIN_NOPULL);
    }
    /* Let the input buffers settle before reading IN. */
    k_busy_wait(10);

    for (size_t i = 0; i < config->count; i++) {
        uint32_t pin = config->pins[i];
        nrf_gpio_pin_sense_t sense =
            nrf_gpio_pin_read(pin) ? NRF_GPIO_PIN_SENSE_LOW : NRF_GPIO_PIN_SENSE_HIGH;
        nrf_gpio_cfg_sense_set(pin, sense);
        LOG_DBG("Switch waker: pin %u armed for %s", pin,
                sense == NRF_GPIO_PIN_SENSE_LOW ? "low" : "high");
    }
    return 0;
}

static int switch_waker_disarm(const struct device *dev) {
    const struct switch_waker_config *config = dev->config;

    for (size_t i = 0; i < config->count; i++) {
        nrf_gpio_cfg_sense_set(config->pins[i], NRF_GPIO_PIN_NOSENSE);
    }
    return 0;
}

static int switch_waker_pm_action(const struct device *dev, enum pm_device_action action) {
    switch (action) {
    case PM_DEVICE_ACTION_RESUME:
        return switch_waker_arm(dev);
    case PM_DEVICE_ACTION_SUSPEND:
        return switch_waker_disarm(dev);
    default:
        return -ENOTSUP;
    }
}

static int switch_waker_init(const struct device *dev) {
    /* Idle until soft off resumes it; the direct kscan owns the pins until then. */
    pm_device_init_suspended(dev);
    pm_device_wakeup_enable(dev, true);
    return 0;
}

#define SWITCH_WAKER_PIN(node_id, prop, idx) NRF_DT_GPIOS_TO_PSEL_BY_IDX(node_id, prop, idx),

#define SWITCH_WAKER_INST(n)                                                                       \
    static const struct switch_waker_config switch_waker_config_##n = {                            \
        .count = DT_INST_PROP_LEN(n, gpios),                                                       \
        .pins = {DT_INST_FOREACH_PROP_ELEM(n, gpios, SWITCH_WAKER_PIN)},                           \
    };                                                                                             \
    PM_DEVICE_DT_INST_DEFINE(n, switch_waker_pm_action);                                           \
    DEVICE_DT_INST_DEFINE(n, switch_waker_init, PM_DEVICE_DT_INST_GET(n), NULL,                    \
                          &switch_waker_config_##n, POST_KERNEL,                                   \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, NULL);

DT_INST_FOREACH_STATUS_OKAY(SWITCH_WAKER_INST)
