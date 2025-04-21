/*
 * Copyright 2025, Hiroyuki OYAMA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <stdio.h>
#include <string.h>

#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "ble_midi.h"
#include "bootsel_button.h"

#define MASTER_BPM        140
#define STEPS_PER_BEAT    4
#define NUM_STEPS         16
#define CLICK_DIVISION    (NUM_STEPS / 4)
#define STEP_MS           (60000 / (MASTER_BPM * STEPS_PER_BEAT))
#define MAX_RECORD_STEPS  NUM_STEPS

#define LONGPRESS_US      (500 * 1000)

#define ANSI_CYAN     "\e[36m"
#define ANSI_MAGENTA  "\e[35m"
#define ANSI_CLEAR    "\e[0m"

typedef struct {
    const char *name;
    uint8_t note;
    uint8_t channel;
    bool data[NUM_STEPS];
} track_t;

#define MIDI_ACCOUSTIC_BASS_DRUM  36
#define MIDI_ACOUSTIC_SNARE       38
#define MIDI_RIDE_CYMBAL1  51

#define NUM_TRACKS 2
track_t tracks[NUM_TRACKS] = {
    { "Bass", MIDI_ACCOUSTIC_BASS_DRUM, 9, {0} },
    { "Snare", MIDI_ACOUSTIC_SNARE, 9, {0} },
};

static bool recording = false;
static uint8_t record_step_count = 0;
static uint8_t current_track = 0;
static volatile bool track_switch_pending = false;
static volatile bool record_pending_on_press = false;
static uint8_t current_step = 0;

static inline void set_led(bool on) {
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, on);
}

static void print_track(const char *label, bool *track) {
    printf("%s", label);
    for (size_t i = 0; i < NUM_STEPS; i++) {
        printf("%s", track[i] ? "x" : " ");
    }
    printf("\n");
}

static void print_step_indicator(const char *label, int step) {
    printf("%s", label);
    for (size_t i = 0; i < NUM_STEPS; i++) {
        printf("%s", (size_t)step == i ? "^" : " ");
    }
    printf("\n");
}

static void step_timer_handler(btstack_timer_source_t *ts) {
    printf("%s (Track %s)\n",
           (recording ? ANSI_MAGENTA "Record" ANSI_CLEAR : ANSI_CYAN "Play" ANSI_CLEAR),
           tracks[current_track].name);
    print_track("Track Buss  ", tracks[0].data);
    print_track("Track Snare ", tracks[1].data);
    print_step_indicator("            ", current_step);

    // play click
    if ((current_step % (NUM_STEPS / CLICK_DIVISION)) == 0) {
        send_midi_click(current_step == 0);
    }

    // play note per track
    for (uint8_t i = 0; i < NUM_TRACKS; i++) {
        bool note_on = tracks[i].data[current_step];
        if (note_on) {
            if (!recording || current_track != i)
                send_midi_note(tracks[i].channel, tracks[i].note, 0x7f);
            if (current_track == i && !recording)
                set_led(1);
        } else if (current_track == i && !recording) {
            set_led(0);
        }
    }

    // recording coltroll
    if (recording) {
        record_step_count++;
        set_led(1);
    }
    if (record_step_count >= MAX_RECORD_STEPS) {
        if (recording)
            set_led(0);
        recording = false;
    }

    // switch track
    if (track_switch_pending) {
        current_track = (current_track + 1) % NUM_TRACKS;
        track_switch_pending = false;
        send_midi_note(9, MIDI_RIDE_CYMBAL1, 0x7f);
    }

    current_step = (current_step + 1) % NUM_STEPS;

    btstack_run_loop_set_timer(ts, STEP_MS);
    btstack_run_loop_add_timer(ts);
}

int main(void) {
    stdio_init_all();
    printf("[MAIN] BLE MIDI Looper start\n");

    cyw43_arch_init();
    ble_midi_init(step_timer_handler, STEP_MS);

    bool last_button_state = true;
    bool button_pressed = false;
    uint64_t press_start_us = 0;
    bool long_press_triggered = false;
    bool undo_track[NUM_STEPS] = {0};

    while (true) {
        bool current_button_state = bb_get_bootsel_button();
        uint64_t now_us = time_us_64();

        // Click down
        if (current_button_state && !last_button_state) {
            press_start_us = now_us;
            button_pressed = true;
            long_press_triggered = false;
            record_pending_on_press = true;

            // Save tracks in case of long presses
            memcpy(undo_track, tracks[current_track].data, NUM_STEPS);

            send_midi_note(tracks[current_track].channel, tracks[current_track].note, 0x7f);
        }

        // Long press
        if (current_button_state && button_pressed && !long_press_triggered && (now_us - press_start_us >= LONGPRESS_US)) {
            track_switch_pending = true;
            long_press_triggered = true;

            // Restoring Tracks
            memcpy(tracks[current_track].data, undo_track, NUM_STEPS);
        }

        // release short click
        if (!current_button_state && last_button_state && button_pressed) {
            button_pressed = false;
        }
        last_button_state = current_button_state;

        if (record_pending_on_press) {
            uint32_t idx = current_step;
            if (!recording) {
                record_step_count = 0;
                recording = true;
                memset(tracks[current_track].data, 0, NUM_STEPS);
            }

            tracks[current_track].data[idx] = true;
            record_pending_on_press = false;
        }
    }

    return 0;
}
