#pragma once

#include <Arduino.h>
#include "P1P2Parser.h"

namespace P1P2Processor
{

void process(const P1P2Parser::Packet &packet);

}