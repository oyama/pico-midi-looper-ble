/*
 * tap_tempo.c
 *
 * Tap-tempo detection and BPM estimation.
 *
 * ENTER : BUTTON_EVENT_LONG_HOLD_RELEASE (≥ 2 s)
 * EXIT : BUTTON_EVENT_HOLD_RELEASE (≥ 0.5 s)
 * Resets after 1 s inactivity or after 4 taps.
 *
 * Copyright 2025, Hiroyuki OYAMA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <stdbool.h>
#include <stdint.h>
#include "pico/time.h"

#include "tap_tempo.h"

// Configuration constants
enum {
    TAP_MIN_BPM = 40,  // clamp lower bound
    TAP_MAX_BPM = 240,
    TAP_MAX_TAPS = 4,
    TIMEOUT_US = 1000 * 1000,  // 1 s idle-timeout
};

typedef enum { TT_IDLE, TT_COLLECT } tt_state_t;

// Internal state
typedef struct {
    tt_state_t state;
    uint64_t stamp[TAP_MAX_TAPS];
    uint8_t count;   // current tap count [0-4]
    bool is_active;  // detect COLLECT mode
} tap_ctx_t;

static tap_ctx_t ctx = {0};
static uint16_t latest_bpm = 120;

// convert intervals to BPM and clamp to range
static uint16_t calc_bpm(uint64_t first_us, uint64_t last_us, uint8_t intervals) {
    uint64_t delta_us = last_us - first_us;
    if (delta_us == 0)
        return TAP_MIN_BPM;

    // Convert µs interval to BPM using integer math
    uint32_t delta_ms = (uint32_t)((delta_us + 500) / 1000);
    uint32_t bpm = (60000U * intervals + delta_ms / 2) / delta_ms;

    // clamp to TAP_MIN_BPM .. TAP_MAX_BPM
    if (bpm < TAP_MIN_BPM)
        bpm = TAP_MIN_BPM;
    if (bpm > TAP_MAX_BPM)
        bpm = TAP_MAX_BPM;

    return (uint16_t)bpm;
}

// Public API: main FSM event handler
tap_result_t taptempo_handle_event(button_event_t ev) {
    uint64_t now = time_us_64();

    switch (ctx.state) {
        case TT_IDLE:
            if (ev == BUTTON_EVENT_HOLD_RELEASE || ev == BUTTON_EVENT_LONG_HOLD_RELEASE) {
                ctx.state = TT_IDLE;
                return TAP_EXIT;
            } else if (ev == BUTTON_EVENT_CLICK_RELEASE) {
                ctx.count = 0;
                ctx.state = TT_COLLECT;
                ctx.stamp[0] = now;  // mark entry time
            }
            return TAP_NONE;

        case TT_COLLECT:
            if (ev == BUTTON_EVENT_HOLD_RELEASE || ev == BUTTON_EVENT_LONG_HOLD_RELEASE) {
                ctx.state = TT_IDLE;
                return TAP_EXIT;
            }
            if (ctx.count && (now - ctx.stamp[ctx.count ? ctx.count - 1 : 0]) > TIMEOUT_US) {
                ctx.count = 0;
                ctx.state = TT_IDLE;
                return TAP_NONE;
            } else if (ev == BUTTON_EVENT_CLICK_RELEASE) {
                if (ctx.count < TAP_MAX_TAPS)
                    ctx.stamp[ctx.count++] = now;

                if (ctx.count >= 2) {
                    latest_bpm = calc_bpm(ctx.stamp[0], ctx.stamp[ctx.count - 1], ctx.count - 1);
                }
                /* 2 taps → PRELIM, 3 taps → FINAL, 4 taps → FINAL+reset */
                if (ctx.count == 2) {
                    return TAP_PRELIM;
                } else if (ctx.count == 3) {
                    return TAP_FINAL;
                } else if (ctx.count == TAP_MAX_TAPS) {  // 4 taps
                    ctx.count = 0;                       // auto-reset
                    return TAP_FINAL;
                }
            }
            return TAP_NONE;
        default:
            /* should not reach here */
            ctx.state = TT_IDLE;
            return TAP_NONE;
    }
}

uint16_t taptempo_get_bpm(void) { return latest_bpm; }
bool taptempo_active(void) { return ctx.state == TT_COLLECT; }
