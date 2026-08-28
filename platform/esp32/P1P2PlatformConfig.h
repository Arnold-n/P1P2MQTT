#pragma once

/*
 * P1P2MQTT-ESP32
 *
 * Confirmed UART pin assignment
 *
 * UART0:
 *   ESP32 -> ATmega path
 *   RX = GPIO3
 *   TX = GPIO1
 *
 * UART2:
 *   external ATmega connector
 *   RX = GPIO17
 *   TX = GPIO16
 *
 * Current implementation:
 *   UART0 = receive/sniffer only
 *   UART2 = receive + future transmit
 */

#define ENABLE_UART0_SNIFFER 1

#define UART0_RX_PIN 3
#define UART0_TX_PIN 1

#define UART2_RX_PIN 17
#define UART2_TX_PIN 16

#define UART_BAUD 250000