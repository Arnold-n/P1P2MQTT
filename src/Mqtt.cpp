#include "Mqtt.h"

#include <AsyncMQTT_ESP32.h>
#include <ETH.h>

#include "P1P2_NetworkParams.h"
#include "P1P2_Config.h"

// NOTE: deliberately NOT including P1P2_HomeAssistant.h here.
// It holds real (non-extern) global definitions used by the Arnold
// compat layer (P1P2_Compat.cpp); including it from a second .cpp
// file causes "multiple definition" link errors. We only need the
// topic buffer size, so a small local constant is enough.
constexpr size_t MQTT_TOPIC_BUF_LEN = 200;

#include "P1P2_CompatAPI.h"
#include "AtmegaProtocol.h"
#include "WebSerial.h"

const char* P1P2Compat_mqttServer();
uint16_t P1P2Compat_mqttPort();
const char* P1P2Compat_mqttUser();
const char* P1P2Compat_mqttPassword();
const char* P1P2Compat_mqttClientName();
bool P1P2Compat_mqttEnabled();

void subscribeControlTopics();

namespace
{
    AsyncMqttClient mqtt;

    bool mqttStarted = false;
    bool mqttConnected = false;
    bool mqttEnabled = true;

    uint32_t lastConnectAttemptMs = 0;
    uint32_t mqttConnectAttempts = 0;

    uint32_t mqttPublishCalls = 0;
    uint32_t mqttPublishSuccess = 0;
    uint32_t mqttPublishFailed = 0;
    uint32_t mqttLastPublishMs = 0;

    bool mqttLastPublishResult = false;

    constexpr size_t MQTT_TEST_TOPIC_LEN = 232;

    char mqttLastPublishTopic[MQTT_TEST_TOPIC_LEN] = "";

    constexpr uint32_t MQTT_RECONNECT_INTERVAL = 5000;

    //
    // AsyncMQTT statistics
    //
    uint32_t mqttPublishQueued = 0;
    uint32_t mqttPublishAcknowledged = 0;

    uint32_t mqttPublishRejected = 0;


    void onMqttConnect(bool sessionPresent)
    {
        (void)sessionPresent;

        mqttConnected = true;

        logPrintf("[MQTT] Connected");

        subscribeControlTopics();

        P1P2Compat_onMqttConnected();
    }


void onMqttDisconnect(
    AsyncMqttClientDisconnectReason reason)
{
    mqttConnected = false;

    P1P2Compat_onMqttDisconnected();

    // Log the actual reason instead of discarding it -- this is the
    // only way to tell a TCP-level failure (broker unreachable, network
    // not ready yet) apart from a real MQTT-level rejection (bad
    // credentials, identifier rejected, ...).
    const char* reasonText = "UNKNOWN";

    switch (reason)
    {
        case AsyncMqttClientDisconnectReason::TCP_DISCONNECTED:
            reasonText = "TCP_DISCONNECTED";
            break;
        case AsyncMqttClientDisconnectReason::MQTT_UNACCEPTABLE_PROTOCOL_VERSION:
            reasonText = "MQTT_UNACCEPTABLE_PROTOCOL_VERSION";
            break;
        case AsyncMqttClientDisconnectReason::MQTT_IDENTIFIER_REJECTED:
            reasonText = "MQTT_IDENTIFIER_REJECTED";
            break;
        case AsyncMqttClientDisconnectReason::MQTT_SERVER_UNAVAILABLE:
            reasonText = "MQTT_SERVER_UNAVAILABLE";
            break;
        case AsyncMqttClientDisconnectReason::MQTT_MALFORMED_CREDENTIALS:
            reasonText = "MQTT_MALFORMED_CREDENTIALS";
            break;
        case AsyncMqttClientDisconnectReason::MQTT_NOT_AUTHORIZED:
            reasonText = "MQTT_NOT_AUTHORIZED";
            break;
        case AsyncMqttClientDisconnectReason::ESP8266_NOT_ENOUGH_SPACE:
            reasonText = "NOT_ENOUGH_SPACE";
            break;
        case AsyncMqttClientDisconnectReason::TLS_BAD_FINGERPRINT:
            reasonText = "TLS_BAD_FINGERPRINT";
            break;
        default:
            break;
    }

    logPrintf(
        "[MQTT] Disconnected, reason=%s (%d)",
        reasonText,
        (int)reason
    );
}

    void onMqttPublish(const uint16_t& packetId)
    {
        (void)packetId;

        mqttPublishAcknowledged++;
    }


