# ESP32-S3 Remote Display Project

This repository contains the source code for a system that remotely displays text messages on a round TFT display powered by an ESP32-S3 microcontroller. A user types a message in a web browser, which is then sent over the internet to the ESP32 device and shown on its screen.

## System Architecture

The system consists of three main components that communicate over the internet:

`[ESP32-S3 + Display] <--WiFi/Internet--> [Cloud Server] <--Internet--> [Laptop/Browser]`

1.  **ESP32-S3 Device**: A microcontroller with a round TFT touch display.
2.  **Cloud Server**: A Node.js WebSocket server that acts as a bridge between the ESP32 and the web browser.
3.  **Web Application**: A browser-based interface for typing and sending messages.

---

## Hardware

-   **MCU**: ESP32-S3 Development Board (ESP32S3-NxxRxx-128I80T)
-   **Display**: 1.28" round TFT, 240x240 pixels, with a GC9A01 driver.
-   **Interface**: 8-bit parallel (I80)
-   **Touch Controller**: CST816 capacitive touch controller.

---

## Features

### ESP32 Firmware

-   **Wi-Fi Manager**: On first boot, scans for Wi-Fi networks and displays them. Users can select a network and enter the password using an on-screen touch keyboard. Credentials are saved for automatic reconnection.
-   **Cloud Connectivity**: Establishes a persistent WebSocket connection to the cloud server. Automatically reconnects if the connection is lost.
-   **Message Display**: Receives text messages from the server and displays them on the screen.
-   **Touch Controls**:
    -   **Swipe Left/Right**: Navigate between previous and next messages.
    -   **Double Tap**: Toggles the display backlight and enters a low-power standby mode.
-   **Standby Mode**: Reduces power consumption by turning off the backlight and lowering CPU frequency, while keeping the touch controller active for wakeup.

### Cloud Server (Node.js)

-   **WebSocket Broker**: Manages connections from both the ESP32 device and web browser clients, relaying messages between them.
-   **Device Status Notifications**: Notifies all connected browsers when the ESP32 device comes online or goes offline.
-   **Message History**: Stores all messages sent during the day, allowing browsers to retrieve the history.

### Web Application (Browser)

-   **Text Input**: A text field for composing messages.
-   **Live Preview**: A real-time preview of how the message will look on the round display.
-   **Send Message**: A button to send the composed message to the ESP32.
-   **Device Status**: An indicator showing whether the ESP32 is `ONLINE` or `OFFLINE`.
-   **Message History**: A chronological list of messages sent that day, with the ability to resend any message.

---

## Technology Stack

-   **Firmware**:
    -   PlatformIO + Arduino Framework
    -   **LovyanGFX**: Graphics library for the display.
    -   **WebSocketsClient**: For WebSocket communication.
    -   **ArduinoJson**: For parsing JSON messages.
-   **Cloud Server**:
    -   Node.js
    -   **Express**: For serving the web application.
    -   **ws**: For the WebSocket server.
-   **Web Application**:
    -   Vanilla HTML, CSS, and JavaScript.

---

## Project Structure

```
.
├── firmware/               # ESP32 source code (PlatformIO)
│   ├── platformio.ini
│   └── src/
│       ├── main.cpp
│       ├── cloud_client.cpp
│       ├── screen_manager.cpp
│       ├── wifi_manager.cpp
│       └── touch_driver.cpp
├── server/                 # Node.js server and web app
│   ├── index.js
│   └── public/
│       └── index.html
└── README.md
```

---

## How to Use

### Firmware

1.  **Prerequisites**:
    *   [PlatformIO CLI](https://platformio.org/install/cli)
    *   [Python](https://www.python.org/) with `esptool` installed (`pip install esptool`)
2.  **Build**:
    *   Navigate to the `firmware` directory.
    *   Run: `platformio run`
3.  **Flash**:
    *   Connect the ESP32-S3 and put it into bootloader mode.
    *   Run the command below, replacing `COM3` with your device's serial port:
        ```bash
        python -m esptool --chip esp32s3 --port COM3 --baud 921600 write-flash --flash-mode dio --flash-freq 80m --flash-size 16MB 0x0 good_bl_and_pt.bin 0x10000 .pio/build/waveshare_s3_1_28/firmware.bin
        ```

### Server

1.  **Prerequisites**:
    *   [Node.js and npm](https://nodejs.org/)
2.  **Installation**:
    *   Navigate to the `server` directory.
    *   Run `npm install`.
3.  **Start**:
    *   Run `node index.js`.