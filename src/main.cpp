/*
 * ESP32-Discord-WakeOnCommand v0.1
 * Copyright (C) 2023  Neo Ting Wei Terrence
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <Arduino.h>
#include <M5Atom.h>
#include <WiFiMulti.h>
#include <WiFiUdp.h>
#include <WakeOnLan.h>

#include <discord.h>
#include <interactions.h>
#include <privateconfig.h>

 // LED Colors
#define WHITE  0xFFFFFF //Standby
#define RED    0x800000 //Error
#define GREEN  0x008000 //Idle
#define BLUE   0x000080 //Offline Idle
#define PURPLE 0x800080 //Connecting to Discord
#define AMBER  0xFF4000 //Executing command
#define OFF    0x000000

#define LOGIN_INTERVAL 30000 //Cannot be too short to give time to initially retrieve the gateway API

// This sets Arduino Stack Size - comment this line to use default 8K stack size
//SET_LOOP_TASK_STACK_SIZE(16 * 1024); // 16KB

WiFiMulti wifiMulti;
WiFiUDP UDP;
WakeOnLan WOL(UDP);

Discord::Bot discord(botToken);

bool botEnabled = true;
bool broadcastAddrSet = false;
unsigned long lastLoginAttempt = 0;

bool update_wifi_status() {
    if (wifiMulti.run() == WL_CONNECTED) {
        if (broadcastAddrSet) return true;

        // Attention: 255.255.255.255 is denied in some networks
        IPAddress broadcastAddr = WOL.calculateBroadcastAddress(WiFi.localIP(), WiFi.subnetMask());
        Serial.print("[WIFI] Broadcast address set to ");
        broadcastAddr.printTo(Serial);
        Serial.println();
        broadcastAddrSet = true;
        Serial.println("[WIFI] Wi-Fi connection established.");

        return true;
    }
    broadcastAddrSet = false;
    return false;
}

void on_discord_interaction(const char* name, const JsonObject& interaction) {
    M5.dis.drawpix(0, PURPLE);

    if (strcmp(name, "ping") == 0) {
        Discord::Bot::MessageResponse response;

#ifdef _DISCORD_CLIENT_DEBUG
        String msg("Uplink online. Uptime: ");
        msg += millis();
        msg += "ms, Stack remaining: ";
        msg += uxTaskGetStackHighWaterMark(NULL);
        msg += "b";
        response.content = msg.c_str();
#else
        response.content = "Uplink online.";
#endif
        discord.sendCommandResponse(Discord::Bot::InteractionResponse::CHANNEL_MESSAGE_WITH_SOURCE, response);
    }
    else if (strcmp(name, "wake") == 0) {
        Discord::Bot::MessageResponse response;
        uint64_t id;
        if (interaction.containsKey("member")) {
            id = interaction["member"]["user"]["id"];
        }
        else {
            id = interaction["user"]["id"];
        }

        bool authorised = false;
        for (int i = 0; i < sizeof(botOwnerIds) / sizeof(botOwnerIds[0]); ++i) {
            if (id != botOwnerIds[i]) continue;

            authorised = true;
            response.content = "Command acknowledged. Initiating remote wake sequence.";
            discord.sendCommandResponse(Discord::Bot::InteractionResponse::CHANNEL_MESSAGE_WITH_SOURCE, response);
            if (WOL.sendMagicPacket(macAddress)) {
                Serial.println("[WOL] Packet sent.");
            }
            else {
                Serial.println("[WOL] Packet failed to send.");
                M5.dis.drawpix(0, RED);
            }
        }

        if (!authorised) {
            response.content = "Access denied.";
            response.flags = Discord::Bot::MessageResponse::Flags::EPHEMERAL;
            discord.sendCommandResponse(Discord::Bot::InteractionResponse::CHANNEL_MESSAGE_WITH_SOURCE, response);
        }
    }

    vTaskDelay(500);
}

void registerCommands() {
    Serial.println("Registering commands...");
    Discord::Interactions::ApplicationCommand cmd;
    // 1. /ping
    cmd.name = "ping";
    cmd.type = Discord::Interactions::CommandType::CHAT_INPUT;
    cmd.description = "Ping the bot for a response.";
    cmd.default_member_permissions = 2147483648; //Use Application Commands

    uint64_t id = Discord::Interactions::registerGlobalCommand(discord.applicationId(), cmd, botToken);
    if (id == 0) {
        Serial.println("Command registration failed!");
    }
    else {
        Serial.print("Registered ping command to id ");
        Serial.println(id);
    }

    //2. /wake
    cmd.name = "wake";
    cmd.type = Discord::Interactions::CommandType::CHAT_INPUT;
    cmd.description = "Send a wake signal to the main terminal. Authorized users only.";
    cmd.default_member_permissions = 2147483648;

    id = Discord::Interactions::registerGlobalCommand(discord.applicationId(), cmd, botToken);
    if (id == 0) {
        Serial.println("Command registration failed!");
    }
    else {
        Serial.print("Registered wake command to id ");
        Serial.println(id);
    }
}

// PROGRAM BEGIN

// put your setup code here, to run once:
void setup() {
    // Clear the serial port buffer and set the serial port baud rate to 115200.
    // Do not Initialize I2C. Initialize the LED matrix.
    M5.begin(true, false, true);
    M5.dis.drawpix(0, WHITE);
    Serial.print("[CONFIG] Target MAC address set to ");
    Serial.println(macAddress);
    Serial.print("[CONFIG] Default network set to ");
    Serial.println(wifiSSID);
    wifiMulti.addAP(wifiSSID, wifiPassword);

    discord.onInteraction(on_discord_interaction);
}

#ifdef _DISCORD_CLIENT_DEBUG
long lastStackValue = 0;
long lastStackCheck = 0;
long lastHeapValue = 0;
#endif

void loop() {
    M5.update();
    // put your main code here, to run repeatedly:
    unsigned long now = millis();

    //Current record:
    //Loop() - Free Stack Space: 3132
#ifdef _DISCORD_CLIENT_DEBUG
    if (now - lastStackCheck >= 2000) {
        // Print unused stack for the task that is running loop()
        long currentStack = uxTaskGetStackHighWaterMark(NULL);
        if (currentStack != lastStackValue) {
            Serial.print("[STACK CHANGE] Loop() - Free Stack Space: ");
            Serial.print(currentStack);
            Serial.print(" (");
            Serial.print(currentStack - lastStackValue);
            Serial.println(")");
            lastStackValue = currentStack;
        }
        long currentFree = esp_get_free_heap_size();
        if (currentFree != lastHeapValue) {
            Serial.print("[HEAP CHANGE] - Free Heap Space: ");
            Serial.print(currentFree);
            Serial.print(" (");
            Serial.print(currentFree - lastHeapValue);
            Serial.println(")");
            lastHeapValue = currentFree;
        }
        lastStackCheck = now;
    }
#endif

    if (!update_wifi_status()) {
        M5.dis.drawpix(0, RED);
        Serial.println("[WIFI] Wi-Fi connection not established.");
        vTaskDelay(1000);
        return;
    }

    if (botEnabled) {
        if (!discord.online()) {
            M5.dis.drawpix(0, PURPLE);
            if (now > lastLoginAttempt) {
                lastLoginAttempt = now + LOGIN_INTERVAL;
                discord.login(4096); // DIRECT_MESSAGES
            }
        }
        else {
            M5.dis.drawpix(0, GREEN);
        }
        discord.update(now);
    }
    else {
        M5.dis.drawpix(0, BLUE);
    }

    if (M5.Btn.pressedFor(5000)) {
        M5.dis.drawpix(0, AMBER);
    }
    else if (M5.Btn.pressedFor(2500)) {
        M5.dis.drawpix(0, PURPLE);
    }
    if (M5.Btn.wasReleasefor(5000)) {
        botEnabled = !botEnabled;

        if (!botEnabled && discord.online()) {
            discord.logout();
        }
    }
    else if (M5.Btn.wasReleasefor(2500)) {
        if (!botEnabled) {
            Serial.println("Bot offline, command update not performed.");
        }
        else {
            registerCommands();
        }
    }
    else if (M5.Btn.wasReleased()) {
        if (WOL.sendMagicPacket(macAddress)) {
            Serial.println("[WOL] Packet sent.");
            M5.dis.drawpix(0, AMBER);
        }
        else {
            Serial.println("[WOL] Packet failed to send.");
            M5.dis.drawpix(0, RED);
        }
        vTaskDelay(100);
    }
}
