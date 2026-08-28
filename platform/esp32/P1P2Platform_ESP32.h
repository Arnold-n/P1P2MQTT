#pragma once

#include <Arduino.h>

namespace P1P2Platform
{
    // P1/P2 physical interface
    constexpr int INPUT_CAPTURE_PIN  = 16;
    constexpr int OUTPUT_COMPARE_PIN = 17;

    // LEDs - tymczasowo, później dopasujemy do płytki
    constexpr int LED_POWER = 25;
    constexpr int LED_READ  = 26;
    constexpr int LED_WRITE = 27;
    constexpr int LED_ERROR = 2;

    // P1/P2 timing clock
    constexpr uint32_t TIMER_FREQUENCY = 1000000; // 1 MHz = 1 tick/us
}