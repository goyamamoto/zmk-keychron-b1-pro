/*
 * Copyright (c) 2020-2021 The ZMK Contributors
 * Copyright (c) 2026 zmk-kb1 contributors
 *
 * SPDX-License-Identifier: MIT
 *
 * GPIO key matrix without diodes (col2row: columns driven, rows read).
 *
 * Based on ZMK's kscan_gpio_matrix.c at 9ebbeff0a8b69a42f14aec022cdf16c7a107b9e0,
 * with two changes, both as the stock Keychron firmware of the B1 Pro does:
 *
 * 1. Column drive. Upstream drives every column all the time, low when not
 *    scanned. Without diodes, two pressed keys on one row then connect the
 *    scanned (high) column to a low one, and the row reads wrong. Here a
 *    column is an output only while it is scanned: driven high, read, driven
 *    low to discharge it, then made an input (high impedance) again. While
 *    waiting for a key (interrupt mode) all columns are driven high, which
 *    cannot conflict.
 *
 * 2. Ghosts. Without diodes, keys linked through pressed keys read as pressed
 *    (three corners of a rectangle make the fourth). The raw readings of the
 *    whole pass are collected first, then gf_mask() (ghost_filter.c) keeps
 *    new presses out of ambiguous rows before debouncing.
 */

#define DT_DRV_COMPAT zmk_kb1_kscan_gpio_matrix_nodiode

#include "kscan_gpio.h"

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/kscan.h>
#include <zephyr/pm/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/__assert.h>
#include <zephyr/sys/util.h>
#include <string.h>

#include <zmk/debounce.h>

#include <kb1/ghost_filter.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define INST_ROWS_LEN(n) DT_INST_PROP_LEN(n, row_gpios)
#define INST_COLS_LEN(n) DT_INST_PROP_LEN(n, col_gpios)
#define INST_MATRIX_LEN(n) (INST_ROWS_LEN(n) * INST_COLS_LEN(n))

#define USE_POLLING IS_ENABLED(CONFIG_KB1_KSCAN_MATRIX_POLLING)
#define USE_INTERRUPTS (!USE_POLLING)

#define COND_INTERRUPTS(code) COND_CODE_1(CONFIG_KB1_KSCAN_MATRIX_POLLING, (), code)

#define KSCAN_GPIO_ROW_CFG_INIT(idx, inst_idx)                                                     \
    KSCAN_GPIO_GET_BY_IDX(DT_DRV_INST(inst_idx), row_gpios, idx)
#define KSCAN_GPIO_COL_CFG_INIT(idx, inst_idx)                                                     \
    KSCAN_GPIO_GET_BY_IDX(DT_DRV_INST(inst_idx), col_gpios, idx)

struct kscan_matrix_irq_callback {
    const struct device *dev;
    struct gpio_callback callback;
};

struct kscan_matrix_data {
    const struct device *dev;
    /** The rows. */
    struct kscan_gpio_list inputs;
    kscan_callback_t callback;
    struct k_work_delayable work;
#if USE_INTERRUPTS
    /** Array of length rows. */
    struct kscan_matrix_irq_callback *irqs;
#endif
    /** Timestamp of the current or scheduled scan. */
    int64_t scan_time;
    /** Debounce state, flattened (col * rows + row). */
    struct zmk_debounce_state *matrix_state;
    /** Pins disconnected by PM suspend: leave them alone until resume. */
    bool suspended;
    /** Per-pass buffers (kept off the stack: the first reads run on the
     * small main stack during init). */
    uint32_t raw[GF_MAX_ROWS];
    uint32_t pressed[GF_MAX_ROWS];
    uint32_t masked[GF_MAX_ROWS];
};

struct kscan_matrix_config {
    /** The columns. */
    struct kscan_gpio_list outputs;
    struct zmk_debounce_config debounce_config;
    size_t rows;
    size_t cols;
    int32_t debounce_scan_period_ms;
    int32_t poll_period_ms;
};

static int state_index_rc(const struct kscan_matrix_config *config, const int row, const int col) {
    __ASSERT(row < config->rows, "Invalid row %i", row);
    __ASSERT(col < config->cols, "Invalid column %i", col);

    return (col * config->rows) + row;
}

