#pragma once

#include <Arduino.h>

namespace P1P2Parser
{

constexpr size_t MAX_PACKET_SIZE = 24;

enum class LineType : uint8_t
{
    EMPTY,
    INFO,
    PACKET,
    TIMING,
    DUPLICATE,
    ERROR,
    INVALID
};

struct Packet
{
    uint8_t data[MAX_PACKET_SIZE];
    uint8_t length;

    uint8_t crc;
    uint8_t crcExpected;

    bool crcValid;

    // Original ATmega R-line.
    // Kept so P1P2/R can reproduce Arnold's raw MQTT payload.
    char rawLine[128];
};

LineType parseLine(const char *line, Packet &packet);

bool parseRLine(const char *line, Packet &packet);

uint8_t crcEseries(const uint8_t *data, uint8_t length);

}
