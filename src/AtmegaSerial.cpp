#include "AtmegaSerial.h"
#include "Config.h"
#include "WebSerial.h"

static HardwareSerial atmegaSerial(2);

namespace
{

void monitorRX(uint8_t byte)
{
    webSerialWriteUART2(byte);
}

void monitorTX(const uint8_t *data, size_t length)
{
    webSerialWriteUART2((const uint8_t *)"\n[TX] ", 6);
    webSerialWriteUART2(data, length);

    // Repurposed: this buffer/endpoint used to carry raw UART0 bytes,
    // but UART0 RX is now physically disconnected from the ATmega (see
    // Config.h / README). Reusing its existing ring buffer + web panel
    // slot to show a dedicated, TX-only view of UART2 instead -- easier
    // to spot outgoing commands than scrolling through the combined
    // RX+TX stream in the UART2 panel below it.
    webSerialWriteUART0((const uint8_t *)"[TX] ", 5);
    webSerialWriteUART0(data, length);
    webSerialWriteUART0((const uint8_t *)"\n", 1);
}

}

namespace AtmegaSerial
{

void begin()
{
    atmegaSerial.begin(
        ATMEGA_UART_BAUD,
        SERIAL_8N1,
        ATMEGA_UART_RX_PIN,
        ATMEGA_UART_TX_PIN
    );

    atmegaSerial.setTimeout(0);

    Serial.println();
    Serial.println("=== ATmega UART START ===");

    Serial.print("RX : GPIO");
    Serial.println(ATMEGA_UART_RX_PIN);

    Serial.print("TX : GPIO");
    Serial.println(ATMEGA_UART_TX_PIN);

    Serial.print("Baud : ");
    Serial.println(ATMEGA_UART_BAUD);
}

void loop()
{
    // UART transport is consumed by available()/read().
}

bool available()
{
    return atmegaSerial.available() > 0;
}

int read()
{
    int value = atmegaSerial.read();

    if (value >= 0)
    {
        monitorRX((uint8_t)value);
    }

    return value;
}

void writeByte(uint8_t byte)
{
    monitorTX(&byte, 1);
    atmegaSerial.write(byte);
}

void write(const uint8_t *data, size_t length)
{
    if (!data || length == 0)
        return;

    monitorTX(data, length);
    atmegaSerial.write(data, length);
}

void sendCommand(const char *command)
{
    if (!command)
        return;

    char buffer[512];

    int n = snprintf(
        buffer,
        sizeof(buffer),
        "%s%s\r\n",
        SERIAL_MAGICSTRING,
        command
    );

    if (n <= 0)
        return;

    if ((size_t)n >= sizeof(buffer))
    {
        Serial.println("ERROR: ATmega command too long");
        return;
    }

    write((const uint8_t *)buffer, (size_t)n);
}

}