/* A column not being scanned: discharge it, then leave it floating. */
static int column_release(const struct gpio_dt_spec *gpio) {
    int err = gpio_pin_configure_dt(gpio, GPIO_OUTPUT_INACTIVE);
    if (err) {
        return err;
    }
    return gpio_pin_configure_dt(gpio, GPIO_INPUT);
}

static int column_drive(const struct gpio_dt_spec *gpio) {
    return gpio_pin_configure_dt(gpio, GPIO_OUTPUT_ACTIVE);
}

static int kscan_matrix_release_all_columns(const struct device *dev) {
    const struct kscan_matrix_config *config = dev->config;

    for (int i = 0; i < config->outputs.len; i++) {
        int err = column_release(&config->outputs.gpios[i].spec);
        if (err) {
            LOG_ERR("Failed to release column %i: %i", i, err);
            return err;
        }
    }
    return 0;
}

#if USE_INTERRUPTS
static int kscan_matrix_drive_all_columns(const struct device *dev) {
    const struct kscan_matrix_config *config = dev->config;

    for (int i = 0; i < config->outputs.len; i++) {
        int err = column_drive(&config->outputs.gpios[i].spec);
        if (err) {
            LOG_ERR("Failed to drive column %i: %i", i, err);
            return err;
        }
    }
    return 0;
}

static int kscan_matrix_interrupt_configure(const struct device *dev, const gpio_flags_t flags) {
    const struct kscan_matrix_data *data = dev->data;

    for (int i = 0; i < data->inputs.len; i++) {
        const struct gpio_dt_spec *gpio = &data->inputs.gpios[i].spec;

        int err = gpio_pin_interrupt_configure_dt(gpio, flags);
        if (err) {
            LOG_ERR("Unable to configure interrupt for pin %u on %s", gpio->pin, gpio->port->name);
            return err;
        }
    }

    return 0;
}

static int kscan_matrix_interrupt_enable(const struct device *dev) {
    int err = kscan_matrix_interrupt_configure(dev, GPIO_INT_LEVEL_ACTIVE);
    if (err) {
        return err;
    }

    // While interrupts are enabled, drive all columns so that a pressed key
    // triggers an interrupt. All columns are at the same level: no conflict.
    return kscan_matrix_drive_all_columns(dev);
}

static int kscan_matrix_interrupt_disable(const struct device *dev) {
    const struct kscan_matrix_data *data = dev->data;

    int err = kscan_matrix_interrupt_configure(dev, GPIO_INT_DISABLE);
    if (err) {
        return err;
    }

    /* After PM suspend the pins are disconnected (lowest current in System
     * OFF). A later disable, as ZMK's composite kscan issues when it is
     * suspended in turn, must not reconnect the columns as floating inputs. */
    if (data->suspended) {
        return 0;
    }
    return kscan_matrix_release_all_columns(dev);
}

static void kscan_matrix_irq_callback_handler(const struct device *port, struct gpio_callback *cb,
                                              const gpio_port_pins_t pin) {
    struct kscan_matrix_irq_callback *irq_data =
        CONTAINER_OF(cb, struct kscan_matrix_irq_callback, callback);
    struct kscan_matrix_data *data = irq_data->dev->data;

    // Disable our interrupts temporarily to avoid re-entry while we scan.
    kscan_matrix_interrupt_disable(data->dev);

    data->scan_time = k_uptime_get();

    k_work_reschedule(&data->work, K_NO_WAIT);
}
#endif

static void kscan_matrix_read_continue(const struct device *dev) {
    const struct kscan_matrix_config *config = dev->config;
    struct kscan_matrix_data *data = dev->data;

    data->scan_time += config->debounce_scan_period_ms;

    k_work_reschedule(&data->work, K_TIMEOUT_ABS_MS(data->scan_time));
}

static void kscan_matrix_read_end(const struct device *dev) {
#if USE_INTERRUPTS
    // Return to waiting for an interrupt.
    kscan_matrix_interrupt_enable(dev);
#else
    struct kscan_matrix_data *data = dev->data;
    const struct kscan_matrix_config *config = dev->config;

    data->scan_time += config->poll_period_ms;

    // Return to polling slowly.
    k_work_reschedule(&data->work, K_TIMEOUT_ABS_MS(data->scan_time));
#endif
}

