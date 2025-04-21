/*
 * Copyright 2025, Hiroyuki OYAMA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <stdio.h>

#include "ble_midi.h"
#include "bootsel_button.h"
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"

#define STEP_MS 120
#define NUM_STEPS 16
#define CLICK_DIVISION (NUM_STEPS / 4)

#define LONGPRESS_MS 500
#define DEBOUNCE_MS_SHORT 10

typedef enum {
    TRACK_BASS,
    TRACK_SNARE,
} track_t;

static track_t current_track = TRACK_BASS;

static bool bass_data[NUM_STEPS] = {0};
static bool snare_data[NUM_STEPS] = {0};

static uint64_t raw_press_us = 0;
static uint64_t press_start_us = 0;
static bool press_in_progress = false;
static bool long_press_done = false;
static volatile bool long_press_event = false;

static uint8_t current_step = 0;
static volatile bool button_event = false;
static bool recording = false;
static uint64_t loop_start_us = 0;
static uint64_t last_button_event_us = 0;

static void print_track(const char *label, bool *track) {
    printf("%s", label);
    for (size_t i = 0; i < NUM_STEPS; i++) {
        printf("%s", track[i] ? "x" : " ");
    }
    printf("\n");
}

static void print_step(const char *label, int step) {
    printf("%s", label);
    for (size_t i = 0; i < NUM_STEPS; i++) {
        printf("%s", (size_t)step == i ? "^" : " ");
    }
    printf("\n");
}

static void step_timer_handler(btstack_timer_source_t *ts) {
    printf("%s (Track %s)\n", (recording ? "Record" : "Play"),
           (current_track == TRACK_SNARE ? "Snare" : "Buss"));
    print_track("Track Buss  ", bass_data);
    print_track("Track Snare ", snare_data);
    print_step("            ", current_step);

    // play click
    if ((current_step % (NUM_STEPS / CLICK_DIVISION)) == 0) {
        send_midi_click(current_step == 0);
    }

    if ((!recording || current_track == TRACK_SNARE) && bass_data[current_step]) {
        send_midi_bass();
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
    }
    if ((!recording || current_track == TRACK_BASS) && snare_data[current_step]) {
        send_midi_snare();
    }
    if (!bass_data[current_step]) {
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
    }

    if (recording) {
        if (current_step + 1 >= NUM_STEPS) {
            recording = false;
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
        }
    }

    if (long_press_event && current_track == TRACK_BASS) {
        current_track = TRACK_SNARE;
        long_press_event = false;
    } else if (long_press_event && current_track == TRACK_SNARE) {
        current_track = TRACK_BASS;
        long_press_event = false;
    }

    current_step = (current_step + 1) % NUM_STEPS;
    if (current_step == 0) {
        loop_start_us = time_us_64();
    }

    btstack_run_loop_set_timer(ts, STEP_MS);
    btstack_run_loop_add_timer(ts);
}

int main(void) {
    stdio_init_all();
    printf("[MAIN] BLE MIDI Looper started\n");

    cyw43_arch_init();
    ble_midi_init(step_timer_handler, STEP_MS);
    printf("[MAIN] BLE MIDI Looper started 2nd\n");

    loop_start_us = time_us_64();
    bool last_button = true;
    while (true) {
        bool pressed = bb_get_bootsel_button();
        uint64_t now = time_us_64();

        if (pressed && !last_button) {
            raw_press_us = now;
        }

        // short click
        if (pressed && !last_button && now - last_button_event_us >= DEBOUNCE_MS_SHORT * 1000) {
            last_button_event_us = now;
            press_start_us = now;
            press_in_progress = true;
            long_press_done = false;
            if (current_track == TRACK_BASS) {
                send_midi_bass();
            } else {
                send_midi_snare();
            }
        }

        // long press
        if (pressed && press_in_progress && !long_press_done &&
            now - press_start_us >= LONGPRESS_MS * 1000) {
            long_press_done = true;
            long_press_event = true;
        }

        // release short click
        if (!pressed && last_button && press_in_progress) {
            if (!long_press_done) {
                button_event = true;
            }
            press_in_progress = false;
        }
        last_button = pressed;

        if (button_event) {
            uint32_t idx = current_step;

            if (!recording) {
                recording = true;
                if (current_track == TRACK_BASS) {
                    memset(bass_data, 0, sizeof(bass_data));
                } else {
                    memset(snare_data, 0, sizeof(snare_data));
                }
            }
            if (current_track == TRACK_BASS)
                bass_data[idx] = true;
            else
                snare_data[idx] = true;
            button_event = false;
        }
    }

    return 0;
}
