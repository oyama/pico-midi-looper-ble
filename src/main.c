/* main.c
 * BLE MIDI looper for Raspberry Pi Pico W.
 * A minimal 1-bar loop recorder using a single button to record and switch tracks.
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

#define MASTER_BPM 120      // Beats per minute (global tempo)
#define LOOP_LENGTH_BARS 1  // Loop length in bars
#define BEATS_PER_BAR 4     // Time signature numerator (e.g., 4/4)
#define STEPS_PER_BEAT 4    // Resolution (4 = 16th notes)

#define TOTAL_STEPS (STEPS_PER_BEAT * BEATS_PER_BAR * LOOP_LENGTH_BARS)
#define CLICK_DIVISION (TOTAL_STEPS / STEPS_PER_BEAT)
#define STEP_MS (60000 / (MASTER_BPM * STEPS_PER_BEAT))

#define ANSI_BG_HILITE "\x1b[47m"
#define ANSI_CLEAR "\e[0m"

enum {
    MIDI_CHANNEL_1 = 0,
    MIDI_CHANNEL_10 = 9,
};

enum {
    STANDARD_BASS_DRUM = 36,
    STANDARD_SNARE = 38,
    CLOSED_HIHAT = 42,
    OPEN_HIHAT = 46,
};

// Represents the current playback or recording state.
typedef enum {
    LOOPER_STATE_WAITING = 0,   // BLE not connected, waiting.
    LOOPER_STATE_PLAYING,       // Playing back loop.
    LOOPER_STATE_RECORDING,     // One-bar recording in progress.
    LOOPER_STATE_TRACK_SWITCH,  // Switching to next track.
} looper_state_t;

typedef struct {
    uint64_t last_step_time_us;  // Time of last step transition
    uint64_t press_start_us;     // Timestamp when button was pressed
} looper_timing_t;

/*
 * Runtime playback state, managed globally.
 * Holds track index, current step, recording progress, and last tick time.
 */
typedef struct {
    looper_state_t state;       // Current looper mode (e.g. PLAYING, RECORDING).
    uint8_t current_track;      // Index of the active track (for recording or preview).
    uint8_t current_step;       // Index of the current step in the sequence loop.
    uint8_t record_step_count;  // Number of steps recorded so far in this session.
    looper_timing_t timing;
} looper_status_t;

// Represents each MIDI track with note and sequence pattern.
typedef struct {
    const char *name;                // Human-readable name of the track.
    uint8_t note;                    // MIDI note to trigger.
    uint8_t channel;                 // MIDI channel.
    bool pattern[TOTAL_STEPS];       // Current active pattern
    bool undo_pattern[TOTAL_STEPS];  // Backup for undoing modifications
} track_t;

static looper_status_t looper_status = {.state = LOOPER_STATE_WAITING};

#define NUM_TRACKS 2
track_t tracks[NUM_TRACKS] = {
    {"Bass", STANDARD_BASS_DRUM, MIDI_CHANNEL_10, {0}, {0}},
    {"Snare", STANDARD_SNARE, MIDI_CHANNEL_10, {0}, {0}},
};

static bool onboard_led_is_on = false;

/*
 * Controls the built-in LED on the Pico W.
 * Used for indicating the active track or recording.
 */
static inline void set_onboard_led(bool on) { onboard_led_is_on = on; }

// Sends a click sound at key steps
static void send_midi_click(bool accent) {
    send_midi_note(MIDI_CHANNEL_1, CLOSED_HIHAT, accent ? 0x7f : 0x5f);
}

/*
 * Manages LED blinking when in WAITING state (BLE disconnected).
 * Otherwise turns LED on/off depending on current playback state.
 */