static int kscan_matrix_read(const struct device *dev) {
    struct kscan_matrix_data *data = dev->data;
    const struct kscan_matrix_config *config = dev->config;
    uint32_t *raw = data->raw;
    uint32_t *pressed = data->pressed;
    uint32_t *masked = data->masked;

    memset(data->raw, 0, sizeof(data->raw));
    memset(data->pressed, 0, sizeof(data->pressed));

    // Scan the matrix, one column at a time, into raw[row] bit column.
    for (int i = 0; i < config->outputs.len; i++) {
        const struct kscan_gpio *out_gpio = &config->outputs.gpios[i];

        int err = column_drive(&out_gpio->spec);
        if (err) {
            LOG_ERR("Failed to drive column %i: %i", out_gpio->index, err);
            return err;
        }

#if CONFIG_KB1_KSCAN_MATRIX_WAIT_BEFORE_INPUTS > 0
        k_busy_wait(CONFIG_KB1_KSCAN_MATRIX_WAIT_BEFORE_INPUTS);
#endif
        struct kscan_gpio_port_state state = {0};

        for (int j = 0; j < data->inputs.len; j++) {
            const struct kscan_gpio *in_gpio = &data->inputs.gpios[j];
            const int active = kscan_gpio_pin_get(in_gpio, &state);
            if (active < 0) {
                LOG_ERR("Failed to read port %s: %i", in_gpio->spec.port->name, active);
                return active;
            }
            if (active) {
                raw[in_gpio->index] |= BIT(out_gpio->index);
            }
        }

        err = column_release(&out_gpio->spec);
        if (err) {
            LOG_ERR("Failed to release column %i: %i", out_gpio->index, err);
            return err;
        }

#if CONFIG_KB1_KSCAN_MATRIX_WAIT_BETWEEN_OUTPUTS > 0
        k_busy_wait(CONFIG_KB1_KSCAN_MATRIX_WAIT_BETWEEN_OUTPUTS);
#endif
    }

    // Keep new presses out of ambiguous rows (ghosts), then debounce.
    for (int r = 0; r < config->rows; r++) {
        for (int c = 0; c < config->cols; c++) {
            if (zmk_debounce_is_pressed(&data->matrix_state[state_index_rc(config, r, c)])) {
                pressed[r] |= BIT(c);
            }
        }
    }
    gf_mask(raw, pressed, masked, config->rows);

    for (int r = 0; r < config->rows; r++) {
        for (int c = 0; c < config->cols; c++) {
            zmk_debounce_update(&data->matrix_state[state_index_rc(config, r, c)],
                                (masked[r] & BIT(c)) != 0, config->debounce_scan_period_ms,
                                &config->debounce_config);
        }
    }

    // Process the new state.
    bool continue_scan = false;

    for (int r = 0; r < config->rows; r++) {
        for (int c = 0; c < config->cols; c++) {
            const int index = state_index_rc(config, r, c);
            struct zmk_debounce_state *state = &data->matrix_state[index];

            if (zmk_debounce_get_changed(state)) {
                const bool is_pressed = zmk_debounce_is_pressed(state);

                LOG_DBG("Sending event at %i,%i state %s", r, c, is_pressed ? "on" : "off");
                /* ZMK reads the kscan at boot before it sets the callback. */
                if (data->callback) {
                    data->callback(dev, r, c, is_pressed);
                }
            }

            // Keep scanning while any key reads pressed, also a held-back one.
            continue_scan = continue_scan || zmk_debounce_is_active(state) || raw[r];
        }
    }

    if (continue_scan) {
        kscan_matrix_read_continue(dev);
    } else {
        kscan_matrix_read_end(dev);
    }

    return 0;
}

static void kscan_matrix_work_handler(struct k_work *work) {
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    struct kscan_matrix_data *data = CONTAINER_OF(dwork, struct kscan_matrix_data, work);
    kscan_matrix_read(data->dev);
}

