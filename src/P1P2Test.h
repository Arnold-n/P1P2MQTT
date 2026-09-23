#pragma once

#include <Arduino.h>

namespace P1P2Test
{
    void begin();

    // Runs the transport-level P1P2 test:
    // sends the existing Arnold dummy line and verifies that the
    // expected 42 TX bytes were accepted by the UART transport.
    bool run();

    bool running();
    bool passed();

    uint32_t startedMillis();
    uint32_t elapsedMillis();
    uint32_t txDelta();
    uint32_t rxDelta();

    const char* resultText();
}
