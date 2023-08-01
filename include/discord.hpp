#include <discord.h>
#ifdef _DISCORD_CLIENT_DEBUG
#include <StreamUtils.h>
#endif

namespace Discord {
    template<size_t sz>
    inline bool sendRest(
        HTTPClient& client,
        const char* method,
        const String& uri,
        const String& json,
        const char* authorisationToken,
        StaticJsonDocument<sz>* responseDoc) {

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
                if (responseDoc)
                {
                    // Here we pass getString instead of getStream. While ArduinoJson recommends against this,
                    // this allows us to keep the benefits of HTTP 1.1+, since Discord's payloads are usually small.
#ifdef _DISCORD_CLIENT_DEBUG
                    String p = client.getString();
                    DeserializationError e = deserializeJson(*responseDoc, p);
                    Serial.println(p);
#else
                    DeserializationError e = deserializeJson(*responseDoc, client.getString());
#endif
                    if (e) {
                        Serial.print(F("deserializeJson() failed with code "));
                        Serial.println(e.f_str());

                        // Serialisation failed, free resources
                        //client.end();
                        return false;
                    }
                }
                else {
                    Serial.println(client.getString());
                }
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

    template<size_t sz>
    AsyncAPIRequest<sz>::AsyncAPIRequest(
        HTTPClient& httpClient,
        const char* method,
        const String& uri,
        const String& json,
        const char* authorisationToken,
        std::function<void(const StaticJsonDocument<sz>& json)> cb,
        std::mutex* mtx) :
        client { httpClient },
        method { method },
        uri { uri },
        json { json },
        authorisationToken { authorisationToken },
        callback { cb },
        clientMtx { mtx } {}

    template<size_t sz>
    void sendPostAsync(
        HTTPClient& httpClient,
        const char* method,
        const String& uri,
        const String& json,
        const char* authorisationToken,
        std::function<void(const StaticJsonDocument<sz>& json)> cb,
        std::mutex* mtx) {

        AsyncAPIRequest<sz>* request = new AsyncAPIRequest<sz>(
            httpClient, method, uri, json, authorisationToken, std::move(cb), mtx);

        TaskHandle_t task = nullptr;
        // Task priority of 2 will ensure the post request gets sent first within the 3s window.
        // IIRC, this also avoids the scheduler from switching back and forth, avoiding race conditions.
        xTaskCreate(
            sendPostTask<sz>,
            "DiscordSendPostTask",
            4 * 1024 + sz,
            static_cast<void*>(request),
            tskIDLE_PRIORITY + 2, &task);

#ifdef _DISCORD_CLIENT_DEBUG
        Serial.print("Async task created with ");
        Serial.print(4 * 1024 + sz);
        Serial.println(" bytes of stack allocated.");
#endif
    }

    template<size_t sz>
    void sendPostTask(void* parameter) {
        AsyncAPIRequest<sz>* request = static_cast<AsyncAPIRequest<sz>*>(parameter);

        // Lock the HttpClient via the provided mutex if needed to avoid race conditions on multiple tasks.
        // std::unique_lock<std::mutex> lock = (request->clientMtx == nullptr) ?
        //     std::unique_lock<std::mutex>() : std::unique_lock<std::mutex>(*request->clientMtx);
        if (request->clientMtx) {
            request->clientMtx->lock();
        }

        request->client.setURL(request->uri);

        int httpResponseCode = 0;

        request->client.addHeader("Content-Type", "application/json");
        if (strlen(request->authorisationToken) > 0) {
            String headerTok = "Bot ";
            headerTok += request->authorisationToken;
            request->client.addHeader("Authorization", headerTok);
        }

#ifdef _DISCORD_CLIENT_DEBUG
        if (!request->json.isEmpty()) {
#endif
            httpResponseCode = request->client.POST(request->json);
#ifdef _DISCORD_CLIENT_DEBUG
        }
        else {
            // Request failed
            Serial.print("[DISCORD] No payload to POST with!");
            delete request;
            vTaskDelete(nullptr);
        }
#ifdef ESP32
        log_d("[DISCORD] Sent %s request to %s", request->method, request->uri.c_str());
#else
        Serial.print(request->method);
        Serial.print(" request to ");
        Serial.println(request->uri);
#endif
        long currentStack = uxTaskGetStackHighWaterMark(NULL);
        Serial.print("[STACK CHECK] sendPostTask() - Free Stack Space: ");
        Serial.println(currentStack);
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
            if (httpResponseCode == HTTP_CODE_BAD_REQUEST) {
                Serial.println("[DISCORD] 400 Bad Request.");
            }
            else if (httpResponseCode == HTTP_CODE_UNAUTHORIZED) {
                Serial.println("[DISCORD] 401 Not Authorised.");
            }
            else if (request->callback != nullptr) {
                StaticJsonDocument<sz> response;

                // Here we pass getString instead of getStream. While ArduinoJson recommends against this,
                // this allows us to keep the benefits of HTTP 1.1+, since Discord's payloads are usually small.
                if (httpResponseCode != HTTP_CODE_NO_CONTENT) {
#ifdef _DISCORD_CLIENT_DEBUG
                    String p = request->client.getString();
                    DeserializationError e = deserializeJson(response, p);
                    Serial.println(p);
#else
                    DeserializationError e = deserializeJson(response, request->client.getString());
#endif
                    if (e) {
                        Serial.print(F("deserializeJson() failed with code "));
                        Serial.println(e.c_str());
                    }
                }
                request->callback(response);
            }
            if (request->clientMtx) {
                request->clientMtx->unlock();
            }
            delete request;
            vTaskDelete(nullptr);
        }

        // Request failed
        if (request->clientMtx) {
            request->clientMtx->unlock();
        }
        Serial.print("[DISCORD] Error code: ");
        Serial.println(httpResponseCode);
        delete request;
        vTaskDelete(nullptr);
    }
}