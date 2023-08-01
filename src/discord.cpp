#include <discord.h>

#define DISCORD_MESSAGE_PREFIX "[DISCORD] "

namespace Discord {

    Bot::Bot(const char* botToken, bool enableRateLimit) :
        _botToken { botToken }, _rateLimit { enableRateLimit } {}

    void Bot::login(unsigned int intents) {
        _https.begin(DISCORD_HOST, nullptr);
        //Establish a connection with the Gateway after fetching and caching a WSS URL using the Get Gateway endpoint.
        if (_gatewayURL.isEmpty()) {
            StaticJsonDocument<64> doc;
            if (sendRest<64>(_https, "GET", DISCORD_API_URI "/gateway", "", "", &doc)) {
                _gatewayURL = doc["url"].as<const char*>() + 6; // Remove the 'wss://' prefix
                Serial.print(DISCORD_MESSAGE_PREFIX "Gateway URL set to ");
                Serial.println(_gatewayURL);
            }
            else {
#ifdef ESP32
                log_e(DISCORD_MESSAGE_PREFIX "Failed to set Gateway URL.");
#else
                Serial.println(DISCORD_MESSAGE_PREFIX "Failed to set Gateway URL.");
#endif
                return;
            }
        }

        _socket.onEvent([=](WStype_t type, uint8_t* payload, size_t length) {
            this->onWebSocketEvents(type, payload, length);
            });
        Serial.print(DISCORD_MESSAGE_PREFIX "Attempting connection via WebSocket to ");
        Serial.println(_gatewayURL);
        _socket.beginSSL(_gatewayURL, 443, DISCORD_GATEWAY_SUFFIX);

        _intents = intents;
        _heartbeatInterval = 0;
        _lastHeartbeatAck = 0;
        _lastHeartbeatSend = 0;
    }

    void Bot::update(unsigned long now) {
        _now = now;
        _socket.loop();
        _online = _socket.isConnected();
        if (!_online && !_gatewayURL.isEmpty()) {
            //Clear gateway/resume URL cache
            _gatewayURL.clear();
            return;
        }

        if (_rateLimit && now - _lastRateReset > 60000) {
            // Serial.print("[DISCORD] Rate limit reset. Sent last minute: ");
            // Serial.println(_eventsSent);
            _eventsSent = 0;
            _lastRateReset = now;
        }

        if (_heartbeatInterval > 0 && _now > (_firstHeartbeat > 0 ? _lastHeartbeatSend + _firstHeartbeat : _lastHeartbeatSend + _heartbeatInterval)) {
            heartbeat();
            _firstHeartbeat = 0;
        }

        if (_lastHeartbeatAck > _lastHeartbeatSend + (_heartbeatInterval / 2))
        {
#ifdef ESP32
            log_w(DISCORD_MESSAGE_PREFIX "Heartbeat acknowledgement timeout!");
#else
            Serial.println(DISCORD_MESSAGE_PREFIX "Heartbeat acknowledgement timeout!");
#endif
            logout();
        }
    }

    void Bot::logout() {
        if (_socket.isConnected()) {
            _socket.disconnect();
            _online = false;
            _sessionId.clear();
            Serial.println(DISCORD_MESSAGE_PREFIX "Logout complete.");
        }
        _https.end();
    }

    void Bot::onEvent(const EventCallback& cb) {
        _outerCallback = cb;
    }

    void Bot::onInteraction(const InteractionCallback& cb) {
        _interactionCallback = cb;
    }

