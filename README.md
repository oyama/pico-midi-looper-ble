# Pico MIDI Looper

Make beats fly from a $6 microcontroller.
Pico MIDI Looper turns your Raspberry Pi Pico W into a one-button wireless rhythm looper.
With just the single onboard button, you can record and play back a one-bar rhythm loop consisting of kick and snare tracks. The loop is transmitted over BLE-MIDI in real time and can be played by software instruments on smartphones or PCs that support BLE-MIDI.
This project is designed to serve as:

- A template for BLE-MIDI output in physical computing and electronics projects
- An inexpensive and accessible learning material for workshops and education
- A toolbox component for experimental or artistic sound-based expression

## Features

- Runs standalone on a Raspberry Pi Pico W
- Single-button operation using the onboard switch
  - Short press: Input kick or snare
  - Long press: Switch between tracks
- LED feedback visualizes recorded rhythms
- MIDI notes are used to notify track switching
- Two tracks: kick and snare
- One-bar looping at a fixed tempo (e.g., 120 BPM)
- Wireless MIDI transmission via BLE-MIDI

## Build and Installation

This project uses **pico-sdk** for building. Please follow the instructions in [Getting Started with Raspberry Pi Pico](https://datasheets.raspberrypi.com/pico/getting-started-with-pico.pdf) to set up your development environment in advance.

```bash
git clone https://github.com/oyama/pico-midi-looper.git
cd pico-midi-looper

mkdir build; cd build;
PICO_SDK_PATH=/path/to/pico-sdk cmake .. -DPICO_BOARD=pico_w
make
```

After a successful build, drag and drop the generated `pico-midi-looper.uf2` file onto your Pico W while it's connected in `BOOTSEL` mode to complete installation.

## License

This project is licensed under the 3-Clause BSD License. For details, see the [LICENSE](LICENSE.md) file.

## Contributing

Simplicity is at the heart of this project.
We welcome pull requests and issues that help refine the code or improve the user experience —while keeping things minimal.
Feel free to fork the project and adapt it to your own creative or educational needs.
We’d love to see how you build upon this tiny MIDI machine.

---
Made with love in Japan.  
We hope you enjoy building with it as much as we did.
