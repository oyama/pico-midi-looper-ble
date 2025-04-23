/*
 * button.c
 *
 * Handles physical button state (BOOTSEL on Pico) and generates logical button events.
 * Internally uses a state machine to detect short press, long press, and release.
 *
 *
 * Copyright 2025, Hiroyuki OYAMA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/structs/ioqspi.h"
#include "hardware/sync.h"
#include "button.h"

#define BUTTON_DEBOUNCE_MAX 10
#define LONGPRESS_US (500 * 1000)

typedef enum {
    BUTTON_STATE_IDLE = 0,
    BUTTON_STATE_PRESSED,
    BUTTON_STATE_LONGPRESS_ACTIVE
} button_state_t;

typedef struct {
    button_state_t state;
    uint64_t press_start_us;
} button_fsm_t;

bool __no_inline_not_in_flash_func(get_bootsel_button)(void) {
    static uint8_t counter = 0;
    static bool stable_state = false;
    const uint CS_PIN_INDEX = 1;

    uint32_t flags = save_and_disable_interrupts();
    hw_write_masked(&ioqspi_hw->io[CS_PIN_INDEX].ctrl,
                    GPIO_OVERRIDE_LOW << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB,
                    IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS);
    for (volatile int i = 0; i < 1000; ++i);
    bool button_state = !(sio_hw->gpio_hi_in & (1u << CS_PIN_INDEX));
    hw_write_masked(&ioqspi_hw->io[CS_PIN_INDEX].ctrl,
                    GPIO_OVERRIDE_NORMAL << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB,
                    IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS);
    restore_interrupts(flags);

    // debounce
    if (button_state) {
        if (counter < BUTTON_DEBOUNCE_MAX)
            counter++;
    } else {
        if (counter > 0)
            counter--;
    }

    stable_state = (counter == BUTTON_DEBOUNCE_MAX) ? true : (counter == 0) ? false : stable_state;
    return stable_state;
}

/*
 * Reads the current physical button state and returns the corresponding logical event.
 * Maintains internal FSM to distinguish short press, long press, and release.
 */
button_event_t poll_button_event(void) {
    static button_fsm_t fsm = {0};
    button_event_t ev = BUTTON_EVENT_NONE;
    bool current_down = get_bootsel_button();
    uint64_t now_us = time_us_64();

    switch (fsm.state) {
        case BUTTON_STATE_IDLE:
            if (current_down) {
                fsm.state = BUTTON_STATE_PRESSED;
                fsm.press_start_us = now_us;
                ev = BUTTON_EVENT_DOWN;
            }
            break;

        case BUTTON_STATE_PRESSED:
            if (!current_down) {
                fsm.state = BUTTON_STATE_IDLE;
                ev = BUTTON_EVENT_SHORT_PRESS_RELEASE;
            } else if (now_us - fsm.press_start_us > LONGPRESS_US) {
                fsm.state = BUTTON_STATE_LONGPRESS_ACTIVE;
                ev = BUTTON_EVENT_LONG_PRESS_BEGIN;
            }
            break;

        case BUTTON_STATE_LONGPRESS_ACTIVE:
            if (!current_down) {
                fsm.state = BUTTON_STATE_IDLE;
                ev = BUTTON_EVENT_LONG_PRESS_RELEASE;
            }
            break;
    }

    return ev;
}
