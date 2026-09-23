#pragma once

#include <Arduino.h>

namespace AtmegaProtocol
{

void begin();
void loop();

bool available();
const char *line();

void sendCommand(const char *command);
void sendDummyLine();

// Arnold serial input statistics
uint8_t errorDataShort();
uint8_t errorCS();
uint8_t errorXOR();
uint8_t errorCRC();

uint16_t bufferLength();

}