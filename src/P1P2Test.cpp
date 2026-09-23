#include "P1P2Test.h"

#include "AtmegaProtocol.h"
#include "AtmegaSerial.h"

namespace
{
    constexpr uint32_t EXPECTED_TX_BYTES = 42;

    bool testRunning = false;
    bool testPassed = false;

    uint32_t testStart = 0;
    uint32_t testElapsed = 0;
    uint32_t testTxBefore = 0;
    uint32_t testRxBefore = 0;
    uint32_t testTxDelta = 0;
    uint32_t testRxDelta = 0;
}

namespace P1P2Test
{
    void begin()
    {
        testRunning = false;
        testPassed = false;
        testStart = 0;
        testElapsed = 0;
        testTxBefore = 0;
        testRxBefore = 0;
        testTxDelta = 0;
        testRxDelta = 0;
    }

    bool run()
    {
        testRunning = true;
        testPassed = false;

        testStart = millis();

        testTxBefore = AtmegaSerial::txBytes();
        testRxBefore = AtmegaSerial::rxBytes();

        // Existing protocol-level dummy traffic: two 21-byte lines.
        AtmegaProtocol::sendDummyLine();

        testTxDelta = AtmegaSerial::txBytes() - testTxBefore;
        testRxDelta = AtmegaSerial::rxBytes() - testRxBefore;

        testElapsed = millis() - testStart;

        // This is deliberately a transport test. The Arnold dummy line
        // does not define a response, so RX is informational, not PASS/FAIL.
        testPassed = (testTxDelta >= EXPECTED_TX_BYTES);

        testRunning = false;

        return testPassed;
    }

    bool running()
    {
        return testRunning;
    }

    bool passed()
    {
        return testPassed;
    }

    uint32_t startedMillis()
    {
        return testStart;
    }

    uint32_t elapsedMillis()
    {
        return testElapsed;
    }

    uint32_t txDelta()
    {
        return testTxDelta;
    }

    uint32_t rxDelta()
    {
        return testRxDelta;
    }

    const char* resultText()
    {
        if (testPassed)
            return "PASS";

        if (testStart == 0)
            return "NOT RUN";

        return "FAIL";
    }
}
