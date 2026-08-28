#pragma once

#include <Arduino.h>

constexpr size_t WEB_SERIAL_BUFFER_SIZE = 4096;

// Inicjalizacja
void webSerialSetup();

// Obsługa
void webSerialLoop();

// UART0
void webSerialWriteUART0(uint8_t b);
void webSerialWriteUART0(const uint8_t *data, size_t len);

// UART2
void webSerialWriteUART2(uint8_t b);
void webSerialWriteUART2(const uint8_t *data, size_t len);

// Kasowanie obu buforów
void webSerialClear();

// Rozmiar buforów
size_t webSerialSizeUART0();
size_t webSerialSizeUART2();

// Liczniki zapisanych bajtów
uint32_t webSerialTotalWrittenUART0();
uint32_t webSerialTotalWrittenUART2();

// Pobierz nowe dane od ostatniego odczytu
String webSerialGetSinceUART0(uint32_t &sinceTotal, bool &overflow);
String webSerialGetSinceUART2(uint32_t &sinceTotal, bool &overflow);

// System log: plain, timestamped text lines (e.g. "[MQTT] Connected"),
// same idea as UART0/UART2 but for formatted debug/status output instead
// of raw bus bytes. Use logPrintf() anywhere you'd otherwise use
// Serial.printf() -- it does both (still prints to Serial, plus keeps
// a copy here for the web log viewer).
void logPrintf(const char *fmt, ...);

void webSerialWriteLog(const char *msg);
size_t webSerialSizeLog();
uint32_t webSerialTotalWrittenLog();
String webSerialGetSinceLog(uint32_t &sinceTotal, bool &overflow);

enum WebSerialFormat
{
    SERIAL_ASCII = 0,
    SERIAL_HEX   = 1,
    SERIAL_BOTH  = 2
};

void webSerialSetFormatUART0(WebSerialFormat f);
void webSerialSetFormatUART2(WebSerialFormat f);

WebSerialFormat webSerialGetFormatUART0();
WebSerialFormat webSerialGetFormatUART2();