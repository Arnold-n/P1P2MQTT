#pragma once

#ifdef HW_KAMOD

#include <Arduino.h>
#include <Adafruit_ILI9341.h>

namespace KamodMqttConfig
{
    void begin(Adafruit_ILI9341& display);

    void draw();

    bool handleTouch(
        int16_t x,
        int16_t y
    );

    bool editing();

    void cancelEdit();
}

#endif
