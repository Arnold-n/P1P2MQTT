#include <Arduino.h>
#include <ArduinoOTA.h>

#include "Config.h"
#include "P1P2_NetworkParams.h"

static bool otaStarted = false;

void otaSetup()
{
    if (otaStarted)
        return;

    ArduinoOTA.setHostname(OTA_HOSTNAME);
    ArduinoOTA.setPassword(OTA_PASSWORD);

    ArduinoOTA.onStart([]()
    {
        Serial.println();
        Serial.println("================================");
        Serial.println("OTA Update Started");
        Serial.println("================================");
    });

    ArduinoOTA.onEnd([]()
    {
        Serial.println();
        Serial.println("================================");
        Serial.println("OTA Update Finished");
        Serial.println("================================");
    });

    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
    {
        static uint8_t lastPercent = 255;

        uint8_t percent = (progress * 100) / total;

        if (percent != lastPercent)
        {
            lastPercent = percent;
            Serial.printf("OTA: %u%%\n", percent);
        }
    });

    ArduinoOTA.onError([](ota_error_t error)
    {
        Serial.printf("OTA ERROR %u : ", error);

        switch (error)
        {
            case OTA_AUTH_ERROR:
                Serial.println("Authentication Failed");
                break;

            case OTA_BEGIN_ERROR:
                Serial.println("Begin Failed");
                break;

            case OTA_CONNECT_ERROR:
                Serial.println("Connect Failed");
                break;

            case OTA_RECEIVE_ERROR:
                Serial.println("Receive Failed");
                break;

            case OTA_END_ERROR:
                Serial.println("End Failed");
                break;

            default:
                Serial.println("Unknown");
                break;
        }
    });

    ArduinoOTA.begin();

    otaStarted = true;

    Serial.println();
    Serial.println("================================");
    Serial.println("Arduino OTA Ready");
    Serial.print("Hostname : ");
    Serial.println(OTA_HOSTNAME);
    Serial.println("================================");
}

void otaLoop()
{
    if (otaStarted)
        ArduinoOTA.handle();
}