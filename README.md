# ESP32 Watch Project

This repository contains the source code for an ESP32-S3 based watch.

## Project Structure

The project is divided into two main parts:

-   `/firmware`: Contains the C++ source code for the ESP32-S3 microcontroller. This code is managed using PlatformIO.
-   `/server`: Contains a simple Node.js server that the watch can communicate with.

## Firmware

The firmware is built using the Arduino framework via PlatformIO. It handles:
- Display rendering on the GC9A01 round display.
- Touch input via the CST816S controller.
- Wi-Fi connectivity.
- Communication with the backend server.

### How to Build and Flash

1.  **Prerequisites:**
    *   [PlatformIO CLI](https://platformio.org/install/cli)
    *   [Python](https://www.python.org/) with `esptool` installed (`pip install esptool`)

2.  **Build:**
    *   Navigate to the `firmware` directory.
    *   Run the command: `platformio run`

3.  **Flash:**
    *   Connect the ESP32-S3 device and put it into bootloader mode.
    *   Use the following command, replacing `COM3` with your device's serial port:
        ```bash
        python -m esptool --chip esp32s3 --port COM3 --baud 921600 write-flash --flash-mode dio --flash-freq 80m --flash-size 16MB 0x0 good_bl_and_pt.bin 0x10000 .pio/build/waveshare_s3_1_28/firmware.bin
        ```

## Server

The server is a simple Node.js application that can be used to send data to the watch.

### How to Run

1.  **Prerequisites:**
    *   [Node.js and npm](https://nodejs.org/)

2.  **Installation:**
    *   Navigate to the `server` directory.
    *   Run `npm install` to install dependencies.

3.  **Start the server:**
    *   Run `node index.js`.
