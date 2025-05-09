/*
 * Copyright 2025, Hiroyuki OYAMA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#pragma once

#include "btstack.h"

void ble_midi_init(void (*step_cb)(btstack_timer_source_t *ts), uint32_t step_period_ms);

bool ble_midi_is_connected(void);

void ble_midi_send_note(uint8_t channel, uint8_t note, uint8_t velocity);
