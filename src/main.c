/*
 * Pico BLE MIDI Looper
 *
 * A minimal 1-bar loop recorder for drum tracks, using Raspberry Pi Pico W.
 * - Step-based sequencer synchronized to MASTER_BPM
 * - BLE MIDI transmission with real-time user interaction
 * - Supports basic recording, playback, and track switching via a single button
 *
 * Copyright 2025, Hiroyuki OYAMA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <stdio.h>
#include <string.h>

#include "ble_midi.h"
#include "button.h"
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"

// ---- Timing Parameters ----
#define MASTER_BPM 120       // Beats per minute (global tempo)
#define LOOP_LENGTH_BARS 1   // Loop length in bars
#define STEPS_PER_QUARTER 4  // Resolution (4 = 16th notes)
#define BEATS_PER_BAR 4      // Time signature numerator (e.g., 4/4)

// ---- Derived Constants ----
#define TOTAL_STEPS (STEPS_PER_QUARTER * BEATS_PER_BAR * LOOP_LENGTH_BARS)
#define CLICK_DIVISION (TOTAL_STEPS / STEPS_PER_QUARTER)
#define STEP_MS (int)((float)60000 / (float)(MASTER_BPM * STEPS_PER_QUARTER))

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

// Represents the state of the looper's internal state machine.
typedef enum {
    LOOPER_STATE_WAITING = 0,
    LOOPER_STATE_PLAYING,
    LOOPER_STATE_RECORDING,
    LOOPER_STATE_TRACK_SWITCH,
} looper_state_t;

// Holds the full runtime status of the looper, including current step and track.
typedef struct {
    looper_state_t state;
    uint8_t record_step_count;
    uint8_t current_track;
    uint8_t current_step;
    uint64_t last_step_time_us;
} looper_status_t;

static looper_status_t looper_status = {.record_step_count = 0,
                                        .current_track = 0,
                                        .current_step = 0,
                                        .last_step_time_us = 0};
static bool onboard_led_is_on = false;

/*
 * Controls the built-in LED on the Pico W.
 * Used for indicating the active track or recording.
 */
static inline void set_onboard_led(bool on) { onboard_led_is_on = on; }

/*
 * Sends a MIDI note-on message representing a metronome click.
 * The pitch varies if it's a downbeat (first step in loop).
 */
static void send_midi_click(bool accent) {
    send_midi_note(MIDI_CHANNEL_1, SIDE_STICK, accent ? 0x5f : 0x20);
}

/*
 * Manages LED blinking when in WAITING state (BLE disconnected).
 * Otherwise turns LED on/off depending on current playback state.
 */
void update_status_led(void) {
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

    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, onboard_led_is_on);
}

/*
 * Outputs the step pattern of a given track to stdout for debugging.
 */
static void print_track(const char *label, bool *track) {
    printf("%s", label);
    for (size_t i = 0; i < TOTAL_STEPS; i++) {
        printf("%s", track[i] ? "x" : " ");
    }
    printf("\n");
}

/*
 * Prints a visual indicator (caret '^') showing the current playback step.
 */
static void print_step_indicator(const char *label, int step) {
    printf("%s", label);
    for (size_t i = 0; i < TOTAL_STEPS; i++) {
        printf("%s", (size_t)step == i ? "^" : " ");
    }
    printf("\n");
}

static void show_looper_status() {
    bool connection = ble_midi_connection_status();
    printf("%s (Track %s)\n",
           (connection ? (looper_status.state == LOOPER_STATE_RECORDING ? ANSI_MAGENTA "Record" ANSI_CLEAR
                                                                            : ANSI_CYAN "Play" ANSI_CLEAR)
                       : "Pause"),
           tracks[looper_status.current_track].name);
    print_track("Track Buss  ", tracks[0].data);
    print_track("Track Snare ", tracks[1].data);
    print_step_indicator("            ", looper_status.current_step);
}

static void send_click_if_needed(void) {
    if ((looper_status.current_step % CLICK_DIVISION) == 0) {
        send_midi_click(looper_status.current_step == 0);
    }
}

static void play_looper_notes(void) {
    for (uint8_t i = 0; i < NUM_TRACKS; i++) {
        bool note_on = tracks[i].data[looper_status.current_step];
        if (note_on) {
            send_midi_note(tracks[i].channel, tracks[i].note, 0x7f);
            if (i == looper_status.current_track)
                set_onboard_led(1);
        } else if (i == looper_status.current_track) {
            set_onboard_led(0);
        }
    }
}