static int kscan_matrix_configure(const struct device *dev, const kscan_callback_t callback) {
    struct kscan_matrix_data *data = dev->data;

    if (!callback) {
        return -EINVAL;
    }

    data->callback = callback;
    return 0;
}

static int kscan_matrix_enable(const struct device *dev) {
    struct kscan_matrix_data *data = dev->data;

    data->scan_time = k_uptime_get();

    // Read will automatically start interrupts/polling once done.
    return kscan_matrix_read(dev);
}

static int kscan_matrix_disable(const struct device *dev) {
    struct kscan_matrix_data *data = dev->data;

    k_work_cancel_delayable(&data->work);

#if USE_INTERRUPTS
    return kscan_matrix_interrupt_disable(dev);
#else
    return 0;
#endif
}

static int kscan_matrix_init_input_inst(const struct device *dev, const struct kscan_gpio *gpio) {
    if (!device_is_ready(gpio->spec.port)) {
        LOG_ERR("GPIO is not ready: %s", gpio->spec.port->name);
        return -ENODEV;
    }

    int err = gpio_pin_configure_dt(&gpio->spec, GPIO_INPUT);
    if (err) {
        LOG_ERR("Unable to configure pin %u on %s for input", gpio->spec.pin,
                gpio->spec.port->name);
        return err;
    }

#if USE_INTERRUPTS
    struct kscan_matrix_data *data = dev->data;
    struct kscan_matrix_irq_callback *irq = &data->irqs[gpio->index];

    irq->dev = dev;
    gpio_init_callback(&irq->callback, kscan_matrix_irq_callback_handler, BIT(gpio->spec.pin));
    err = gpio_add_callback(gpio->spec.port, &irq->callback);
    if (err) {
        LOG_ERR("Error adding the callback to the input device: %i", err);
        return err;
    }
#endif

    return 0;
}

static int kscan_matrix_init_inputs(const struct device *dev) {
    const struct kscan_matrix_data *data = dev->data;

    for (int i = 0; i < data->inputs.len; i++) {
        int err = kscan_matrix_init_input_inst(dev, &data->inputs.gpios[i]);
        if (err) {
            return err;
        }
    }

    return 0;
}

#if IS_ENABLED(CONFIG_PM_DEVICE)

static int kscan_matrix_disconnect_all(const struct device *dev) {
    const struct kscan_matrix_data *data = dev->data;
    const struct kscan_matrix_config *config = dev->config;

    for (int i = 0; i < data->inputs.len; i++) {
        int err = gpio_pin_configure_dt(&data->inputs.gpios[i].spec, GPIO_DISCONNECTED);
        if (err) {
            return err;
        }
    }
    for (int i = 0; i < config->outputs.len; i++) {
        int err = gpio_pin_configure_dt(&config->outputs.gpios[i].spec, GPIO_DISCONNECTED);
        if (err) {
            return err;
        }
    }

    return 0;
}

#endif // IS_ENABLED(CONFIG_PM_DEVICE)

static void kscan_matrix_setup_pins(const struct device *dev) {
    kscan_matrix_init_inputs(dev);
    kscan_matrix_release_all_columns(dev);
}

static int kscan_matrix_init(const struct device *dev) {
    struct kscan_matrix_data *data = dev->data;

    data->dev = dev;

    // Sort inputs by port so we can read each port just once per scan.
    kscan_gpio_list_sort_by_port(&data->inputs);

    k_work_init_delayable(&data->work, kscan_matrix_work_handler);

#if IS_ENABLED(CONFIG_PM_DEVICE)
    pm_device_init_suspended(dev);

#if IS_ENABLED(CONFIG_PM_DEVICE_RUNTIME)
    pm_device_runtime_enable(dev);
#endif

#else
    kscan_matrix_setup_pins(dev);
#endif

    return 0;
}

#if IS_ENABLED(CONFIG_PM_DEVICE)

static int kscan_matrix_pm_action(const struct device *dev, enum pm_device_action action) {
    switch (action) {
    case PM_DEVICE_ACTION_SUSPEND: {
        struct kscan_matrix_data *data = dev->data;
        int err = kscan_matrix_disable(dev);
        kscan_matrix_disconnect_all(dev);
        data->suspended = true;
        return err;
    }
    case PM_DEVICE_ACTION_RESUME: {
        struct kscan_matrix_data *data = dev->data;
        data->suspended = false;
        kscan_matrix_setup_pins(dev);
        return kscan_matrix_enable(dev);
    }
    default:
        return -ENOTSUP;
    }
}

