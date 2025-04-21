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


#define MASTER_BPM     140
#define STEPS_PER_BEAT 4
#define STEP_MS        (60000 / (MASTER_BPM * STEPS_PER_BEAT))

#define NUM_STEPS 16
#define CLICK_DIVISION (NUM_STEPS / 4)

#define LONGPRESS_MS 500
#define DEBOUNCE_MS_SHORT 1

typedef struct {
    const char *name;
    uint8_t note;
    uint8_t channel;
    bool data[NUM_STEPS];
} track_t;

#define NUM_TRACKS 2
track_t tracks[NUM_TRACKS] = {
    { "Bass", 36, 9, {0} },
    { "Snare", 38, 9, {0} },
};
static uint8_t current_track = 0;

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

static uint8_t record_step_count = 0;
static const uint8_t MAX_RECORD_STEPS = NUM_STEPS;


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
    printf("%s (Track %s)\n", (recording ? "Record" : "Play"), tracks[current_track].name);
    print_track("Track Buss  ", tracks[0].data);
    print_track("Track Snare ", tracks[1].data);
    print_step("            ", current_step);

    // play click
    if ((current_step % (NUM_STEPS / CLICK_DIVISION)) == 0) {
        send_midi_click(current_step == 0);
    }

    for (uint8_t i = 0; i < NUM_TRACKS; i++) {
        if (!tracks[i].data[current_step]) {
            if (current_track == i)
                cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
            continue;
        }

        if (!recording || current_track != i)
            send_midi_note(tracks[i].channel, tracks[i].note, 0x7f);
        if (current_track == i)
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
    }

    if (recording)
        record_step_count++;
    if (record_step_count >= MAX_RECORD_STEPS) {
        recording = false;
    }

    if (long_press_event) {
        current_track = (current_track + 1) % NUM_TRACKS;
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
                send_midi_note(tracks[current_track].channel, tracks[current_track].note, 0x7f);
            }
            press_in_progress = false;
        }
        last_button = pressed;

        if (button_event) {
            uint32_t idx = current_step;

            if (!recording) {
                record_step_count = 0;
                recording = true;
                memset(tracks[current_track].data, 0, NUM_STEPS);
                printf("Start Recording.\n");
            }

            tracks[current_track].data[idx] = true;
            button_event = false;
        }
    }

    return 0;
}
