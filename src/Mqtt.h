#pragma once

#include <Arduino.h>

namespace Esp32Mqtt
{
    void begin();
    void loop();

    bool started();
    bool connected();

    const char* server();
    uint16_t port();
    const char* clientName();

    bool enabled();
    void setEnabled(bool value);
    bool reconnect();
    void disconnect();

    int state();
    const char* stateText();

    uint32_t connectAttempts();
    uint32_t lastConnectAttempt();
    uint32_t publishCalls();
    uint32_t publishSuccess();
    uint32_t publishFailed();

    uint32_t publishQueued();
    uint32_t publishAcknowledged();
    uint32_t publishRejected();

    uint32_t lastPublishMs();
    const char* lastPublishTopic();
    bool lastPublishResult();

    bool publish(const char* topic,
                 uint8_t qos,
                 bool retain,
                 const char* value);

    bool publishNull(const char* topic,
                     uint8_t qos,
                     bool retain);
    bool publishBudgetExhausted();
}