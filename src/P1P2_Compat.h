#pragma once

#include <Arduino.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#ifndef str
#define str(s) #s
#endif
#ifndef xstr
#define xstr(s) str(s)
#endif

void compatPrintfTopicS(const char* formatstring, ...);

#ifndef printfTopicS
#define printfTopicS(formatstring, ...) \
  compatPrintfTopicS(formatstring, ##__VA_ARGS__)
#endif

#include "Config.h"
#include "P1P2_Config.h"
#include "P1P2_NetworkParams.h"
#include "P1P2_HomeAssistant.h"
#include "P1P2_System.h"

// -----------------------------------------------------------------------------
// Arnold compatibility context
// -----------------------------------------------------------------------------
// This header intentionally exposes only the data, macros and function
// declarations required by P1P2_ParameterConversion.h. Implementations live
// in P1P2_Compat.cpp.

#define RESERVED_LEN 80
#define TZ_STRING_LEN 50

typedef struct EEPROMSettings {
  char signature[EEPROM_SIGNATURE_LEN];
  char mqttUser[MQTT_USER_LEN];
  char mqttPassword[MQTT_PASSWORD_LEN];
  char mqttServer[MQTT_SERVER_LEN];

  bool mqttEnabled;

  uint16_t EE_size;

  byte useDeviceNameInTopic;
  byte useDeviceNameInEntityName;
  int mqttPort;

  byte useBridgeNameInDeviceIdentity;
  byte useBridgeNameInTopic;
  byte useBridgeNameInEntityName;
  byte useDeviceNameInDeviceIdentity;

  uint32_t outputMode;
  byte outputFilter;
  byte useDeviceNameInDeviceNameHA;

  byte ESPhwID;
  byte EE_version;

  byte noWiFi;
  byte useStaticIP;

  char static_ip[16];
  char static_gw[16];
  char static_nm[16];

  char wifiManager_SSID[WIFIMAN_SSID_LEN];
  char wifiManager_password[WIFIMAN_PASSWORD_LEN];
  char mdnsName[MDNS_NAME_LEN];
  char mqttClientName[MQTT_CLIENTNAME_LEN];
  char mqttPrefix[MQTT_PREFIX_LEN];

  char deviceName[DEVICE_NAME_LEN];
  char bridgeName[BRIDGE_NAME_LEN];
  char haConfigPrefix[HACONFIG_PREFIX_LEN];

  char telnetMagicword[TELNET_MAGICWORD_LEN];
  char otaHostname[OTA_HOSTNAME_LEN];
  char otaPassword[OTA_PASSWORD_LEN];

  char deviceShortNameHA[DEVICE_SHORT_NAME_HA_LEN];
  byte useBridgeNameInDeviceNameHA;

#ifdef W_SERIES
  char meterURL[MQTT_INPUT_TOPIC_LEN];
#endif

#ifdef E_SERIES
  char mqttElectricityPower[MQTT_INPUT_TOPIC_LEN];
  char mqttElectricityTotal[MQTT_INPUT_TOPIC_LEN];
  char mqttBUHpower[MQTT_INPUT_TOPIC_LEN];
  char mqttBUHtotal[MQTT_INPUT_TOPIC_LEN];

  byte useTotal;
  int8_t R1Toffset;
  int8_t R2Toffset;
  int8_t R4Toffset;
  int16_t RToffset;
#endif

  char reservedText[RESERVED_LEN];

#ifdef E_SERIES
  byte useR1T;
#endif

  byte useTZ;
  char userTZ[TZ_STRING_LEN];

#ifdef E_SERIES
  uint32_t electricityConsumedCompressorHeating1;
  uint32_t energyProducedCompressorHeating1;
  byte D13;
  bool haSetup;
#endif

  bool minuteTimeStamp;
  byte voltage;
  byte nrPhases;
  byte powerBUH1;
  byte powerBUH2;

#ifdef F_SERIES
  uint8_t setpointCoolingMin;
  uint8_t setpointCoolingMax;
  uint8_t setpointHeatingMin;
  uint8_t setpointHeatingMax;
  bool useAirIntake;
#endif
} EEPROMSettings;

// State required by Arnold's conversion layer.
extern EEPROMSettings EE;
extern bool EE_dirty;
extern uint32_t espUptime;

extern byte pseudo0B;
extern byte pseudo0C;
extern byte pseudo0D;
extern byte pseudo0E;
extern byte pseudo0F;
extern byte throttle;
extern byte throttleValue;

extern char mqtt_value[MQTT_VALUE_LEN];
extern char mqttTopic[MQTT_TOPIC_LEN];

extern byte mqttTopicChar;
extern byte mqttSlashChar;
extern byte mqttTopicCatChar;
extern byte mqttTopicSrcChar;
extern byte mqttTopicPrefixLength;

extern byte readHex[HB];

extern char haConfigTopic[HA_KEY_LEN];
extern uint16_t haConfigTopicLength;
extern uint16_t haConfigTopicLengthMax;

extern char haConfigMessage[HA_VALUE_LEN];
extern uint16_t haConfigMessageLength;
extern uint16_t haConfigMessageLengthMax;

// -----------------------------------------------------------------------------
// Arnold topic/configuration macros
// -----------------------------------------------------------------------------

#define HACONFIGTOPIC(formatstring, ...) { \
  haConfigTopicLength = snprintf_P(haConfigTopic, HA_KEY_LEN, PSTR(formatstring) __VA_OPT__(,) __VA_ARGS__); \
  if (haConfigTopicLength > haConfigTopicLengthMax) haConfigTopicLengthMax = haConfigTopicLength; \
  if (haConfigTopicLength >= HA_KEY_LEN) { \
    printfTopicS("haConfigTopic too long %i >= %i (%.20s)", haConfigTopicLength, HA_KEY_LEN, haConfigTopic); \
    return 0; \
  } \
}

