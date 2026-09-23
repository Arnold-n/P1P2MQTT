#pragma once

#include <Arduino.h>

namespace KamodHealth
{
    void begin();

    // ------------------------------------------------------------
    // System
    // ------------------------------------------------------------

    uint32_t uptimeSeconds();
    uint32_t freeHeap();
    uint32_t minFreeHeap();

    const char* resetReason();

    String ip();
    String mac();

    // ------------------------------------------------------------
    // Ethernet
    // ------------------------------------------------------------

    bool ethernetOK();
    String ethernetSpeed();

    // ------------------------------------------------------------
    // MQTT
    // ------------------------------------------------------------

    bool mqttOK();

    uint32_t mqttConnectAttempts();

    uint32_t mqttPublishCalls();
    uint32_t mqttPublishSuccess();
    uint32_t mqttPublishFailed();

    uint32_t mqttPublishQueued();
    uint32_t mqttPublishAcknowledged();
    uint32_t mqttPublishRejected();

    bool mqttPublishBudgetExhausted();

    bool mqttLastPublishResult();

    // ------------------------------------------------------------
    // P1/P2 / UART2
    // ------------------------------------------------------------

    uint32_t uart2Bytes();

    size_t uart2BufferUsed();

    bool uart2Active();

    // ------------------------------------------------------------
    // Local log
    // ------------------------------------------------------------

    uint32_t logBytes();
    size_t logUsed();
}