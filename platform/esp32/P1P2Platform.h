#pragma once

#include <Arduino.h>

namespace P1P2Platform
{
    // GPIO
    void initGPIO();

    void setLedPower(bool state);
    void setLedRead(bool state);
    void setLedWrite(bool state);
    void setLedError(bool state);

    // P1/P2 bus input
    bool readBusInput();

    // P1/P2 hardware timer
    void initP1P2Timer();

    uint32_t getTimerCount();

    void setCompareR(uint32_t value);
    uint32_t getCompareR();

    void setCompareW(uint32_t value);
    uint32_t getCompareW();

    // Input capture
    void enableInputCapture();
    void disableInputCapture();

    void configureCaptureFalling();
    void configureCaptureRising();

    uint32_t getInputCapture();

    // TX compare
    void enableCompareR();
    void disableCompareR();

    void enableCompareW();
    void disableCompareW();

    // Millisecond timer
    void initMsTimer();
    void enableMsTimer();
    void disableMsTimer();

    // Second timer
    void initSecondTimer();
    void enableSecondTimer();
    void disableSecondTimer();

    // Critical sections
    uint32_t enterCritical();
    void exitCritical(uint32_t state);
}	