    void configureConnection()
    {
        mqtt.setServer(
            P1P2Compat_mqttServer(),
            P1P2Compat_mqttPort()
        );

        char clientId[MQTT_CLIENTNAME_LEN + 8];

        snprintf(
            clientId,
            sizeof(clientId),
            "%s_ESP32",
            P1P2Compat_mqttClientName()
        );

        mqtt.setClientId(clientId);

        mqtt.setKeepAlive(15);

        mqtt.setWill(
            P1P2Compat_mqttAvailabilityTopic(),
            MQTT_QOS_WILL,
            MQTT_RETAIN_WILL,
            "offline"
        );

        if (strlen(P1P2Compat_mqttUser()) > 0)
        {
            mqtt.setCredentials(
                P1P2Compat_mqttUser(),
                P1P2Compat_mqttPassword()
            );
        }
    }

    void connectMqtt()
    {

        if (!mqttEnabled)
            return;
        if (mqtt.connected())
            return;

        // Don't even try before the network is actually up (ETH may still
        // report no IP for a few seconds right after boot on slower
        // switches).
        if (!ETH.linkUp() || !ETH.localIP())
            return;

        if (millis() - lastConnectAttemptMs < MQTT_RECONNECT_INTERVAL)
            return;

        lastConnectAttemptMs = millis();
        mqttConnectAttempts++;

        // Empirically, a bare mqtt.connect() here sometimes never
        // succeeds after a fresh boot/restart -- even with the network
        // confirmed up -- while an explicit disconnect() + full
        // reconfigure (server/client id/will/credentials) right before
        // connect() reliably works (this is exactly what the manual
        // "Save & Reconnect" button does). The disconnect() matters:
        // it resets whatever internal state a previous stuck/failed
        // attempt left behind. Doing this every attempt is cheap (no
        // real network I/O besides the connect itself) and removes the
        // need to ever click that button by hand.
        mqtt.disconnect();

        configureConnection();

        logPrintf("[MQTT] Connecting...");

        mqtt.connect();
    }

void onMqttMessage(
    char* topic,
    char* payload,
    const AsyncMqttClientMessageProperties& properties,
    const size_t& len,
    const size_t& index,
    const size_t& total)
{
    (void)properties;

    if (!topic || !payload)
        return;

    /*
     * For now we only handle small complete messages.
     * This is sufficient for homeassistant/status.
     */
    if (index != 0 || len != total)
    {
        Serial.printf(
            "[MQTT] Ignoring fragmented message: %s "
            "index=%u len=%u total=%u\n",
            topic,
            (unsigned)index,
            (unsigned)len,
            (unsigned)total
        );

        return;
    }

    // homeassistant/status, and write commands (bridge-specific + broadcast)
    // are all short, single-shot messages -- 128 bytes is plenty for a
    // "1P2P"-style hex command like "E3500401" and leaves headroom.
    char message[128];

    size_t copyLen = len;

    if (copyLen >= sizeof(message))
        copyLen = sizeof(message) - 1;

    memcpy(
        message,
        payload,
        copyLen
    );

    message[copyLen] = '\0';

    Serial.printf(
        "[MQTT] %s = %s\n",
        topic,
        message
    );

    if (strcmp(topic, "homeassistant/status") == 0)
    {
        if (strcmp(message, "online") == 0)
        {
            P1P2Compat_onHomeAssistantOnline();
        }

        return;
    }

    // "W" write topics: forward the payload to the ATmega as-is.
    // AtmegaProtocol::sendCommand() / AtmegaSerial::sendCommand() add the
    // "1P2P" prefix and "\r\n" terminator, then write it out on UART2 TX.
    char broadcastTopic[MQTT_TOPIC_BUF_LEN];

    P1P2Compat_mqttWriteBroadcastTopic(
        broadcastTopic,
        sizeof(broadcastTopic)
    );

    if (strcmp(topic, P1P2Compat_mqttWriteTopic()) == 0 ||
        strcmp(topic, broadcastTopic) == 0)
    {
        // Arnold's D#/L# commands are ESP-local (restart ESP, factory
        // reset, UI mode switches, ...) and are NEVER meant to reach the
        // ATmega -- P1P2Monitor has no concept of e.g. "restart ESP".
        // Only D0 is implemented so far; other D#/L# codes are TODO and
        // currently fall through to the ATmega below (harmless: it just
        // ignores commands it doesn't recognize).
        if (strcmp(message, "D0") == 0)
        {
            logPrintf("[MQTT] D0 received: restarting ESP32");
            P1P2Compat_restartEsp();
            return;
        }

        // The existing HA button "Restart_P1P2Monitor_ATmega" sends "A",
        // matching Arnold's ESP8266 hard-reset command (ESP toggles a
        // GPIO wired to the ATmega's physical RESET pin). We have no such
        // GPIO on this board, so "A" would just be forwarded to the
        // ATmega as a meaningless serial string and silently ignored.
        // P1P2Monitor DOES understand a real, serial-only self-reset
        // ('K'/'k' -> resetFunc()), which works over our existing UART2
        // TX with no extra wiring. Remap A -> K so that button actually
        // does something.
        if (strcmp(message, "A") == 0)
        {
            logPrintf("[MQTT] A received: remapping to k (ATmega serial self-reset)");
            AtmegaProtocol::sendCommand("k");
            return;
        }

        logPrintf(
            "[MQTT] Write command received: %s",
            message
        );

        AtmegaProtocol::sendCommand(message);

        return;
    }
}


}


