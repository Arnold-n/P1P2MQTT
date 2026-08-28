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
}
