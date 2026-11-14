#include "AtmegaSerial.h"
#include "Config.h"
#include "WebSerial.h"

static HardwareSerial atmegaSerial(2);
static uint32_t rxByteCount = 0;
static uint32_t txByteCount = 0;
static uint32_t lastRxTime = 0;
static uint32_t lastTxTime = 0;

namespace
{

#ifdef HW_KAMOD
constexpr uint8_t UART_ACTIVITY_LED = 2;
constexpr uint32_t UART_ACTIVITY_HOLD_MS = 80;

bool uartLedActive = false;
uint32_t uartLedOffAt = 0;

void uartActivity()
{
    digitalWrite(UART_ACTIVITY_LED, HIGH);
    uartLedOffAt = millis() + UART_ACTIVITY_HOLD_MS;
    uartLedActive = true;
}
#endif

void monitorRX(uint8_t byte)
{
    rxByteCount++;
    lastRxTime = millis();

#ifdef HW_KAMOD
    uartActivity();
#endif

    webSerialWriteUART2(byte);
}

void monitorTX(const uint8_t *data, size_t length)
{
    txByteCount += length;
    lastTxTime = millis();

#ifdef HW_KAMOD
    uartActivity();
#endif

    webSerialWriteUART2((const uint8_t *)"\n[TX] ", 6);
    webSerialWriteUART2(data, length);

    webSerialWriteUART0((const uint8_t *)"[TX] ", 5);
    webSerialWriteUART0(data, length);
    webSerialWriteUART0((const uint8_t *)"\n", 1);
}

} // namespace


namespace AtmegaSerial
{

void begin()
{
#ifdef HW_KAMOD
    pinMode(UART_ACTIVITY_LED, OUTPUT);
    digitalWrite(UART_ACTIVITY_LED, LOW);
#endif

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

uint32_t rxBytes()
{
    return rxByteCount;
}

uint32_t txBytes()
{
    return txByteCount;
}

uint32_t lastRxMillis()
{
    return lastRxTime;
}

uint32_t lastTxMillis()
{
    return lastTxTime;
}

void resetStats()
{
    rxByteCount = 0;
    txByteCount = 0;
    lastRxTime = 0;
    lastTxTime = 0;
}

void loop()
{
#ifdef HW_KAMOD
    if (uartLedActive &&
        (int32_t)(millis() - uartLedOffAt) >= 0)
    {
        digitalWrite(UART_ACTIVITY_LED, LOW);
        uartLedActive = false;
    }
#endif
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

} // namespace AtmegaSerial