namespace Esp32Mqtt
{

bool enabled()
{
    return mqttEnabled;
}


void setEnabled(bool value)
{
    mqttEnabled = value;

    if (!value)
    {
        if (mqtt.connected())
            mqtt.disconnect();

        mqttConnected = false;

        logPrintf("[MQTT] Disabled");
        return;
    }

    logPrintf("[MQTT] Enabled");

    if (mqttStarted)
    {
        lastConnectAttemptMs = 0;
        connectMqtt();
    }
}


void begin()
{
    /*
     * Load Arnold compatibility state/settings before
     * reading MQTT configuration.
     */
    P1P2Compat_init();

    mqttEnabled = P1P2Compat_mqttEnabled();

    configureConnection();

    mqtt.onConnect(onMqttConnect);
    mqtt.onDisconnect(onMqttDisconnect);
    mqtt.onPublish(onMqttPublish);
    mqtt.onMessage(onMqttMessage);

    mqttStarted = true;

    if (!mqttEnabled)
    {
        logPrintf("[MQTT] Disabled by configuration");
        return;
    }

    logPrintf("[MQTT] AsyncMQTT initialized");
}


void disconnect()
{
    if (mqtt.connected())
        mqtt.disconnect();

    mqttConnected = false;
}

bool reconnect()
{
    if (!mqttStarted)
        return false;

    if (!mqttEnabled)
    {
        disconnect();
        return false;
    }

    if (mqtt.connected())
        mqtt.disconnect();

    mqttConnected = false;

    mqtt.setServer(
        P1P2Compat_mqttServer(),
        P1P2Compat_mqttPort()
    );

    char clientId[MQTT_CLIENTNAME_LEN + 8];

    snprintf(
        clientId,
        sizeof(clientId),
        "%s_ESP32",
        P1P2Compat_mqttClientName()
    );

    mqtt.setClientId(clientId);

    mqtt.setKeepAlive(15);


    mqtt.setWill(
        P1P2Compat_mqttAvailabilityTopic(),
        MQTT_QOS_WILL,
        MQTT_RETAIN_WILL,
        "offline"
    );

    mqtt.setCredentials(
        P1P2Compat_mqttUser(),
        P1P2Compat_mqttPassword()
    );

    lastConnectAttemptMs = 0;

    logPrintf("[MQTT] Reconfiguring...");
    logPrintf(
        "[MQTT] Server: %s:%u",
        P1P2Compat_mqttServer(),
        P1P2Compat_mqttPort()
    );

    connectMqtt();

    return true;
}

void loop()
{
    if (!mqttStarted)
        return;

    //
    // AsyncMQTT does not require mqtt.loop().
    //
    // We only initiate reconnect attempts here.
    //
    if (!mqtt.connected())
    {
        connectMqtt();
    }

    yield();
}


bool started()
{
    return mqttStarted;
}


bool connected()
{
    return mqttStarted && mqtt.connected();
}


const char* server()
{
    return P1P2Compat_mqttServer();
}

uint16_t port()
{
    return P1P2Compat_mqttPort();
}

const char* clientName()
{
    static char clientId[MQTT_CLIENTNAME_LEN + 8];

    snprintf(
        clientId,
        sizeof(clientId),
        "%s_ESP32",
        P1P2Compat_mqttClientName()
    );

    return clientId;
}

int state()
{
    if (!mqttStarted)
        return -100;

    if (mqtt.connected())
        return 0;

    return -1;
}


const char* stateText()
{
    if (!mqttStarted)
        return "NOT_STARTED";

    if (mqtt.connected())
        return "CONNECTED";

    return "DISCONNECTED";
}


uint32_t connectAttempts()
{
    return mqttConnectAttempts;
}


uint32_t lastConnectAttempt()
{
    return lastConnectAttemptMs;
}


uint32_t publishCalls()
{
    return mqttPublishCalls;
}


uint32_t publishSuccess()
{
    return mqttPublishSuccess;
}


uint32_t publishFailed()
{
    return mqttPublishFailed;
}


uint32_t publishQueued()
{
    return mqttPublishQueued;
}


uint32_t publishAcknowledged()
{
    return mqttPublishAcknowledged;
}


uint32_t publishRejected()
{
    return mqttPublishRejected;
}

uint32_t lastPublishMs()
{
    return mqttLastPublishMs;
}


const char* lastPublishTopic()
{
    return mqttLastPublishTopic;
}


bool lastPublishResult()
{
    return mqttLastPublishResult;
}


bool publish(const char* topic,
             uint8_t qos,
             bool retain,
             const char* value)
{
    if (!mqttEnabled)
        return false;

    mqttPublishCalls++;
    mqttLastPublishMs = millis();

    if (!topic)
    {
        mqttLastPublishResult = false;
        mqttPublishFailed++;

        mqttPublishRejected++;

        mqttLastPublishTopic[0] = '\0';

        return false;
    }

    if (!mqtt.connected())
    {
        mqttLastPublishResult = false;
        mqttPublishFailed++;

        mqttPublishRejected++;

        snprintf(
            mqttLastPublishTopic,
            sizeof(mqttLastPublishTopic),
            "%s",
            topic
        );

        return false;
    }


    snprintf(
        mqttLastPublishTopic,
        sizeof(mqttLastPublishTopic),
        "%s",
        topic
    );

    const char* payload = value ? value : "";


    //
    // IMPORTANT:
    //
    // AsyncMQTT publish() is asynchronous.
    //
    // A non-zero packet ID means that the message was accepted
    // by the MQTT client for transmission.
    //
    uint16_t packetId =
        mqtt.publish(
            topic,
            qos,
            retain,
            payload
        );


    bool result = (packetId != 0);

    mqttLastPublishResult = result;


    if (result)
    {
        mqttPublishSuccess++;
        mqttPublishQueued++;
    }
    else
    {
        mqttPublishFailed++;
        mqttPublishRejected++;
    }


    return result;
}


bool publishNull(const char* topic,
                 uint8_t qos,
                 bool retain)
{
    if (!mqttEnabled)
        return false;

    mqttPublishCalls++;
    mqttLastPublishMs = millis();


    if (!topic)
    {
        mqttLastPublishResult = false;
        mqttPublishFailed++;
        mqttPublishRejected++;

        mqttLastPublishTopic[0] = '\0';

        return false;
    }


    if (!mqtt.connected())
    {
        mqttLastPublishResult = false;
        mqttPublishFailed++;
        mqttPublishRejected++;

        snprintf(
            mqttLastPublishTopic,
            sizeof(mqttLastPublishTopic),
            "%s",
            topic
        );

        return false;
    }


    snprintf(
        mqttLastPublishTopic,
        sizeof(mqttLastPublishTopic),
        "%s",
        topic
    );

    uint16_t packetId =
        mqtt.publish(
            topic,
            qos,
            retain,
            nullptr,
            0
    );


    bool result = (packetId != 0);

    mqttLastPublishResult = result;


    if (result)
    {
        mqttPublishSuccess++;
        mqttPublishQueued++;
    }
    else
    {
        mqttPublishFailed++;
        mqttPublishRejected++;
    }


    return result;
}


bool publishBudgetExhausted()
{
    //
    // There is no artificial publish-per-loop limit
    // with AsyncMQTT.
    //
    return false;
}

}


void subscribeControlTopics()
{
    char topic[MQTT_TOPIC_BUF_LEN];

    // Broadcast:
    P1P2Compat_mqttWriteBroadcastTopic(
        topic,
        sizeof(topic)
    );

    uint16_t packetId =
        mqtt.subscribe(
            topic,
            MQTT_QOS_CONTROL
        );

    Serial.printf(
        "[MQTT] Subscribe %s packetId=%u\n",
        topic,
        packetId
    );


    // Specific bridge:
    const char* specific =
        P1P2Compat_mqttWriteTopic();

    packetId =
        mqtt.subscribe(
            specific,
            MQTT_QOS_CONTROL
        );

    Serial.printf(
        "[MQTT] Subscribe %s packetId=%u\n",
        specific,
        packetId
    );


    // HA status:
    packetId =
        mqtt.subscribe(
            "homeassistant/status",
            MQTT_QOS_CONTROL
        );

    Serial.printf(
        "[MQTT] Subscribe homeassistant/status packetId=%u\n",
        packetId
    );
}