# ESP32 Discord Wake-On-Command Bot
![Screenshot 2023-08-02 110951](https://github.com/Blocker226/ESP32-Discord-WakeOnCommand/assets/6292676/a28d357d-0267-4ea9-b922-60c7b87f66b8)

Send a Discord command or DM, and wake a target device anywhere, anytime!

## Description
A self-contained Discord Bot that runs on an [M5Stack Atom Lite](https://shop.m5stack.com/products/atom-lite-esp32-development-kit) and sends  a Wake-On-Lan signal to a hardcoded target device from a slash command. Built for ESP32 devices using Arduino C++.

Currently the main code makes use of the M5Atom architecture for certain onboard capabilities such as LED and button control. **Expanded compatability for other ESP32 devices will come eventually.**

## Features
- Simple ping command to poll responsiveness
- Wake command to send WOL packet to a specific device's MAC address
    - Limited access to a specific user
- Manual WOL packet sending via button press
- Based on a expandable ESP32 Discord Bot framework (to be published separately)
    - Built-in command registration
- *Cool LED status indicator*

## Dependencies
- [M5Atom](https://github.com/m5stack/M5Atom) 0.1.0
    - [FastLED](https://github.com/FastLED/FastLED) 3.6.0
- [WakeOnLan](https://github.com/a7md0/WakeOnLan) 1.1.7
- [ArduinoJson](https://github.com/bblanchon/ArduinoJson) 6.21.2
- [arduinoWebSockets](https://github.com/Links2004/arduinoWebSockets) 2.4.1

## Installation
1. Download the source code and open it in Visual Studio Code.
2. Use the [PlatformIO IDE](https://platformio.org/install/ide?install=vscode) to setup dependencies and build environments, or do it manually.
4. Configure Wifi, Discord Bot token, and your own user ID in `privateconfig.template`, and rename the file to `privateconfig.h`.
5. Plug in the M5Stack Atom to your PC via its USB-C port, then build and upload the code.
6. Once the LED is green, press and hold the button for 2.5 seconds, the LED will turn purple, releasing it will let the ESP32 register the two global commands. This must be done when changes are made to the command structure.

## Usage

Once the status light is green, the bot is online and connected to Discord. Invite it to a server and use the slash command in the server, or DM the bot.

To toggle the bot's connection to Discord, press and hold for 5 seconds until the light turns amber. The blue status light means the bot is no longer connected, and the ESP32 only responds to manual button presses.

### Commands
- `/ping` - Checks for responsiveness. The bot will reply with "Uplink online."
- `/wake` - Sends a WOL packet to the target MAC address specified in `privateconfig.h`. This only works for the user id specified in the file, and access will be denied for anyone else attempting to use the command.

### LED Status Colours
| Colour | Status                                                      |
|--------|-------------------------------------------------------------|
| White  | Initialising Wi-Fi connection.                              |
| Red    | An error has occurred with the Wi-Fi or Discord connection. |
| Green  | Connected to Discord, currently idle.                       |
| Blue   | Disconnected from Discord, currently idle.                  |
| Purple | Connecting to Discord.                                      |
| Amber  | Processing command.                                         |

The LED turns purple when receiving commands via Discord, and amber during manual operation.

### Troubleshooting
If the LED turns red, it could be for 3 reasons:

1. Unable to connect to Wi-Fi.
2. Unable to connect to Discord.
3. The WOL packet failed to send.

Plug the M5Stack Atom into a PC, reboot and check serial if needed. Using PlatformIO, the `m5stack-atom-debug` configuration defines an additional debug symbol to allow the bot to print additional debug information.

## Contributing

If you've found a reproducible bug or error, do file an issue! Further contributing guidelines will be made when necessary.
