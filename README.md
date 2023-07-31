# ESP32 Discord Wake-On-Command Bot

Send a Discord command or DM, and wake a target device anywhere, anytime!

## Description
A self-contained Discord Bot that runs on an M5Stack Atom Lite and sends  a Wake-On-Lan signal to a hardcoded target device from a slash command. Built for ESP32 devices using Arduino C++, and runs on an M5Stack Atom Lite.

Currently the main code makes use of the M5Atom architecture for certain onboard capabilities such as LED and button control. **Expanded compatability for other ESP32 devices will come eventually.**

## Features
- Simple ping command to poll responsiveness
- Wake command to send WOL packet to a specific device's MAC address
- Manual WOL packet sending via button press
- Based on a expandable ESP32 Discord Bot framework (to be published separately)
- *Cool LED status indicator*

## Dependencies
- [M5Atom](https://github.com/m5stack/M5Atom) 0.1.0
    - FastLED 3.6.0
- [WakeOnLan](https://github.com/a7md0/WakeOnLan) 1.1.7
- [ArduinoJson](https://github.com/bblanchon/ArduinoJson) 6.21.2
- [arduinoWebSockets](https://github.com/Links2004/arduinoWebSockets) 2.4.1

## Installation
1. Configure Wifi, Discord Bot token, and your own user ID in `privateconfig.template`, and rename the file to `privateconfig.h`.
2. Build and upload the code to the M5Stack Atom.
3. Once the LED is green, press and hold the button for 2.5 seconds, the LED will turn purple, releasing it will let the ESP32 register the two global commands. This must be done when changes are made to the command structure.

TBC

## Usage
TBA