    inline void Bot::sendCommandResponse(const InteractionResponse& type, const StaticJsonDocument<256>& response) {

#ifdef _DISCORD_CLIENT_DEBUG
        unsigned long start = millis();
#endif

        String url(DISCORD_API_URI "/interactions/");
        url.reserve(sizeof(uint64_t) + _interactionToken.length() + 11);
        url += _interactionId;
        url += "/";
        url += _interactionToken;
        url += "/callback";

        String json((char*)0);
        json.reserve(256);
        serializeJson(response, json);

        sendPostAsync<256>(_https, "POST", std::move(url), json, _botToken,
#ifdef _DISCORD_CLIENT_DEBUG
            [start](const StaticJsonDocument<256>& response) {
#else
            [](const StaticJsonDocument<256>& response) {
#endif

                log_i(DISCORD_MESSAGE_PREFIX "[COMMAND] Response sent.");
#ifdef _DISCORD_CLIENT_DEBUG
                unsigned long end = millis();
                Serial.print("Time to respond (ms): ");
                Serial.println(end - start);
#endif
            }, & _httpsMtx);

        return;
    }

    void Bot::sendCommandResponse(const InteractionResponse & type, const MessageResponse & response) {
        if (_interactionId == 0 || _interactionToken.isEmpty()) {
#ifdef ESP32
            log_e(DISCORD_MESSAGE_PREFIX "[COMMAND] No token or id available!");
#else
            Serial.println(DISCORD_MESSAGE_PREFIX "[COMMAND] No token or id available!");
#endif
            return;
        }
        StaticJsonDocument<256> doc;
        doc["type"] = static_cast<unsigned short>(type);
        JsonObject data = doc.createNestedObject("data");
        if (response.tts) {
            data["tts"] = true;
        }

        /*
        This theory and its following safeguard is currently untested.

        Buffer safety: If too many simultaneous interactions come in, the ESP32 will have trouble responding to all
        of the interactions within their allotted 3-second window due to not having enough heap space to
        queue and send responses. If that happens, a warning message is appended to the last possible response
        to notify users the bot is being overloaded, and the bot will fail to respond to subsequent interactions until
        the existing responses have been sent out.
        */
        if (esp_get_free_heap_size() < 2 * (4 * 1024 + 256)) {
            data["content"] = response.content;
        }
        else {
            String msg((char*)0);
            msg.reserve(strlen(response.content) + 102);
            msg += response.content;
            msg += "\n\n**Warning: Not enough memory for further processing. Please wait before sending further commands.**";
            data["content"] = msg;
        }


        if (static_cast<uint8_t>(response.flags)) {
            data["flags"] = static_cast<uint8_t>(response.flags);
            Serial.print("Flags: ");
            Serial.println(static_cast<uint8_t>(response.flags));
        }

        sendCommandResponse(type, doc);
    }

    void Bot::onWebSocketEvents(WStype_t type, uint8_t * payload, size_t length) {
        switch (type) {
            case WStype_ERROR:
                if (payload) {
                    Serial.print(DISCORD_MESSAGE_PREFIX "WebSocket error occured: ");
                    Serial.println((char*)payload);
                }
                else {
                    Serial.println(DISCORD_MESSAGE_PREFIX "A WebSocket connection error has occured.");
                }
                break;
            case WStype_DISCONNECTED:
                Serial.println(DISCORD_MESSAGE_PREFIX "Connection closed.");
                _online = false;
                break;
            case WStype_CONNECTED:
                Serial.println(DISCORD_MESSAGE_PREFIX "Connected to gateway.");
                _online = true;
                break;
            case WStype_TEXT:
#ifdef _DISCORD_CLIENT_DEBUG
#ifdef ESP32
                log_v(DISCORD_MESSAGE_PREFIX "Message received.");
#else
                Serial.println(DISCORD_MESSAGE_PREFIX "Message received.");
#endif
#endif
                parseMessage(payload, length);
                break;
            case WStype_BIN:
                break;
            case WStype_FRAGMENT_TEXT_START:
                break;
            case WStype_FRAGMENT_BIN_START:
                break;
            case WStype_FRAGMENT:
                break;
            case WStype_FRAGMENT_FIN:
                break;
            case WStype_PING:
                Serial.println(DISCORD_MESSAGE_PREFIX "Ping received.");
                break;
            case WStype_PONG:
                Serial.println(DISCORD_MESSAGE_PREFIX "Pong received.");
                break;
        }
    }

    void Bot::parseMessage(uint8_t * payload, size_t length) {
        //Deserialize the first part of our payload
        DynamicJsonDocument doc(2048);
        DeserializationError e = deserializeJson(doc, payload, length);
        if (e) {
            Serial.print("Payload deserializeJson() call failed with code ");
            Serial.println(e.c_str());
            // Handle the error here, don't pass it upward.
            return;
        }

#ifdef _DISCORD_CLIENT_DEBUG
        serializeJsonPretty(doc, Serial);
        Serial.println();
#endif

        switch (static_cast<Event>(doc[_op].as<int>()))
        {
            case Event::Dispatch:
                // Dispatch (opcode 0) events are the most common type of event.
                // Most Gateway events which represent actions taking place in a guild will be sent as Dispatch events.
                _lastSocketSequence = doc["s"];

                if (_outerCallback != nullptr) {
                    _outerCallback(Event::Dispatch, doc);
                }

                if (doc[_t] == "READY") {
                    _ready = true;
                    _sessionId = doc[_d]["session_id"].as<const char*>();
                    _gatewayURL = doc[_d]["resume_gateway_url"].as<const char*>() + 6;
                    _applicationId = doc[_d]["application"]["id"];
                    Serial.print(DISCORD_MESSAGE_PREFIX "Gateway URL set to resume on ");
                    Serial.println(_gatewayURL);
                    Serial.println(DISCORD_MESSAGE_PREFIX "Ready to comply.");
                    if (_outerCallback != nullptr) {
                        _outerCallback(Event::Ready, doc);
                    }
                    return;
                }
                else if (doc[_t] == "RESUMED") {
                    Serial.println(DISCORD_MESSAGE_PREFIX "Session resumed.");
                    if (_outerCallback != nullptr) {
                        _outerCallback(Event::Resumed, doc);
                    }
                    return;
                }
                else if (doc[_t] == "INTERACTION_CREATE") {
                    _interactionToken.reserve(256);
                    _interactionToken = doc[_d]["token"].as<const char*>();
                    _interactionId = doc[_d]["id"];

                    const char* interactionName = doc[_d]["data"]["name"];
                    Serial.print(DISCORD_MESSAGE_PREFIX "[COMMAND] Command ");
                    Serial.print(doc[_d]["data"]["id"].as<const char*>());
                    Serial.print(" used: ");
                    Serial.println(interactionName);

                    if (_interactionCallback != nullptr) {
                        _interactionCallback(interactionName, doc[_d].as<JsonObject>());
                    }
                    else {
                        Serial.println(DISCORD_MESSAGE_PREFIX "No interaction callback was found, no response given.");
                    }
                    return;
                }
                // Privileged intent MESSAGE_CONTENT required to see message contents outside of DMs and mentions.
                else if (doc[_t] == "MESSAGE_CREATE") {
                    //Ignore our own messages
                    if (doc[_d]["author"]["id"].as<uint64_t>() == _applicationId) return;
                    Serial.println(DISCORD_MESSAGE_PREFIX "New chat message received.");
                    if (_outerCallback != nullptr) {
                        _outerCallback(Event::MessageCreate, doc);
                    }
                    return;
                }
                if (_outerCallback != nullptr) {
                    _outerCallback(static_cast<Event>(doc[_op].as<int>()), doc);
                    return;
                }
                Serial.print(DISCORD_MESSAGE_PREFIX "Unmanaged dispatch event type: ");
                Serial.println(doc["t"].as<const char*>());
                return;
            case Event::Heartbeat:
                heartbeat();
                break;
            case Event::Identify:
                break;
            case Event::PresenceUpdate:
                break;
            case Event::VoiceStateUpdate:
                break;
            case Event::Resume:
                break;
            case Event::Reconnect:
                logout();
                login(_intents);
                break;
            case Event::RequestGuildMembers:
                break;
            case Event::InvalidSession:
                Serial.println(DISCORD_MESSAGE_PREFIX "Invalid session!");
                if (doc[_d].as<bool>() == false) {
                    Serial.println(DISCORD_MESSAGE_PREFIX "Clearing Gateway URL and session id.");
                    _gatewayURL.clear();
                    _sessionId.clear();
                    logout();
                    login(_intents);
                }
                else {
                    logout();
                }
                break;
            case Event::Hello:
                _heartbeatInterval = doc[_d]["heartbeat_interval"];
                Serial.print(DISCORD_MESSAGE_PREFIX "Heartbeat interval (ms): ");
                Serial.println(_heartbeatInterval);

                // Jitter is an offset value between 0 and heartbeat_interval that is meant to prevent too many clients 
                // from reconnecting at the exact same time (which could cause an influx of traffic).
                _firstHeartbeat = (random(0, 50) / 100.0f) * _heartbeatInterval;
                Serial.print(DISCORD_MESSAGE_PREFIX "First heartbeat (ms):");
                Serial.println(_firstHeartbeat);

                if (_sessionId.isEmpty()) {
                    identify();
                }
                else {
                    resume();
                }

                _lastHeartbeatSend = _now;
                _lastHeartbeatAck = _now;
                _lastRateReset = _now;

                if (_outerCallback != nullptr) {
                    _outerCallback(Event::Hello, doc);
                }
                break;
            case Event::HeartbeatAck:
                _lastHeartbeatAck = _now;
#ifdef _DISCORD_CLIENT_DEBUG 
#ifdef ESP32
                log_v(DISCORD_MESSAGE_PREFIX "Heartbeat acknowledged.");
#else
                Serial.println(DISCORD_MESSAGE_PREFIX "Heartbeat acknowledged.");
#endif
#endif
                break;
            default:
                break;
        }
    }

    void Bot::identify() {
        String payload;
        StaticJsonDocument<256> doc;

        doc[_op] = 2;

        JsonObject d = doc.createNestedObject(_d);
        d["token"] = _botToken;
        d["intents"] = _intents;

        JsonObject d_properties = d.createNestedObject("properties");
        d_properties["os"] = "esp32";
        d_properties["browser"] = "esp32";
        d_properties["device"] = "m5stack";

        serializeJson(doc, payload);

        if (!sendWS(payload.c_str(), payload.length())) return;

        Serial.print(DISCORD_MESSAGE_PREFIX "Identify event sent. Intents: ");
        Serial.println(_intents);
    }

    void Bot::heartbeat() {
        if (!_socket.isConnected()) {
            log_e(DISCORD_MESSAGE_PREFIX "Heartbeat not sent. No active connection.");
            return;
        }
        String payload = "{\"op\":1,\"d\":";
        if (_lastSocketSequence > 0) {
            payload += _lastSocketSequence;
            payload += "}";
        }
        else {
            payload += "null}";
        }

        if (!sendWS(payload.c_str(), payload.length())) return;
        // Send a periodic request to Discord to preserve the TCP connection.
        sendRest(_https, "GET", DISCORD_API_URI "/gateway");

        _lastHeartbeatSend = _now;

        if (_lastSocketSequence > 0) {
            Serial.print(DISCORD_MESSAGE_PREFIX "Heartbeat sent. Sequence: ");
            Serial.println(_lastSocketSequence);
        }
        else {
            Serial.println(F(DISCORD_MESSAGE_PREFIX "Heartbeat sent."));
        }
    }

    void Bot::resume() {
        if (_sessionId.isEmpty()) {
            Serial.println(DISCORD_MESSAGE_PREFIX "No session id found! Unable to resume.");
        }
        String payload;
        StaticJsonDocument<256> doc;

        doc[_op] = 6;

        JsonObject d = doc.createNestedObject(_d);
        d["token"] = _botToken;
        d["session_id"] = _sessionId;
        d["seq"] = _lastSocketSequence;

        serializeJson(doc, payload);

        if (!sendWS(payload.c_str(), payload.length())) return;

        Serial.println(_sessionId);
        Serial.println(_lastSocketSequence);
        Serial.println(DISCORD_MESSAGE_PREFIX "Resume event sent.");
    }

    inline bool Bot::sendWS(const char* payload, size_t length) {
        if (_rateLimit && _eventsSent >= 120) {
            Serial.println(DISCORD_MESSAGE_PREFIX "Rate limit reached! Maximum of 120 WebSocket events/min.");
            return false;
        }
        if (_socket.sendTXT(payload, length)) {
            ++_eventsSent;
            return true;
        }
        return false;
    }

    bool sendRest(HTTPClient & client, const char* method, const String & uri, const String & json, const char* authorisationToken) {
        client.setURL(uri);

        if (strcmp(method, "GET") != 0) {
            client.addHeader("Content-Type", "application/json");
            if (strlen(authorisationToken) > 0) {
                String headerTok = "Bot ";
                headerTok += authorisationToken;
                client.addHeader("Authorization", headerTok);
            }
        }

        int httpResponseCode = 0;
        if (!json.isEmpty()) {
            httpResponseCode = client.sendRequest(method, json);

        }
        else {
            httpResponseCode = client.sendRequest(method);
        }
#ifdef _DISCORD_CLIENT_DEBUG
#ifdef ESP32
        log_d("[DISCORD] Sent %s request to %s", method, uri.c_str());
#else
        Serial.print(method);
        Serial.print(" request to ");
        Serial.println(uri);
#endif
#endif

        if (httpResponseCode > 0) {
#ifdef _DISCORD_CLIENT_DEBUG
#ifdef ESP32
            log_d("[DISCORD] HTTP Response code: %d", httpResponseCode);
#else
            Serial.print("[DISCORD] HTTP Response code: ");
            Serial.println(httpResponseCode);
#endif
#endif
            if (httpResponseCode != 204) { //204 no content
#ifdef _DISCORD_CLIENT_DEBUG
#ifdef ESP32
                log_d("%s", client.getString().c_str());
#else
                Serial.println(client.getString());
#endif
#endif
            }
            if (httpResponseCode == 401) {
                Serial.println("[DISCORD] 401 Not Authorised.");
                return false;
            }
            return true;
        }

        // Request failed
        Serial.print("[DISCORD] Error code: ");
        Serial.println(httpResponseCode);
        return false;
    }
}