static void update_status_led(void) {
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

// Prints a single track row with step highlighting and note indicators.
static void print_track(const char *label, const bool *steps, bool is_selected) {
    printf("%s%-7s [", is_selected ? ">" : " ", label);
    for (int i = 0; i < TOTAL_STEPS; ++i) {
        bool note_on = steps[i];
        if (looper_status.current_step == i) {
            printf("%s%s%s", ANSI_BG_HILITE, note_on ? "x" : " ", ANSI_CLEAR);
        } else if (note_on) {
            printf("x");
        } else {
            printf(" ");
        }
    }
    printf("]\n");
}

// Displays the looper's playback state, connection status, and track patterns.
static void show_looper_status() {
    bool connection = ble_midi_connection_status();
    printf("[BLE %s]\n", connection ? "CONNECTED" : "WAITING");
    const char *state_label =
        (connection ? (looper_status.state == LOOPER_STATE_RECORDING ? "RECORDING" : "PLAYING")
                    : "PAUSE");
    printf("[%s]\n", state_label);
    for (uint8_t i = 0; i < NUM_TRACKS; i++)
        print_track(tracks[i].name, tracks[i].pattern, i == looper_status.current_track);
    fflush(stdout);
}

// Sends a MIDI click at specific steps to indicate rhythm.
static void send_click_if_needed(void) {
    if ((looper_status.current_step % CLICK_DIVISION) == 0) {
        send_midi_click(looper_status.current_step == 0);
    }
}

static void play_looper_notes(void) {
    for (uint8_t i = 0; i < NUM_TRACKS; i++) {
        bool note_on = tracks[i].pattern[looper_status.current_step];
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
        bool note_on = tracks[i].pattern[looper_status.current_step];
        if (note_on)
            send_midi_note(tracks[i].channel, tracks[i].note, 0x7f);
    }
}

// Updates the current step index and timestamp based on current loop progress.
static void advance_sequencer(uint64_t now_us) {
    looper_status.timing.last_step_time_us = now_us;
    looper_status.current_step = (looper_status.current_step + 1) % TOTAL_STEPS;
}

// Re-arms the timer to fire again after STEP_MS, adjusting for processing time.
static void reset_interval_timer(btstack_timer_source_t *ts, uint64_t start_us) {
    uint64_t handler_delay_ms = (time_us_64() - start_us) / 1000;
    btstack_run_loop_set_timer(ts, STEP_MS - handler_delay_ms);
    btstack_run_loop_add_timer(ts);
}

/*
 * Returns the step index closest to the specified press time.
 * The result is quantized to the nearest step relative to the last tick.
 */
static uint8_t get_quantized_step_from_time() {
    int64_t delta_us = looper_status.timing.press_start_us - looper_status.timing.last_step_time_us;
    // Convert to step offset using rounding (nearest step)
    int32_t relative_steps = (delta_us + (STEP_MS * 500)) / 1000 / STEP_MS;
    uint8_t previous_step = (looper_status.current_step + TOTAL_STEPS - 1) % TOTAL_STEPS;
    uint8_t estimated_step = (previous_step + relative_steps + TOTAL_STEPS) % TOTAL_STEPS;
    return estimated_step;
}

// Processes the looper's main state machine, called by the step timer.
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
            looper_status.current_track = (looper_status.current_track + 1) % NUM_TRACKS;
            send_midi_note(MIDI_CHANNEL_10, OPEN_HIHAT, 0x7f);
            advance_sequencer(start_us);
            looper_status.state = LOOPER_STATE_PLAYING;
            break;
        default:
            break;
    }

    reset_interval_timer(ts, start_us);
}

/*
 * Handles button events and updates the looper state accordingly.
 *
 * This includes triggering MIDI notes, starting/stopping recording,
 * and switching tracks based on short or long presses.
 *
 * Event constants are defined in button.c (see button_event_t).
 */
static void handle_button_event(button_event_t event) {
    track_t *track = &tracks[looper_status.current_track];
    switch (event) {
        case BUTTON_EVENT_DOWN:
            // Button pressed: start timing and preview sound
            looper_status.timing.press_start_us = time_us_64();
            send_midi_note(track->channel, track->note, 0x7f);
            // Save tracks in care of long press
            memcpy(track->undo_pattern, track->pattern, TOTAL_STEPS);
            break;
        case BUTTON_EVENT_SHORT_PRESS_RELEASE:
            // Short press release: quantize and record step
            if (looper_status.state != LOOPER_STATE_RECORDING) {
                looper_status.record_step_count = 0;
                looper_status.state = LOOPER_STATE_RECORDING;
                memset(track->pattern, 0, TOTAL_STEPS);
            }
            uint8_t quantized_step = get_quantized_step_from_time();
            track->pattern[quantized_step] = true;
            break;
        case BUTTON_EVENT_LONG_PRESS_RELEASE:
            // Long press release: revert track and switch
            memcpy(track->pattern, track->undo_pattern, TOTAL_STEPS);
            looper_status.state = LOOPER_STATE_TRACK_SWITCH;
            break;
        default:
            break;
    }
}

int main(void) {
    stdio_init_all();
    cyw43_arch_init();
    ble_midi_init(process_looper_state, STEP_MS);
    printf("[MAIN] BLE MIDI Looper start\n");
    while (true) {
        button_event_t button_event = poll_button_event();
        handle_button_event(button_event);
        update_status_led();
    }
    return 0;
}
