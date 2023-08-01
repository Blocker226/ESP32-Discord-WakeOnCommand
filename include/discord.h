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

#include <mutex>

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WebSocketsClient.h>

#ifndef _DISCORD_ESP32A_H_
#define _DISCORD_ESP32A_H_

#define DISCORD_HOST "https://discord.com"
#define DISCORD_API_URI "/api/v10"
#define DISCORD_GATEWAY_SUFFIX "/?v=10&encoding=json"

namespace Discord {
    class Bot {
    public:
        enum class Event {
            Dispatch,
            Heartbeat,
            Identify,
            PresenceUpdate,
            VoiceStateUpdate,
            Resume = 6,
            Reconnect,
            RequestGuildMembers,
            InvalidSession,
            Hello,
            HeartbeatAck,

            // These are subsets of the actual Dispatch event, used for callback event handling optimisation
            Ready,
            Resumed,
            ApplicationCommandPermissionsUpdate,
            // Auto-moderation
            AutoModerationRuleCreate,
            AutoModerationRuleUpdate,
            AutoModerationRuleDelete,
            AutoModerationRuleExecution,
            // Channels
            ChannelCreate,
            ChannelUpdate,
            ChannelDelete,
            ThreadCreate,
            ThreadUpdate,
            ThreadDelete,
            ThreadListSync,
            ThreadMemberUpdate,
            ThreadMembersUpdate,
            ChannelPinsUpdate,
            // Guilds
            GuildCreate,
            GuildUpdate,
            GuildDelete,
            GuildAuditLogEntryCreate,
            GuildBanAdd,
            GuildBanRemove,
            GuildEmojisUpdate,
            GuildStickersUpdate,
            GuildIntegrationsUpdate,
            GuildMemberAdd,
            GuildMemberRemove,
            GuildMemberUpdate,
            GuildMembersChunk,
            GuildRoleCreate,
            GuildRoleUpdate,
            GuildRoleDelete,
            GuildScheduledEventCreate,
            GuildScheduledEventUpdate,
            GuildScheduledEventDelete,
            GuildScheduledEventUserAdd,
            GuildScheduledEventUserRemove,
            //Integrations
            IntegrationCreate,
            IntegrationUpdate,
            IntegrationDelete,
            //Interactions
            InteractionCreate,
            //Invite
            InviteCreate,
            InviteDelete,
            //Messages
            MessageCreate,
            MessageUpdate,
            MessageDelete,
            MessageDeleteBulk,
            //Reactions
            MessageReactionAdd,
            MessageReactionRemove,
            MessageReactionRemoveAll,
            MessageReactionRemoveEmoji,
            //Stage
            StageInstanceCreate,
            StageInstanceUpdate,
            StageInstanceDelete,
            //Misc
            TypingStart,
            UserUpdate,
            VoiceServerUpdate,
            WebhooksUpdate
        };

        enum class InteractionResponse {
            // ACK a Ping
            PONG = 1,
            // Respond to an interaction with a message
            CHANNEL_MESSAGE_WITH_SOURCE = 4,
            // ACK an interaction and edit a response later, the user sees a loading state
            DEFERRED_CHANNEL_MESSAGE_WITH_SOURCE,
            // Components: ACK and edit the original message later; the user does not see a loading state
            DEFERRED_UPDATE_MESSAGE,
            // Components: edit the message the component was attached to
            UPDATE_MESSAGE,
            // Respond to an autocomplete interaction with suggested choices
            APPLICATION_COMMAND_AUTOCOMPLETE_RESULT,
            // Respond to an interaction with a popup modal
            MODAL
        };

        // struct User {
        //     enum class PremiumType {
        //         NONE,
        //         NITRO_CLASSIC,
        //         NITRO,
        //         NITRO_BASIC
        //     };

        //     uint64_t id = 0;
        //     const char* username = "";
        //     uint16_t discriminator = 0;
        //     const char* globalName = "";
        //     const char* avatarHash = "";
        //     bool bot = false;
        //     bool system = false;
        //     bool mfaEnabled = false;
        //     const char* bannerHash = "";
        //     int accentColor = 0;
        //     const char* locale = "";
        //     bool verified = false;
        //     const char* email = "";
        //     uint32_t flags = 0;
        //     PremiumType premiumType = PremiumType::NONE;
        //     uint32_t publicFlags = 0;
        //     const char* avatarDecoHash = "";
        // };

        // struct Member {
        //     const char* nick = "";
        //     const char* avatarHash = "";
        //     std::vector<uint64_t> roles;
        //     //TODO: joined_at, premium_since
        //     bool deaf = false;
        //     bool mute = false;
        //     uint32_t flags = 0;
        //     bool pending = false;
        //     uint64_t permissions = 0;
        //     //TODO: communication_disabled_until?
        // };

        // struct Interaction {
        //     enum class Type {
        //         INVALID,
        //         PING = 1,
        //         APPLICATION_COMMAND,
        //         MESSAGE_COMPONENT,
        //         APPLICATION_COMMAND_AUTOCOMPLETE,
        //         MODAL_SUBMIT
        //     };

