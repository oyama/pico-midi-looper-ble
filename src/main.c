/* main.c
 * BLE MIDI looper for Raspberry Pi Pico W.
 * A minimal 2-bars loop recorder using a single button to record and switch tracks.
 *
 * Copyright 2025, Hiroyuki OYAMA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <stdio.h>
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"

#include "ble_midi.h"
#include "looper.h"

/*
 * Entry point for the Pico MIDI Looper application.
 *
 * Looper is driven by two input sources:
 *  - Timer ticks (looper_handle_tick) for sequencer state progression
 *  - Button events (looper_handle_input) for user-driven updates
 */
int main(void) {
    stdio_init_all();
    cyw43_arch_init();
    looper_update_bpm(LOOPER_DEFAULT_BPM);
    ble_midi_init(looper_handle_tick, looper_get_step_interval_ms());

    printf("[MAIN] Pico MIDI Looper start\n");
    while (true) {
        looper_handle_input();
        sleep_us(500);
    }
    return 0;
}
