/*
 * Copyright 2025, Hiroyuki OYAMA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#pragma once

#include <hardware/sync.h>

typedef enum {
    BUTTON_EVENT_NONE,
    BUTTON_EVENT_DOWN,
    BUTTON_EVENT_CLICK_RELEASE,
    BUTTON_EVENT_HOLD_BEGIN,
    BUTTON_EVENT_HOLD_RELEASE,
    BUTTON_EVENT_LONG_HOLD_BEGIN,
    BUTTON_EVENT_LONG_HOLD_RELEASE,
    BUTTON_EVENT_VERY_LONG_HOLD_BEGIN,
    BUTTON_EVENT_VERY_LONG_HOLD_RELEASE,
} button_event_t;

bool __no_inline_not_in_flash_func(bb_get_bootsel_button)();

button_event_t button_poll_event(void);