#define HACONFIGMESSAGE_ADD(formatstring, ...) { \
  haConfigMessageLength += snprintf_P( \
      haConfigMessage + haConfigMessageLength, \
      HA_VALUE_LEN - haConfigMessageLength, \
      PSTR(formatstring) __VA_OPT__(,) __VA_ARGS__); \
  if (haConfigMessageLength > haConfigMessageLengthMax) \
    haConfigMessageLengthMax = haConfigMessageLength; \
  if (haConfigMessageLength >= HA_VALUE_LEN) { \
    char* nameP = strstr_P(haConfigMessage, PSTR("name\":\"")); \
    nameP = nameP ? nameP + 7 : haConfigMessage; \
    printfTopicS("haConfigMsg too long %i >= %i (%.25s)", \
                 haConfigMessageLength, HA_VALUE_LEN, nameP); \
    return 0; \
  } \
}

#define topicCharSpecific(x) \
  do { \
    mqttTopic[mqttTopicChar - 1] = '/'; \
    mqttTopic[mqttTopicChar] = x; \
    mqttTopic[mqttTopicChar + 1] = '/'; \
    mqttTopic[mqttSlashChar] = '\0'; \
  } while (0)

#define topicCharSpecificSlash(x) \
  do { \
    mqttTopic[mqttTopicChar - 1] = '/'; \
    mqttTopic[mqttTopicChar] = x; \
    mqttTopic[mqttTopicChar + 1] = '/'; \
    mqttTopic[mqttSlashChar] = '/'; \
    mqttTopic[mqttSlashChar + 2] = '/'; \
  } while (0)

#define topicWrite topicCharSpecific('W')

#ifndef HA_SW
#define HA_SW FW_VERSION
#endif

#ifndef MAXRH
#define MAXRH 23
#endif

#ifndef P1P2_COMPAT_TRACE
#define P1P2_COMPAT_TRACE 0
#endif

#ifndef P1P2_DISABLE_HA_DISCOVERY
#define P1P2_DISABLE_HA_DISCOVERY 0
#endif

// -----------------------------------------------------------------------------
// Compatibility API
// -----------------------------------------------------------------------------

bool clientPublishMqtt(const char* key, uint8_t qos, bool retain,
                       const char* value = nullptr);

bool clientPublishMqttChar(char key, uint8_t qos, bool retain,
                           const char* value = nullptr);

bool P1P2Compat_publishStateSnapshot();

void clientPublishTelnet(bool includeTopic, const char* value,
                         bool addDate = true);

void clientPublishTelnetChar(char key, const char* value);

void writePseudoPacket(byte* WB, byte rh);

bool publishHomeAssistantConfig(
    const char* deviceSubName,
    const hadevice haDevice,
    const haentity haEntity,
    const haentitycategory haEntityCategory,
    byte haPrecision,
    const habuttondeviceclass haButtonDeviceClass,
    bool useSrc,
    bool useCommonName = false,
    const char* commonNameString = nullptr);

bool deleteHomeAssistantConfig(
    const char* deviceSubName,
    const hadevice haDevice,
    const haentity haEntity,
    const haentitycategory haEntityCategory,
    byte haPrecision,
    const habuttondeviceclass haButtonDeviceClass,
    bool useSrc,
    bool useCommonName = false,
    const char* commonNameString = nullptr);

void P1P2Compat_init();
void P1P2Compat_tick();
void P1P2Compat_resetData();

EEPROMSettings& P1P2Compat_settings();
void P1P2Compat_saveSettings();

// -----------------------------------------------------------------------------
// Arnold energy state
// -----------------------------------------------------------------------------

extern uint16_t ePower;
extern uint16_t bPower;
extern uint32_t eTotal;
extern uint32_t bTotal;
extern uint32_t ePowerTime;
extern uint32_t eTotalTime;
extern uint32_t bPowerTime;
extern uint32_t bTotalTime;
extern byte ePowerAvailable;
extern bool eTotalAvailable;
extern byte bPowerAvailable;
extern bool bTotalAvailable;

#ifndef outputUnknown
#define outputUnknown (EE.outputMode & 0x0008)
#endif
uint32_t compatGetOutputMode();
uint8_t compatGetOutputFilter();
bool compatGetHaSetup();

const char* P1P2Compat_mqttServer();
uint16_t P1P2Compat_mqttPort();
const char* P1P2Compat_mqttUser();
const char* P1P2Compat_mqttPassword();
const char* P1P2Compat_mqttClientName();

bool P1P2Compat_mqttEnabled();
void P1P2Compat_setMqttServer(const char* value);
void P1P2Compat_setMqttPort(uint16_t value);
void P1P2Compat_setMqttUser(const char* value);
void P1P2Compat_setMqttPassword(const char* value);
void P1P2Compat_setMqttClientName(const char* value);
void P1P2Compat_setMqttEnabled(bool value);
