# Pico MIDI Looper

![Build Firmware](https://github.com/oyama/pico-midi-looper/actions/workflows/build-firmware.yml/badge.svg)

A minimal 1-bar drum looper for Raspberry Pi Pico W using MIDI over Bluetooth Low Energy(BLE-MIDI).
It lets you record and play rhythms using a single button, making it perfect for workshops, prototyping, and creative experiments.

[![YouTube Demo](https://img.youtube.com/vi/biRl0yx8jz4/0.jpg)](https://www.youtube.com/watch?v=biRl0yx8jz4)

## Features

- BLE-MIDI compatible (works with GarageBand, DAWs, and synth apps)
- 1-bar loop (16 steps: 4 beats × 4 subdivisions)
- Two tracks: bass drum and snare
- Quantized note input
- LED feedback for step visualization
- Single-button control with expressive timing
- Designed for education, installations, and minimalist instruments

## How It Works

You build up your loop by switching between two tracks and entering notes step by step.
Once powered on, the looper is ready to use.
All interaction is handled via a single button. The length of your press determines the action.

### Button Actions

| Action             | Description                                            |
|--------------------|--------------------------------------------------------|
| **Short Press**    | Starts recording and plays a note on the current track |
|                    | Automatically switches to playback mode after one bar  |
| **Long Press**     | Switches to the other track (open hi-hat cue sound)    |

### Tracks and Sounds

| Function            | MIDI Note / Sound              |
|---------------------|--------------------------------|
| Track 1 (Bass Drum) | Note 36 (commonly shown as C1) |
| Track 2 (Snare Drum)| Note 38 (commonly shown as D1) |
| Metronome Click     | Note 42 (Closed Hi-Hat)        |
| Track Switch Cue    | Note 46 (Open Hi-Hat)          |

- Notes are automatically quantized to 1/16th-note steps.
- Recording only affects the currently selected track.

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
This will produce `pico-midi-looper.uf2` in the `build/` directory.

## Architecture

The looper is implemented as a simple finite state machine:

`Waiting / Recording / Playing / TrackSwitch`

Each transition is triggered by intuitive user input.
The core logic fits in under 300 lines of code and is designed to be easy to read and modify — ideal for use in education or interactive art.
➡️ For the full state diagram, see: [docs/looper_fsm.svg](docs/looper_fsm.svg)

## License

This project is licensed under the 3-Clause BSD License. For details, see the [LICENSE](LICENSE.md) file.

## Contributing

Simplicity is at the heart of this project.
We welcome pull requests and issues that help refine the code or enhance the user experience — while keeping things minimal and readable.
Feel free to fork the project and adapt it to your own creative or educational needs.
We’d love to see how you build upon this tiny MIDI machine.

Made with love in Japan.
We hope you enjoy building with it as much as we did.
