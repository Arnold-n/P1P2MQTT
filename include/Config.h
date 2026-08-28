#pragma once

//
// Firmware
//
#define FW_VERSION "1.0.3"
#define FW_AUTHOR  "JackyKNX"

//
// OTA
//
#define OTA_PASSWORD "P1P2MQTT"

// UART0 (ESP8266 -> ATmega) na plycie

// ============================================================
// ATmega328P UART2
// ============================================================

// Physical UART mapping confirmed on the board.
//
// ESP32 TX -> ATmega RX
// ESP32 RX <- ATmega TX

#define ATMEGA_UART_RX_PIN 17
#define ATMEGA_UART_TX_PIN 16

#define ATMEGA_UART_BAUD 250000

//
// UART0 sniffer
//
// ESP32 UART0 RX (GPIO3) listens to ATmega TX.
// TX is not used.
//
#define ENABLE_UART0_SNIFFER 1
#define UART0_RX_PIN 3
#define UART0_TX_PIN 1
#define UART_BAUD 250000


// Original P1P2Monitor serial protocol
#define SERIAL_MAGICSTRING "1P2P"

//
// Ethernet (M5Stack PoESP32 U138)
//
#define PHY_ADDR   1
#define ETH_POWER_PIN  5
#define ETH_MDC_PIN    23
#define ETH_MDIO_PIN   18
