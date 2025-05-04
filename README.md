# Pico MIDI Looper BLE-MIDI

[![Build](https://github.com/oyama/pico-midi-looper-ble/actions/workflows/build-firmware.yml/badge.svg)](https://github.com/oyama/pico-midi-looper-ble/actions)
![Price](https://img.shields.io/badge/Hardware-$6-green)

**This repository preserves the final BLE‑MIDI firmware (`v1.1.3-ble-final`)**

## Overview

The Pico MIDI Looper is a minimalist 2-bar drum looping device that leverages the Raspberry Pi Pico W's capabilities to provide wireless MIDI functionality via Bluetooth Low Energy (BLE-MIDI). Its primary purpose is to offer a compact, easy-to-build musical instrument that enables rhythm creation with minimal hardware requirements.

The system is designed for:
- Educational contexts and workshops
- Live performance setups
- Prototyping musical ideas
- Minimalist music creation

The looper requires only the standard Raspberry Pi Pico W hardware with no additional components, utilizing just the onboard BOOTSEL button for all interactions.

[![YouTube Demo](https://img.youtube.com/vi/_J5nquZ1nLc/0.jpg)](https://www.youtube.com/shorts/_J5nquZ1nLc)

## Features

| Feature                | Description                                |
|------------------------|--------------------------------------------|
| BLE-MIDI Compatibility | Wirelessly connects to DAWs and synth apps |
| 2-Bar Pattern Length   | 32 steps (4 beats × 4 subdivisions × 2 bars)|
| 4-Track Structure      | Separate tracks for different drum sounds  |
| Tap-Tempo              | Set BPM by tapping the button              |
| Quantized Input        | Notes automatically align to 16th-note grid|
| LED Feedback           | Visual indication of current step          |
| Serial Display         | Pattern visualization via terminal         |
| Single-Button Control  | All functions accessible through one button|

The system is intentionally minimalist yet expressive, providing musicians with a compact but capable looping tool.

## Getting Started

### Flash the Firmware

You can either download a prebuilt `.uf2` from the [Releases](https://github.com/oyama/pico-midi-looper-ble/releases/latest) page, or build it yourself.

To flash the firmware:

1. Hold the `BOOTSEL` button while connecting your Raspberry Pi Pico W via USB.
2. Copy the `pico-midi-looper-ble.uf2` file onto the mounted USB drive.
3. The device will reboot and start running the looper.

### Connect via BLE-MIDI

1. Open a BLE-MIDI compatible app (e.g., GarageBand on iOS).
2. Look for `Pico` and connect to it.
3. Start recording and playing right away.

For detailed instructions on iOS, see
[Getting Started with GarageBand](docs/getting-started-with-garageband.md)

## How It Works

You build up your loop by switching between four tracks and entering notes step by step.
Once powered on, the looper is ready to use.
All interaction is handled via a single button. The length of your press determines the action.

### Interaction

Interaction with the Pico MIDI Looper is handled entirely through the single BOOTSEL button, with different press durations triggering different functions.

| Action          | Duration | Function                           |
|-----------------|----------|------------------------------------|
| Click           | < 0.5s   | Record a note on the current track |
| Press           | ≥ 0.5s   | Switch to the next track           |
| Long Press      | ≥ 2s     | Enter tap-tempo mode               |
| Very Long Press | ≥ 5s     | Clear all tracks                   |

The button interface is handled by a dedicated subsystem that detects press durations and generates appropriate events.

### Tracks and Sounds

The looper provides four distinct tracks, each associated with a specific MIDI note number that corresponds to standard General MIDI drum sounds:

| Track	  | Sound	        |MIDI Note |
|---------|---------------|----------|
| Track 1	| Bass Drum     | 36       |
| Track 2	| Snare Drum    | 38       |
| Track 3	| Closed Hi-Hat | 42       |
| Track 4	| Open Hi-Hat   | 46       |

Additionally, the system uses two special sounds for feedback:

- Hand Clap (MIDI Note 39) for track switching cues
- Rim Shot (MIDI Note 37) for metronome clicks

These MIDI notes are defined in the code and transmitted over BLE-MIDI to compatible receivers.

### Display

When connected to a serial terminal (at 115200 bps), the Pico MIDI Looper provides a visual representation of the current patterns across all tracks:

![Pattern display over serial](https://github.com/user-attachments/assets/2bc9e6dc-1d39-46f6-961d-7481962bb068)

The display shows:

- All four tracks labeled by their drum sound
- A 32-step sixteenth-note loop (2 bars)  
- The current playback position

This visual feedback makes it easier to understand the current pattern and position during playback and recording.

## Building from Source

This project uses the [pico-sdk](https://github.com/raspberrypi/pico-sdk).
Refer to the official guide, [Getting Started with Raspberry Pi Pico](https://datasheets.raspberrypi.com/pico/getting-started-with-pico.pdf), to set up your development environment.

Once ready:

```bash
git clone https://github.com/oyama/pico-midi-looper-ble.git
cd pico-midi-looper-ble
mkdir build && cd build
PICO_SDK_PATH=/path/to/pico-sdk cmake .. -DPICO_BOARD=pico_w
make
```

This will produce the `pico-midi-looper-ble.uf2` in your `build/` directory.

## Architecture

The firmware follows a clear two‑layer design.

### Core layer

Located under `src/`, the core consists of fewer than three hundred lines of hardware‑agnostic C. It is orchestrated by one main finite‑state machine—the *looper*, which advances the step sequencer—and one subsidiary FSM: the *tap‑tempo* estimator, which transforms a series of taps into a BPM value.

### Drivers layer

Residing in `drivers/`, the drivers provide the project’s only connection to hardware. Each driver exposes a narrow C interface and fulfils a single purpose—for example, sending BLE‑MIDI packets, debouncing the BOOTSEL switch, or printing a visualisation over UART. Replacing a driver (say, swapping BLE‑MIDI for USB‑MIDI or switching from a UART text view to an OLED display) requires no changes to the core and no rebuild of the remaining drivers.

### How the firmware runs

At run time the three state machines advance in response to two independent streams of events:

1. **Tempo ticks** – A BTstack user timer fires at an interval derived from the current BPM. Each tick calls `looper_handle_tick`, moving the sequencer forward by exactly one step with sample‑accurate timing.
2. **Button events** – The button driver debounces the hardware switch and raises high‑level events such as `CLICK_RELEASE` or `HOLD_RELEASE`. These events are processed by `looper_handle_input`, enabling the user to start or stop recording, switch tracks, clear patterns, or enter tap‑tempo mode.

This split keeps time‑critical audio pacing deterministic while preserving a responsive user interface.

*Read the full design in* [`docs/architecture.md`](docs/architecture.md).

## License

This project is licensed under the 3-Clause BSD License. For details, see the [LICENSE](LICENSE.md) file.

## Contributing

Simplicity is at the heart of this project.
We welcome pull requests and issues that help refine the code or enhance the user experience — while keeping things minimal and readable.
Feel free to fork the project and adapt it to your own creative or educational needs.
We’d love to see how you build upon this tiny MIDI machine.

_Made with love in Japan._
