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

#include <interactions.h>
#include "discord.h"
#ifdef _DISCORD_CLIENT_DEBUG
#include <StreamUtils.h>
#endif

#define DISCORD_INTERACTION_LOG_PREFIX "[DISCORD][COMMAND] "

namespace Discord::Interactions {
    uint64_t registerGlobalCommand(uint64_t applicationId, const ApplicationCommand& command, const char* botToken) {
        StaticJsonDocument<1024> doc;

        if (!serializeCommand(command, doc)) return 0;

        String url(DISCORD_HOST DISCORD_API_URI "/applications/");
        url += applicationId;
        url += "/commands";

        String json((char*)0);
        json.reserve(1024);
        serializeJson(doc, json);
        HTTPClient _http;
        _http.begin(url, nullptr);
        StaticJsonDocument<512> response;
        if (sendRest<512>(_http, "POST", url, json, botToken, &response)) {
            uint64_t idString = response["id"];

            Serial.print(DISCORD_INTERACTION_LOG_PREFIX "Global command ");
            Serial.print(idString);
            Serial.println(" registered.");
            _http.end();
            return idString;
        }
        _http.end();
        return 0;
    }

    uint64_t registerGuildCommand(uint64_t applicationId, const char* guildId, const ApplicationCommand& command, const char* botToken) {
        StaticJsonDocument<1024> doc;

        if (!serializeCommand(command, doc)) return 0;

        String url(DISCORD_HOST DISCORD_API_URI "/applications/");
        url += applicationId;
        url += "/guilds/";
        url += guildId;
        url += "/commands";

        String json((char*)0);
        json.reserve(1024);
        serializeJson(doc, json);
        HTTPClient _http;
        _http.begin(url, nullptr);
        StaticJsonDocument<512> response;
        if (sendRest<512>(_http, "POST", url, json, botToken, &response)) {
            uint64_t idString = response["id"];

            Serial.print(DISCORD_INTERACTION_LOG_PREFIX "[COMMAND] Guild command ");
            Serial.print(idString);
            Serial.println(" registered.");
            _http.end();
            return idString;
        }
        _http.end();
        return 0;
    }

    bool deleteGlobalCommand(uint64_t applicationId, const String& commandId, const char* botToken) {
        HTTPClient _http;
        String url(DISCORD_HOST DISCORD_API_URI "/applications/");
        url += applicationId;
        url += "/commands/";
        url += commandId;
        _http.begin(url);

        bool result = sendRest(_http, "DELETE", url, "", botToken);
        _http.end();
        return result;
    }

    bool deleteGuildCommand(
        uint64_t applicationId, const char* guildId, const String& commandId, const char* botToken) {
        HTTPClient _http;
        String url(DISCORD_HOST DISCORD_API_URI "/applications/");
        url += applicationId;
        url += "/guilds/";
        url += guildId;
        url += "/commands/";
        url += commandId;
        _http.begin(url);

        bool result = sendRest(_http, "DELETE", url, "", botToken);
        _http.end();
        return result;
    }

    bool serializeCommand(const ApplicationCommand& command, StaticJsonDocument<1024>& doc) {
        if (!strlen(command.name) || strlen(command.name) > 32) {
            Serial.println(DISCORD_INTERACTION_LOG_PREFIX "Invalid name provided!");
            return false;
        }
        doc["name"] = command.name;
        doc["type"] = static_cast<int>(command.type);
        doc["description"] = command.description;
        if (command.optionsLength > 0) {
            JsonArray options = doc.createNestedArray("options");

            for (size_t i = 0; i < command.optionsLength; ++i)
            {
                JsonObject option_obj = options.createNestedObject();
                ApplicationCommand::Option& option = command.options[i];
                if (!strlen(option.name) || strlen(option.name) > 32) {
                    Serial.println(DISCORD_INTERACTION_LOG_PREFIX "Invalid option name provided!");
                    return false;
                }

                option_obj["name"] = option.name;
                option_obj["description"] = option.description;
                option_obj["type"] = static_cast<int>(option.type);
                option_obj["required"] = option.required;

                if (option.choicesLength > 0) {
                    JsonArray choice_array = option_obj.createNestedArray("choices");
                    for (size_t i = 0; i < option.choicesLength; i++)
                    {
                        JsonObject choice_obj = choice_array.createNestedObject();
                        ApplicationCommand::Option::Choice& choice = option.choices[i];
                        if (!strlen(choice.name) || strlen(choice.name) > 32) {
                            Serial.println(DISCORD_INTERACTION_LOG_PREFIX "Invalid option choice name provided!");
                            return false;
                        }

                        choice_obj["name"] = choice.name;
                        switch (option.type)
                        {
                            case ApplicationCommand::OptionType::STRING:
                                choice_obj["value"] = choice.stringValue;
                                break;
                            case ApplicationCommand::OptionType::INTEGER:
                                choice_obj["value"] = choice.intValue;
                                break;
                            case ApplicationCommand::OptionType::NUMBER:
                                choice_obj["value"] = choice.doubleValue;
                                break;
                            default:
                                Serial.println(DISCORD_INTERACTION_LOG_PREFIX "Invalid option type provided with choice!");
                                break;
                        }
                    }
                }
            }
            if (command.dm_permission) {
                doc["dm_permission"] = true;
            }
            if (command.default_member_permissions > 0) {
                doc["default_member_permissions"] = command.default_member_permissions;
            }
            if (command.nsfw) {
                doc["nsfw"] = command.nsfw;
            }
        }
        return true;
    }
}
