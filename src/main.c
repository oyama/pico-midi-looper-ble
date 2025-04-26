/* main.c
 * BLE MIDI looper for Raspberry Pi Pico W.
 * A minimal 2-bars loop recorder using a single button to record and switch tracks.
 *
 * Copyright 2025, Hiroyuki OYAMA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"

#include "ble_midi.h"
#include "button.h"
#include "tap_tempo.h"

#define LOOPER_DEFAULT_BPM 120   // Beats per minute (global tempo)
#define LOOPER_BARS 2            // Loop length in bars
#define LOOPER_BEATS_PER_BAR 4   // Time signature numerator (e.g., 4/4)
#define LOOPER_STEPS_PER_BEAT 4  // Resolution (4 = 16th notes)

#define LOOPER_TOTAL_STEPS (LOOPER_STEPS_PER_BEAT * LOOPER_BEATS_PER_BAR * LOOPER_BARS)
#define LOOPER_CLICK_DIV (LOOPER_TOTAL_STEPS / LOOPER_BARS / LOOPER_STEPS_PER_BEAT )

#define ANSI_BG_HILITE "\x1b[47m"
#define ANSI_CLEAR "\x1b[0m"

enum {
    MIDI_CHANNEL_1 = 0,
    MIDI_CHANNEL_10 = 9,
};

enum {
    BASS_DRUM = 36,
    RIM_SHOT = 37,
    SNARE_DRUM = 38,
    HAND_CLAP = 39,
    CLOSED_HIHAT = 42,
    OPEN_HIHAT = 46,
    CYMBAL = 49,
};

// Represents the current playback or recording state.
typedef enum {
    LOOPER_STATE_WAITING = 0,   // BLE not connected, waiting.
    LOOPER_STATE_PLAYING,       // Playing back loop.
    LOOPER_STATE_RECORDING,     // recording in progress.
    LOOPER_STATE_TRACK_SWITCH,  // Switching to next track.
    LOOPER_STATE_TAP_TEMPO,
    LOOPER_STATE_CLEAR_ALL,
} looper_state_t;

typedef struct {
    uint64_t last_step_time_us;      // Time of last step transition
    uint64_t button_press_start_us;  // Timestamp when button was pressed
} looper_timing_t;

/*
 * Runtime playback state, managed globally.
 * Holds track index, current step, recording progress, and last tick time.
 */
typedef struct {
    uint32_t bpm;
    uint32_t step_duration_ms;
    looper_state_t state;          // Current looper mode (e.g. PLAYING, RECORDING).
    uint8_t current_track;         // Index of the active track (for recording or preview).
    uint8_t current_step;          // Index of the current step in the sequence loop.
    uint8_t recording_step_count;  // Number of steps recorded so far in this session.
    looper_timing_t timing;
} looper_status_t;

// Represents each MIDI track with note and sequence pattern.
typedef struct {
    const char *name;                       // Human-readable name of the track.
    uint8_t note;                           // MIDI note to trigger.
    uint8_t channel;                        // MIDI channel.
    bool pattern[LOOPER_TOTAL_STEPS];       // Current active pattern
    bool undo_pattern[LOOPER_TOTAL_STEPS];  // Backup for undoing modifications
} track_t;

static looper_status_t looper_status = {.bpm = LOOPER_DEFAULT_BPM, .state = LOOPER_STATE_WAITING};

static track_t tracks[] = {
    {"Bass", BASS_DRUM, MIDI_CHANNEL_10, {0}, {0}},
    {"Snare", SNARE_DRUM, MIDI_CHANNEL_10, {0}, {0}},
    {"Hi-hat", CLOSED_HIHAT, MIDI_CHANNEL_10, {0}, {0}},
    {"Open Hi-hat", OPEN_HIHAT, MIDI_CHANNEL_10, {0}, {0}},
};
static const size_t NUM_TRACKS = sizeof(tracks) / sizeof(track_t);

static bool status_led_on = false;

/*
 * Controls the built-in LED on the Pico W.
 * Used for indicating the active track or recording.
 */
static inline void looper_set_status_led(bool on) { status_led_on = on; }

static void looper_update_bpm(uint32_t bpm) {
    looper_status.bpm = bpm;
    looper_status.step_duration_ms = 60000 / (bpm * LOOPER_STEPS_PER_BEAT);
}

// Sends a click/hi-hat
static void send_midi_click() {
    ble_midi_send_note(MIDI_CHANNEL_1, RIM_SHOT, 0x20);
}

/*
 * Manages LED blinking when in WAITING state (BLE disconnected).
 * Otherwise mirrors the `status_led_on` flag during playback/recording.
 */
static void looper_update_status_led(void) {
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, status_led_on);
}

