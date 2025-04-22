/*
 * Copyright 2025, Hiroyuki OYAMA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <stdio.h>
#include <string.h>

#include "ble_midi.h"
#include "bootsel_button.h"
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"

#define MASTER_BPM 120
#define LOOP_LENGTH_BARS 1
#define STEPS_PER_QUARTER 4
#define BEATS_PER_BAR 4
#define TOTAL_STEPS (STEPS_PER_QUARTER * BEATS_PER_BAR * LOOP_LENGTH_BARS)
#define CLICK_DIVISION (TOTAL_STEPS / STEPS_PER_QUARTER)
#define STEP_MS (int)((float)60000 / (float)(MASTER_BPM * STEPS_PER_QUARTER))

#define LONGPRESS_US (500 * 1000)

#define ANSI_CYAN "\e[36m"
#define ANSI_MAGENTA "\e[35m"
#define ANSI_CLEAR "\e[0m"

enum {
    MIDI_CHANNEL_1 = 0,
    MIDI_CHANNEL_10 = 9,
};

enum {
    STANDARD_BASS_DRUM = 36,
    SIDE_STICK = 37,
    STANDARD_SNARE = 38,
    RIDE_CYMBAL1 = 51,
};

typedef struct {
    const char *name;
    uint8_t note;
    uint8_t channel;
    bool data[TOTAL_STEPS];
} track_t;

#define NUM_TRACKS 2
track_t tracks[NUM_TRACKS] = {
    {"Bass", STANDARD_BASS_DRUM, MIDI_CHANNEL_10, {0}},
    {"Snare", STANDARD_SNARE, MIDI_CHANNEL_10, {0}},
};

typedef struct {
    bool is_recording;
    uint8_t record_step_count;
    uint8_t current_track;
    bool track_switch_pending;
    uint8_t current_step;
} recording_status_t;

static recording_status_t recording_status = {.is_recording = false,
                                              .record_step_count = 0,
                                              .current_track = 0,
                                              .track_switch_pending = false,
                                              .current_step = 0};
static bool onboard_led = false;

static inline void set_onboard_led(bool on) { onboard_led = on; }

static void send_midi_click(bool accent) {
    send_midi_note(MIDI_CHANNEL_1, SIDE_STICK, accent ? 0x5f : 0x20);
}

static void send_midi_bass(void) { send_midi_note(MIDI_CHANNEL_10, STANDARD_BASS_DRUM, 0x7f); }

static void send_midi_snare(void) { send_midi_note(MIDI_CHANNEL_10, STANDARD_SNARE, 0x7f); }

void led_task(void) {
    if (!ble_midi_connection_status()) {
        // If there is no BLE connection, the LED will blink to indicate "PAUSE" status
        static absolute_time_t next_toggle_time;
        static bool led_on = false;
        const int on_duration_ms = 100;
        const int off_duration_ms = 1300;

        if (absolute_time_diff_us(get_absolute_time(), next_toggle_time) < 0) {
            led_on = !led_on;
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_on);
            next_toggle_time =
                delayed_by_ms(get_absolute_time(), led_on ? on_duration_ms : off_duration_ms);
        }
        return;
    }

    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, onboard_led);
}

static void print_track(const char *label, bool *track) {
    printf("%s", label);
    for (size_t i = 0; i < TOTAL_STEPS; i++) {
        printf("%s", track[i] ? "x" : " ");
    }
    printf("\n");
}

static void print_step_indicator(const char *label, int step) {
    printf("%s", label);
    for (size_t i = 0; i < TOTAL_STEPS; i++) {
        printf("%s", (size_t)step == i ? "^" : " ");
    }
    printf("\n");
}

static void step_timer_handler(btstack_timer_source_t *ts) {
    uint64_t start_us = time_us_64();
    bool connection = ble_midi_connection_status();

    printf("%s (Track %s)\n",
           (connection ? (recording_status.is_recording ? ANSI_MAGENTA "Record" ANSI_CLEAR
                                                        : ANSI_CYAN "Play" ANSI_CLEAR)
                       : "Pause"),
           tracks[recording_status.current_track].name);
    print_track("Track Buss  ", tracks[0].data);
    print_track("Track Snare ", tracks[1].data);
    print_step_indicator("            ", recording_status.current_step);

    if (!connection) {
        // Waiting for BLE-MIDI connection
        goto finish;
    }

    // play click
    if ((recording_status.current_step % (TOTAL_STEPS / CLICK_DIVISION)) == 0) {
        send_midi_click(recording_status.current_step == 0);
    }

    // play note per track
    for (uint8_t i = 0; i < NUM_TRACKS; i++) {
        bool note_on = tracks[i].data[recording_status.current_step];
        if (note_on) {
            if (!recording_status.is_recording || recording_status.current_track != i)
                send_midi_note(tracks[i].channel, tracks[i].note, 0x7f);
            if (recording_status.current_track == i && !recording_status.is_recording)
                set_onboard_led(1);
        } else if (recording_status.current_track == i && !recording_status.is_recording) {
            set_onboard_led(0);
        }
    }

    // recording coltroll
    if (recording_status.is_recording) {
        recording_status.record_step_count++;
        set_onboard_led(1);
    }
    if (recording_status.record_step_count >= TOTAL_STEPS) {
        if (recording_status.is_recording)
            set_onboard_led(0);
        recording_status.is_recording = false;
    }

    // switch track
    if (recording_status.track_switch_pending) {
        recording_status.current_track = (recording_status.current_track + 1) % NUM_TRACKS;
        recording_status.track_switch_pending = false;
        send_midi_note(9, RIDE_CYMBAL1, 0x7f);
    }

    recording_status.current_step = (recording_status.current_step + 1) % TOTAL_STEPS;

finish:
    uint64_t handler_delay_ms = (time_us_64() - start_us) / 1000;
    btstack_run_loop_set_timer(ts, STEP_MS - handler_delay_ms);
    btstack_run_loop_add_timer(ts);
}

typedef enum { STATE_IDLE, STATE_PRESSED_SHORT, STATE_PRESSED_LONG } button_state_t;

int main(void) {
    uint64_t press_start_us = 0;
    bool undo_track[TOTAL_STEPS] = {0};
    bool record_pending_on_press = false;

    stdio_init_all();
    cyw43_arch_init();
    ble_midi_init(step_timer_handler, STEP_MS);
    printf("[MAIN] BLE MIDI Looper start\n");
    button_state_t state = STATE_IDLE;
    while (true) {
        bool current_button_state = bb_get_bootsel_button();
        uint64_t now_us = time_us_64();

        switch (state) {
            case STATE_IDLE:
                if (current_button_state) {
                    // Button down
                    press_start_us = now_us;
                    record_pending_on_press = true;

                    send_midi_note(tracks[recording_status.current_track].channel,
                                   tracks[recording_status.current_track].note, 0x7f);

                    // Save tracks in care of long press
                    memcpy(undo_track, tracks[recording_status.current_track].data, TOTAL_STEPS);

                    state = STATE_PRESSED_SHORT;
                }
                break;
            case STATE_PRESSED_SHORT:
                if (!current_button_state) {
                    // Release short press
                    state = STATE_IDLE;
                } else if (now_us - press_start_us >= LONGPRESS_US) {
                    // Long press
                    recording_status.track_switch_pending = true;

                    // Restoring Tracks
                    memcpy(tracks[recording_status.current_track].data, undo_track, TOTAL_STEPS);

                    state = STATE_PRESSED_LONG;
                }
                break;
            case STATE_PRESSED_LONG:
                if (!current_button_state)
                    // Release long press
                    state = STATE_IDLE;
                break;
            default:
                break;
        }

        if (record_pending_on_press) {
            if (!recording_status.is_recording) {
                recording_status.record_step_count = 0;
                recording_status.is_recording = true;
                memset(tracks[recording_status.current_track].data, 0, TOTAL_STEPS);
            }

            tracks[recording_status.current_track].data[recording_status.current_step] = true;

            record_pending_on_press = false;
        }

        led_task();
    }

    return 0;
}