#endif // IS_ENABLED(CONFIG_PM_DEVICE)

static const struct kscan_driver_api kscan_matrix_api = {
    .config = kscan_matrix_configure,
    .enable_callback = kscan_matrix_enable,
    .disable_callback = kscan_matrix_disable,
};

#define KSCAN_MATRIX_INIT(n)                                                                       \
    BUILD_ASSERT(DT_INST_ENUM_IDX(n, diode_direction) == 1,                                        \
                 "zmk,kb1-kscan-gpio-matrix-nodiode supports col2row only");                       \
    BUILD_ASSERT(INST_ROWS_LEN(n) <= GF_MAX_ROWS, "too many rows");                                \
    BUILD_ASSERT(INST_COLS_LEN(n) <= GF_MAX_COLS, "too many columns");                             \
    BUILD_ASSERT(DT_INST_PROP(n, debounce_press_ms) <= DEBOUNCE_COUNTER_MAX,                       \
                 "debounce-press-ms is too large");                                                \
    BUILD_ASSERT(DT_INST_PROP(n, debounce_release_ms) <= DEBOUNCE_COUNTER_MAX,                     \
                 "debounce-release-ms is too large");                                              \
                                                                                                   \
    static struct kscan_gpio kscan_matrix_rows_##n[] = {                                           \
        LISTIFY(INST_ROWS_LEN(n), KSCAN_GPIO_ROW_CFG_INIT, (, ), n)};                              \
                                                                                                   \
    static struct kscan_gpio kscan_matrix_cols_##n[] = {                                           \
        LISTIFY(INST_COLS_LEN(n), KSCAN_GPIO_COL_CFG_INIT, (, ), n)};                              \
                                                                                                   \
    static struct zmk_debounce_state kscan_matrix_state_##n[INST_MATRIX_LEN(n)];                   \
                                                                                                   \
    COND_INTERRUPTS(                                                                               \
        (static struct kscan_matrix_irq_callback kscan_matrix_irqs_##n[INST_ROWS_LEN(n)];))        \
                                                                                                   \
    static struct kscan_matrix_data kscan_matrix_data_##n = {                                      \
        .inputs = KSCAN_GPIO_LIST(kscan_matrix_rows_##n),                                          \
        .matrix_state = kscan_matrix_state_##n,                                                    \
        COND_INTERRUPTS((.irqs = kscan_matrix_irqs_##n, ))};                                       \
                                                                                                   \
    static const struct kscan_matrix_config kscan_matrix_config_##n = {                            \
        .rows = ARRAY_SIZE(kscan_matrix_rows_##n),                                                 \
        .cols = ARRAY_SIZE(kscan_matrix_cols_##n),                                                 \
        .outputs = KSCAN_GPIO_LIST(kscan_matrix_cols_##n),                                         \
        .debounce_config =                                                                         \
            {                                                                                      \
                .debounce_press_ms = DT_INST_PROP(n, debounce_press_ms),                           \
                .debounce_release_ms = DT_INST_PROP(n, debounce_release_ms),                       \
            },                                                                                     \
        .debounce_scan_period_ms = DT_INST_PROP(n, debounce_scan_period_ms),                       \
        .poll_period_ms = DT_INST_PROP(n, poll_period_ms),                                         \
    };                                                                                             \
                                                                                                   \
    PM_DEVICE_DT_INST_DEFINE(n, kscan_matrix_pm_action);                                           \
                                                                                                   \
    DEVICE_DT_INST_DEFINE(n, &kscan_matrix_init, PM_DEVICE_DT_INST_GET(n), &kscan_matrix_data_##n, \
                          &kscan_matrix_config_##n, POST_KERNEL, CONFIG_KSCAN_INIT_PRIORITY,       \
                          &kscan_matrix_api);

DT_INST_FOREACH_STATUS_OKAY(KSCAN_MATRIX_INIT);