static void play_looper_notes_recording(void) {
    set_onboard_led(1);

    for (uint8_t i = 0; i < NUM_TRACKS; i++) {
        bool note_on = tracks[i].data[looper_status.current_step];
        if (note_on)
            send_midi_note(tracks[i].channel, tracks[i].note, 0x7f);
    }
}

static void advance_sequencer(uint64_t now_us) {
    looper_status.last_step_time_us = now_us;
    looper_status.current_step = (looper_status.current_step + 1) % TOTAL_STEPS;
}

static void reset_interval_timer(btstack_timer_source_t *ts, uint64_t start_us) {
    uint64_t handler_delay_ms = (time_us_64() - start_us) / 1000;
    btstack_run_loop_set_timer(ts, STEP_MS - handler_delay_ms);
    btstack_run_loop_add_timer(ts);
}

/*
 * Main timing handler called every STEP_MS milliseconds.
 * Updates playback position, triggers MIDI output, handles recording and track switching.
 */
static void process_looper_state(btstack_timer_source_t *ts) {
    uint64_t start_us = time_us_64();
    bool connection = ble_midi_connection_status();

    show_looper_status();

    switch (looper_status.state) {
        case LOOPER_STATE_WAITING:
            if (connection)
                looper_status.state = LOOPER_STATE_PLAYING;
            break;
        case LOOPER_STATE_PLAYING:
            if (!connection)
                looper_status.state = LOOPER_STATE_WAITING;

            send_click_if_needed();
            play_looper_notes();
            advance_sequencer(start_us);
            break;

        case LOOPER_STATE_RECORDING:
            if (!connection)
                looper_status.state = LOOPER_STATE_WAITING;

            send_click_if_needed();
            play_looper_notes_recording();
            advance_sequencer(start_us);

            looper_status.record_step_count++;
            if (looper_status.record_step_count >= TOTAL_STEPS) {
                set_onboard_led(0);
                looper_status.state = LOOPER_STATE_PLAYING;
            }
            break;

        case LOOPER_STATE_TRACK_SWITCH:
            looper_status.state = LOOPER_STATE_PLAYING;
            send_midi_note(MIDI_CHANNEL_10, RIDE_CYMBAL1, 0x7f);
            advance_sequencer(start_us);
            break;
        default:
            break;
    }

    reset_interval_timer(ts, start_us);
}

/*
 * Returns the step index closest to the specified press time.
 * The result is quantized to the nearest step relative to the last tick.
 */
static uint8_t get_quantized_step_from_time(uint64_t press_time_us) {
    int64_t delta_us = press_time_us - looper_status.last_step_time_us;
    // Convert to step offset using rounding (nearest step)
    int32_t relative_steps = (delta_us + (STEP_MS * 500)) / 1000 / STEP_MS;
    uint8_t previous_step = (looper_status.current_step + TOTAL_STEPS - 1) % TOTAL_STEPS;
    uint8_t estimated_step = (previous_step + relative_steps + TOTAL_STEPS) % TOTAL_STEPS;
    return estimated_step;
}

int main(void) {
    uint64_t press_start_us = 0;
    bool undo_track[TOTAL_STEPS] = {0};

    stdio_init_all();
    cyw43_arch_init();
    ble_midi_init(process_looper_state, STEP_MS);
    printf("[MAIN] BLE MIDI Looper start\n");
    while (true) {
        button_event_t button_event = poll_button_event();

        switch (button_event) {
            case BUTTON_EVENT_DOWN:
                press_start_us = time_us_64();
                send_midi_note(tracks[looper_status.current_track].channel,
                               tracks[looper_status.current_track].note, 0x7f);
                // Save tracks in care of long press
                memcpy(undo_track, tracks[looper_status.current_track].data, TOTAL_STEPS);
                break;
            case BUTTON_EVENT_SHORT_PRESS_RELEASE:
                if (looper_status.state != LOOPER_STATE_RECORDING) {
                    looper_status.record_step_count = 0;
                    looper_status.state = LOOPER_STATE_RECORDING;
                    memset(tracks[looper_status.current_track].data, 0, TOTAL_STEPS);
                }
                uint8_t quantized_step = get_quantized_step_from_time(press_start_us);
                tracks[looper_status.current_track].data[quantized_step] = true;
                break;
            case BUTTON_EVENT_LONG_PRESS_RELEASE:
                // Restoring Tracks
                memcpy(tracks[looper_status.current_track].data, undo_track, TOTAL_STEPS);
                looper_status.state = LOOPER_STATE_TRACK_SWITCH;
                break;
            default:
                break;
        }

        update_status_led();
    }
    return 0;
}
