/*
 * Copyright (c) 2026 zmk-kb1 contributors
 *
 * SPDX-License-Identifier: MIT
 *
 * Key events that arrive before ZMK's keymap is ready are held back and
 * replayed once it is.
 *
 * With ZMK Studio, the keymap's bindings are empty until keymap_init()
 * copies the stock keymap in (app/src/keymap.c). That runs at the same init
 * level as the physical layouts, which enable the kscan first. A switch or
 * key already on at power-on is reported one debounce time later (the
 * Mac/Win switch after about 60 ms); if the inits in between take longer
 * (on the B1 Pro they do: settings are read from flash), the keymap finds no
 * binding and drops the event. The Win position would then be ignored until
 * the switch moves, and so would the BT position of the connection switch.
 *
 * This listener comes before every other position listener (it is the first
 * source of the module, and ZMK's own listeners come after the module's). Until
 * the keymap is ready it records the events and stops them; then it replays
 * them in order, on the system work queue like the kscan events, before any
 * later event goes on.
 */

#include <string.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>

#include <zmk/event_manager.h>
#include <zmk/events/position_state_changed.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

static struct zmk_position_state_changed held[CONFIG_KB1_EARLY_EVENTS_MAX];
static int held_count;
static int dropped;
static atomic_t keymap_ready;

/* Runs on the system work queue only (from the listener or the work item). */
static void replay_held(void) {
    struct zmk_position_state_changed events[CONFIG_KB1_EARLY_EVENTS_MAX];
    int count = held_count;

    memcpy(events, held, count * sizeof(events[0]));
    held_count = 0;
    if (count > 0 || dropped > 0) {
        LOG_INF("Replaying %d key events from before the keymap was ready (%d dropped)", count,
                dropped);
    }
    for (int i = 0; i < count; i++) {
        raise_zmk_position_state_changed(events[i]);
    }
}

static void replay_work_handler(struct k_work *work) { replay_held(); }
static K_WORK_DEFINE(replay_work, replay_work_handler);

static int early_events_listener(const zmk_event_t *eh) {
    const struct zmk_position_state_changed *ev = as_zmk_position_state_changed(eh);
    if (ev == NULL) {
        return ZMK_EV_EVENT_BUBBLE;
    }
    if (atomic_get(&keymap_ready)) {
        /* Events held back go first, even if the work item has not run yet. */
        if (held_count > 0) {
            replay_held();
        }
        return ZMK_EV_EVENT_BUBBLE;
    }
    if (held_count < ARRAY_SIZE(held)) {
        held[held_count++] = *ev;
    } else {
        dropped++;
    }
    return ZMK_EV_EVENT_HANDLED;
}

ZMK_LISTENER(kb1_early_events, early_events_listener);
ZMK_SUBSCRIPTION(kb1_early_events, zmk_position_state_changed);

/* keymap_init() is a SYS_INIT at APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY:
 * one priority later, it has run. */
static int early_events_keymap_ready(void) {
    atomic_set(&keymap_ready, 1);
    k_work_submit(&replay_work);
    return 0;
}

BUILD_ASSERT(CONFIG_APPLICATION_INIT_PRIORITY < 99, "no init priority after the keymap's");
SYS_INIT(early_events_keymap_ready, APPLICATION, UTIL_INC(CONFIG_APPLICATION_INIT_PRIORITY));