// Prints a single track row with step highlighting and note indicators.
static void print_track(const char *label, const bool *steps, bool is_selected) {
    printf("%s%-11s [", is_selected ? ">" : " ", label);
    for (int i = 0; i < LOOPER_TOTAL_STEPS; ++i) {
        bool note_on = steps[i];
        if (looper_status.current_step == i)
            printf("%s%s%s", ANSI_BG_HILITE, note_on ? "x" : " ", ANSI_CLEAR);
        else if (note_on)
            printf("x");
        else
            printf(" ");
    }
    printf("]\n");
}

// Displays the looper's playback state, connection status, and track patterns.
static void show_looper_status() {
    bool connection = ble_midi_is_connected();
    printf("[BLE %s]\n", connection ? "CONNECTED" : "WAITING");
    const char *state_label = "PAUSE";
    if (connection) {
        switch (looper_status.state) {
            case LOOPER_STATE_PLAYING:
            case LOOPER_STATE_TRACK_SWITCH:
                state_label = "PLAYING";
                break;
            case LOOPER_STATE_RECORDING:
                state_label = "RECORDING";
                break;
            case LOOPER_STATE_TAP_TEMPO:
                state_label = "TAP TEMPO";
                break;
            default:
                break;
        }
    }

    printf("[%s]\n", state_label);
    for (uint8_t i = 0; i < NUM_TRACKS; i++)
        print_track(tracks[i].name, tracks[i].pattern, i == looper_status.current_track);
    fflush(stdout);
}

// Sends a MIDI click at specific steps to indicate rhythm.
static void send_click_if_needed(void) {
    if ((looper_status.current_step % LOOPER_CLICK_DIV) == 0)
        send_midi_click();
}

static void looper_output_notes(void) {
    for (uint8_t i = 0; i < NUM_TRACKS; i++) {
        bool note_on = tracks[i].pattern[looper_status.current_step];
        if (note_on) {
            ble_midi_send_note(tracks[i].channel, tracks[i].note, 0x7f);
            if (i == looper_status.current_track)
                looper_set_status_led(1);
        } else if (i == looper_status.current_track) {
            looper_set_status_led(0);
        }
    }
}

static void looper_output_notes_recording(void) {
    looper_set_status_led(1);

    for (uint8_t i = 0; i < NUM_TRACKS; i++) {
        bool note_on = tracks[i].pattern[looper_status.current_step];
        if (note_on)
            ble_midi_send_note(tracks[i].channel, tracks[i].note, 0x7f);
    }
}

// Updates the current step index and timestamp based on current loop progress.
static void looper_next_step(uint64_t now_us) {
    looper_status.timing.last_step_time_us = now_us;
    looper_status.current_step = (looper_status.current_step + 1) % LOOPER_TOTAL_STEPS;
}

// Re-arms the timer to fire again after LOOPER_STEP_DURATION_MS, adjusting for processing time.
static void looper_reset_step_timer(btstack_timer_source_t *ts, uint64_t start_us) {
    uint64_t handler_delay_ms = (time_us_64() - start_us) / 1000;
    uint32_t delay = (handler_delay_ms >= looper_status.step_duration_ms)
                         ? 1
                         : looper_status.step_duration_ms - handler_delay_ms;
    btstack_run_loop_set_timer(ts, delay);
    btstack_run_loop_add_timer(ts);
}

/*
 * Returns the step index nearest to the stored `button_press_start_us` timestamp.
 * The result is quantized to the nearest step relative to the last tick.
 */
static uint8_t looper_quantize_step() {
    int64_t delta_us =
        looper_status.timing.button_press_start_us - looper_status.timing.last_step_time_us;
    // Convert to step offset using rounding (nearest step)
    int32_t relative_steps =
        (int32_t)round((double)delta_us / 1000.0 / looper_status.step_duration_ms);
    uint8_t previous_step =
        (looper_status.current_step + LOOPER_TOTAL_STEPS - 1) % LOOPER_TOTAL_STEPS;
    uint8_t estimated_step =
        (previous_step + relative_steps + LOOPER_TOTAL_STEPS) % LOOPER_TOTAL_STEPS;
    return estimated_step;
}

static void looper_clear_all_tracks() {
    for (uint8_t i = 0; i < NUM_TRACKS; i++)
        memset(tracks[i].pattern, 0, sizeof(tracks[i].pattern));
}

