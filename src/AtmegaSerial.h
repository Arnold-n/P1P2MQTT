#pragma once

#include <Arduino.h>

namespace AtmegaSerial
{
    void begin();

    void loop();

    bool available();

    int read();

    void writeByte(uint8_t byte);

    void write(const uint8_t *data, size_t length);

    void sendCommand(const char *command);

    uint32_t rxBytes();
    uint32_t txBytes();
    uint32_t lastRxMillis();
    uint32_t lastTxMillis();
    void resetStats();

}