        //     struct Data {
        //         uint64_t commandId = 0;
        //         const char* name = "";
        //         CommandType type = CommandType::INVALID;
        //         // TODO: resolved?, options?
        //         uint64_t guildId = 0;
        //         uint64_t targetId = 0;
        //     };

        //     uint64_t id = 0;
        //     uint64_t applicationId = 0;
        //     Type type = Type::INVALID;
        //     Data data;
        //     uint64_t guildId = 0;
        //     // TODO: channel?
        //     uint64_t channelId = 0;
        //     Member member;
        //     User user;
        //     uint8_t version = 1;
        //     // TODO: message?
        //     uint64_t permissions = 0;
        //     const char* locale = "";
        //     // Not available for PING interactions
        //     const char* guildLocale = "";
        // };

        typedef std::function<void(Event type, const StaticJsonDocument<1024>& json)> EventCallback;
        typedef std::function<void(const char* name, const JsonObject& interaction)> InteractionCallback;
        //typedef std::function<void(const char* name, const Interaction& interaction)> InteractionCallback;

        struct MessageResponse {
            enum class Flags : char {
                NONE = 0,
                SUPPRESS_EMBEDS = 1 << 2,
                EPHEMERAL = 1 << 6,
            };

            bool tts = false;
            const char* content = "";
            // TODO: array of embed objects, 
            // allowed_mentions object, 
            // array of components, 
            // array of partial attachments
            Flags flags = Flags::NONE;
        };

        Bot(const char* botToken, bool rateLimit = true);

        void login(unsigned int intents = 0);

        void update(unsigned long now);

        void logout();

        void onEvent(const EventCallback& cb);
        void onInteraction(const InteractionCallback& cb);

        void sendCommandResponse(const InteractionResponse& type, const StaticJsonDocument<512>& response);
        void sendCommandResponse(const InteractionResponse& type, const MessageResponse& response);

        //void updatePresence();

        bool online() { return _online; }

        uint64_t applicationId() { return _applicationId; }
    private:
        void onWebSocketEvents(WStype_t type, uint8_t* payload, size_t length);
        void parseMessage(uint8_t* payload, size_t length);

        void heartbeat();
        void identify();
        void resume();

        bool sendWS(const char* payload, size_t length);

        std::mutex _httpsMtx;
        HTTPClient _https;
        WebSocketsClient _socket;
        EventCallback _outerCallback;
        InteractionCallback _interactionCallback;

        String _gatewayURL;

        const char* _op = "op";
        const char* _d = "d";
        const char* _t = "t";
        const char* _botToken = nullptr;
        uint64_t _applicationId = 0;
        unsigned int _intents = 0;

        uint64_t _interactionId;
        String _interactionToken;

        bool _online = false;

        unsigned long _now;
        unsigned long _heartbeatInterval = 0;
        unsigned long _lastHeartbeatAck = 0;
        unsigned long _lastHeartbeatSend = 0;
        unsigned long _firstHeartbeat = 0;

        bool _ready = false;
        String _sessionId;
        // You need to cache the most recent non-null sequence value for heartbeats, and to pass when resuming a connection.
        unsigned int _lastSocketSequence = 0;

        // Rate limiting
        bool _rateLimit = true;
        unsigned short _eventsSent = 0;
        unsigned long _lastRateReset = 0;
    };

    inline Bot::MessageResponse::Flags operator | (Bot::MessageResponse::Flags lhs, Bot::MessageResponse::Flags rhs) {
        return static_cast<Bot::MessageResponse::Flags>(static_cast<int>(lhs) | static_cast<int>(rhs));
    }

    template <size_t sz>
    struct AsyncAPIRequest {
        AsyncAPIRequest(
            HTTPClient& httpClient,
            const char* method,
            const String& uri,
            const String& json = "",
            const char* authorisationToken = "",
            std::function<void(const StaticJsonDocument<sz>& json)> cb = nullptr,
            std::mutex* mtx = nullptr);

        HTTPClient& client;
        const char* method;
        const String uri = "";
        const String json = "";
        const char* authorisationToken = "";
        std::function<void(const StaticJsonDocument<sz>& json)> callback;
        std::mutex* clientMtx = nullptr;
    };

    bool sendRest(
        HTTPClient& client,
        const char* method,
        const String& uri,
        const String& json = "",
        const char* authorisationToken = "");

    template <size_t sz>
    bool sendRest(
        HTTPClient& client,
        const char* method,
        const String& uri,
        const String& json = "",
        const char* authorisationToken = "",
        StaticJsonDocument<sz>* responseDoc = nullptr);

    template <size_t sz>
    void sendPostAsync(
        HTTPClient& httpClient,
        const char* method,
        const String& uri,
        const String& json,
        const char* authorisationToken,
        std::function<void(const StaticJsonDocument<sz>& json)> cb,
        std::mutex* mtx);

    template <size_t sz>
    void sendPostTask(void* parameter);
}

#include <discord.hpp>

#endif //_DISCORD_ESP32A_H_
