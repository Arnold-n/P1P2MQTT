#pragma once

#include "P1P2Parser.h"

// Public interface between our P1P2 processor and the
// Arnold compatibility layer.
//
// Arnold-specific headers and globals must NOT leak into
// P1P2Processor.cpp.

void P1P2Compat_init();

void P1P2Compat_process(const P1P2Parser::Packet& packet);

void P1P2Compat_onMqttConnected();

void P1P2Compat_onHomeAssistantOnline();

void P1P2Compat_onMqttDisconnected();

const char* P1P2Compat_mqttAvailabilityTopic();

const char* P1P2Compat_mqttIpTopic();

void P1P2Compat_publishIp();

const char* P1P2Compat_mqttWriteTopic();
const char* P1P2Compat_mqttWriteBridgeTopic();

void P1P2Compat_mqttWriteBroadcastTopic(
    char* topic,
    size_t topicSize
);

// Restarts the ESP32 the same way Arnold's "D0" command does: publish
// "offline" on the availability topic first (clean disconnect instead of
// waiting for the MQTT LWT timeout), then ESP.restart(). Used both by
// the MQTT write-topic command router and the web UI restart button.
void P1P2Compat_restartEsp();

const char* P1P2Compat_mqttWriteTopic();