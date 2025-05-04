# Getting Started with Pico MIDI Looper on GarageBand for iPhone

This guide explains how to connect the Raspberry Pi Pico MIDI Looper to GarageBand on your iPhone via Bluetooth MIDI and use it as a drum sound source.

## Requirements

- [Raspberry Pi Pico W](https://www.raspberrypi.com/products/raspberry-pi-pico/)
- iPhone
- [GarageBand](https://apps.apple.com/app/garageband/id408709785) (available for free on the App Store)

## 1. Start the Pico MIDI Looper

Connect your Pico W (with the [Pico MIDI Looper BLE-MIDI firmware](https://github.com/oyama/pico-midi-looper-ble/releases/latest) installed) to a power source. The onboard LED will begin blinking, indicating that it is waiting for a Bluetooth MIDI connection.
![plug](https://github.com/user-attachments/assets/6164990e-653f-4680-a00d-201dc9530452)

## 2. Set Up GarageBand

1. Launch GarageBand on your iPhone. 
2. Open any project, then go to the _TRACKS_ view and select _DRUMS > Acoustic Drams_. ![IMG_5078-landscape](https://github.com/user-attachments/assets/c4f14e5e-1dde-4388-9acf-14d0d37e2a8b)
3. Tap the gear icon to open Settings, then go to _Advanced > Bluetooth MIDI Devices_. ![IMG_5079-landscape](https://github.com/user-attachments/assets/0de7f875-c3fd-430b-a838-082298015087)![IMG_5080-landscape](https://github.com/user-attachments/assets/919be488-6a92-4dcc-9fec-87349fe17b4b)![IMG_5081-landscape](https://github.com/user-attachments/assets/a5e86d0c-6d91-4703-8b0d-04922a87a926)
4. When prompted with _"Allow GarageBand to find Bluetooth devices?"_, tap **Allow** to grant permission.![IMG_5082-landscape](https://github.com/user-attachments/assets/55bc73c5-8234-4a11-8b6d-ca518312ba5f)
5. In the list of available devices, tap on Pico to connect.![IMG_5083-landscape](https://github.com/user-attachments/assets/24cff487-72d0-4020-ace5-71b84237f2fe)

When you hear the click sound, you're ready to go. Press the button on your Pico and let your rhythm come to life!

> ⚠️ Note: Wireless earphones introduce noticeable latency, which makes them unsuitable for live rhythm performance. For the best experience, use the device’s built-in speaker or wired headphones.
