#include "AtmegaProtocol.h"
#include "AtmegaSerial.h"
#include "WebSerial.h"

namespace
{

constexpr size_t RX_BUFFER_SIZE = 1000;

char rxBuffer[RX_BUFFER_SIZE + 1];

size_t rxLength = 0;
bool lineReady = false;

// Arnold compatibility state
uint8_t errorDataShortCount = 0;
uint8_t errorCSCount = 0;
uint8_t errorXORCount = 0;
uint8_t errorCRCCount = 0;

uint8_t ignoreRemainder = 2;

uint32_t atmegaUptimePrev = 0;

}

namespace AtmegaProtocol
{

void begin()
{
    rxLength = 0;
    lineReady = false;

    errorDataShortCount = 0;
    errorCSCount = 0;
    errorXORCount = 0;
    errorCRCCount = 0;

    ignoreRemainder = 2;
    atmegaUptimePrev = 0;

    sendDummyLine();
}

void loop()
{
    while (AtmegaSerial::available())
    {
        int value = AtmegaSerial::read();

        if (value < 0)
            break;

        char c = static_cast<char>(value);

        /*
         * Arnold does not store the terminating LF.
         * CR immediately before LF is ignored.
         */


        if (c == '\n')
        {
            if (rxLength > 0 && rxBuffer[rxLength - 1] == '\r')
            {
                rxLength--;
            }

            rxBuffer[rxLength] = '\0';

            if (rxLength > 0)
            {
                lineReady = true;
            }

            rxLength = 0;

            break;
        }
        /*
         * Arnold RB = 1000.
         *
         * Do not allow the buffer to overflow.
         * We intentionally discard the current line if it exceeds RB.
         */
        if (rxLength < RX_BUFFER_SIZE)
        {
            rxBuffer[rxLength++] = c;
        }
        else
        {
            /*
             * Buffer overrun.
             *
             * Drop the current line and wait for the next LF.
             */
            rxLength = 0;

            logPrintf(
                "[ATMEGA] serial input line too long - discarded"
            );
        }
    }
}


bool available()
{
    return lineReady;
}

const char *line()
{
    if (!lineReady)
        return nullptr;

    lineReady = false;

    return rxBuffer;
}

void sendCommand(const char *command)
{
    AtmegaSerial::sendCommand(command);
}


void sendDummyLine()
{
    AtmegaSerial::write(
        (const uint8_t *)"1P2P* Dummy line 1.\r\n",
        strlen("1P2P* Dummy line 1.\r\n")
    );

    AtmegaSerial::write(
        (const uint8_t *)"1P2P* Dummy line 2.\r\n",
        strlen("1P2P* Dummy line 2.\r\n")
    );
}

uint8_t errorDataShort()
{
    return errorDataShortCount;
}

uint8_t errorCS()
{
    return errorCSCount;
}

uint8_t errorXOR()
{
    return errorXORCount;
}

uint8_t errorCRC()
{
    return errorCRCCount;
}

uint16_t bufferLength()
{
    return static_cast<uint16_t>(rxLength);
}

}