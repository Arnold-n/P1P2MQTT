#include <Arduino.h>
#include <ETH.h>
#include <WiFi.h>
#include "AtmegaSerial.h"
#include "AtmegaProtocol.h"

#include "Config.h"
#include "P1P2_Config.h"
#include "P1P2_NetworkParams.h"

#include "OTAManager.h"
#include "WebManager.h"
#include "WebSerial.h"

#include "P1P2Parser.h"

#include "P1P2Processor.h"

#include "Mqtt.h"


bool eth_connected = false;

static bool servicesStarted = false;

void startServices()
{
    if (servicesStarted)
        return;

    otaSetup();
    webSetup();

    servicesStarted = true;
}


void WiFiEvent(WiFiEvent_t event)
{
    switch (event)
    {
        case ARDUINO_EVENT_ETH_START:

            logPrintf("=== Ethernet START ===");

            ETH.setHostname(OTA_HOSTNAME);

            break;

        case ARDUINO_EVENT_ETH_CONNECTED:

            logPrintf("=== Ethernet LINK UP ===");

            break;

        case ARDUINO_EVENT_ETH_GOT_IP:

            eth_connected = true;

            logPrintf("=== Ethernet GOT IP ===");

            logPrintf("IP      : %s", ETH.localIP().toString().c_str());
            logPrintf("Mask    : %s", ETH.subnetMask().toString().c_str());
            logPrintf("Gateway : %s", ETH.gatewayIP().toString().c_str());
            logPrintf("MAC     : %s", ETH.macAddress().c_str());

            startServices();

            break;

        case ARDUINO_EVENT_ETH_DISCONNECTED:

            eth_connected = false;

            logPrintf("=== Ethernet LINK DOWN ===");

            break;

        case ARDUINO_EVENT_ETH_STOP:

            eth_connected = false;

            logPrintf("=== Ethernet STOP ===");

            break;

        default:
            break;
    }
}


namespace
{

constexpr size_t UART0_LINE_BUFFER_SIZE = 1024;

char uart0LineBuffer[UART0_LINE_BUFFER_SIZE];
size_t uart0LineLength = 0;

void processUart0Line()
{
    if (uart0LineLength == 0)
        return;

    uart0LineBuffer[uart0LineLength] = '\0';

    P1P2Parser::Packet packet;

    P1P2Parser::LineType type =
        P1P2Parser::parseLine(
            uart0LineBuffer,
            packet
        );

    if (type == P1P2Parser::LineType::PACKET)
    {
        P1P2Processor::process(packet);
    }

    uart0LineLength = 0;
}
}

void processUart0()
{
    // NOTE: physical UART0 RX is currently disconnected from the ATmega
    // (see Config.h / README) -- Serial.available() should stay 0 here in
    // practice. Its web panel slot/ring buffer (webSerialWriteUART0) has
    // been repurposed by AtmegaSerial.cpp's monitorTX() to show UART2 TX
    // traffic instead. If UART0 RX is ever physically reconnected, the
    // two sources will interleave in that same panel -- split them back
    // into separate buffers at that point.

    uint8_t linesProcessed = 0;

    while (Serial.available())
    {
        uint8_t b = Serial.read();

        webSerialWriteUART0(b);

        if (b == '\n')
        {
            if (uart0LineLength > 0 &&
                uart0LineBuffer[uart0LineLength - 1] == '\r')
            {
                uart0LineLength--;
            }

            processUart0Line();

            linesProcessed++;

            if (linesProcessed >= 2)
                break;

            continue;
        }

        if (uart0LineLength < UART0_LINE_BUFFER_SIZE - 1)
        {
            uart0LineBuffer[uart0LineLength++] =
                static_cast<char>(b);
        }
        else
        {
            uart0LineLength = 0;
        }
    }
}


void serviceNetwork()
{
    otaLoop();
    webLoop();
    webSerialLoop();
    yield();
}


void setup()
{
    Serial.begin(115200);

    delay(1000);

    webSerialSetup();

    logPrintf("======================================");
    logPrintf("        P1P2MQTT ESP32");
    logPrintf("Version : %s (%s)", FW_VERSION, FW_AUTHOR);
    logPrintf("======================================");

    WiFi.onEvent(WiFiEvent);

    logPrintf("Starting Ethernet...");

    ETH.begin(
        PHY_ADDR,
        ETH_POWER_PIN,
        ETH_MDC_PIN,
        ETH_MDIO_PIN,
        ETH_PHY_IP101,
        ETH_CLOCK_GPIO0_IN
    );

    logPrintf("Waiting for DHCP...");

    uint32_t timeout = millis();

    while (!ETH.localIP())
    {
        delay(100);

        if (millis() - timeout > 10000)
        {
            logPrintf("WARNING: DHCP not ready yet, continuing in background.");
            break;
        }
    }

    // OTA i WWW zostaną uruchomione automatycznie
    // po otrzymaniu adresu IP przez Ethernet.


#if ENABLE_UART0_SNIFFER

    logPrintf("Switching UART0 to sniffer...");
    Serial.flush();

    delay(100);

    Serial.end();

    Serial.setRxBufferSize(4096);

    Serial.begin(
        UART_BAUD,
        SERIAL_8N1,
        UART0_RX_PIN,
        UART0_TX_PIN
    );

    delay(50);

#endif

    // Start communication with ATmega328P
    Esp32Mqtt::begin();
    AtmegaSerial::begin();
    AtmegaProtocol::begin();

}



void loop()
{
//
// NETWORK / OTA FIRST
//

serviceNetwork();

//
// MQTT
//

Esp32Mqtt::loop();

serviceNetwork();

//
// ATmega / UART2
//

AtmegaProtocol::loop();

serviceNetwork();

// UART2 is now the PRIMARY decode source: as of this wiring revision,
// ATmega TX is physically connected only to UART2 RX (GPIO17) -- UART0
// RX (GPIO3) has been physically disconnected. UART2 TX still carries
// write commands to ATmega RX (MQTT "W" -> AtmegaProtocol::sendCommand()).
// Its traffic is also monitored on Web Serial via monitorRX()/monitorTX()
// inside AtmegaSerial.cpp, independent of the block below.
//
// NOTE: this used to be commented out to avoid double-processing when
// UART0 and UART2 both received the same ATmega TX signal (a temporary
// fan-out during testing). That fan-out no longer exists physically, so
// this is safe to enable -- if UART0 ever gets reconnected to ATmega TX
// again, this block must be disabled again (or UART0's processUart0()
// call below removed) to avoid publishing every packet twice.

if (AtmegaProtocol::available())
{
    const char* line = AtmegaProtocol::line();

    if (line)
    {
        P1P2Parser::Packet packet;

        P1P2Parser::LineType type =
            P1P2Parser::parseLine(
                line,
                packet
            );

        if (type == P1P2Parser::LineType::PACKET)
        {
            P1P2Processor::process(packet);
        }
    }
}

serviceNetwork();

//
// UART0 - currently disconnected from ATmega TX (see note above).
// Kept enabled as a passive listener/display only; harmless if idle.
//

#if ENABLE_UART0_SNIFFER

    processUart0();

#endif

serviceNetwork();
 
}