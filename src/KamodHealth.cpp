#include "KamodHealth.h"

#ifdef HW_KAMOD

#include <ETH.h>
#include <esp_system.h>

#include "Mqtt.h"
#include "WebSerial.h"

namespace
{
    uint32_t bootMillis = 0;

    // ------------------------------------------------------------
    // UART2 activity detection
    // ------------------------------------------------------------

    uint32_t lastUart2Bytes = 0;
    uint32_t lastUart2Check = 0;

    bool uart2Activity = false;

    constexpr uint32_t UART2_ACTIVITY_WINDOW_MS = 3000;

    // ------------------------------------------------------------
    // Reset reason
    // ------------------------------------------------------------

    const char* resetReasonText(
        esp_reset_reason_t reason
    )
    {
        switch (reason)
        {
            case ESP_RST_UNKNOWN:
                return "UNKNOWN";

            case ESP_RST_POWERON:
                return "POWERON";

            case ESP_RST_EXT:
                return "EXTERNAL";

            case ESP_RST_SW:
                return "SOFTWARE";

            case ESP_RST_PANIC:
                return "PANIC";

            case ESP_RST_INT_WDT:
                return "INT WDT";

            case ESP_RST_TASK_WDT:
                return "TASK WDT";

            case ESP_RST_WDT:
                return "WDT";

            case ESP_RST_DEEPSLEEP:
                return "DEEPSLEEP";

            case ESP_RST_BROWNOUT:
                return "BROWNOUT";

            case ESP_RST_SDIO:
                return "SDIO";

            default:
                return "OTHER";
        }
    }

    void updateUart2Activity()
    {
        uint32_t now = millis();

        uint32_t current =
            webSerialTotalWrittenUART2();

        if (current != lastUart2Bytes)
        {
            lastUart2Bytes = current;
            lastUart2Check = now;
            uart2Activity = true;
            return;
        }

        if (
            now - lastUart2Check >
            UART2_ACTIVITY_WINDOW_MS
        )
        {
            uart2Activity = false;
        }
    }
}

// ================================================================
// PUBLIC API
// ================================================================

namespace KamodHealth
{
    // ------------------------------------------------------------
    // System
    // ------------------------------------------------------------

    void begin()
    {
        bootMillis = millis();

        lastUart2Bytes =
            webSerialTotalWrittenUART2();

        lastUart2Check = millis();

        uart2Activity = false;
    }

    uint32_t uptimeSeconds()
    {
        return
            (millis() - bootMillis) /
            1000UL;
    }

    uint32_t freeHeap()
    {
        return ESP.getFreeHeap();
    }

    uint32_t minFreeHeap()
    {
        return ESP.getMinFreeHeap();
    }

    const char* resetReason()
    {
        return resetReasonText(
            esp_reset_reason()
        );
    }

    String ip()
    {
        return ETH.localIP().toString();
    }

    String mac()
    {
        return ETH.macAddress();
    }

    // ------------------------------------------------------------
    // Ethernet
    // ------------------------------------------------------------

    bool ethernetOK()
    {
        return ETH.linkUp();
    }

    String ethernetSpeed()
    {
        if (!ethernetOK())
            return "DOWN";

        String result;

        result += ETH.linkSpeed();
        result += " Mbps ";

        result +=
            ETH.fullDuplex()
                ? "FD"
                : "HD";

        return result;
    }

    // ------------------------------------------------------------
    // MQTT
    // ------------------------------------------------------------

    bool mqttOK()
    {
        return Esp32Mqtt::connected();
    }

    uint32_t mqttConnectAttempts()
    {
        return Esp32Mqtt::connectAttempts();
    }

    uint32_t mqttPublishCalls()
    {
        return Esp32Mqtt::publishCalls();
    }

    uint32_t mqttPublishSuccess()
    {
        return Esp32Mqtt::publishSuccess();
    }

    uint32_t mqttPublishFailed()
    {
        return Esp32Mqtt::publishFailed();
    }

    uint32_t mqttPublishQueued()
    {
        return Esp32Mqtt::publishQueued();
    }

    uint32_t mqttPublishAcknowledged()
    {
        return Esp32Mqtt::publishAcknowledged();
    }

    uint32_t mqttPublishRejected()
    {
        return Esp32Mqtt::publishRejected();
    }

    bool mqttPublishBudgetExhausted()
    {
        return Esp32Mqtt::publishBudgetExhausted();
    }

    bool mqttLastPublishResult()
    {
        return Esp32Mqtt::lastPublishResult();
    }

    // ------------------------------------------------------------
    // P1/P2 / UART2
    // ------------------------------------------------------------

    uint32_t uart2Bytes()
    {
        return webSerialTotalWrittenUART2();
    }

    size_t uart2BufferUsed()
    {
        return webSerialSizeUART2();
    }

    bool uart2Active()
    {
        updateUart2Activity();

        return uart2Activity;
    }

    // ------------------------------------------------------------
    // Local log
    // ------------------------------------------------------------

    uint32_t logBytes()
    {
        return webSerialTotalWrittenLog();
    }

    size_t logUsed()
    {
        return webSerialSizeLog();
    }
}

#endif