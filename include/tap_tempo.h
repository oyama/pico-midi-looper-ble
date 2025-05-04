#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "drivers/button.h"

typedef enum {
    TAP_NONE = 0,
    TAP_PRELIM,    /* 2-tap provisional BPM  */
    TAP_FINAL,     /* 3/4-tap averaged BPM   */
    TAP_EXIT       /* long-press → leave mode */
} tap_result_t;

bool taptempo_active(void);
tap_result_t taptempo_handle_event(button_event_t ev);
uint16_t taptempo_get_bpm(void);
bool taptempo_is_ready(void);
