#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>

#ifndef _DISCORD_ESP32A_INTERACTIONS_H_
#define _DISCORD_ESP32A_INTERACTIONS_H_

namespace Discord::Interactions {
    enum class CommandType {
        INVALID,
        // Slash commands; a text-based command that shows up when a user types /
        CHAT_INPUT = 1,
        // A UI-based command that shows up when you right click or tap on a user
        USER,
        // A UI-based command that shows up when you right click or tap on a message
        MESSAGE
    };

    struct ApplicationCommand {
        enum class OptionType {
            SUB_COMMAND = 1,
            SUB_COMMAND_GROUP,
            STRING,
            // Any integer between -2^53 and 2^53
            INTEGER,
            BOOLEAN,
            USER,
            // Includes all channel types + categories
            CHANNEL,
            ROLE,
            // Includes users and roles
            MENTIONABLE,
            // Any double between -2^53 and 2^53
            NUMBER,
            // Discord attachment object
            ATTACHMENT
        };

        struct Option {
            struct Choice {
                const char* name;
                const char* stringValue = "";
                int intValue;
                double doubleValue;
            };
            const char* name;
            const char* description = "";
            OptionType type;
            bool required;
            Choice* choices;
            size_t choicesLength = 0;
        };

        const char* name;
        CommandType type;
        const char* description = "";
        Option* options;
        size_t optionsLength = 0;
        bool dm_permission = true;
        uint64_t default_member_permissions;
        bool nsfw = false;
    };

    /// @brief Registers a global command for the bot.
    /// @param applicationId Your bot's application ID, found on the developer portal.
    /// @param command Details of the command.
    /// @param botToken The bot's token, used for authentication.
    /// @return The id of the command if it returned successfully, or an empty string if it failed.
    uint64_t registerGlobalCommand(
        uint64_t applicationId, const ApplicationCommand& command, const char* botToken);

    /// @brief Registers a guild command for the bot.
    /// @param applicationId Your bot's application ID, found on the developer portal.
    /// @param guildId The guild or server ID, can be copied via right-click on the server's name
    /// @param command Details of the command.
    /// @param botToken The bot's token, used for authentication.
    /// @return The id of the command if it returned successfully, or an empty string if it failed.
    uint64_t registerGuildCommand(
        uint64_t applicationId, const char* guildId, const ApplicationCommand& command, const char* botToken);

    bool deleteGlobalCommand(uint64_t applicationId, const String& commandId, const char* botToken);
    bool deleteGuildCommand(
        uint64_t applicationId, const char* guildId, const String& commandId, const char* botToken);

    bool serializeCommand(const ApplicationCommand& command, StaticJsonDocument<1024>& doc);
}

#endif //_DISCORD_ESP32A_INTERACTIONS_H_