// Processes the looper's main state machine, called by the step timer.
static void looper_process_state(btstack_timer_source_t *ts) {
    uint64_t start_us = time_us_64();
    bool connection = ble_midi_is_connected();
    show_looper_status();

    if (!connection)
        looper_status.state = LOOPER_STATE_WAITING;
    switch (looper_status.state) {
        case LOOPER_STATE_WAITING:
            if (connection) {
                looper_status.state = LOOPER_STATE_PLAYING;
                looper_status.current_step = 0;
            }
            looper_set_status_led((looper_status.current_step % (LOOPER_CLICK_DIV * 4)) == 0);
            looper_next_step(start_us);
            break;
        case LOOPER_STATE_PLAYING:
            send_click_if_needed();
            looper_output_notes();
            looper_next_step(start_us);
            break;
        case LOOPER_STATE_RECORDING:
            send_click_if_needed();
            looper_output_notes_recording();
            looper_next_step(start_us);

            looper_status.recording_step_count++;
            if (looper_status.recording_step_count >= LOOPER_TOTAL_STEPS) {
                looper_set_status_led(0);
                looper_status.state = LOOPER_STATE_PLAYING;
            }
            break;
        case LOOPER_STATE_TRACK_SWITCH:
            looper_status.current_track = (looper_status.current_track + 1) % NUM_TRACKS;
            ble_midi_send_note(MIDI_CHANNEL_10, HAND_CLAP, 0x7f);
            looper_next_step(start_us);
            looper_status.state = LOOPER_STATE_PLAYING;
            break;
        case LOOPER_STATE_TAP_TEMPO:
            send_click_if_needed();
            looper_set_status_led((looper_status.current_step % LOOPER_CLICK_DIV) == 0);
            looper_next_step(start_us);
            break;
        case LOOPER_STATE_CLEAR_ALL:
            looper_clear_all_tracks();
            looper_status.current_track = 0;
            looper_next_step(start_us);
            looper_status.state = LOOPER_STATE_PLAYING;
        default:
            break;
    }

    looper_reset_step_timer(ts, start_us);
}

// Routes button events related to tap-tempo mode.
static tap_result_t taptempo_handle_button_event(button_event_t event) {
    tap_result_t result = taptempo_handle_event(event);
    switch (result) {
        case TAP_PRELIM:
        case TAP_FINAL:
            looper_update_bpm(taptempo_get_bpm());
            break;
        case TAP_EXIT: /* leave mode */
            break;
        default:
            break;
    }
    return result;
}

/*
 * Handles button events and updates the looper state accordingly.
 *
 * This includes triggering MIDI notes, starting/stopping recording,
 * and switching tracks based on short or long presses.
 *
 * Event constants are defined in button.c (see button_event_t).
 */
static void looper_handle_button_event(button_event_t event) {
    track_t *track = &tracks[looper_status.current_track];

    switch (event) {
        case BUTTON_EVENT_DOWN:
            // Button pressed: start timing and preview sound
            looper_status.timing.button_press_start_us = time_us_64();
            ble_midi_send_note(track->channel, track->note, 0x7f);
            // Backup track pattern in case this press becomes a long-press (undo)
            memcpy(track->undo_pattern, track->pattern, LOOPER_TOTAL_STEPS);
            break;
        case BUTTON_EVENT_CLICK_RELEASE:
            // Short press release: quantize and record step
            if (looper_status.state != LOOPER_STATE_RECORDING) {
                looper_status.recording_step_count = 0;
                looper_status.state = LOOPER_STATE_RECORDING;
                memset(track->pattern, 0, LOOPER_TOTAL_STEPS);
            }
            uint8_t quantized_step = looper_quantize_step();
            track->pattern[quantized_step] = true;
            break;
        case BUTTON_EVENT_HOLD_RELEASE:
            // Long press release: revert track and switch
            memcpy(track->pattern, track->undo_pattern, LOOPER_TOTAL_STEPS);
            looper_status.state = LOOPER_STATE_TRACK_SWITCH;
            break;
        case BUTTON_EVENT_LONG_HOLD_RELEASE:
            // ≥2 s hold: enter Tap-tempo (no track switch)
            looper_status.state = LOOPER_STATE_TAP_TEMPO;
            ble_midi_send_note(MIDI_CHANNEL_10, HAND_CLAP, 0x7f);
            break;
        case BUTTON_EVENT_VERY_LONG_HOLD_RELEASE:
            // ≥5 s hold: clear track data
            looper_status.state = LOOPER_STATE_CLEAR_ALL;
            ble_midi_send_note(MIDI_CHANNEL_10, HAND_CLAP, 0x7f);
            break;
        default:
            break;
    }
}

int main(void) {
    stdio_init_all();
    cyw43_arch_init();
    looper_update_bpm(LOOPER_DEFAULT_BPM);

    ble_midi_init(looper_process_state, looper_status.step_duration_ms);
    printf("[MAIN] BLE MIDI Looper start\n");
    while (true) {
        button_event_t event = button_poll_event();
        if (looper_status.state == LOOPER_STATE_TAP_TEMPO) {
            if (taptempo_handle_button_event(event) == TAP_EXIT) {
                looper_status.state = LOOPER_STATE_PLAYING;
            }
        } else {
             looper_handle_button_event(event);
        }
        looper_update_status_led();
    }
    return 0;
}
