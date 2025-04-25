# Pico MIDI Looper

[![Build](https://github.com/oyama/pico-midi-looper/actions/workflows/build-firmware.yml/badge.svg)](https://github.com/oyama/pico-midi-looper/actions)

A minimal 2-bar drum looper for Raspberry Pi Pico W that speaks Bluetooth Low-Energy MIDI (BLE-MIDI).
Build a palm-sized looper in under 10 minutes —ideal for workshops, prototyping, or live-coding sets.
Record and play back grooves with nothing but the built-in `BOOTSEL` button.

[![YouTube Demo](https://img.youtube.com/vi/biRl0yx8jz4/0.jpg)](https://www.youtube.com/watch?v=biRl0yx8jz4)

## Bill of Materials

| Item                       | Qty | Notes                                                     |
|----------------------------|-----|-----------------------------------------------------------|
| [Raspberry Pi Pico W](https://www.raspberrypi.com/products/raspberry-pi-pico/) | 1   | Comes with on-board BOOTSEL button and LED. |
| Micro-USB cable            | 1   | For power and firmware install.                           |
| BLE-MIDI-capable synth/DAW | 1   | e.g. GarageBand on iOS, Logic Pro, etc.                   |

## Features

- BLE-MIDI compatible (works with GarageBand, DAWs, and synth apps)
- 2-bar loop (32 steps: 4 beats x 4 subdivisions x 2 bars)
- 4-tracks: Bass drum, Snare, Closed Hi-hat and Open Hi-hat
- Tap-tempo: set the global BPM with 2-4 taps on the same button
- Quantized note input
- LED feedback for step visualization
- Graphical track-pattern display via UART/USB-CDC serial terminal
- Single-button control with expressive timing
- Designed for education, installations, and minimalist instruments

## Getting Started

### Flash the Firmware

You can either download a prebuilt `.uf2` from the [Releases](https://github.com/oyama/pico-midi-looper/releases/latest) page, or build it yourself.

To flash the firmware:

1. Hold the `BOOTSEL` button while connecting your Pico W via USB.
2. Copy the `pico-midi-looper.uf2` file onto the mounted USB drive.
3. The device will reboot and start running the looper.

### Connect via BLE-MIDI

1. Open a BLE-MIDI compatible app (e.g., GarageBand on iOS).
2. Look for `Pico` and connect to it.
3. Start recording and playing right away.

For detailed instructions on iOS, see
[Getting Started with GarageBand on iPhone](docs/getting-started-with-garageband.md)

## How It Works

You build up your loop by switching between two tracks and entering notes step by step.
Once powered on, the looper is ready to use.
All interaction is handled via a single button. The length of your press determines the action.

### Button Actions

| Action                 | Hold-time |  Result                                                                               |
|------------------------|-----------|---------------------------------------------------------------------------------------|
| **Short Press**        | < 0.5 s   | Records a note on the current track. Automatically returns to playback after two bars |
| **Long Press**         | ≥ 0.5 s  | Switches to the next track (hand-clap cue)                                            |
| **Very-Long Press**    | ≥ 2 s    | Enters **Tap-tempo** mode. Long Press again (≥0.5 s) to confirm the tempo and return to Playing mode.|

### Tracks and Sounds

| Function          | Sound         | MIDI Note |
|-------------------|---------------|-----------|
| **Track 1**       | Bass Drum     | Note 36   |
| **Track 2**       | Snare Drum    | Note 38   |
| **Track 3**       | Closed Hi-Hat | Note 42   |
| **Track 4**       | Open Hi-Hat   | Note 46   |
| Track Switch Cue  | Hand Clap     | Note 39   |
| Metronome Click   | Rim shot      | Note 37   |

- Switching tracks cycles through 1 to 4.
- Notes are automatically quantized to 1/16th-note steps.
- Recording only affects the currently selected track.

## Building from Source

This project uses the [pico-sdk](https://github.com/raspberrypi/pico-sdk).
Refer to the official guide, [Getting Started with Raspberry Pi Pico](https://datasheets.raspberrypi.com/pico/getting-started-with-pico.pdf), to set up your development environment.

Once ready:

```bash
git clone https://github.com/oyama/pico-midi-looper.git
cd pico-midi-looper
mkdir build && cd build
PICO_SDK_PATH=/path/to/pico-sdk cmake .. -DPICO_BOARD=pico_w
make
```
This will produce `pico-midi-looper.uf2` in the `build` directory.

## Architecture

The core logic is organized as a small set of finite-state machines - the main looper (5 states) plus two tiny sub-FSMs for button timing and tap-tempo - yet still fits in under 400 lines of C.

Read the full architecture with FSM diagram:
[docs/architecture.md](docs/architecture.md)

## License

This project is licensed under the 3-Clause BSD License. For details, see the [LICENSE](LICENSE.md) file.

## Contributing

Simplicity is at the heart of this project.
We welcome pull requests and issues that help refine the code or enhance the user experience — while keeping things minimal and readable.
Feel free to fork the project and adapt it to your own creative or educational needs.
We’d love to see how you build upon this tiny MIDI machine.

Made with love in Japan.
We hope you enjoy building with it as much as we did.
