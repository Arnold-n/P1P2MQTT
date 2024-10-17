/* P1P2MQTT-bridge: Host application for ESP8266 interfacing to P1P2Monitor on Arduino Uno,
 *                  supports mqtt/wifi/ethernet/wifiManager/OTA
 *
 * Copyright (c) 2019-2024 Arnold Niessen, arnold.niessen-at-gmail-dot-com - licensed under CC BY-NC-ND 4.0 with exceptions (see LICENSE.md)
 *
 * Requires board support package:
 * http://arduino.esp8266.com/stable/package_esp8266com_index.json
 *
 * Requires libraries:
 * WiFiManager 0.16.0 by tzapu (installed using Arduino IDE)
 * AsyncMqttClient 0.9.0 by Marvin Roger obtained with git clone or as ZIP from https://github.com/marvinroger/async-mqtt-client
 * ESPAsyncTCP 2.0.1 by Phil Bowles obtained with git clone or as ZIP from https://github.com/philbowles/ESPAsyncTCP/
 * ESP_Telnet 2.0.0 by  Lennart Hennigs (installed using Arduino IDE)
 * and for W_SERIES also
 * ArduinoJson 6.11.3 by Benoit Blanchon
 *
 * Version history
 * 20240519 v0.9.51 onMqtt improved (D12 fixes + lower mem)
 * 20240519 v0.9.49 fix haConfigMsg max length
 * 20240515 v0.9.46 HA/MQTT discovery climate controls, remove BINDATA, improve TZ, remove json output format, add W_SERIES for HomeWizard electricity meter bridge
 * 20230806 v0.9.41 restart after MQTT reconnect, Eseries water pressure, Fseries name fix, web server for ESP update
 * 20230611 v0.9.38 H-link data support
 * 20230604 v0.9.37 support for P1P2MQTT bridge v1.2; separate hwID for ESP and ATmega
 * 20230526 v0.9.36 threshold
 * 20230423 v0.9.35 (skipped)
 * 20230322 v0.9.34 AP timeout
 * 20230108 v0.9.31 sensor prefix, +2 valves in HA, fix bit history for 0x30/0x31, +pseudo controlLevel
 * 20221228 v0.9.30 switch from modified ESP_telnet library to ESP_telnet v2.0.0
 * 20221211 v0.9.29 misc fixes, defrost E-series
 * 20221116 v0.9.28 reset-line behaviour, IPv4 EEPROM init
 * 20221112 v0.9.27 static IP support, fix to get Power_* also in HA
 * 20221109 v0.9.26 clarify WiFiManager screen, fix to accept 80-char user/password also in WiFiManager
 * 20221108 v0.9.25 move EEPROM outside ETHERNET code
 * 20221102 v0.9.24 noWiFi option, w5500 reset added, fix switch to verbose=9, misc
 * 20221029 v0.9.23 ISPAVR over BB SPI, ADC, misc, W5500 ethernet
 * 20220918 v0.9.22 degree symbol, hwID, 32-bit EE.outputMode
 * 20220907 v0.9.21 EE.outputMode/outputFilter status survive ESP reboot (EEPROM), added MQTT_INPUT_BINDATA/MQTT_INPUT_HEXDATA (see P1P2_Config.h), reduced uptime update MQTT traffic
 * 20220904 v0.9.20 added E_SERIES/F_SERIES defines, and F-series VRV reporting in MQTT for 2 models
 * 20220903 v0.9.19 longer MQTT user/password, ESP reboot reason (define REBOOT_REASON) added in reporting
 * 20220829 v0.9.18 state_class added in MQTT discovery enabling visibility in HA energy overview
 * 20220817 v0.9.17 handling telnet welcomestring, error/scopemode time info via P1P2/R/#, v9=ignoreserial
 * 20220808 v0.9.15 extended verbosity command, unique OTA hostname, minor fixes
 * 20220802 v0.9.14 AVRISP, wifimanager, mqtt settings, EEPROM, telnet, EE.outputMode, outputFilter, ...
 * 20190908 v0.9.8 Minor improvements: error handling, hysteresis, improved output (a.o. x0Fx36)
 * 20190831 v0.9.7 Error handling improved
 * 20190829 v0.9.7 Minor improvements
 * 20190824 v0.9.6 Added x0Fx35 parameter saving and reporting
 * 20190823        Separated NetworkParams.h
 * 20190817 v0.9.4 Initial version
 *
 * This program is designed to run on an ESP8266 and bridge between telnet and MQTT (wifi) and P1P2Monitor's serial in/output
 * Additionally, it can reset and program the ATmega328P if corresponding connections are available
 */

#include "ESPTelnet.h"
#include "P1P2_Config.h"
#include "P1P2_NetworkParams.h"
#include "P1P2_HomeAssistant.h"
#include "P1P2_System.h"
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>

#ifdef W_SERIES
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
uint16_t electricityMeterActivePower =  0;   // in W, max 65kW
uint32_t electricityMeterTotal =  0; // in Wh, max  4GWh (= 20y * 200 000 kWh)
byte electricityMeterDataValid =  0;
StaticJsonDocument<2048> electricityMeterJsonDocument;
#endif /* W_SERIES */

#include <EEPROM.h>
#include <time.h>

#ifdef WEBSERVER
#include "P1P2_ESP8266HTTPUpdateServer/P1P2_ESP8266HTTPUpdateServer.h"
ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;
#endif /* WEBSERVER */

#include <TZ.h>
#define NR_PREDEFINED_TZ 4
//const char* PREDEFINED_TZ[] = {TZ_none, TZ_user, TZ_Europe_Amsterdam, TZ_Europe_London};
const char* PREDEFINED_TZ[ NR_PREDEFINED_TZ ] = { "", "", "CET-1CEST,M3.5.0/02,M10.5.0/03"   , "GMT0BST,M3.5.0/1,M10.5.0" }; // TODO add other time zones
time_t now;
tm tm;
#define TZ_PREFIX_LEN 29 // "* [ESP] date_time " or "* [ESP] ESPuptime%10d " are both 29 bytes incl \0
char sprint_value[ SPRINT_VALUE_LEN + 29 ] = "* [ESP]                     ";

#ifdef AVRISP
#include <SPI.h>
#include <ESP8266AVRISP.h>
#endif /* AVRISP */

#ifdef ARDUINO_OTA
#include <ArduinoOTA.h>
#endif /* ARDUINO_OTA */
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <AsyncMqttClient.h> // should be included after include P1P2_Config which defines MQTT_MIN_FREE_MEMORY

#ifdef DEBUG_OVER_SERIAL
#define Serial_print(...) Serial.print(__VA_ARGS__)
#define Serial_println(...) Serial.println(__VA_ARGS__)
#else /* DEBUG_OVER_SERIAL */
#define Serial_print(...) {};
#define Serial_println(...) {};
#endif /* DEBUG_OVER_SERIAL */

char mqtt_value[ MQTT_VALUE_LEN ] = "\0";

typedef struct EEPROMSettings {
  char signature[ EEPROM_SIGNATURE_LEN ];
  char mqttUser[ MQTT_USER_LEN ];         // will be overwritten in case of shouldSaveConfig
  char mqttPassword[ MQTT_PASSWORD_LEN ]; // will be overwritten in case of shouldSaveConfig
  char mqttServer[ MQTT_SERVER_LEN ];     // will be overwritten in case of shouldSaveConfig // shortened 19+1 -> 15+1
  // 4 bytes available >0.9.43 by shortening mqttServer to 15+1
  uint16_t EE_size;
  byte useDeviceNameInTopic;
  byte useDeviceNameInEntityName;
  // mqttPort is 32bit and word-aligned
  int  mqttPort;
  // 4 bytes available >0.9.43 by removing rebootReason
  // byte rebootReason; // <= v0.9.43 took 4 bytes due to alignment
  byte useBridgeNameInDeviceIdentity;
  byte useBridgeNameInTopic;
  byte useBridgeNameInEntityName;
  byte useDeviceNameInDeviceIdentity;
  // word-alignment
  uint32_t outputMode;
  // word-alignment
  byte outputFilter;
  byte useDeviceNameInDeviceNameHA; // was <=0.9.43: byte mqttInputByte4;
  // EEPROM values added for EEPROM_SIGNATURE_NEW
  byte ESPhwID;
  byte EE_version;
  // word-alignment
  byte noWiFi;
  byte useStaticIP;
  char static_ip[16];
  char static_gw[16];
  char static_nm[16];
  // no longer used: byte useSensorPrefixH;
  // ======================================
  // EEPROM values added after v0.9.43:
  char wifiManager_SSID[WIFIMAN_SSID_LEN];
  char wifiManager_password[WIFIMAN_PASSWORD_LEN];
  char mdnsName[MDNS_NAME_LEN];
  char mqttClientName[ MQTT_CLIENTNAME_LEN ]; // must be unique
  char mqttPrefix[ MQTT_PREFIX_LEN ];
  char deviceName[ DEVICE_NAME_LEN ];
  char bridgeName[ BRIDGE_NAME_LEN ];
  char haConfigPrefix[ HACONFIG_PREFIX_LEN ];
  char telnetMagicword[TELNET_MAGICWORD_LEN];
  char otaHostname[ OTA_HOSTNAME_LEN ];
  char otaPassword[ OTA_PASSWORD_LEN ];
  char deviceShortNameHA[ DEVICE_SHORT_NAME_HA_LEN ]; // used if none of deviceName/bridgeName are used in device name for HA
  byte useBridgeNameInDeviceNameHA;
#ifdef W_SERIES
  char meterURL[ MQTT_INPUT_TOPIC_LEN ];
#endif
#ifdef E_SERIES
  char mqttElectricityPower[ MQTT_INPUT_TOPIC_LEN ];
  char mqttElectricityTotal[ MQTT_INPUT_TOPIC_LEN ];
  char mqttBUHpower[ MQTT_INPUT_TOPIC_LEN ];
  char mqttBUHtotal[ MQTT_INPUT_TOPIC_LEN ];
  byte useTotal;
  int8_t R1Toffset;
  int8_t R2Toffset;
  int8_t R4Toffset;
  int16_t RToffset;
#endif /* E_SERIES */
#define RESERVED_LEN 80
  char reservedText[ RESERVED_LEN ];
#ifdef E_SERIES
  byte useR1T; // use R1T instead of R2T
#endif /* E_SERIES */
  byte useTZ;
#define TZ_STRING_LEN 50
  char userTZ[ TZ_STRING_LEN ];
#ifdef E_SERIES
  uint32_t electricityConsumedCompressorHeating1;
  uint32_t energyProducedCompressorHeating1;
  byte D13;
  bool haSetup;
#endif /* E_SERIES */
  bool minuteTimeStamp;
#ifdef F_SERIES
  uint8_t setpointCoolingMin;
  uint8_t setpointCoolingMax;
  uint8_t setpointHeatingMin;
  uint8_t setpointHeatingMax;
#endif /* F_SERIES */
#ifdef H_SERIES
  uint8_t hitachiModel;
#endif /* H_SERIES */
};

EEPROMSettings EE;

bool EE_dirty = 0;
bool factoryReset = 0;

#define PARAM_MAX_LEN 81      // max of *_LEN below
#define PARAM_MAX_LEN_TXT "81"
#define PARAM_NR (sizeof(paramName) / 4)  // nr of configurable items below
#define PARAM_JMASK 19       // param nr of outputMode
#define PARAM_DEVICE_NAME 13 // param nr of deviceName
#define PARAM_BRIDGE_NAME 14 // param nr of bridgeName
#define PARAM_USE_BRIDGE_NAME_IN_TOPIC 28
#define PARAM_USE_DEVICE_NAME_IN_TOPIC 24
#define PARAM_MQTT_PREFIX 12
#define PARAM_USE_TZ 33
#define PARAM_USER_TZ 34
#ifdef E_SERIES
#define PARAM_R4T_OFFSET 39
#define PARAM_R1T_OFFSET 40
#define PARAM_R2T_OFFSET 41
#define PARAM_RT_OFFSET 42
#define PARAM_EC 45
#define PARAM_EP 46
#define PARAM_HA_SETUP 47
#define xstr(s) str(s)
#define str(s) #s
#endif /* E_SERIES */
#ifdef F_SERIES
#define PARAM_SETPOINT_COOLING_MIN 35
#define PARAM_SETPOINT_COOLING_MAX 36
#define PARAM_SETPOINT_HEATING_MIN 37
#define PARAM_SETPOINT_HEATING_MAX 38
#endif /* F_SERIES */

// wifiManager
const char paramName_00[] PROGMEM = "wifiManager SSID        ";
const char paramName_01[] PROGMEM = "wifiManager password    ";

// network
const char paramName_02[] PROGMEM = "MDNS name               ";
const char paramName_03[] PROGMEM = "use static-IP (0=DHCP)  ";
const char paramName_04[] PROGMEM = "static-IP IPv4 address  ";
const char paramName_05[] PROGMEM = "static-IP IPv4 gateway  ";
const char paramName_06[] PROGMEM = "static-IP IPv4 netmask  ";

// MQTT server
const char paramName_07[] PROGMEM = "MQTT server IPv4 address";
const char paramName_08[] PROGMEM = "MQTT port               ";
const char paramName_09[] PROGMEM = "MQTT username           ";
const char paramName_10[] PROGMEM = "MQTT password           ";
const char paramName_11[] PROGMEM = "MQTT clientname         ";

// MQTT topic construction
const char paramName_12[] PROGMEM = "MQTT topic prefix       "; // PARAM_MQTT_PREFIX 12
const char paramName_13[] PROGMEM = "device name             "; // PARAM_DEVICE_NAME 13
const char paramName_14[] PROGMEM = "bridge name             "; // PARAM_BRIDGE_NAME 14
// MQTT topic construction HA-specific
const char paramName_15[] PROGMEM = "HAconfig prefix         ";

// telnet
const char paramName_16[] PROGMEM = "telnet magic word       ";

// OTA
const char paramName_17[] PROGMEM = "OTA hostname            ";
const char paramName_18[] PROGMEM = "OTA password            ";

// bridge modes
const char paramName_19[] PROGMEM = "output mode             "; // PARAM_JMASK 19
const char paramName_20[] PROGMEM = "output filter           ";
const char paramName_21[] PROGMEM = "noWiFi (for eth)        ";

//HA
const char paramName_22[] PROGMEM = "use device for HA name  ";
const char paramName_23[] PROGMEM = "use device name in devId";
const char paramName_24[] PROGMEM = "use device name in topic"; // PARAM_USE_DEVICE_NAME_IN_TOPIC 24
const char paramName_25[] PROGMEM = "use device name in ent. ";
const char paramName_26[] PROGMEM = "use bridge for HA name  ";
const char paramName_27[] PROGMEM = "use bridge name in devId";
const char paramName_28[] PROGMEM = "use bridge name in topic"; // PARAM_USE_BRIDGE_NAME_IN_TOPIC 28
const char paramName_29[] PROGMEM = "use bridge name in ent. ";
const char paramName_30[] PROGMEM = "HA short device name    ";

const char paramName_31[] PROGMEM = "ESPhwID [do not change] ";

const char paramName_32[] PROGMEM = "(reserved)              ";

const char paramName_33[] PROGMEM = "use NTP for time stamp  "; // PARAM_USE_TZ
const char paramName_34[] PROGMEM = "user-defined TZ         "; // PARAM_USER_TZ

#ifdef W_SERIES
const char paramName_35[] PROGMEM = "Homewizard HWE-KWH1 URL ";
#endif /* W_SERIES */

#ifdef E_SERIES
// MQTT input topics
const char paramName_35[] PROGMEM = "mqtt_Electricity_Power  ";
const char paramName_36[] PROGMEM = "mqtt_Electricity_Total  ";
const char paramName_37[] PROGMEM = "mqtt_BUH_Power          ";
const char paramName_38[] PROGMEM = "mqtt_BUH_Total          ";
// sensor offsets
const char paramName_39[] PROGMEM = "R4T offset (0.01C) RWT  "; // PARAM_R4T_OFFSET // change #defines above if PARAM_* changes
const char paramName_40[] PROGMEM = "R1T offset (0.01C) mid  "; // PARAM_R1T_OFFSET
const char paramName_41[] PROGMEM = "R2T offset (0.01C) LWT  "; // PARAM_R2T_OFFSET
const char paramName_42[] PROGMEM = "RT offset (0.01C) room  "; // PARAM_RT_OFFSET
const char paramName_43[] PROGMEM = "use R1T instead of R2T  ";

// COP calculation
const char paramName_44[] PROGMEM = "use energytotal         ";
const char paramName_45[] PROGMEM = "Electricity use <bridge "; // PARAM_EC
const char paramName_46[] PROGMEM = "Energy produced <bridge "; // PARAM_EP

// HA dashboard setup
const char paramName_47[] PROGMEM = "HA dashboard setup      "; // PARAM_HA_SETUP

// Date_Time_Daikin updated per second or per minute
const char paramName_48[] PROGMEM = "Minute timestamp only   ";
#endif /* E_SERIES */

#ifdef F_SERIES
// Configurable min/max cooling/heating
const char paramName_35[] PROGMEM = "Setpoint cooling minimum"; // PARAM_SETPOINT_COOLING_MIN
const char paramName_36[] PROGMEM = "Setpoint cooling maximum"; // PARAM_SETPOINT_COOLING_MAX
const char paramName_37[] PROGMEM = "Setpoint heating minimum"; // PARAM_SETPOINT_HEATING_MIN
const char paramName_38[] PROGMEM = "Setpoint heating maximum"; // PARAM_SETPOINT_HEATING_MAX
#endif /* F_SERIES */

#ifdef H_SERIES
const char paramName_35[] PROGMEM = "Hitachi Model Type      ";
#endif /* H_SERIES */

const char* const paramName[] PROGMEM = {
  paramName_00,
  paramName_01,
  paramName_02,
  paramName_03,
  paramName_04,
  paramName_05,
  paramName_06,
  paramName_07,
  paramName_08,
  paramName_09,
  paramName_10,
  paramName_11,
  paramName_12,
  paramName_13,
  paramName_14,
  paramName_15,
  paramName_16,
  paramName_17,
  paramName_18,
  paramName_19,
  paramName_20,
  paramName_21,
  paramName_22,
  paramName_23,
  paramName_24,
  paramName_25,
  paramName_26,
  paramName_27,
  paramName_28,
  paramName_29,
  paramName_30,
  paramName_31,
  paramName_32,
  paramName_33,
  paramName_34,
#ifdef W_SERIES
  paramName_35,
#endif /* W_SERIES */
#ifdef E_SERIES
  paramName_35,
  paramName_36,
  paramName_37,
  paramName_38,
  paramName_39,
  paramName_40,
  paramName_41,
  paramName_42,
  paramName_43,
  paramName_44,
  paramName_45,
  paramName_46,
  paramName_47,
  paramName_48,
#endif /* E_SERIES */
#ifdef F_SERIES
  paramName_35,
  paramName_36,
  paramName_37,
  paramName_38,
#endif /* F_SERIES */
#ifdef H_SERIES
  paramName_35,
#endif /* H_SERIES */
};

typedef enum {
  P_STRING,
  P_UINT,
  P_HEX,
  P_BOOL,
  P_INTdiv100,
} paramTypes;

const paramTypes PROGMEM paramType[] = {
  P_STRING,
  P_STRING,
  P_STRING,
  P_BOOL,
  P_STRING,
  P_STRING,
  P_STRING,
  P_STRING,
  P_UINT,
  P_STRING,
  P_STRING,
  P_STRING,
  P_STRING,
  P_STRING,
  P_STRING,
  P_STRING,
  P_STRING,
  P_STRING,
  P_STRING,
  P_HEX,
  P_BOOL,
  P_BOOL,
  P_BOOL,
  P_BOOL,
  P_BOOL,
  P_BOOL,
  P_BOOL,
  P_BOOL,
  P_BOOL,
  P_BOOL,
  P_STRING,
  P_BOOL,
  P_STRING,
  P_UINT,
  P_STRING,
#ifdef W_SERIES
  P_STRING,
#endif /* W_SERIES */
#ifdef E_SERIES
  P_STRING,
  P_STRING,
  P_STRING,
  P_STRING,
  P_INTdiv100,
  P_INTdiv100,
  P_INTdiv100,
  P_INTdiv100,
  P_BOOL,
  P_BOOL,
  P_UINT,
  P_UINT,
  P_BOOL,
  P_BOOL,
#endif /* E_SERIES */
#ifdef F_SERIES
  P_UINT,
  P_UINT,
  P_UINT,
  P_UINT,
#endif /* F_SERIES */
#ifdef H_SERIES
  P_UINT,
#endif /* H_SERIES */
};

const int PROGMEM paramSize[] = {
  1,
  1,
  1,
  1,
  1,
  1,
  1,
  1,
  4, // MQTT port
  1,
  1,
  1,
  1,
  1,
  1,
  1,
  1,
  1,
  1,
  2, // J-mask
  1,
  1,
  1,
  1,
  1,
  1,
  1,
  1,
  1,
  1,
  1,
  1,
  1,
  1,
  1,
#ifdef W_SERIES
  1,
#endif /* W_SERIES */
#ifdef E_SERIES
  1,
  1,
  1,
  1,
  1,
  1,
  1,
  2,
  1,
  1,
  4,
  4,
  1,
  1,
#endif /* E_SERIES */
#ifdef F_SERIES
  1,
  1,
  1,
  1,
#endif /* F_SERIES */
#ifdef H_SERIES
  1,
#endif /* H_SERIES */
};

const int PROGMEM paramMax[] = { // non-string: max-value (inclusive); string: max-length (length includes \0)
  WIFIMAN_SSID_LEN,
  WIFIMAN_PASSWORD_LEN,
  MDNS_NAME_LEN,
  1,
  STATIC_IP_LEN,
  STATIC_IP_LEN,
  STATIC_IP_LEN,
  MQTT_SERVER_LEN,
  65535,
  MQTT_USER_LEN,
  MQTT_PASSWORD_LEN,
  MQTT_CLIENTNAME_LEN,
  MQTT_PREFIX_LEN,
  DEVICE_NAME_LEN,
  BRIDGE_NAME_LEN,
  HACONFIG_PREFIX_LEN,
  TELNET_MAGICWORD_LEN,
  OTA_HOSTNAME_LEN,
  OTA_PASSWORD_LEN,
  0x00FFFF, // J-mask
  9,
  1,
  1,
  1,
  1,
  1,
  1,
  1,
  1,
  1,
  DEVICE_SHORT_NAME_HA_LEN,
  1,
  RESERVED_LEN,
  NR_PREDEFINED_TZ - 1,
  TZ_STRING_LEN,
#ifdef W_SERIES
  MQTT_INPUT_TOPIC_LEN, // E
#endif /* W_SERIES */
#ifdef E_SERIES
  MQTT_INPUT_TOPIC_LEN, // E
  MQTT_INPUT_TOPIC_LEN, // E
  MQTT_INPUT_TOPIC_LEN, // E
  MQTT_INPUT_TOPIC_LEN, // E
  100, // div100
  100, // div100
  100, // div100
  300, // div100
  1, // E
  1, // E
  999999999, // E
  999999999, // E
  1,
  1,
#endif /* E_SERIES */
#ifdef F_SERIES
  40,
  40,
  40,
  40,
#endif /* F_SERIES */
#ifdef H_SERIES
   2,
#endif /* H_SERIES */
};

char* const PROGMEM paramLocation[] = {
  EE.wifiManager_SSID,
  EE.wifiManager_password,
  EE.mdnsName,
  (char*) &EE.useStaticIP,
  EE.static_ip,
  EE.static_gw,
  EE.static_nm,
  EE.mqttServer,
  (char*) &EE.mqttPort,
  EE.mqttUser,
  EE.mqttPassword,
  EE.mqttClientName,
  EE.mqttPrefix,
  EE.deviceName,
  EE.bridgeName,
  EE.haConfigPrefix,
  EE.telnetMagicword,
  EE.otaHostname,
  EE.otaPassword,
  (char*) &EE.outputMode,
  (char*) &EE.outputFilter,
  (char*) &EE.noWiFi,
  (char*) &EE.useDeviceNameInDeviceNameHA,
  (char*) &EE.useDeviceNameInDeviceIdentity,
  (char*) &EE.useDeviceNameInTopic,
  (char*) &EE.useDeviceNameInEntityName,
  (char*) &EE.useBridgeNameInDeviceNameHA,
  (char*) &EE.useBridgeNameInDeviceIdentity,
  (char*) &EE.useBridgeNameInTopic,
  (char*) &EE.useBridgeNameInEntityName,
  EE.deviceShortNameHA,
  (char*) &EE.ESPhwID,
  (char*) &EE.reservedText,
  (char*) &EE.useTZ,
  EE.userTZ,
#ifdef W_SERIES
  (char*) &EE.meterURL,
#endif /* W_SERIES */
#ifdef E_SERIES
  EE.mqttElectricityPower,
  EE.mqttElectricityTotal,
  EE.mqttBUHpower,
  EE.mqttBUHtotal,
  (char*) &EE.R4Toffset,
  (char*) &EE.R1Toffset,
  (char*) &EE.R2Toffset,
  (char*) &EE.RToffset,
  (char*) &EE.useR1T,
  (char*) &EE.useTotal,
  (char*) &EE.electricityConsumedCompressorHeating1,
  (char*) &EE.energyProducedCompressorHeating1,
  (char*) &EE.haSetup,
  (char*) &EE.minuteTimeStamp,
#endif /* E_SERIES */
#ifdef F_SERIES
  (char*) &EE.setpointCoolingMin,
  (char*) &EE.setpointCoolingMax,
  (char*) &EE.setpointHeatingMin,
  (char*) &EE.setpointHeatingMax,
#endif /* F_SERIES */
#ifdef H_SERIES
  (char*) &EE.hitachiModel,
#endif /* H_SERIES */
};

#define outputUnknown (EE.outputMode & 0x0008)

bool ethernetConnected  = false;

#define ATMEGA_SERIAL_ENABLE 15 // required for v1.2

#ifdef ETHERNET
// Connections W5500: Arduino Mega pins 50 (MISO), 51 (MOSI), 52 (SCK), and 10 (SS) (https://www.arduino.cc/en/Reference/Ethernet)
//                       Pin 53 (hardware SS) is not used but must be kept as output. In addition, connect RST, GND, and 5V.

// W5500:
// MISO GPIO12 pin 6
// MOSI GPIO13 pin 7
// CLK  GPIO14 pin 5
// ~RST GPIO16 pin 4
// SCS  GPIO4  pin 19

#define ETH_CS_PIN        4  // GPIO4
#define ETH_RESET_PIN     16 // GPIO16
#define SHIELD_TYPE       "ESP8266_W5500 Ethernet"

#ifndef AVRISP
#include <SPI.h>
#endif

#include "W5500lwIP.h"
Wiznet5500lwIP eth(ETH_CS_PIN);
#endif /* ETHERNET */

#include <WiFiClient.h> // WiFiClient (-> TCPClient), WiFiServer (->TCPServer)
 using TCPClient = WiFiClient;
 using TCPServer = WiFiServer;

char MQTT_payload[ MQTT_PAYLOAD_LEN + 1 ]; // +1 for '\0'
char mqttBuffer[MQTT_BUFFER_SIZE];
volatile uint16_t mqttBufferHead = 0;
volatile uint16_t mqttBufferHead2 = 0;
volatile uint16_t mqttBufferTail = 0;
volatile uint16_t mqttBufferFree = MQTT_BUFFER_SIZE - 1;
// mqttBufferTail increases with reads
// mqttBufferHead increases with writes
// mqttBufferHead2 mimics mqttBufferHead, but updates only when '\n' is written (so no partial lines will be read)

bool mqttBuffer_writeChar (const char c) {
  if (!mqttBufferFree) return 0;
  mqttBuffer[mqttBufferHead] = c;
  if (++mqttBufferHead >= MQTT_BUFFER_SIZE) mqttBufferHead = 0;
  if (c == '\n') mqttBufferHead2 = mqttBufferHead;
  mqttBufferFree--;
  return 1;
}

bool mqttBufferWriteString (const char* c, uint16_t len) {
  if (mqttBufferFree < len) return 0;
  uint16_t len1 = len;
  uint16_t len2 = 0;
  if (mqttBufferHead + len > MQTT_BUFFER_SIZE) {
    len1 = MQTT_BUFFER_SIZE - mqttBufferHead;
    len2 = len - len1;
  }
  strncpy(mqttBuffer + mqttBufferHead, c, len1);
  if (len2) strncpy(mqttBuffer, c + len1, len2);
  mqttBufferHead += len;
  if (mqttBufferHead >= MQTT_BUFFER_SIZE) {
    mqttBufferHead = len2;
  }
  mqttBufferFree -= len;
  return 1;
}

int16_t mqttBufferReadChar (void) {
// returns -1 if buffer empty (or acts as if empty as it contains only a partial line), or c otherwise
  if (mqttBufferTail == mqttBufferHead2) return -1;
  char c = mqttBuffer[mqttBufferTail];
  if (++mqttBufferTail >= MQTT_BUFFER_SIZE) mqttBufferTail = 0;
  mqttBufferFree++;
  return c;
}

uint16_t snprintfLength = 0;
uint16_t snprintfLengthMax = 0;

// printfTopicS publishes string, always prefixed by NTP-date, via Mqtt topic P1P2/S, telnet, and/or serial, depending on connectivity

#define printfTopicS_mqttserialonly(formatstring, ...) { \
  if ((snprintfLength = snprintf_P(sprint_value + TZ_PREFIX_LEN - 1, SPRINT_VALUE_LEN, PSTR(formatstring) __VA_OPT__(,) __VA_ARGS__)) > SPRINT_VALUE_LEN - 2) { \
    delayedPrintfTopicS("Too long:"); \
  }; \
  if (snprintfLength > snprintfLengthMax) snprintfLengthMax = snprintfLength; \
  Serial_println(sprint_value);\
  clientPublishMqttChar('S', MQTT_QOS_SIGNAL, MQTT_RETAIN_SIGNAL, sprint_value);\
};

#define printfTopicS(formatstring, ...) { \
  if ((snprintfLength = snprintf_P(sprint_value + TZ_PREFIX_LEN - 1, SPRINT_VALUE_LEN, PSTR(formatstring) __VA_OPT__(,) __VA_ARGS__)) > SPRINT_VALUE_LEN - 2) { \
    delayedPrintfTopicS("Too long:"); \
  }; \
  if (snprintfLength > snprintfLengthMax) snprintfLengthMax = snprintfLength; \
  Serial_println(sprint_value);\
  clientPublishMqttChar('S', MQTT_QOS_SIGNAL, MQTT_RETAIN_SIGNAL, sprint_value);\
  clientPublishTelnet(0, sprint_value + 2, false);\
};

#define printfTopicS_MON(formatstring, ...) { \
  strncpy(sprint_value + 3, "MON", 3);\
  if ((snprintfLength = snprintf_P(sprint_value + TZ_PREFIX_LEN - 1, SPRINT_VALUE_LEN, PSTR(formatstring) __VA_OPT__(,) __VA_ARGS__)) > SPRINT_VALUE_LEN - 2) { \
    delayedPrintfTopicS("Too long:"); \
  }; \
  if (snprintfLength > snprintfLengthMax) snprintfLengthMax = snprintfLength; \
  Serial_println(sprint_value);\
  clientPublishMqttChar('S', MQTT_QOS_SIGNAL, MQTT_RETAIN_SIGNAL, sprint_value);\
  clientPublishTelnet(0, sprint_value + 2, false);\
  strncpy(sprint_value + 3, "ESP", 3);\
};


#define printfTelnet_MON(formatstring, ...) { \
  strncpy(sprint_value + 3, "MON", 3);\
  if ((snprintfLength = snprintf_P(sprint_value + TZ_PREFIX_LEN - 1, SPRINT_VALUE_LEN, PSTR(formatstring) __VA_OPT__(,) __VA_ARGS__)) > SPRINT_VALUE_LEN - 2) { \
    delayedPrintfTopicS("Too long:"); \
  }; \
  if (snprintfLength > snprintfLengthMax) snprintfLengthMax = snprintfLength; \
  Serial_println(sprint_value);\
  clientPublishTelnet(0, sprint_value + 2, false);\
  strncpy(sprint_value + 3, "ESP", 3);\
};

// delayedPrintfTopicS for call-back routines for delayed printing via mqttBuffer
// allowed in call-back functions:

byte mqttBufferFullReported = 0;

#define delayedPrintfTopicS(formatstring, ...) { \
  byte snprintValueLength; \
  snprintfLength = snprintf_P(sprint_value + TZ_PREFIX_LEN - 1, SPRINT_VALUE_LEN, PSTR(formatstring) __VA_OPT__(,) __VA_ARGS__); \
  if (snprintfLength > snprintfLengthMax) snprintfLengthMax = snprintfLength; \
  if (mqttBufferFree < snprintfLength + 2 + MQTT_BUFFER_SPARE) { \
    if ((!mqttBufferFullReported) && (mqttBufferFree >= 3)) { \
      mqttBuffer_writeChar('^'); \
      mqttBuffer_writeChar('D'); \
      mqttBuffer_writeChar('\n'); \
      mqttBufferFullReported = 1; \
    } \
  } else { \
    mqttBufferFullReported = 0; \
    if (snprintfLength > SPRINT_VALUE_LEN - 2) { \
      mqttBuffer_writeChar('%'); /* too long */ \
      mqttBufferWriteString(sprint_value + TZ_PREFIX_LEN - 1, SPRINT_VALUE_LEN - 1); \
    } else { \
      mqttBuffer_writeChar('<'); \
      mqttBufferWriteString(sprint_value + TZ_PREFIX_LEN - 1, snprintfLength); \
    } \
    mqttBuffer_writeChar('\n'); \
  } \
}

#ifdef ETHERNET
bool initEthernet()
{
  SPI.begin();
  SPI.setClockDivider(SPI_CLOCK_DIV4); // 4 MHz
  SPI.setBitOrder(MSBFIRST);
  SPI.setDataMode(SPI_MODE0);
  eth.setDefault();
  delay(100);
  // see also https://github.com/esp8266/Arduino/pull/8395 and https://github.com/esp8266/Arduino/issues/8144
  if (EE.useStaticIP) {
    Serial_println(F("* [ESP] Using static IP"));
    delayedPrintfTopicS("initEthernet using static IP");
    IPAddress _ip, _gw, _nm;
    _ip.fromString(EE.static_ip);
    _gw.fromString(EE.static_gw);
    _nm.fromString(EE.static_nm);
    eth.config(_ip, _gw, _nm, _gw);
  }
  // eth.setDefault();
  if (!eth.begin()) {
    Serial_println("* [ESP] No ethernet hardware detected");
    delayedPrintfTopicS("No ethernet hardware detected");
    return false;
  } else {
    Serial_println("* [ESP] Connecting to network");
    delayedPrintfTopicS("initEthernet connecting to network");
    int timeoutcnt = 0;
    while (!eth.connected()) {
      if (++timeoutcnt > ETHERNET_CONNECTION_TIMEOUT) {
        if (EE.noWiFi) {
          Serial_println(F("* [ESP] Ethernet failed, restart"));
          saveRebootReason(REBOOT_REASON_ETH);
          delay(500);
          ESP.restart();
          delay(100);
        } else {
          Serial_println(F("* [ESP] Ethernet failed, trying WiFi"));
          delayedPrintfTopicS("initEthernet failed, trying WiFi");
          return false;
        }
      }
      Serial_print("* [ESP] Waiting for ethernet connection... ");
      Serial_println(timeoutcnt);
      delayedPrintfTopicS("Waiting for ethernet connection... %d", timeoutcnt);
      delay(1000);
    }
    Serial_println(F("* [ESP] Ethernet connected"));
    delayedPrintfTopicS("Ethernet connected");
    return true;
  }
}
#endif /* ETHERNET */

AsyncMqttClient mqttClient;

uint32_t espUptime = 0;
bool shouldSaveConfig = false;
//#ifdef MHI_SERIES
//static byte cs_gen = CS_GEN;
//#endif /* MHI_SERIES */


#ifdef AVRISP
const uint16_t avrisp_port = 328;
ESP8266AVRISP* avrprog;
#endif

bool telnetConnected = false;
char mqttTopic[ MQTT_TOPIC_LEN ];
byte mqttTopicChar;
byte mqttSlashChar;
byte mqttTopicCatChar;
byte mqttTopicSrcChar;
byte mqttTopicPrefixLength;

// P1P2/ <topicChar /P1P2MQTT/bridge0 <slashchar> [/ or /a]
#define topicCharGeneric(x)       { mqttTopic[mqttTopicChar - 1] = '/'; mqttTopic[mqttTopicChar] = x;   mqttTopic[mqttTopicChar + 1] = '\0'; }
#define topicCharGenericHash(x)   { mqttTopic[mqttTopicChar - 1] = '/'; mqttTopic[mqttTopicChar] = x;   mqttTopic[mqttTopicChar + 1] = '/'; mqttTopic[mqttTopicChar + 2] = '#'; mqttTopic[mqttTopicChar + 3] = '\0';}
#define topicCharSpecific(x)      { mqttTopic[mqttTopicChar - 1] = '/'; mqttTopic[mqttTopicChar] = x;   mqttTopic[mqttTopicChar + 1] = '/'; mqttTopic[mqttSlashChar] = '\0';                                  }
#define topicCharSpecificSlash(x) { mqttTopic[mqttTopicChar - 1] = '/'; mqttTopic[mqttTopicChar] = x;   mqttTopic[mqttTopicChar + 1] = '/'; mqttTopic[mqttSlashChar] = '/';                                       mqttTopic[mqttSlashChar + 2] = '/'; }

#define topicCharBin(x)           { mqttTopic[mqttTopicChar - 1] = '/'; mqttTopic[mqttTopicChar] = 'M'; mqttTopic[mqttTopicChar + 1] = '/'; mqttTopic[mqttSlashChar] = '/';     mqttTopic[mqttSlashChar + 1] = x; mqttTopic[mqttSlashChar + 2] = '\0';} // overwrites topic, acceptable in main functions only

byte saveTopicCharPrev = '_';
byte saveTopicChar = '_';
byte saveTopicCharNext = '_';
byte saveTopicCharNext2 = '_';
byte saveTopicCharNext3 = '_';
byte saveSlashChar = '_';
byte saveSlashCharNext = '_';
byte saveSlashCharNext2 = '_';

void saveTopic() {
  saveTopicCharPrev = mqttTopic[mqttTopicChar - 1];
  saveTopicChar = mqttTopic[mqttTopicChar];
  saveTopicCharNext = mqttTopic[mqttTopicChar + 1];
  saveTopicCharNext2 = mqttTopic[mqttTopicChar + 2];
  saveTopicCharNext3 = mqttTopic[mqttTopicChar + 3];
  saveSlashChar = mqttTopic[mqttSlashChar];
  saveSlashCharNext = mqttTopic[mqttSlashChar + 1];
  saveSlashCharNext2 = mqttTopic[mqttSlashChar + 2];
}

void restoreTopic() {
  mqttTopic[mqttTopicChar - 1] = saveTopicCharPrev;
  mqttTopic[mqttTopicChar] = saveTopicChar;
  mqttTopic[mqttTopicChar + 1] = saveTopicCharNext;
  mqttTopic[mqttTopicChar + 2] = saveTopicCharNext2;
  mqttTopic[mqttTopicChar + 3] = saveTopicCharNext3;
  mqttTopic[mqttSlashChar] = saveSlashChar;
  mqttTopic[mqttSlashChar + 1] = saveSlashCharNext;
  mqttTopic[mqttSlashChar + 2] = saveSlashCharNext2;
}

#define topicWrite { topicCharSpecific('W'); }

#ifdef TELNET
ESPTelnet telnet;
uint16_t telnet_port=23;
bool telnetUnlock = 0;

// call-backs
void onTelnetConnect(String ip) {
  Serial_print(F("- Telnet: "));
  Serial_print(ip);
  Serial_println(F(" connected"));
  delayedPrintfTopicS("Telnet %s connected", ip.c_str());

  telnet.print("\nWelcome " + telnet.getIP() + " to ");
  telnet.print(F(WELCOMESTRING));
  telnet.print(" compiled ");
  telnet.print(__DATE__);
  telnet.print(" ");
  telnet.print(__TIME__);
#ifdef E_SERIES
  telnet.println(" for Daikin E-Series");
#endif /* E_SERIES */
#ifdef F_SERIES
  telnet.println(" for Daikin F-Series");
#endif /* F_SERIES */
#ifdef H_SERIES
  telnet.println(" for Hitachi");
#endif /* H_SERIES */
#ifdef MHI_SERIES
  telnet.println(" for MHI");
#endif /* MHI_SERIES */
  telnet.println(F("(Use ^] + q  to disconnect.)"));
  telnetUnlock = !(EE.telnetMagicword[1]);
  if (!telnetUnlock) telnet.println(F("Please type magic word (starting with _) to unlock telnet functionality"));
  if (telnetUnlock) telnet.println(F("No telnet lock - use 'Pxx _lockword' to add magic lock word"));
  telnetConnected = true;
}

void onTelnetDisconnect(String ip) {
  Serial_print(F("- Telnet: "));
  Serial_print(ip);
  Serial_println(F(" disconnected"));
  telnetConnected = false;
  delayedPrintfTopicS("Telnet %s disconnected", ip.c_str());
}

void onTelnetReconnect(String ip) {
  Serial_print(F("- Telnet: "));
  Serial_print(ip);
  Serial_println(F(" reconnected"));
  telnetConnected = true;
  delayedPrintfTopicS("Telnet %s reconnected", ip.c_str());
}

void onTelnetConnectionAttempt(String ip) {
  Serial_print(F("- Telnet: "));
  Serial_print(ip);
  Serial_println(F(" tried to connect"));
  delayedPrintfTopicS("Telnet %s tried to connect", ip.c_str());
}
#endif

#define EMPTY_PAYLOAD 0xFF

byte mqttConnected = 0;
// .connect()        sets _state to CONNECTING, and when _onConnAck is called,  _state to CONNECTED  and calls onConnectUserCallback()
// .disconnect()     sets _state to DISCONNECTING,  should call _onDisconnect from AsyncTcp lib, which sets _state to DISCONNECTED and calls _onDisconnectUserCallback()
// .disconnect(true) sets _state to DISCONNECTED, unuser whether this calls _onDisconnectUserCallback()

uint16_t Mqtt_msgSkipLowMem = 0;
uint16_t Mqtt_msgSkipNotConnected = 0;
uint16_t Mqtt_disconnects = 0;
uint16_t Mqtt_disconnectSkippedPackets = 0;
uint16_t Mqtt_disconnectTime = 0;
uint16_t Mqtt_disconnectTimeTotal = 0;
byte Mqtt_reconnectDelay = 10;
uint16_t prevpacketId = 0;
uint32_t Mqtt_waitCounter = 0;

#define MAXWAIT 100 // * 5ms
#define MAXWAIT_QOS 200 // * 5ms

uint32_t mqttPublished = 0;

#ifdef DEBUG_OVER_SERIAL
#define ignoreSerial 1
#else
#define ignoreSerial (EE.outputMode & 0x8000)
#endif

volatile bool Skipped = false;

bool clientPublishMqtt(const char* key, uint8_t qos, bool retain, const char* value = nullptr) {
  if (mqttConnected) {
    byte i = 0;
    while ((ESP.getMaxFreeBlockSize() < MQTT_MIN_FREE_MEMORY) && (i++ < (qos ? MAXWAIT_QOS : MAXWAIT))) {
      Mqtt_waitCounter++;
      delay(5);
    }
    if (ESP.getMaxFreeBlockSize() < MQTT_MIN_FREE_MEMORY) {
      Mqtt_msgSkipLowMem++;
      if (!Skipped) delayedPrintfTopicS("--(mqtt low mem skip)--");
      Skipped = true;
      return 0;
    }
    mqttPublished++;
    mqttClient.publish(key, qos, retain, value);
    Skipped = false;
    return 1;
  } else {
    Mqtt_msgSkipNotConnected++;
    if (!Skipped) delayedPrintfTopicS("~~(mqtt reconnected)~~");
    Skipped = true;
    return 0;
  }
}

void clientPublishMqttChar(const char key, uint8_t qos, bool retain, const char* value = nullptr) {
  topicCharSpecific(key);
  clientPublishMqtt(mqttTopic, qos, retain, value);
}

void clientPublishTelnet(bool includeTopic, const char* value, bool addDate = true) {
  if (telnetConnected) {
    if (!telnetUnlock) {
      // telnet.println(F("locked"));
      return;
    }
    if (((EE.useTZ > 1) || (EE.useTZ == 1) && EE.userTZ[0]) && addDate) {
      sprint_value[ TZ_PREFIX_LEN - 1 ] = '\0';
      telnet.print(sprint_value + 2);
    }
    if (includeTopic) {
      telnet.print(mqttTopic);
      telnet.print(F(" "));
    }
    telnet.println(value);
  }
}

void clientPublishTelnetChar(const char key, const char* value) {
  if (key) {
    topicCharSpecific(key);
    clientPublishTelnet(1, /*mqttTopic, */value);
  } else {
    // should not happen
  }
}

#define MAXRH 23
#define PWB (23 * 2 + 36) // max pseudopacket 23 bytes (excl CRC byte), 33 bytes for timestamp-prefix, 1 byte for terminating null, 2 for CRC byte

static byte pseudo0B = 0;
static byte pseudo0C = 0;
static byte pseudo0D = 0;
static byte pseudo0E = 0;
static byte pseudo0F = 9;
byte mqttDeleting = 0;
uint32_t mqttUnsubscribeTime = 0;

void writePseudoPacket(byte* WB, byte rh)
// rh is pseudo packet size (without CRC or CS byte)
{
  char pseudoWriteBuffer[PWB];
  if (rh > MAXRH) {
    printfTopicS("rh > %d", MAXRH);
    return;
  }
  sprint_value[ TZ_PREFIX_LEN - 1 ] = '\0';
  snprintf_P(pseudoWriteBuffer, 33, PSTR("R%sP         "), sprint_value + 7);
#ifdef MHI_SERIES
  uint8_t cs = 0;
#else /* MHI_SERIES */
  uint8_t crc = CRC_FEED;
#endif /* MHI_SERIES */
  for (uint8_t i = 0; i < rh; i++) {
    uint8_t c = WB[i];
    snprintf(pseudoWriteBuffer + TZ_PREFIX_LEN + 3 + (i << 1), 3, "%02X", c);
#ifdef MHI_SERIES
    if (CS_GEN != 0) cs += c;
#else /* MHI_SERIES */
    if (CRC_GEN != 0) for (uint8_t i = 0; i < 8; i++) {
      crc = ((crc ^ c) & 0x01 ? ((crc >> 1) ^ CRC_GEN) : (crc >> 1));
      c >>= 1;
    }
#endif /* MHI_SERIES */
  }
#ifdef MHI_SERIES
  WB[rh] = cs;
  if (CS_GEN) snprintf(pseudoWriteBuffer + TZ_PREFIX_LEN + 3 + (rh << 1), 3, "%02X", cs);
#else /* MHI_SERIES */
  WB[rh] = crc;
  if (CRC_GEN) snprintf(pseudoWriteBuffer + TZ_PREFIX_LEN + 3+ (rh << 1), 3, "%02X", crc);
#endif /* MHI_SERIES */
  if (EE.outputMode & 0x0004) clientPublishMqttChar('R', MQTT_QOS_HEX, MQTT_RETAIN_HEX, pseudoWriteBuffer);
  // pseudoWriteBuffer[22] = 'R';
  if (EE.outputMode & 0x0010) printfTelnet_MON("R %s", pseudoWriteBuffer + 22);
  if ((EE.outputMode & 0x0022) && !mqttDeleting) process_for_mqtt(WB, rh);
}

byte readHex[HB];

char haConfigTopic[HA_KEY_LEN];
uint16_t haConfigTopicLength = 0;
uint16_t haConfigTopicLengthMax = 0;

char haConfigMessage[HA_VALUE_LEN];
uint16_t haConfigMessageLength = 0;
uint16_t haConfigMessageLengthMax = 0;

#define HACONFIGTOPIC(formatstring, ...) { \
  haConfigTopicLength = snprintf_P(haConfigTopic, HA_KEY_LEN, PSTR(formatstring) __VA_OPT__(,) __VA_ARGS__); \
  if (haConfigTopicLength > haConfigTopicLengthMax) haConfigTopicLengthMax = haConfigTopicLength; \
  if (haConfigTopicLength >= HA_KEY_LEN) { \
    printfTopicS("haConfigTopic too long %i >= %i (%.20s)", haConfigTopicLength, HA_KEY_LEN, haConfigTopic); \
    return 0; \
  } \
}

#define HACONFIGMESSAGE_ADD(formatstring, ...) { \
  haConfigMessageLength += snprintf_P(haConfigMessage + haConfigMessageLength, HA_VALUE_LEN - haConfigMessageLength, PSTR(formatstring) __VA_OPT__(,) __VA_ARGS__); \
  if (haConfigMessageLength > haConfigMessageLengthMax) haConfigMessageLengthMax = haConfigMessageLength; \
  if (haConfigMessageLength >= HA_VALUE_LEN) { \
    char* nameP; \
    if ((nameP = strstr_P(haConfigMessage, PSTR("name\":\""))) != NULL) { \
      nameP = nameP + 7; \
    } else { \
      nameP = haConfigMessage; \
    } \
    printfTopicS("haConfigMsg too long %i >= %i (%.25s)", haConfigMessageLength, HA_VALUE_LEN, nameP); return 0; \
  } \
}

bool publishHomeAssistantConfig(const char* deviceSubName,
                                const hadevice haDevice,
                                const haentity haEntity,
                                const haentitycategory haEntityCategory,
                                byte haPrecision,
                                const habuttondeviceclass haButtonDeviceClass,
                                bool useSrc, bool useCommonName = 0, const char* commonNameString = nullptr) {
  if (useCommonName && commonNameString) {
    snprintf_P(entityUniqId, ENTITY_UNIQ_ID_LEN, PSTR("%s_%s_%s"), EE.deviceName, EE.bridgeName, commonNameString);
  } else {
    snprintf_P(entityUniqId, ENTITY_UNIQ_ID_LEN, PSTR("%s_%s_%c%c_%s_%c"), EE.deviceName, EE.bridgeName, mqttTopic[mqttTopicCatChar], mqttTopic[mqttTopicSrcChar], mqttTopic + mqttTopicPrefixLength, mqttTopic[mqttTopicSrcChar]);
  }

  if (EE.useDeviceNameInEntityName) {
    snprintf_P(entityName, ENTITY_UNIQ_ID_LEN, PSTR("%s_"), EE.deviceName);
  } else {
    entityName[0] = '\0';
  }
  if (EE.useBridgeNameInEntityName && EE.bridgeName[0]) {
    snprintf_P(entityName + strlen(entityName), ENTITY_UNIQ_ID_LEN - strlen(entityName), PSTR("%s_"), EE.bridgeName);
  }
  snprintf_P(entityName + strlen(entityName), ENTITY_UNIQ_ID_LEN - strlen(entityName), PSTR("%s"), mqttTopic + mqttTopicPrefixLength);
  if (useSrc) snprintf_P(entityName + strlen(entityName), ENTITY_UNIQ_ID_LEN - strlen(entityName), PSTR("_%c"), mqttTopic[mqttTopicSrcChar]);

  HACONFIGTOPIC("%s/%s/%s/%s/config",
    /* home assistant prefix */ EE.haConfigPrefix, haPrefixString[haDevice],
    /* node_id */ EE.bridgeName,
    /* object_id = uniq_id = entity ID */ entityUniqId);

  topicCharSpecific('L');

  HACONFIGMESSAGE_ADD("\"name\":\"%s\",\"uniq_id\":\"%s\",\"avty\":[{\"topic\":\"%s\",\"pl_avail\":\"online\",\"pl_not_avail\":\"offline\"}%s],\"avty_mode\":\"all\",\"dev\":{\"name\":\"%s%s\",\"ids\":[\"%s%s\"],\"mf\":\"%s\",\"mdl\":\"%s\",\"sw\":\"%s\"}",
    /* name        */  entityName, //mqttTopic + mqttTopicPrefixLength, useSrcString,
    /* uniq_id */      entityUniqId,
    /* avty/topic1 */       mqttTopic, // (L)
    /* avty/topic2 */       extraAvailabilityString, // (L)
    /* device */
      /* name */         deviceNameHA, /*underscoreSubName,*/ deviceSubName,
      /* ids */
        /* id */         deviceUniqId, /*underscoreSubName,*/ deviceSubName,
      /* mf */           HA_MF,
      /* mdl */          HA_DEVICE_MODEL,
      /* sw */           HA_SW);


  if (haEntityCategory) {
    HACONFIGMESSAGE_ADD(",\"ent_cat\":\"%s\"", haEntityCategoryString[haEntityCategory]);
  }

  // icon (for some: also device_class unit-of-meas and/or stateclass) determined by entity
  if (haEntity) {
    HACONFIGMESSAGE_ADD(",\"ic\":\"%s\"", haIconString[haEntity]);
    switch (haDevice) {
      case HA_SENSOR :    // state_class
                          //if (haPrecision) {
                            HACONFIGMESSAGE_ADD(",\"sug_dsp_prc\":%d", haPrecision);
                          //}

                          HACONFIGMESSAGE_ADD(",\"stat_cla\":\"%s\"", haStateClassString[haStateClass[haEntity]]);
                          // fall-through
      case HA_NUMBER :    // unit-of-measure
                          HACONFIGMESSAGE_ADD(",\"unit_of_meas\":\"%s\"", haUomString[haEntity]);
                          // fall-through
      case HA_BINSENSOR : // device_class
                          if (haDeviceClassString[haEntity][0]) {
                            HACONFIGMESSAGE_ADD(",\"dev_cla\":\"%s\"", haDeviceClassString[haEntity]);
                          }
                          // fall-through
      default :           break;
    }
  }

  // device_class (only for button)
  if ((haDevice == HA_BUTTON) && haButtonDeviceClass) {
    HACONFIGMESSAGE_ADD(",\"dev_cla\":%s\"", haButtonDeviceClassString[haButtonDeviceClass]);
  }

  // status_topic
  topicCharSpecificSlash('P'); // slash to add entityname
  switch (haDevice) {
    case HA_BINSENSOR  : // pl_of pl_on
                         HACONFIGMESSAGE_ADD(",\"pl_off\":0,\"pl_on\":1");
                         // fall-through
    case HA_SELECT     : // fall-through
    case HA_TEXT       : // fall-through
    case HA_SENSOR     : // fall-through
    case HA_NUMBER     : // fall-through
    case HA_SWITCH     : // state_topic based on mqttTopic (P)
                         HACONFIGMESSAGE_ADD(",\"stat_t\":\"%s\"", mqttTopic);
                         // value_template for all-except-button
    default            : break;
  }

  // add qos
  if (haQos) {
    HACONFIGMESSAGE_ADD(",\"qos\":1");
  }

  // command_topic and command_template for number switch button select text, done via initial string before calling this function, idem for
  // min/max/step/mode for number
  // options for select
  // min/max/mode/pattern for text
  // hvac topics

  HACONFIGMESSAGE_ADD("}");
#define DELAY_HA 50
  delay(DELAY_HA);

  return clientPublishMqtt(haConfigTopic, MQTT_QOS_CONFIG, MQTT_RETAIN_CONFIG, haConfigMessage);
}

bool deleteHomeAssistantConfig(const char* deviceSubName,
                                const hadevice haDevice,
                                const haentity haEntity,
                                const haentitycategory haEntityCategory,
                                byte haPrecision,
                                const habuttondeviceclass haButtonDeviceClass,
                                bool useSrc, bool useCommonName = 0, const char* commonNameString = nullptr) {

  if (useCommonName && commonNameString) {
    snprintf_P(entityUniqId, ENTITY_UNIQ_ID_LEN, PSTR("%s_%s_%s"), EE.deviceName, EE.bridgeName, commonNameString);
  } else {
    snprintf_P(entityUniqId, ENTITY_UNIQ_ID_LEN, PSTR("%s_%s_%c%c_%s_%c"), EE.deviceName, EE.bridgeName, mqttTopic[mqttTopicCatChar], mqttTopic[mqttTopicSrcChar], mqttTopic + mqttTopicPrefixLength, mqttTopic[mqttTopicSrcChar]);
  }

  if (EE.useDeviceNameInEntityName) {
    snprintf_P(entityName, ENTITY_UNIQ_ID_LEN, PSTR("%s_"), EE.deviceName);
  } else {
    entityName[0] = '\0';
  }
  if (EE.useBridgeNameInEntityName && EE.bridgeName[0]) {
    snprintf_P(entityName + strlen(entityName), ENTITY_UNIQ_ID_LEN - strlen(entityName), PSTR("%s_"), EE.bridgeName);
  }
  snprintf_P(entityName + strlen(entityName), ENTITY_UNIQ_ID_LEN - strlen(entityName), PSTR("%s"), mqttTopic + mqttTopicPrefixLength);
  if (useSrc) snprintf_P(entityName + strlen(entityName), ENTITY_UNIQ_ID_LEN - strlen(entityName), PSTR("_%c"), mqttTopic[mqttTopicSrcChar]);

  HACONFIGTOPIC("%s/%s/%s/%s/config",
    /* home assistant prefix */ EE.haConfigPrefix, haPrefixString[haDevice],
    /* node_id */ EE.bridgeName,
    /* object_id = uniq_id = entity ID */ entityUniqId);

#define DELAY_HA_NULL 10
  delay(DELAY_HA_NULL);
  return clientPublishMqtt(haConfigTopic, MQTT_QOS_CONFIG, MQTT_RETAIN_CONFIG /* nullmessage */);
}

uint16_t ePower = 0;            // ePower (W) (max 32767W)
uint16_t bPower = 0;            // bPower (W)
uint32_t eTotal = 0;            // eTotal (Wh) = main heat pump consumption
uint32_t bTotal = 0;            // bTotal (Wh) = BUH consumption
uint32_t ePowerTime = 0;        // time of last ePower update (in uptime/s)
uint32_t eTotalTime = 0;        // time of last eTotal update (in uptime/s)
uint32_t bPowerTime = 0;        // time of last bPower update (in uptime/s)
uint32_t bTotalTime = 0;        // time of last bTotal update (in uptime/s)
bool ePowerAvailable = false;   // indicates if ePower is available
bool eTotalAvailable = false;   // indicates if eTotal is available
bool bPowerAvailable = false;   // indicates if bPower is available
bool bTotalAvailable = false;   // indicates if bTotal is available

// include file for parameter conversion
// include here such that printfTopicS() is available in header file code

#include "P1P2_ParameterConversion.h"

#define MQTT_SAVE_TOPIC_CHAR 'A'

#define doubleResetAddress 0x20
#define rebootReasonAddress 0x21
#define RTC_REGISTER 0x22
uint32_t rebootReasonData;

// all local
char mqttSaveTopicChar = MQTT_SAVE_TOPIC_CHAR;
char* mqttSaveBlockStart = (char*) &M;
uint16_t mqttSaveBytesLeft = 0;
byte mqttSaveReceived = 0;
uint32_t doubleResetData;

bool doubleResetDataDone = 0;
bool doubleResetWiFi = 0;
byte prec_prev = 0;

void saveRebootReason(const byte x) {
  rebootReasonData = x | 0xAAAAAA00;
  ESP.rtcUserMemoryWrite(rebootReasonAddress, &rebootReasonData, sizeof(rebootReasonData));
}

byte rebootReason(void) {
  ESP.rtcUserMemoryRead(rebootReasonAddress, &rebootReasonData, sizeof(rebootReasonData));
  return ((rebootReasonData & 0xFFFFFF00) == 0xAAAAAA00) ? (rebootReasonData & 0xFF) : 0x00;
}

void saveRTC(void) {
  ESP.rtcUserMemoryWrite(RTC_REGISTER, reinterpret_cast<uint32_t *>(&M.R), sizeof(M.R));
}

void mqttClientPublishHex(uint16_t blockSize, char* blockStart) {
  char mqttStringHex[MQTT_SAVE_BLOCK_SIZE * 2 + 1];
  for (uint16_t i = 0; i < blockSize; i++) snprintf(mqttStringHex + (i << 1), 3, "%02X", blockStart[i]);
  mqttClient.publish(mqttTopic, 0, 1, mqttStringHex);
}

void saveMQTT() {
#define MQTT_SAVE_BLOCK_WAIT 20
  mqttSaveBlockStart = (char*) &M;
  mqttSaveTopicChar = MQTT_SAVE_TOPIC_CHAR;
  mqttSaveBytesLeft = sizeof(M);
  while (mqttSaveBytesLeft > 0) {
    topicCharBin(mqttSaveTopicChar);
    if (mqttSaveBytesLeft >= MQTT_SAVE_BLOCK_SIZE) {
      mqttClientPublishHex(MQTT_SAVE_BLOCK_SIZE, mqttSaveBlockStart);
      mqttSaveBytesLeft -= MQTT_SAVE_BLOCK_SIZE;
      mqttSaveBlockStart += MQTT_SAVE_BLOCK_SIZE;
      if (mqttSaveTopicChar == 'Z') {
        mqttSaveTopicChar = 'a';
      } else {
        mqttSaveTopicChar++;
      }
      delay(MQTT_SAVE_BLOCK_WAIT);
    } else {
      mqttClientPublishHex(mqttSaveBytesLeft, mqttSaveBlockStart);
      break; // mqttSaveBytesLeft = 0;
    }
  }
}

void saveData() {
  if (!mqttDeleting) {
    saveRTC();
    saveMQTT();
  }
}

bool loadRTC(void) {
  if ((doubleResetData & 0xFF) <= 1) {
    // power-up, no RTC data
    printfTopicS("loadRTC fails (power-up detected)");
    // no need to do anything; M.R was readback or initalized by loadMQTT
    return 0;
  }
  // load only first 4 bytes (RTCversion and RTCdataLength are uint16_t)
  ESP.rtcUserMemoryRead(RTC_REGISTER, reinterpret_cast<uint32_t *>(&M.R), 4);
  if ((sizeof(M.R) != M.R.RTCdataLength) || (M.R.RTCversion != RTC_VERSION)) {
    printfTopicS("Refuse to loadRTC given wrong version/length %i %i / %i %i", sizeof(M.R), M.R.RTCdataLength, M.R.RTCversion, RTC_VERSION);
    // restore length/version as already readback or initialized by loadMQTT
    M.R.RTCdataLength = sizeof(M.R);
    M.R.RTCversion    = RTC_VERSION;
    // other data was readback or initalized by loadMQTT
    return 0;
  }
  ESP.rtcUserMemoryRead(RTC_REGISTER, reinterpret_cast<uint32_t *>(&M.R), sizeof(M.R));
  printfTopicS("loadRTC readback OK");
  return 1;
}

void loadMQTT() {
  mqttSaveBlockStart = (char*) &M;
  mqttSaveTopicChar = MQTT_SAVE_TOPIC_CHAR;
  mqttSaveBytesLeft = sizeof(M);
  while (mqttSaveBytesLeft > 0) {
    topicCharBin(mqttSaveTopicChar);
    if (mqttClient.subscribe(mqttTopic, 0)) {
      byte i;
      for (i = 0; i < 50; i++) {
        delay(50);
        if (mqttSaveReceived) {
          mqttClient.unsubscribe(mqttTopic);
          if (mqttSaveTopicChar == MQTT_SAVE_TOPIC_CHAR) {
            // first block received via MQTT, check length/version of M.R RTC data block
            if ((sizeof(M.R) != M.R.RTCdataLength) || (M.R.RTCversion != RTC_VERSION)) {
              printfTopicS("M.R.RTC length or version mismatch, full reset data/RTC");
              resetDataStructures();
              initDataRTC();
              return;
            }
            if ((sizeof(M) != M.MdataLength) || (M.Mversion != M_VERSION)) {
              printfTopicS("M length or version mismatch, full reset data/RTC");
              resetDataStructures();
              initDataRTC();
              return;
            }
          }
          if (mqttSaveBytesLeft <= MQTT_SAVE_BLOCK_SIZE) {
            printfTopicS("Mqtt readback OK %s", mqttTopic);
            return;
          }
          mqttSaveReceived = 0;
          if (mqttSaveTopicChar == 'Z') {
            mqttSaveTopicChar = 'a';
          } else {
            mqttSaveTopicChar++;
          }
          mqttSaveBytesLeft -= MQTT_SAVE_BLOCK_SIZE;
          mqttSaveBlockStart += MQTT_SAVE_BLOCK_SIZE;
          i = 0;
          break;
        }
      }
      if (i == 50) {
        mqttClient.unsubscribe(mqttTopic);
        printfTopicS("Mqtt readback %c failed (time-out), init data ..", mqttSaveTopicChar);
        resetDataStructures();
        initDataRTC();
        return;
      }
    } else {
      printfTopicS("Subscribe failed, init data ..");
      resetDataStructures();
      initDataRTC();
      return;
    }
  }
}

uint16_t mqttDeleted = 0;
uint16_t mqttDeleteDetected = 0;
uint16_t mqttDeleteOverrun = 0;
uint8_t deleteSpecific = 0;
uint8_t mqttDeletingMax = 0;
uint8_t mqttDeletePrefixLength = 0;

#define SUBSCRIBE_TOPIC_LEN 100
char deleteSubscribeTopic[ SUBSCRIBE_TOPIC_LEN ];

void mqttSubscribeToDelete(bool specific) {
  // subscribe to homeassistant
  mqttDeleted = 0;
  mqttDeleteDetected = 0;
  mqttDeleteOverrun = 0;
#define DUAL_STRING_FOR_DEVICE_NAME_IF_USED EE.useDeviceNameInTopic ? EE.deviceName : "", EE.useDeviceNameInTopic ? "/" : ""
#define DUAL_STRING_FOR_BRIDGE_NAME_IF_USED EE.useBridgeNameInTopic ? EE.bridgeName : "", EE.useBridgeNameInTopic ? "/" : ""
  if (specific) {
    switch (mqttDeleting) {
      case          1 ... DEL0                 : snprintf_P(deleteSubscribeTopic, SUBSCRIBE_TOPIC_LEN, PSTR("%s/%c/%s%s%s%s#"), EE.mqttPrefix, haDeleteCat[ mqttDeleting - 1 ], DUAL_STRING_FOR_DEVICE_NAME_IF_USED, DUAL_STRING_FOR_BRIDGE_NAME_IF_USED);
                                                 break;
      case (1 + DEL0) ... (DEL0 + DEL1)        : snprintf_P(deleteSubscribeTopic, SUBSCRIBE_TOPIC_LEN, PSTR("%s/%s/%s%s#"), EE.haConfigPrefix, haPrefixString[ mqttDeleting - DEL0 ], DUAL_STRING_FOR_BRIDGE_NAME_IF_USED);
                                                 break;
      case (4 + DEL1) ... (DEL0 + DEL1 + DEL2) : snprintf_P(deleteSubscribeTopic, SUBSCRIBE_TOPIC_LEN, PSTR("%s/P/%s%s%s%s%s"), EE.mqttPrefix, DUAL_STRING_FOR_DEVICE_NAME_IF_USED, DUAL_STRING_FOR_BRIDGE_NAME_IF_USED, haDeleteString[ mqttDeleting - DEL0 - DEL1 - 1 ]);
                                                 break;
    }
    mqttDeletePrefixLength = strlen(deleteSubscribeTopic) - 2;
    mqttDeletingMax = DEL0 + DEL1 + DEL2;
  } else {
    switch (mqttDeleting) {
      case                           1 ... DEL0                      : snprintf_P(deleteSubscribeTopic, SUBSCRIBE_TOPIC_LEN, PSTR("%s/%c/#"), EE.mqttPrefix, haDeleteCat[ mqttDeleting - 1 ], EE.deviceName, EE.bridgeName);
                                                                       mqttDeletePrefixLength = strlen(deleteSubscribeTopic) - 2;
                                                                       break;
      case (1 + DEL0)                  ... (DEL0 + DEL1)             : snprintf_P(deleteSubscribeTopic, SUBSCRIBE_TOPIC_LEN, PSTR("%s/%s/#"), EE.haConfigPrefix, haPrefixString[ mqttDeleting - DEL0 ]);
                                                                       mqttDeletePrefixLength = strlen(deleteSubscribeTopic) - 2;
                                                                       break;
      case (1 + DEL0 + DEL1)           ... (DEL0 + DEL1 + DEL2)      : snprintf_P(deleteSubscribeTopic, SUBSCRIBE_TOPIC_LEN, PSTR("%s/P/+/+/%s"), EE.mqttPrefix, haDeleteString[ mqttDeleting - DEL0 - DEL1 - 1]);
                                                                       mqttDeletePrefixLength = strlen(EE.mqttPrefix) + 2;
                                                                       break;
      case (1 + DEL0 + DEL1 + DEL2)    ... (DEL0 + DEL1 + 2 * DEL2)  : snprintf_P(deleteSubscribeTopic, SUBSCRIBE_TOPIC_LEN, PSTR("%s/P/+/%s"), EE.mqttPrefix, haDeleteString[ mqttDeleting - DEL0 - DEL1 - DEL2 - 1]);
                                                                       mqttDeletePrefixLength = strlen(EE.mqttPrefix) + 2;
                                                                       break;
      case (1 + DEL0 + DEL1 + 2 * DEL2) ... (DEL0 + DEL1 + 3 * DEL2) : snprintf_P(deleteSubscribeTopic, SUBSCRIBE_TOPIC_LEN, PSTR("%s/P/%s"), EE.mqttPrefix, haDeleteString[ mqttDeleting - DEL0 - DEL1  - 2 * DEL2 - 1]);
                                                                       mqttDeletePrefixLength = strlen(EE.mqttPrefix) + 2;
                                                                       break;
    }
    mqttDeletingMax = DEL0 + DEL1 + 3 * DEL2;
  }
  printfTopicS("Deleting step %i/%i subscribing to %s", mqttDeleting, mqttDeletingMax, deleteSubscribeTopic);
  mqttClient.subscribe(deleteSubscribeTopic, MQTT_QOS_DELETE);
}

uint16_t mqttUnsubscribeToDelete() {
  printfTopicS("Deleted/detected %i/%i overrun %i", mqttDeleted, mqttDeleteDetected, mqttDeleteOverrun);
  mqttClient.unsubscribe(deleteSubscribeTopic);
  // printfTopicS("Unsubscribing from %s", deleteSubscribeTopic);
  if (!mqttDeleteOverrun) {
    if (++mqttDeleting > mqttDeletingMax) mqttDeleting = 0;
  }
  return (mqttDeleting ? 1 : 0);
}

void loadData() {
  loadMQTT(); // loads from MQTT, inits if necessary
  loadRTC();  // overwrites data from RTC, but only if doubleResetData signature matches and version/length match
}

static byte throttle = 1;
static byte throttleValue = THROTTLE_VALUE;

void mqttSubscribe() {
  saveTopic();
  // subscribe to P1P2/W and P1P2/W/<bridgename>
  topicCharGeneric('W');
  int result;
  result = mqttClient.subscribe(mqttTopic, MQTT_QOS_CONTROL);
  printfTopicS("Subscribed to %s result %d", mqttTopic, result);
  topicCharSpecific('W');
  result = mqttClient.subscribe(mqttTopic, MQTT_QOS_CONTROL);
  printfTopicS("Subscribed to %s result %d", mqttTopic, result);

  restoreTopic();

  // subscribe to homeassistant/status
  result = mqttClient.subscribe("homeassistant/status", MQTT_QOS_CONTROL);
  printfTopicS("Subscribed to homeassistant/status");

#ifdef E_SERIES
  // subscribe to electricity meter topics
  if (EE.mqttElectricityPower[0]) {
    result = mqttClient.subscribe(EE.mqttElectricityPower, MQTT_QOS_EMETER);
    printfTopicS("Subscribed to %s", EE.mqttElectricityPower);
  }
  if (EE.mqttElectricityTotal[0]) {
    result = mqttClient.subscribe(EE.mqttElectricityTotal, MQTT_QOS_EMETER);
    printfTopicS("Subscribed to %s", EE.mqttElectricityTotal);
  }
  if (EE.mqttBUHpower[0]) {
    result = mqttClient.subscribe(EE.mqttBUHpower, MQTT_QOS_EMETER);
    printfTopicS("Subscribed to %s", EE.mqttBUHpower);
  }
  if (EE.mqttBUHtotal[0]) {
    result = mqttClient.subscribe(EE.mqttBUHtotal, MQTT_QOS_EMETER);
    printfTopicS("Subscribed to %s", EE.mqttBUHtotal);
  }
#endif /* E_SERIES */

}

void onMqttConnect(bool sessionPresent) {
  // postpone to main loop mqttSubscribe();
  mqttConnected = 2;
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  delayedPrintfTopicS("onMqttDisconnect() reason %d", reason);
  Mqtt_disconnects++;
  mqttConnected = 0;
}

// WiFimanager uses Serial output for debugging purposes - messages on serial output start with an asterisk,
// these messages are routed to the Arduino Uno/P1P2Monitor, which is OK as
// P1P2Monitor on Arduino/ATMega ignores or copies any serial input starting with an asterisk

void WiFiManagerConfigModeCallback (WiFiManager *myWiFiManager) {
  Serial_println(F("* Entered WiFiManager config mode"));
  Serial_print(F("* IP address of AP is "));
  Serial_println(WiFi.softAPIP());
  Serial_print(F("* SSID of AP is "));
  Serial_println(myWiFiManager->getConfigPortalSSID());
}

// WiFiManager callback notifying us of the need to save config to EEPROM (including mqtt parameters)
void WiFiManagerSaveConfigCallback () {
  Serial_println(F("* Should save config"));
  delayedPrintfTopicS("SaveConfigCallback");
  shouldSaveConfig = true;
}

uint32_t milliInc = 0;
uint32_t prevMillis = 0; //millis();
static uint32_t reconnectTime = 0;
static uint32_t throttleStepTime = 0;

void ATmega_dummy_for_serial() {
  // printfTopicS("Two dummy lines to ATmega.");
  Serial.print(F(SERIAL_MAGICSTRING));
  Serial.println(F("* Dummy line 1."));
  Serial.print(F(SERIAL_MAGICSTRING));
  Serial.println(F("* Dummy line 2."));
}

char readBuffer[RB];
static char* rb_buffer = readBuffer;
static uint16_t serial_rb = 0;
static int c;
static byte ESP_serial_input_Errors_Data_Short = 0;
#ifdef MHI_SERIES
static byte ESP_serial_input_Errors_CS = 0;
#else /* MHI_SERIES */
static byte ESP_serial_input_Errors_CRC = 0;
#endif /* MHI_SERIES */
static byte ignoreRemainder = 2; // first line from serial input ignored - robustness
static uint32_t ATmega_uptime_prev = 0;
byte fallback = 0;

void checkParam(void) {
  for (byte i = 0; i < PARAM_NR; i++) {
    switch (paramType[i]) {
      case P_STRING : // check length
                      {
                        byte j = 0;
                        while (j < paramMax[i]) {
                          if (!*(paramLocation[i] + j)) break;
                          j++;
                        }
                        if (j == paramMax[i]) {
                          *(paramLocation[i] + j - 1) = '\0';
                          printfTopicS("Error: missing \\0 in %s, replaced last chr by null, becomes %s", paramName[i], paramLocation[i]);
                          break;
                        }
                      }
                      break;
      case P_INTdiv100:
                      // check value
                      {
                        int32_t pValue;
                        switch (paramSize[i]) { // uint
                          case 4 : pValue = *(int32_t*) paramLocation[i];
                                   break;
                          case 2 : pValue = *(int16_t*) paramLocation[i];
                                   break;
                          default:
                          case 1 : pValue = *(int8_t*) paramLocation[i];
                                   break;
                        }
                        if ((pValue < -paramMax[i]) || (pValue > paramMax[i])) {
                            printfTopicS("Error: parameter %s value %1.2f out of allowed range (%1.2f-%1.2f)", paramName[i], pValue * 0.01, -(paramMax[i] * 0.01), paramMax[i] * 0.01);
                        }
                      }
                      break;
      case P_BOOL :
      case P_HEX  :
      case P_UINT : // check value
                      {
                        uint32_t pValue;
                        switch (paramSize[i]) { // uint
                          case 4 : pValue = *(uint32_t*) paramLocation[i];
                                   break;
                          case 2 : pValue = *(uint16_t*) paramLocation[i];
                                   break;
                          default:
                          case 1 : pValue = *(uint8_t*) paramLocation[i];
                                   break;
                        }
                        if (pValue > paramMax[i]) {
                          if (paramType[i] == P_HEX) {
                            // HEX
                            printfTopicS("Error: parameter %s value %02X out of allowed range (0-%02X)", paramName[i], pValue, paramMax[i]);
                          } else {
                            printfTopicS("Error: parameter %s value %i out of allowed range (0-%i)", paramName[i], pValue, paramMax[i]);
                          }
                        }
                      }
                      break;
      default       : printfTopicS("Error: unknown parameter type for parameter %i", i);
                      break;
    }
  }
}

void printModifyParam(byte paramNr, bool modParam = false, int32_t newValue = 0, char* newString = 0) {
  if (modParam) {
    EE_dirty = 1;
    pseudo0F = 9;
  }
  byte wr = 0;
  byte boot = 0;

  uint32_t outputModeOld = EE.outputMode;
  if (paramType[paramNr] == P_STRING) {
    if (newString && modParam) {
      if (strlen(newString) + 1 > paramMax[paramNr]) {
        printfTopicS("Maximum string length for %s is %d, argument entered %s is longer (%d)", paramName[paramNr], paramMax[paramNr] - 1, newString, strlen(newString));
        return;
      }
      if ((paramNr == 20)  && (newString[0] != '_')) {
        printfTopicS("Telnet unlock string must start with _");
        return;
      }
      printfTopicS("P%02i: %s changed from %s to %s", paramNr, paramName[paramNr], paramLocation[paramNr], newString);
      strlcpy(paramLocation[paramNr], newString, paramMax[paramNr]);
      wr = 1;
    } else {
      if (!paramLocation[paramNr][0]) {
        printfTopicS("P%02i: %s is <empty string>",       paramNr, paramName[paramNr]);
      } else {
        printfTopicS("P%02i: %s is %s",                   paramNr, paramName[paramNr], paramLocation[paramNr]);
      }
    }
  } else if (paramType[paramNr] == P_HEX) {
    if (modParam && ((newValue < 0) || (newValue > paramMax[paramNr]))) {
      printfTopicS("Range for %s is 0x0-0x%X, argument entered is 0x%X", paramName[paramNr], paramMax[paramNr], newValue);
      return;
    }
    switch (paramSize[paramNr]) { // hex
      case 4 : if (modParam) {
                 printfTopicS("P%02i: %s changed from 0x%08X to 0x%08X", paramNr, paramName[paramNr], *(uint32_t*)paramLocation[paramNr], newValue);
                 *(uint32_t*) paramLocation[paramNr] = newValue;
               } else {
                 printfTopicS("P%02i: %s is 0x%08X", paramNr, paramName[paramNr], *(uint32_t*) paramLocation[paramNr]);
               }
               break;
      case 3 : if (modParam) {
                 printfTopicS("P%02i: %s changed from 0x%06X to 0x%06X", paramNr, paramName[paramNr], *(uint32_t*)paramLocation[paramNr], newValue);
                 *(uint32_t*) paramLocation[paramNr] = newValue;
               } else {
                 printfTopicS("P%02i: %s is 0x%06X", paramNr, paramName[paramNr], *(uint32_t*) paramLocation[paramNr]);
               }
               break;
      case 2 : if (modParam) {
                 printfTopicS("P%02i: %s changed from 0x%04X to 0x%04X", paramNr, paramName[paramNr], *(uint16_t*)paramLocation[paramNr], newValue);
                 *(uint16_t*) paramLocation[paramNr] = newValue;
               } else {
                 printfTopicS("P%02i: %s is 0x%04X", paramNr, paramName[paramNr], *(uint16_t*) paramLocation[paramNr]);
               }
               break;
      case 1 :
      default: if (modParam) {
                 printfTopicS("P%02i: %s changed from 0x%02X to 0x%02X", paramNr, paramName[paramNr], *(uint8_t*)paramLocation[paramNr], newValue);
                 *(uint8_t*) paramLocation[paramNr] = newValue;
               } else {
                 printfTopicS("P%02i: %s is 0x%02X", paramNr, paramName[paramNr], *(uint8_t*) paramLocation[paramNr]);
               }
               break;
    }
  } else if (paramType[paramNr] == P_INTdiv100) {
    if (modParam && ((newValue < -((int32_t) paramMax[paramNr])) || (newValue > paramMax[paramNr]))) {
      printfTopicS("Range for %s is -%1.2f - %1.2f, argument entered is %1.2f", paramName[paramNr], paramMax[paramNr] * 0.01, paramMax[paramNr] * 0.01, newValue * 0.01);
      return;
    }
    switch (paramSize[paramNr]) { // hex
      case 4 : // fall-through
      case 3 : // fall-through
               printfTopicS(" INTdiv100 >2byte not supported");
               break;
      case 2 : // fall-through
               if (modParam) {
                 printfTopicS("P%02i: %s changed from %1.2f to %1.2f", paramNr, paramName[paramNr], *(int16_t*)paramLocation[paramNr] * 0.01, newValue * 0.01);
                 *(int16_t*) paramLocation[paramNr] = (int16_t) newValue;
               } else {
                 printfTopicS("P%02i: %s is %1.2f", paramNr, paramName[paramNr], *(int16_t*) paramLocation[paramNr] * 0.01);
               }
               break;
      case 1 :
      default: if (modParam) {
                 printfTopicS("P%02i: %s changed from %1.2f to %1.2f", paramNr, paramName[paramNr], *(int8_t*)paramLocation[paramNr] * 0.01, newValue * 0.01);
                 *(int8_t*) paramLocation[paramNr] = (int8_t) newValue;
               } else {
                 printfTopicS("P%02i: %s is %1.2f", paramNr, paramName[paramNr], *(int8_t*) paramLocation[paramNr] * 0.01);
               }
               break;
    }
  } else { // P_UINT
    if (modParam && ((newValue < 0) || (newValue > paramMax[paramNr]))) {
      printfTopicS("Range for %s is 0-%i, argument entered is %i", paramName[paramNr], paramMax[paramNr], newValue);
      return;
    }
    switch (paramSize[paramNr]) { // uint
      case 4 : if (modParam) {
                 printfTopicS("P%02i: %s changed from %i to %i", paramNr, paramName[paramNr], *(uint32_t*) paramLocation[paramNr], newValue);
                 *(uint32_t*) paramLocation[paramNr] = (uint32_t) newValue;
               } else {
                 printfTopicS("P%02i: %s is %i", paramNr, paramName[paramNr], *(uint32_t*) paramLocation[paramNr]);
               }
               break;
      case 2 : if (modParam) {
                 printfTopicS("P%02i: %s changed from %i to %i", paramNr, paramName[paramNr], *(uint16_t*) paramLocation[paramNr], newValue);
                 *(uint16_t*) paramLocation[paramNr] = (uint16_t)(uint32_t) newValue;
               } else {
                 printfTopicS("P%02i: %s is %i", paramNr, paramName[paramNr], *(uint16_t*) paramLocation[paramNr]);
               }
               break;
      default:
      case 1 : if (modParam) {
                 printfTopicS("P%02i: %s changed from %i to %i", paramNr, paramName[paramNr], *(uint8_t*) paramLocation[paramNr], newValue);
                 *(uint8_t*) paramLocation[paramNr] = (uint8_t)(uint32_t) newValue;
               } else {
                 printfTopicS("P%02i: %s is %i", paramNr, paramName[paramNr], *(uint8_t*) paramLocation[paramNr]);
               }
               break;
    }
  }
  if (ignoreSerial) {
    rb_buffer = readBuffer;
    serial_rb = 0;
  }
  if (modParam) {
    switch (paramNr) {
      case PARAM_BRIDGE_NAME              :
      case PARAM_DEVICE_NAME              : strlcpy(EE.mdnsName, EE.deviceName, MDNS_NAME_LEN);
                                            strncpy_P(EE.mdnsName + strlen(EE.mdnsName), PSTR("_"), 2);
                                            strlcpy(EE.mdnsName + strlen(EE.mdnsName), EE.bridgeName, MDNS_NAME_LEN - strlen(EE.mdnsName));
                                            strlcpy(EE.mqttClientName, EE.deviceName, MQTT_CLIENTNAME_LEN);
                                            strncpy_P(EE.mqttClientName + strlen(EE.mqttClientName), PSTR("_"), 2);
                                            strlcpy(EE.mqttClientName + strlen(EE.mqttClientName), EE.bridgeName, MQTT_CLIENTNAME_LEN - strlen(EE.mqttClientName));
                                            strlcpy(EE.otaHostname, EE.deviceName, OTA_HOSTNAME_LEN);
                                            strncpy_P(EE.otaHostname + strlen(EE.otaHostname), PSTR("_"), 2);
                                            strlcpy(EE.otaHostname + strlen(EE.otaHostname), EE.bridgeName, OTA_HOSTNAME_LEN - strlen(EE.otaHostname));
                                            // fall-through
      case PARAM_USE_BRIDGE_NAME_IN_TOPIC :
      case PARAM_USE_DEVICE_NAME_IN_TOPIC :
      case PARAM_MQTT_PREFIX              : printfTopicS("Save (D5) and reset (D0) required to make changes effective");
                                            buildMqttTopic(); // lazy coding, is only needed for mqttPrefix, useDeviceNameInTopic, deviceName, useBridgeNameInTopic, bridgeName
                                            break;
      default                             : break;
    }
  }


#ifdef E_SERIES
  if (modParam && (paramNr == PARAM_RT_OFFSET)) {
    registerUnseenByte(0x00, 0x11, 1);
    registerUnseenByte(0x40, 0x11, 13);
  }
  if (modParam && (paramNr == PARAM_R1T_OFFSET)) {
    registerUnseenByte(0x40, 0x11, 9);
  }
  if (modParam && (paramNr == PARAM_R2T_OFFSET)) {
    registerUnseenByte(0x40, 0x11, 1);
  }
  if (modParam && (paramNr == PARAM_R4T_OFFSET)) {
    registerUnseenByte(0x40, 0x11, 7);
  }
  if ((modParam && (paramNr == PARAM_EP)) || (modParam && (paramNr == PARAM_EC))) {
    pseudo0B = 9;
    EE.D13 = 0; // cancel automatic counter search
    pseudo0F = 9;
  }
  if (modParam && (paramNr == PARAM_HA_SETUP)) unSeen();
#endif /* E_SERIES */

#ifdef F_SERIES
  if (modParam && ((paramNr == PARAM_SETPOINT_COOLING_MIN) || (paramNr == PARAM_SETPOINT_COOLING_MAX))) {
    registerUnseenByte(0x00, 0x38, 4);
    registerUnseenByte(0x40, 0x3B, 4);
  }
  if (modParam && ((paramNr == PARAM_SETPOINT_HEATING_MIN) || (paramNr == PARAM_SETPOINT_HEATING_MAX))) {
    registerUnseenByte(0x00, 0x38, 8);
    registerUnseenByte(0x40, 0x3B, 8);
  }
#endif /* F_SERIES */
  if (modParam && ((paramNr == PARAM_USE_TZ) || (paramNr == PARAM_USER_TZ))) configTZ();
  delay(10);
}

void reconnectMQTT() {
  // TODO enable in future WiFiManager version:
  // if (WiFi.isConnected()) printfTopicS("Connected to WiFi SSID %s", wifiManager.getWiFiSSID());
  // disable ATmega serial output on v1.2
  digitalWrite(ATMEGA_SERIAL_ENABLE, LOW);
  if (mqttClient.connected()) {
    printfTopicS("MQTT will be disconnected now");
    clientPublishMqttChar('L', MQTT_QOS_WILL, MQTT_RETAIN_WILL, "offline");
    delay(100);
    // unsubscribe topics here before disconnect?
    mqttClient.disconnect();
    delay(1000);
  }
  while (mqttClient.connected()) {
    delayedPrintfTopicS("MQTT Client is still connected");
    delay(500);
  }
  delayedPrintfTopicS("MQTT Client is disconnected");
  // re-enable ATmega serial output on v1.2
  digitalWrite(ATMEGA_SERIAL_ENABLE, HIGH);
  mqttClient.setServer(EE.mqttServer, EE.mqttPort);
  mqttClient.setCredentials((EE.mqttUser[0] == '\0') ? 0 : EE.mqttUser, (EE.mqttPassword[0] == '\0') ? 0 : EE.mqttPassword);
  mqttClient.setClientId(EE.mqttClientName);
  printfTopicS("Trying to connect to MQTT server (first attempt may fail)"); // library issue?
  Mqtt_reconnectDelay = 0;
  Mqtt_disconnectTime = 0;
  fallback = 0;
  if (EE.outputMode & 0x800) { // only if 0x0800
    throttleStepTime = espUptime + THROTTLE_STEP_S;
    throttleValue = THROTTLE_VALUE;
    pseudo0F = 9;
    resetDataStructures();
  }
  delay(500);
  reconnectTime = espUptime;
}

WiFiManager wifiManager;
IPAddress local_ip;

#ifdef E_SERIES
void configOffset(void) {
  registerUnseenByte(0x00, 0x11, 1);
  registerUnseenByte(0x40, 0x11, 13);
  registerUnseenByte(0x40, 0x11, 9);
  registerUnseenByte(0x40, 0x11, 1);
  registerUnseenByte(0x40, 0x11, 7);
}
#endif /* E_SERIES */

#ifdef F_SERIES
void configMinMax(void) {
  registerUnseenByte(0x00, 0x38, 4);
  registerUnseenByte(0x40, 0x3B, 4);
  registerUnseenByte(0x00, 0x38, 8);
  registerUnseenByte(0x40, 0x3B, 8);
}
#endif /* F_SERIES */

extern "C" uint32_t _EEPROM_start;
#define EEPROM_sector (((uint32_t)&_EEPROM_start - 0x40200000) / SPI_FLASH_SEC_SIZE)
#define EEPROM_size ((sizeof(EE) + 3) & 0xFFFFFFFC)

void saveEEPROM()
{
  if (ESP.flashEraseSector(EEPROM_sector)) {
    if (!ESP.flashWrite(EEPROM_sector * SPI_FLASH_SEC_SIZE, reinterpret_cast<uint32_t*>((uint8_t*) &EE), EEPROM_size)) {
      printfTopicS("EEPROM write failed");
    }
  } else {
    printfTopicS("EEPROM erase failed");
  }
  EE_dirty = 0;
  pseudo0F = 9;
}

void loadEEPROM() {
  if (EEPROM_size > SPI_FLASH_SEC_SIZE) {
    delayedPrintfTopicS("EEPROM > flash page");
    return;
  }
  if (!ESP.flashRead(EEPROM_sector * SPI_FLASH_SEC_SIZE, reinterpret_cast<uint32_t*>((uint8_t*) &EE), EEPROM_size)) delayedPrintfTopicS("EEPROM read failed");
  EE_dirty = 0;
  pseudo0F = 9;

  if (!strcmp(EE.signature, EEPROM_SIGNATURE_OLD4)) {
    delayedPrintfTopicS("Converting EEPROM data");
    EE.EE_version = 0;
    strlcpy(EE.signature, EEPROM_SIGNATURE_NEW, sizeof(EE.signature));
    EE.mqttServer[ MQTT_SERVER_LEN - 1 ] = '\0'; // just in case
    EE.outputMode = INIT_OUTPUTMODE; // change outputMode definition, use new version
  }
  if (strcmp(EE.signature, EEPROM_SIGNATURE_NEW)) {
    if (strncmp(EE.signature, EEPROM_SIGNATURE_COMMON, strlen(EEPROM_SIGNATURE_COMMON))) {
      delayedPrintfTopicS("Init MQTT credentials");
      strlcpy(EE.mqttUser,     MQTT_USER,            sizeof(EE.mqttUser));
      strlcpy(EE.mqttPassword, MQTT_PASSWORD,        sizeof(EE.mqttPassword));
      strlcpy(EE.mqttServer,   MQTT_SERVER,          sizeof(EE.mqttServer));
      if (sscanf(MQTT_PORT, "%i", &EE.mqttPort) != 1) EE.mqttPort = 1883;
    } else {
      delayedPrintfTopicS("Maintain MQTT credentials");
      EE.mqttServer[ MQTT_SERVER_LEN - 1 ] = '\0';
      EE.mqttUser[ MQTT_USER_LEN - 1 ] = '\0';
      EE.mqttPassword[ MQTT_PASSWORD_LEN - 1 ] = '\0';
    }
    delayedPrintfTopicS("Init EEPROM with NEW signature");
    strlcpy(EE.signature,    EEPROM_SIGNATURE_NEW, sizeof(EE.signature));
    EE.EE_version = 0;
    EE.outputMode = INIT_OUTPUTMODE;
    EE.outputFilter = INIT_OUTPUTFILTER;
    EE.ESPhwID = INIT_ESP_HW_ID;
    EE.noWiFi = INIT_NOWIFI;
    EE.useStaticIP = INIT_USE_STATIC_IP;
    EE.static_ip[0] = '\0';
    EE.static_gw[0] = '\0';
    EE.static_nm[0] = '\0';
  }
  if (EE.EE_version < 1) {
    // delayedPrintfTopicS("Upgrade EEPROM_version to 1");
    // EE.EE_version = 1;
    strlcpy(EE.wifiManager_SSID, WIFIMAN_SSID1, WIFIMAN_SSID_LEN);
    strlcpy(EE.wifiManager_password, WIFIMAN_PASSWORD, WIFIMAN_PASSWORD_LEN);
    strlcpy(EE.mdnsName, MDNS_NAME, MDNS_NAME_LEN);
    strlcpy(EE.mqttClientName, MQTT_CLIENTNAME, MQTT_CLIENTNAME_LEN);
    strlcpy(EE.mqttPrefix, MQTT_PREFIX, MQTT_PREFIX_LEN);
    strlcpy(EE.deviceName, DEVICE_NAME, DEVICE_NAME_LEN);
    strlcpy(EE.bridgeName, BRIDGE_NAME, BRIDGE_NAME_LEN);
    strlcpy(EE.haConfigPrefix, HACONFIG_PREFIX, HACONFIG_PREFIX_LEN);
    strlcpy(EE.telnetMagicword, TELNET_MAGICWORD, TELNET_MAGICWORD_LEN);
    strlcpy(EE.otaHostname, OTA_HOSTNAME, OTA_HOSTNAME_LEN);
    strlcpy(EE.otaPassword, OTA_PASSWORD, OTA_PASSWORD_LEN);
    EE.useDeviceNameInTopic = USE_DEVICE_NAME_IN_TOPIC;
    EE.useDeviceNameInEntityName = USE_DEVICE_NAME_IN_ENTITY_NAME;
    EE.useBridgeNameInDeviceIdentity = USE_BRIDGE_NAME_IN_DEVICE_IDENTITY;
    EE.useBridgeNameInTopic = USE_BRIDGE_NAME_IN_TOPIC;
    EE.useBridgeNameInEntityName = USE_BRIDGE_NAME_IN_ENTITY_NAME;
    EE.useDeviceNameInDeviceNameHA = USE_DEVICE_NAME_IN_DEVICE_NAME_HA;
    EE.useBridgeNameInDeviceNameHA = USE_BRIDGE_NAME_IN_DEVICE_NAME_HA;
    EE.useDeviceNameInDeviceIdentity = USE_DEVICE_NAME_IN_DEVICE_IDENTITY;
    strlcpy(EE.deviceShortNameHA, INIT_DEVICE_SHORT_NAME_HA, DEVICE_SHORT_NAME_HA_LEN);
#ifdef W_SERIES
    strlcpy(EE.meterURL, INIT_METER_URL, MQTT_INPUT_TOPIC_LEN);
#endif /* W_SERIES */
#ifdef E_SERIES
    strlcpy(EE.mqttElectricityPower, MQTT_ELECTRICITY_POWER, MQTT_INPUT_TOPIC_LEN);
    strlcpy(EE.mqttElectricityTotal, MQTT_ELECTRICITY_TOTAL, MQTT_INPUT_TOPIC_LEN);
    strlcpy(EE.mqttBUHpower, MQTT_ELECTRICITY_BUH_POWER, MQTT_INPUT_TOPIC_LEN);
    strlcpy(EE.mqttBUHtotal, MQTT_ELECTRICITY_BUH_TOTAL, MQTT_INPUT_TOPIC_LEN);
    EE.R1Toffset = INIT_R1T_OFFSET;
    EE.R2Toffset = INIT_R2T_OFFSET;
    EE.R4Toffset = INIT_R4T_OFFSET;
    EE.RToffset = INIT_RT_OFFSET;
    EE.useTotal = INIT_USE_TOTAL; // 0=use compressor-heating for COP-lifetime, 1=use total for COP-lifetime
    configOffset();
#endif /* E_SERIES */
#define RESERVED_TEXT ""
    strlcpy(EE.reservedText, RESERVED_TEXT, RESERVED_LEN);
  }
  if (EE.EE_version < 2) {
    // delayedPrintfTopicS("Upgrade EEPROM_version to 2");
    // EE.EE_version = 2;
#ifdef E_SERIES
    EE.useR1T = 0;
#endif /* E_SERIES */
  }
  if (EE.EE_version < 3) {
    // delayedPrintfTopicS("Upgrade EEPROM_version to 3");
    // EE.EE_version = 3;
    EE.useTZ = 2;
    EE.userTZ[0] = '\0';
  }
  if (EE.EE_version < 4) {
    // delayedPrintfTopicS("Upgrade EEPROM_version to 4");
    // EE.EE_version = 4;
#ifdef E_SERIES
    EE.electricityConsumedCompressorHeating1 = 0;
    EE.energyProducedCompressorHeating1 = 0;
    EE.D13 = 0x03;
    EE.haSetup = 1;
#endif /* E_SERIES */
  }
  if (EE.EE_version < 5) {
    // delayedPrintfTopicS("Upgrade EEPROM_version to 5");
    // EE.EE_version = 5;
#ifdef E_SERIES
    EE.minuteTimeStamp = 0;
#endif /* E_SERIES */
    // EE.EE_size = sizeof(EE); // EE_size never used - and not really needed, relying on EE_version and EE.signature
  }
  if (EE.EE_version < 6) {
    // delayedPrintfTopicS("Upgrade EEPROM_version to 6");
    // EE.EE_version = 6;
#ifdef F_SERIES
    EE.setpointCoolingMin = INIT_SETPOINT_COOLING_MIN;
    EE.setpointCoolingMax = INIT_SETPOINT_COOLING_MAX;
    EE.setpointHeatingMin = INIT_SETPOINT_HEATING_MIN;
    EE.setpointHeatingMax = INIT_SETPOINT_HEATING_MAX;
    configMinMax();
#endif /* F_SERIES */
    EE.EE_size = sizeof(EE); // EE_size never used - and not really needed, relying on EE_version and EE.signature
  }
#ifdef H_SERIES
  if (EE.EE_version < 7) {
    EE.hitachiModel = INIT_HITACHI_MODEL;
  }
  if (EE.EE_version < 7) {
    delayedPrintfTopicS("Upgrade EEPROM_version to 7");
    EE.EE_version = 7;
    saveEEPROM();
  }
#else
  if (EE.EE_version < 6) {
    delayedPrintfTopicS("Upgrade EEPROM_version to 6");
    EE.EE_version = 6;
    saveEEPROM();
  }
#endif /* H_SERIES */
  delayedPrintfTopicS("Loaded EEPROM_version %i", EE.EE_version);
}

void handleCommand(char* cmdString) {
// handles a single command (not necessarily '\n'-terminated) received via telnet or MQTT (P1P2/W)
// most of these messages are fowarded over serial to P1P2Monitor on ATmega
// some messages are (also) handled on the ESP
  int temp = 0;
  int temp2 = 0;
  char tempstring[ PARAM_MAX_LEN ];
  byte temphex = 0;
  int n;
  int result = 0;
  byte valueStart = 0;

  switch (cmdString[0]) {
    case '%': printfTopicS("Too long:");
              // fall-through
    case '<': printfTopicS("< %s", cmdString + 1);
              break;
    case '-': //printfTopicS("Deleting topic %s", cmdString + 1);
              mqttDeleted++;
              clientPublishMqtt(cmdString + 1, MQTT_QOS_DELETE, MQTT_RETAIN_DELETE /* nullmessage */);
              break;
    case 'a': // reset ATmega
    case 'A': printfTopicS("Hard resetting ATmega...");
              digitalWrite(RESET_PIN, LOW);
              pinMode(RESET_PIN, OUTPUT);
              delay(1);
              pinMode(RESET_PIN, INPUT);
              digitalWrite(RESET_PIN, HIGH);
              delay(200);
              ignoreRemainder = 2;
              ATmega_dummy_for_serial();
              ATmega_uptime_prev = 0;
              break;
    case 'd': // Various options: reset ESP, data structures, factory reset
    case 'D': n = (sscanf((const char*) (cmdString + 1), "%d", &temp) == 1);
              switch ((n > 0) ? temp : 99) {
                case 11: printfTopicS("Init Data, reset Data, no restart");
                         initDataRTC();
                         resetDataStructures();
                         break;
                case 0 : if (factoryReset) {
                           printfTopicS("Init Data, reset Data...");
                           initDataRTC();
                           resetDataStructures();
                         }
                         // fall-through
                         printfTopicS("Restarting ESP...");
                         saveData();
                         clientPublishMqttChar('L', MQTT_QOS_WILL, MQTT_RETAIN_WILL, "offline");
                         saveRebootReason(REBOOT_REASON_D0);
                         // disable ATmega serial output on v1.2
                         digitalWrite(ATMEGA_SERIAL_ENABLE, LOW);
                         delay(500);
                         ESP.restart();
                         delay(100);
                         break;
                case 1 : printfTopicS("Resetting ESP...");
                         clientPublishMqttChar('L', MQTT_QOS_WILL, MQTT_RETAIN_WILL, "offline");
                         saveRebootReason(REBOOT_REASON_D1);
                         // disable ATmega serial output on v1.2
                         digitalWrite(ATMEGA_SERIAL_ENABLE, LOW);
                         delay(100);
                         ESP.reset();
                         delay(100);
                         break;
                case 4 : if (mqttDeleting) {
                           printfTopicS("Please wait until mqtt-delete action is finished");
                           break;
                         }
                         printfTopicS("Reconnecting to MQTT");
                         reconnectMQTT();
                         // fall-through to 3
                case 3 : if (mqttDeleting) {
                           printfTopicS("Please wait until mqtt-delete action is finished");
                           break;
                         }
                         printfTopicS("Resetting data structures, throttle");
                         throttleStepTime = espUptime + THROTTLE_STEP_S;
                         throttleValue = THROTTLE_VALUE;
                         pseudo0F = 9;
                         clientPublishMqttChar('L', MQTT_QOS_WILL, MQTT_RETAIN_WILL, "online");
                         resetDataStructures();
                         break;
                case 5 : saveEEPROM();
                         printfTopicS("Saving modifications to EEPROM");
                         break;
                case 6 : loadEEPROM();
                         pseudo0F = 9;
                         // some changes are not immediately effective, TODO: improve
                         printfTopicS("Loading data from EEPROM (reversing modifications); may require D4 to reconnect to MQTT server and D0 to take full effect");
                         configTZ();
#ifdef E_SERIES
                         configOffset();
#endif /* E_SERIES */
#ifdef F_SERIES
                         configMinMax();
#endif /* F_SERIES */
                         break;
                case 7 : if (EE_dirty) {
                           printfTopicS("Changes pending, rejecting factory reset. To abandon changes and perform factory reset: D6, then retry D7");
                           break;
                         }
                         printfTopicS("Schedule factory reset (except MQTT credentials) on restart (undo: D8 / confirm,clean-RTC/MQTT-data,restart ESP: D0)");
                         strlcpy(EE.signature, EEPROM_SIGNATURE_COMMON, sizeof(EE.signature)); // maintain only MQTT server credentials
                         saveEEPROM();
                         factoryReset = 1;
                         pseudo0F = 9;
                         break;
                case 8 : printfTopicS("Undo factory reset (works only if ESP has not been restarted after D7)");
                         strlcpy(EE.signature, EEPROM_SIGNATURE_NEW, sizeof(EE.signature));
                         saveEEPROM();
                         factoryReset = 0;
                         pseudo0F = 9;
                         break;
                case 9 : printfTopicS("Resetting wifiManager settings (WiFi/MQTT credentials), ESP config, and restarting ESP/wifiManager AP");
                         wifiManager.resetSettings();
                         WiFi.disconnect(true);
                         ESP.eraseConfig();
                         delay(100);
                         ESP.restart();
                         break;
                case 10: unSeen();
                         break;
                case 33: printfTopicS("HA_VALUE_LEN %i MaxSeen %i", HA_VALUE_LEN, haConfigMessageLengthMax);
                         printfTopicS("HA_KEY_LEN %i MaxSeen %i", HA_KEY_LEN, haConfigTopicLengthMax);
                         printfTopicS("EXTRA_AVAILABILITY_STRING_LEN %i MaxSeen %i", EXTRA_AVAILABILITY_STRING_LEN, extraAvailabilityStringLengthMax);
                         break;
                case 12: if (throttleValue) {
                           printfTopicS("Please wait until throttling is ready - then reissue D12");
                         } else {
                           digitalWrite(ATMEGA_SERIAL_ENABLE, LOW);
                           printfTopicS("Deleting own homeassistant and MQTT entities and resetting and rebuilding MQTT topics and HA configuration");
                           mqttDeleting = 1;
                           deleteSpecific = 1;
                           mqttSubscribeToDelete(deleteSpecific);
                           M.R.RTCdataLength = 0; // invalidate RCT data
                           ESP.rtcUserMemoryWrite(RTC_REGISTER, reinterpret_cast<uint32_t *>(&M.R), sizeof(M.R));
                           mqttUnsubscribeTime = espUptime + DELETE_STEP;
                         }
                         break;
#ifdef E_SERIES
                case 13: if ((M.R.electricityConsumedCompressorHeating > 0) && (M.R.energyProducedCompressorHeating > 0)) {
                           printfTopicS("Set consumption/production to last seen counters %i/%i", M.R.electricityConsumedCompressorHeating, M.R.energyProducedCompressorHeating);
                           printfTopicS("Use D5 to save counters in EEPROM");
                           EE.electricityConsumedCompressorHeating1 = M.R.electricityConsumedCompressorHeating;
                           EE.energyProducedCompressorHeating1 = M.R.energyProducedCompressorHeating;
                           pseudo0B = 9;
                           EE_dirty = 1;
                           EE.D13 = 0x00;
                           pseudo0F = 9;
                         } else {
                           EE.D13 = 0x03;
                           pseudo0F = 9;
                           printfTopicS("Scheduling to set consumption/production to counters when seen, initiating C3 to request counters");
                           printfTopicS("Once available, use D5 to save counters in EEPROM");
                           Serial.println(F(SERIAL_MAGICSTRING "C3"));
                         }
                         break;
#endif /* E_SERIES */
                case 14: if (throttleValue) {
                           printfTopicS("Please wait until throttling is ready - then reissue D14");
                         } else {
                           digitalWrite(ATMEGA_SERIAL_ENABLE, LOW);
                           printfTopicS("Deleting ALL homeassistant and MQTT entities and resetting and rebuilding MQTT topics and HA configuration");
                           mqttDeleting = 1;
                           deleteSpecific = 0;
                           mqttSubscribeToDelete(deleteSpecific);
                           M.R.RTCdataLength = 0; // invalidate RCT data
                           ESP.rtcUserMemoryWrite(RTC_REGISTER, reinterpret_cast<uint32_t *>(&M.R), sizeof(M.R));
                           mqttUnsubscribeTime = espUptime + DELETE_STEP;
                         }
                         break;
                case 99: // no argument, fall-through to:
                default: printfTopicS("D0: restart ESP (+initDataRTC/resetData in case of factoryreset)");
                         printfTopicS("D1: reset ESP");
                         printfTopicS("D3: reset-data-structures");
                         printfTopicS("D4: reconnect to MQTT");
                         printfTopicS("D5: save modifications to EEPROM");
                         printfTopicS("D6: reload settings from EEPROM (ignore changes from this session)");
                         printfTopicS("D7: schedule factory reset (except WiFi/MQTT credentials) on restart ESP (to undo: D8, to confirm: D0)");
                         printfTopicS("D8: undo scheduling of factory reset");
                         printfTopicS("D9: reset WiFi/MQTT credentials (WiFiManager) and restart ESP");
                         printfTopicS("D10: unSeen");
                         printfTopicS("D11: initDataRTC, resetData");
                         printfTopicS("D12: delete own and rebuild retained MQTT config/data (deletes old data from own bridge)");
#ifdef E_SERIES
                         printfTopicS("D13: set consumption/production counters for COP before/after bridge installation");
#endif /* E_SERIES */
                         printfTopicS("D14: delete all and rebuild retained MQTT config/data (deletes old data from all bridges)");
                         reportState();
                         break;
              }
              break;
    case 'p': // Set parameter
    case 'P':
              if ((n = sscanf((const char*) (cmdString + 1), "%d%n", &temp, &temp2)) > 0) {
                if ((temp < 0) || (temp >= PARAM_NR)) {
                  printfTopicS("Parameter %i not supported, range 0-%d", temp, PARAM_NR - 1);
                  break;
                }
                byte wr = 0;
                byte boot = 0;
                bool modifyParam = 0;
                float tempf2 = 0.0;
                switch (paramType[temp]) {
                  case P_STRING    : modifyParam = (sscanf((const char*) (cmdString + 1 + temp2), "%" PARAM_MAX_LEN_TXT "s", &tempstring) > 0);
                                     break;
                  case P_HEX       : modifyParam = (sscanf((const char*) (cmdString + 1 + temp2), "%x", &temp2) > 0);
                                     break;
                  case P_INTdiv100 : modifyParam = (sscanf((const char*) (cmdString + 1 + temp2), "%f", &tempf2) > 0);
                                     switch(paramSize[temp]) {
                                       case 1 : temp2 = (tempf2 > 0) ? ((int8_t) (tempf2 * 100 + 0.5)) : -((int8_t) (-tempf2 * 100 + 0.5));
                                                break;
                                       case 2 : temp2 = (tempf2 > 0) ? ((int16_t) (tempf2 * 100 + 0.5)) : -((int16_t) (-tempf2 * 100 + 0.5));
                                                break;
                                       default: printfTopicS("P_INTdiv100 only supported for 1/2-byte params");
                                                modifyParam = 0;
                                                break;
                                     }
                                     break;
                  case P_BOOL      : modifyParam = (sscanf((const char*) (cmdString + 1 + temp2), "%d", &temp2) > 0);
                                     switch(paramSize[temp]) {
                                       case 1 : if (temp2 > 1) {
                                                  printfTopicS("P_BOOL 1-byte max 1");
                                                  modifyParam = 0;
                                                }
                                                break;
                                       default: printfTopicS("P_BOOL only supported for 1-byte params");
                                                modifyParam = 0;
                                                break;
                                     }
                                     break;
                  case P_UINT      : modifyParam = (sscanf((const char*) (cmdString + 1 + temp2), "%d", &temp2) > 0);
                                     switch(paramSize[temp]) {
                                       case 1 : if (temp2 > 255) {
                                                  printfTopicS("P_UINT 1-byte max 255");
                                                  modifyParam = 0;
                                                }
                                                break;
                                       case 2 : if (temp2 > 65535) {
                                                  printfTopicS("P_UINT 2-byte max 65535");
                                                  modifyParam = 0;
                                                }
                                                break;
                                       case 4 : break;
                                       default: printfTopicS("P_INTdiv100 only supported for 1/2/4-byte params");
                                                modifyParam = 0;
                                                break;
                                     }
                                     break;
                  default          : printfTopicS("P_unsupportedType");
                                     break;
                }
                printModifyParam(temp, modifyParam, temp2, tempstring);

                if (modifyParam) {
                  printfTopicS("To make parameter change permanent, use command D5");
                  printfTopicS("Some parameter changes also require a MQTT reconnect (D4) or an ESP restart (D0)");
                } else {
                  switch (temp) {
                    case 33 : printfTopicS("Options for TZ parameter P%2d", temp);
                              printfTopicS("0: use ESP uptime");
                              printfTopicS("1: user-defined TZ (%s)", EE.userTZ);
                              for (byte i = 2; i < NR_PREDEFINED_TZ; i++) {
                                printfTopicS("%i: %s", i, PREDEFINED_TZ[i]);
                              }
                              break;
#ifdef H_SERIES
                    case 35 : printfTopicS("Options for Hitachi model parameter P%2d", temp);
                              printfTopicS("1: (default) Yutaki S");
                              printfTopicS("2: TBD");
                              break;
#endif /* H_SERIES */
                    default : break;
                  }
                }
              } else {
                for (byte i = 0; i < PARAM_NR; i++) {
                  printModifyParam(i, 0);
                }
                reportState();
              }
              break;
    case 'j': // OutputMode
    case 'J': if (sscanf((const char*) (cmdString + 1), "%x", &temp) == 1) {
                printModifyParam(PARAM_JMASK, 1, temp, 0);
              } else {
                printfTopicS("Outputmode 0x%04X is sum of", EE.outputMode);
                printfTopicS("%ix 0x0001 to output raw packet data (excluding pseudo-packets) over mqtt P1P2/R/xxx", EE.outputMode  & 0x01);
                printfTopicS("%ix 0x0002 to output mqtt individual parameter data over mqtt P1P2/P/xxx/#", (EE.outputMode >> 1) & 0x01);
                printfTopicS("%ix 0x0004 to output pseudo packet data over mqtt P1P2/R/xxx", (EE.outputMode >>2) & 0x01);
                printfTopicS("%ix 0x0008 to have mqtt include parameters even if functionality is unknown, warning: easily overloads ATmega/ESP (best to combine this with outputfilter >=1)", (EE.outputMode >> 3) & 0x01); // -> outputUnknown
                printfTopicS("%ix 0x0010 ESP to output raw data over telnet", (EE.outputMode >> 4) & 0x01);
                printfTopicS("%ix 0x0020 to output mqtt individual parameter data over telnet", (EE.outputMode >> 5) & 0x01);
                printfTopicS("%ix 0x0040 ESP to output timing info over telnet", (EE.outputMode >> 6) & 0x01);
                printfTopicS("%ix 0x0080 (reserved)", (EE.outputMode >> 7) & 0x01);
                printfTopicS("%ix 0x0100 to include non-HACONFIG parameters in P1P2/P/# ", (EE.outputMode >> 8) & 0x01);
                printfTopicS("%ix 0x0200 (reserved)", (EE.outputMode >> 9) & 0x01); // -> HAPCONFIG
                printfTopicS("%ix 0x0400 (reserved)", (EE.outputMode >> 10) & 0x01);
                printfTopicS("%ix 0x0800 to restart HA-data communication (and throttling) after MQTT reconnect ", (EE.outputMode >> 11) & 0x01);
                printfTopicS("%ix 0x1000 to output timing data over P1P2/R/xxx (prefix: C/c)", (EE.outputMode >> 12) & 0x01);
                printfTopicS("%ix 0x2000 to output error data over P1P2/R/xxx (prefix: *)", (EE.outputMode >> 13) & 0x01);
                printfTopicS("%ix 0x4000 (reserved)", (EE.outputMode >> 14) & 0x01);
                printfTopicS("%ix 0x8000 to ignore serial input from ATmega", (EE.outputMode >> 15) & 0x01);
              }
              break;
    case 's': // OutputFilter
    case 'S': if (sscanf((const char*) (cmdString + 1), "%d", &temp) == 1) {
                if (temp > 4) temp = 4;
                EE.outputFilter = temp;
                EE_dirty = 1;
                pseudo0F = 9;
              } else {
                temp = 99;
              }
              switch (EE.outputFilter) {
                case 0 : printfTopicS("Outputfilter %s0, all parameters, no filter", (temp != 99) ? "set to " : ""); break;
                case 1 : printfTopicS("Outputfilter %s1, only changed data", (temp != 99) ? "set to " : ""); break;
                case 2 : printfTopicS("Outputfilter %s2, only changed data, excluding temp/flow", (temp != 99) ? "set to " : ""); break;
                case 3 : printfTopicS("Outputfilter %s3, only changed data, excluding temp/flow/time", (temp != 99) ? "set to " : ""); break;
                case 4 : printfTopicS("Outputfilter %s4, only changed data, excluding temp/flow/time/electricity/heat", (temp != 99) ? "set to " : ""); break;
                default: printfTopicS("Outputfilter illegal state %i", EE.outputFilter); break;
              }
              break;
    case '?': // reset ATmega
    case 'h': // reset ATmega
    case 'H': printfTopicS("ESP commands:");
              printfTopicS("P to print or modify P1P2MQTT/ESP settings");
              printfTopicS("J to modify, or print individual, outputMode (J-mask) settings");
              printfTopicS("V to print system information");
              printfTopicS("D to restart, reconnect, save/restore EEPROM settings, or factory reset");
#ifndef W_SERIES
              printfTopicS("");
              printfTopicS("ATmega commands:");
              printfTopicS("A reset ATmega");
              printfTopicS("M show brand/model/version");
              printfTopicS("L set control mode (Daikin only)");
              printfTopicS("C set counter request mode (Daikin E-series only)");
              printfTopicS("E/F parameter write command (Daikin E/F-series only)");
              printfTopicS("W raw packet write command");
              printfTopicS("T write delay");
              printfTopicS("O write timeout");
#endif /* W_SERIES */
#ifdef MHI_SERIES
              printfTopicS("M MHI 3byte-1byte format conversion on/off (MHI only)");
#endif /* MHI_SERIES */
#if defined MHI_SERIES || defined TH_SERIES
              printfTopicS("X max pause allowed between package bytes (max 76)");
              printfTopicS("E error mask (default 0x7F, Hitachi 0x3B)");
#endif /* MHI_SERIES || TH_SERIES */
              break;
    case '\0':break;
    case '*': break;
    case 'v':
    case 'V': // command v handled both by P1P2MQTT-bridge and P1P2Monitor
              switch (cmdString[0]) {
                case 'v':
                case 'V': printWelcome();
                          break;
                default : break;
              }
              // fallthrough for 'V' command handled both by P1P2MQTT-bridge and P1P2Monitor
    default : // printfTopicS("To ATmega: ->%s<-", cmdString);
              Serial.print(F(SERIAL_MAGICSTRING));
              Serial.println((char *) cmdString);
              if ((cmdString[0] == 'k') || (cmdString[0] == 'K')) {
                delay(200);
                ATmega_dummy_for_serial();
                ATmega_uptime_prev = 0;
              }
              break;
  }
}

bool OTAbusy = 0;

void onMqttMessage(char* topic, char* payload, const AsyncMqttClientMessageProperties& properties,
                   const size_t& len, const size_t& index, const size_t& total) {
  (void) payload;
  static bool MQTT_drop_remainder = false;
  static size_t index_expected = 0;

  if (!len) return; // ignore null messages
  if (OTAbusy) return;

  // check index, len, total, handle fragmentation, collect MQTT_payload, create null-terminated copy
  if (index == 0) MQTT_drop_remainder = false;
  if (!MQTT_drop_remainder && index && (index != index_expected)) {
    delayedPrintfTopicS("Missed part of payload on topic %i", topic);
    MQTT_drop_remainder = true;
  }
  index_expected = index + len;
  bool deleteTopic = 0;
  if (mqttDeleting && !strncmp(topic, deleteSubscribeTopic, mqttDeletePrefixLength)) {
    if (mqttDeleteOverrun) return;
    if (!properties.retain) return;
    deleteTopic = 1;
    MQTT_drop_remainder = true;
  }
  if (!MQTT_drop_remainder && (total > MQTT_PAYLOAD_LEN)) {
    delayedPrintfTopicS("Received MQTT payload index %i length %i total %i, longer than max MQTT_PAYLOAD_LEN %i", index, len, total, MQTT_PAYLOAD_LEN);
    MQTT_drop_remainder = true;
  }
  if (!MQTT_drop_remainder) memcpy(MQTT_payload + index, payload, len);
  if (index + len != total) return;
  // index + len == total
  // drop msg if incomplete or too long, unless it starts with homeassistant (for delete process)
  if (MQTT_drop_remainder) {
    if (!deleteTopic) return;
    MQTT_payload[ 0 ] = '\0'; // too long or deleting -> empty string (not really needed, just in case for later code changes)
  } else {
    MQTT_payload[ total ] = '\0'; // ensure that string is null-terminated
  }
  // continue only for fully received payload which are not too long and/or are to be deleted

  // save current mqttTopic before changing it, restore it before returning!
  saveTopic();

  // handle P1P2/W/devicename/bridgename or P1P2/W
  topicCharSpecific('W');
  if ((!strcmp(topic, mqttTopic)) || (!strncmp(topic, mqttTopic, mqttTopicChar + 1) && (topic[ mqttTopicChar + 1 ] == '\0'))) {
    if (mqttBufferFree < total + 1) {
      // Serial_print(F("* [ESP] mqttBuffer full (W)"));
      if ((mqttBufferFullReported < 2) && (mqttBufferFree >= 3)) {
        mqttBuffer_writeChar('^');
        mqttBuffer_writeChar('W');
        mqttBuffer_writeChar('\n');
        mqttBufferFullReported = 2;
      }
      restoreTopic();
      return;
    }
    if (mqttBufferFullReported > 1) mqttBufferFullReported = 1;
    mqttBufferWriteString(MQTT_payload, total);
    mqttBuffer_writeChar('\n');
    restoreTopic();
    return;
  }

  // /homeassistant/status
  if (!strcmp(topic, "homeassistant/status")) {
    if (!strcmp(MQTT_payload, "online")) {
      if (mqttDeleting) {
      delayedPrintfTopicS("Detected homeassistant/status online - ignored due to ongoing mqtt delete action");
      } else {
        delayedPrintfTopicS("Detected homeassistant/status online");
        throttleStepTime = espUptime + THROTTLE_STEP_S;
        throttleValue = THROTTLE_VALUE;
        pseudo0F = 9;
        resetDataStructures();
      }
    } else if (!strcmp(MQTT_payload, "offline")) {
      delayedPrintfTopicS("Detected homeassistant/status offline");
    } else {
    }
    restoreTopic();
    return;
  }

  // store topic if to be deleted
  if (deleteTopic) {
    if (mqttBufferFree < strlen(topic) + 2 + MQTT_BUFFER_SPARE2) {
      // No space to buffer, signal buffer overrun so delete action for this topic will be repeated
      mqttDeleteOverrun = 1;
      restoreTopic();
      return;
    }
    mqttDeleteDetected++;
    mqttBuffer_writeChar('-');
    mqttBufferWriteString(topic, strlen(topic));
    mqttBuffer_writeChar('\n');
    restoreTopic();
    return;
  }

#ifdef E_SERIES
  // electricity meter topics
  // active_power, ePower, 16 bits!
  if (!strcmp(topic, EE.mqttElectricityPower)) {
    if (sscanf(MQTT_payload, "%hu", &ePower) == 1) {
      ePowerAvailable = true;
      ePowerTime = espUptime;
    } else {
      delayedPrintfTopicS("Illegal ePower %s", MQTT_payload);
    }
    restoreTopic();
    return;
  }

  if (!strcmp(topic, EE.mqttElectricityTotal)) {
    if (sscanf(MQTT_payload, "%u", &eTotal) == 1) {
      eTotalAvailable = true;
      eTotalTime = espUptime;
    } else {
      delayedPrintfTopicS("Illegal eTotal %s", MQTT_payload);
    }
    restoreTopic();
    return;
  }

  // BUH/gas meter topic // bPower in W, bTotal in Wh (even for gas/hybrid)
  if (!strcmp(topic, EE.mqttBUHpower)) {
    if (sscanf(MQTT_payload, "%hu", &bPower) == 1) {
      bPowerAvailable = true;
      bPowerTime = espUptime;
    } else {
      delayedPrintfTopicS("Illegal bPower %s", MQTT_payload);
    }
    restoreTopic();
    return;
  }

  if (!strcmp(topic, EE.mqttBUHtotal)) {
    if (sscanf(MQTT_payload, "%u", &bTotal) == 1) {
      bTotalAvailable = true;
      bTotalTime = espUptime;
    } else {
      delayedPrintfTopicS("Illegal bTotal %s", MQTT_payload);
    }
    restoreTopic();
    return;
  }
#endif /* E_SERIES */

  // mqttSaveTopic 'A'-'Z'/'a'-'z'
  topicCharBin(mqttSaveTopicChar);
  if (!strcmp(topic, mqttTopic)) {
    if (mqttSaveBytesLeft <= MQTT_SAVE_BLOCK_SIZE) {
      // last block
      if (total != 2 * mqttSaveBytesLeft) {
        delayedPrintfTopicS("Recvd wrong last block size mqttSave %c expected %i", mqttSaveTopicChar, mqttSaveBytesLeft);
        restoreTopic();
        return;
      }
    } else {
      // not last block
      if (total != 2 * MQTT_SAVE_BLOCK_SIZE) {
        delayedPrintfTopicS("Recvd wrong block size mqttSave %c expected %i", mqttSaveTopicChar, MQTT_SAVE_BLOCK_SIZE);
        delayedPrintfTopicS("Recvd mqttSave != bs");
        restoreTopic();
        return;
      }
    }
    if (mqttSaveReceived) {
      delayedPrintfTopicS("mqttReceived already 1");
      restoreTopic();
      return;
    }
    uint16_t mpp = 0;
    while ((mpp < MQTT_SAVE_BLOCK_SIZE) && (mpp < (total >> 1)) && sscanf(MQTT_payload + 2 * mpp, "%2hhx", &mqttSaveBlockStart[mpp])) mpp++;
    if (mpp != (total >> 1)) {
      delayedPrintfTopicS("mqttReceived char %c readback %i expected %i", mqttSaveTopicChar, mpp, (total >> 1));
    }
    mqttSaveReceived = 1;
    restoreTopic();
    return;
  }

  // unknown topic received
  delayedPrintfTopicS("Unknown MQTT topic received %s payload %s", topic, MQTT_payload);
  restoreTopic();
}

#ifdef TELNET

//void onInputReceived (String str) {
void onTelnetMessage (String str) {
  if (mqttBufferFree < str.length() + 1) {
    // Serial_println(F("* [ESP] mqttBuffer full (telnet)"));
    if ((mqttBufferFullReported < 2) && (mqttBufferFree >= 3)) {
      mqttBuffer_writeChar('^');
      mqttBuffer_writeChar('T');
      mqttBuffer_writeChar('\n');
      mqttBufferFullReported = 2;
    }
    return;
  }
  if (mqttBufferFullReported > 1) mqttBufferFullReported = 1;
  // check/unlock telnet access
  if (!telnetUnlock) {
    if (!strncmp(str.c_str(), EE.telnetMagicword, TELNET_MAGICWORD_LEN)) {
      telnetUnlock = 1;
      delayedPrintfTopicS("Telnet access unlocked");
      return;
    } else {
      delayedPrintfTopicS("Wrong unlock word");
      delay(100);
      return;
    }
  }
  // copy string to mqttBuffer, add '\n'
  mqttBufferWriteString(str.c_str(), str.length());
  mqttBuffer_writeChar('\n');
  // delayedPrintfTopicS("Telnet recvd: %s", str.c_str());
}
#endif

void buildMqttTopic() {
  // mqttTopic prefix
  strlcpy(mqttTopic, EE.mqttPrefix, MQTT_TOPIC_LEN);
  strlcpy(mqttTopic + strlen(mqttTopic), "/X", 3);
  mqttTopicChar = strlen(mqttTopic) - 1; // location of X in mqttTopic

  if (EE.useDeviceNameInTopic) {
    strlcpy(mqttTopic  + strlen(mqttTopic), "/", 2); \
    strlcpy(mqttTopic  + strlen(mqttTopic), EE.deviceName, DEVICE_NAME_LEN); \
  }

  if (EE.useBridgeNameInTopic) {
    strlcpy(mqttTopic  + strlen(mqttTopic), "/", 2); \
    strlcpy(mqttTopic  + strlen(mqttTopic), EE.bridgeName, BRIDGE_NAME_LEN); \
  }

  strlcpy(mqttTopic + strlen(mqttTopic), "/M/0/", 6);
  mqttTopicCatChar = strlen(mqttTopic) - 4;
  mqttTopicSrcChar = strlen(mqttTopic) - 2;
  mqttSlashChar = strlen(mqttTopic) - 5; // location of first slash before M in mqttTopic
  mqttTopicPrefixLength = strlen(mqttTopic); // length of P1P2/X[/deviceName][/bridgeName]/M/0/
}

void configTZ (void) {
  if (EE.useTZ > 1) {
    printfTopicS("Config NTP server with predefined TZ string %s", PREDEFINED_TZ[ EE.useTZ ]);
    configTime(PREDEFINED_TZ[ EE.useTZ ], MY_NTP_SERVER);
  } else if ((EE.useTZ == 1) && EE.userTZ[0]) {
    printfTopicS("Config NTP server with user-defined TZ string %s", EE.userTZ);
    configTime(EE.userTZ, MY_NTP_SERVER);
  }
}

void reportState(void) {
  printfTopicS("Factory reset scheduled (D8 to undo): %s", factoryReset ? PSTR("yes") : PSTR("no"));
  printfTopicS("EEPROM modifications pending (D5 to save): %s", EE_dirty ? PSTR("yes") : PSTR("no"));
#ifdef E_SERIES
  printfTopicS("Waiting for consumption/production counters (D13): %s", EE.D13 ? PSTR("yes") : PSTR("no"));
#endif /* E_SERIES */
}

void printWelcome(void) {
  printfTopicS(WELCOMESTRING);
#ifdef E_SERIES
  printfTopicS("Compiled %s %s for Daikin E-Series", __DATE__, __TIME__);
#endif /* E_SERIES */
#ifdef F_SERIES
  printfTopicS("Compiled %s %s for Daikin F-Series", __DATE__, __TIME__);
#endif /* F_SERIES */
#ifdef H_SERIES
  printfTopicS("Compiled %s %s for Hitachi", __DATE__, __TIME__);
#endif /* H_SERIES */
#ifdef MHI_SERIES
  printfTopicS("Compiled %s %s for MHI", __DATE__, __TIME__);
#endif /* MHI_SERIES */
#ifdef W_SERIES
  printfTopicS("Compiled %s %s for HomeWizard kWh bridge", __DATE__, __TIME__);
#endif /* W_SERIES */
  if (mqttClient.connected()) {
    printfTopicS("Connected to MQTT server");
  } else {
    delayedPrintfTopicS("Warning: not connected to MQTT server");
  }
  checkParam();
  for (byte i = 0; i < PARAM_NR; i++) {
    printModifyParam(i, 0);
  }
  reportState();
}

char willTopic[ WILL_TOPIC_LEN ] = "\0"; // contents should not change

void setup() {
  doubleResetDataDone = 0;
  doubleResetData = 0x01;
  ESP.rtcUserMemoryRead(doubleResetAddress, &doubleResetData, sizeof(doubleResetData));

  if ((doubleResetData & 0xFFFFFF00) == 0xAAAAAA00) {
    delayedPrintfTopicS("Double reset detected");
    doubleResetWiFi = 1;
    digitalWrite(BLUELED_PIN, LOW);
    pinMode(BLUELED_PIN, OUTPUT);
    if ((doubleResetData & 0xFF) != 0xFF) doubleResetData += 1;
  } else if ((doubleResetData & 0xFFFFFF00) == 0x55555500) {
    // regular reboot detected
    if ((doubleResetData & 0xFF) != 0xFF) doubleResetData += 1;
    doubleResetData ^= 0xFFFFFF00;
  } else {
    delayedPrintfTopicS("Power-up reset");
    doubleResetData = 0xAAAAAA00;
  }
  ESP.rtcUserMemoryWrite(doubleResetAddress, &doubleResetData, sizeof(doubleResetData)); // writes A/cnt

  // init/update/load EEPROM data
  loadEEPROM();

// hardware reset  if !A -> A.....5
// button reset    if A -> 5, detected.

  switch (rebootReason()) {
    case REBOOT_REASON_ETH             : delayedPrintfTopicS("ESP reboot reason: ethernet failed and WiFi fall-back not allowed");
                                         break;
    case REBOOT_REASON_WIFIMAN1        : delayedPrintfTopicS("ESP reboot reason: wifiManager after hard reset");
                                         break;
    case REBOOT_REASON_WIFIMAN2        : delayedPrintfTopicS("ESP reboot reason: wifiManager after WiFi connect failed");
                                         break;
    case REBOOT_REASON_MQTT_FAILURE    : delayedPrintfTopicS("ESP reboot reason: MQTT failuer");
                                         break;
    case REBOOT_REASON_STATICIP        : delayedPrintfTopicS("ESP reboot reason: static IP");
                                         break;
    case REBOOT_REASON_D0              : delayedPrintfTopicS("ESP reboot reason: D0");
                                         break;
    case REBOOT_REASON_D1              : delayedPrintfTopicS("ESP reboot reason: D1");
                                         break;
    case REBOOT_REASON_OTA             : delayedPrintfTopicS("ESP reboot reason: OTA/UPDATE");
                                         break;
    default                            : delayedPrintfTopicS("ESP reboot reason: unknown/reset-button/power-up");
                                         break;
  }

  saveRebootReason(REBOOT_REASON_UNKNOWN);

#ifdef W_SERIES
  WiFiClient meterClient;
#endif /* W_SERIES */

#ifdef ETHERNET
  digitalWrite(ETH_RESET_PIN, LOW);
  pinMode(ETH_RESET_PIN, OUTPUT);
  delay(25);
  digitalWrite(ETH_RESET_PIN, HIGH);
  pinMode(ETH_RESET_PIN, INPUT);
#endif

// Set up Serial from/to P1P2Monitor on ATmega (250kBaud); or serial from/to USB debugging (115.2kBaud);
  delay(100);
  Serial.setRxBufferSize(RX_BUFFER_SIZE); // default value is too low for ESP taking long pauses at random moments...
  Serial.begin(SERIALSPEED);
  while (!Serial);      // wait for Arduino Serial Monitor to open
  Serial.println(F("*"));        // this line is copied back by ATmega as "first line ignored"
  Serial.println(WELCOMESTRING);
  Serial.print(F("* Compiled "));
  Serial.print(__DATE__);
  Serial.print(' ');
  Serial.println(__TIME__);

//  IPAddress local_ip;

#ifdef TELNET
// setup telnet
  // passing on functions for various telnet events
  telnetUnlock = !(EE.telnetMagicword[1]);
  telnet.onConnect(onTelnetConnect);
  telnet.onConnectionAttempt(onTelnetConnectionAttempt);
  telnet.onReconnect(onTelnetReconnect);
  telnet.onDisconnect(onTelnetDisconnect);
  telnet.onInputReceived(onTelnetMessage);
  bool telnetSuccess = false;
#endif TELNET

#ifdef ETHERNET
  Serial_println(F("* [ESP] Trying initEthernet"));
  // delayedPrintfTopicS("Trying initEthernet");
  if (ethernetConnected = initEthernet()) {
    telnetSuccess = telnet.begin(telnet_port, false); // change in v0.9.30 to official unmodified ESP_telnet v2.0.0
    local_ip = eth.localIP();
    Serial_print(F("* [ESP] Connected to ethernet, IP address: "));
    Serial_println(local_ip);
    delayedPrintfTopicS("Connected to ethernet");
  } else
#endif
  {
    // WiFiManager setup
    // Customize AP
    WiFiManagerParameter custom_text("<p><b>P1P2MQTT</b></p>");
    WiFiManagerParameter custom_text1("<b>Your MQTT server IPv4 address</b>");
    WiFiManagerParameter custom_text2("<b>Your MQTT server port</b>");
    WiFiManagerParameter custom_text3("<b>MQTT user</b>");
    WiFiManagerParameter custom_text4("<b>MQTT password</b>");
    WiFiManagerParameter custom_text5("<b>0=DHCP 1=static IP</b>");
    WiFiManagerParameter custom_text6("<b>Static IPv4 (ignored for DHCP)</b>");
    WiFiManagerParameter custom_text7("<b>Gateway (ignored for DHCP)</b>");
    WiFiManagerParameter custom_text8("<b>Subnetwork (ignored for DHCP)</b>");
    // Debug info?
#ifdef DEBUG_OVER_SERIAL
    wifiManager.setDebugOutput(true);
#endif /* DEBUG_OVER_SERIAL */

    // add 4 MQTT settings to WiFiManager, with default settings preconfigured in NetworkParams.h
    WiFiManagerParameter WiFiManMqttServer("mqttserver", "MqttServer (IPv4, required)", MQTT_SERVER, MQTT_SERVER_LEN - 1);
    WiFiManagerParameter WiFiManMqttPort("mqttport", "MqttPort (optional, default 1883)", MQTT_PORT, 5);
    WiFiManagerParameter WiFiManMqttUser("mqttuser", "MqttUser (optional)", MQTT_USER, MQTT_USER_LEN - 1);
    WiFiManagerParameter WiFiManMqttPassword("mqttpassword", "MqttPassword (optional)", MQTT_PASSWORD, MQTT_PASSWORD_LEN - 1);

    // add 4 static IP settings to WiFiManager, with default settings preconfigured in NetworkParams.h
    WiFiManagerParameter WiFiMan_use_static_ip("use_static_ip", "0=DHCP 1=staticIP", INIT_USE_STATIC_IP_STRING, 1);
    WiFiManagerParameter WiFiMan_static_ip("static_ip", "Static IPv4 (optional)", STATIC_IP, STATIC_IP_LEN - 1);
    WiFiManagerParameter WiFiMan_static_gw("static_gw", "Static GW (optional)", STATIC_GW, STATIC_IP_LEN - 1);
    WiFiManagerParameter WiFiMan_static_nm("static_nm", "Static NM (optional)", STATIC_NM, STATIC_IP_LEN - 1);

    // Set config save notify callback
    wifiManager.setSaveConfigCallback(WiFiManagerSaveConfigCallback);

    wifiManager.addParameter(&custom_text);
    wifiManager.addParameter(&custom_text1);
    wifiManager.addParameter(&WiFiManMqttServer);
    wifiManager.addParameter(&custom_text2);
    wifiManager.addParameter(&WiFiManMqttPort);
    wifiManager.addParameter(&custom_text3);
    wifiManager.addParameter(&WiFiManMqttUser);
    wifiManager.addParameter(&custom_text4);
    wifiManager.addParameter(&WiFiManMqttPassword);
    wifiManager.addParameter(&custom_text5);
    wifiManager.addParameter(&WiFiMan_use_static_ip);
    wifiManager.addParameter(&custom_text6);
    wifiManager.addParameter(&WiFiMan_static_ip);
    wifiManager.addParameter(&custom_text7);
    wifiManager.addParameter(&WiFiMan_static_gw);
    wifiManager.addParameter(&custom_text8);
    wifiManager.addParameter(&WiFiMan_static_nm);

    // reset WiFiManager settings - for testing only;
    // wifiManager.resetSettings();

    // Set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
    wifiManager.setAPCallback(WiFiManagerConfigModeCallback);
    // Custom static IP for AP may be configured here
    // wifiManager.setAPStaticIPConfig(IPAddress(10,0,1,1), IPAddress(10,0,1,1), IPAddress(255,255,255,0));
    // Custom static IP for client may be configured here
    // wifiManager.setSTAStaticIPConfig(IPAddress(192,168,0,99), IPAddress(192,168,0,1), IPAddress(255,255,255,0)); // optional DNS 4th argument
    if (EE.useStaticIP) {
      IPAddress _ip, _gw, _nm;
      _ip.fromString(EE.static_ip);
      _gw.fromString(EE.static_gw);
      _nm.fromString(EE.static_nm);
      wifiManager.setSTAStaticIPConfig(_ip, _gw, _nm);
    }
    // WiFiManager start
    // Fetches ssid, password, and tries to connect.
    // If it does not connect it starts an access point with the specified name,
    // and goes into a blocking loop awaiting configuration.
    // First parameter is name of access point, second is the password.
    Serial_println(F("* [ESP] Trying autoconnect"));
#ifdef WIFIPORTAL_TIMEOUT
    wifiManager.setConfigPortalTimeout(WIFIPORTAL_TIMEOUT);
#endif /* WIFIPORTAL_TIMEOUT */

    if (doubleResetWiFi) {
      // prevent double-reset AP upon next restart
      if (!doubleResetDataDone) {
        doubleResetDataDone = 1;
        doubleResetData ^= 0xFFFFFF00;
        ESP.rtcUserMemoryWrite(doubleResetAddress, &doubleResetData, sizeof(doubleResetData));
      }
      if (!wifiManager.startConfigPortal(WIFIMAN_SSID2, WIFIMAN_PASSWORD)) {
        Serial.println("failed to connect and hit timeout");
        delay(3000);
        //reset and try again, or maybe put it to deep sleep
        saveRebootReason(REBOOT_REASON_WIFIMAN1);
        ESP.restart();
        delay(5000);
      }
    }
    // give user some time to perform double-reset
    if (!doubleResetDataDone) {
      delay(500);
      doubleResetDataDone = 1;
      doubleResetData ^= 0xFFFFFF00;
      ESP.rtcUserMemoryWrite(doubleResetAddress, &doubleResetData, sizeof(doubleResetData));
    }
    if (!wifiManager.autoConnect(EE.wifiManager_SSID, EE.wifiManager_password)) {
      Serial_println(F("* [ESP] Failed to connect and hit AP timeout, resetting"));
      // Reset and try again
      saveRebootReason(REBOOT_REASON_WIFIMAN2);
      ESP.reset();
      delay(2000);
    }
    // Connected to WiFi
    local_ip = WiFi.localIP();
//Silence WiFiManager from here on
    wifiManager.setDebugOutput(false);
    delayedPrintfTopicS("Connected to WiFi");
    Serial_print(F("* [ESP] Connected to WiFi, IP address: "));
    Serial_println(local_ip);
    // Serial_print(F("* SSID of WiFi is "));
    // Serial_println(wifiManager.getWiFiSSID()); // TODO enable in future WiFiManager version
    // Serial_print(F("* Pass of AP is "));
    // Serial_println(wifiManager.getWiFiPass());

// Check for new WiFiManager settings
    if (shouldSaveConfig) {
      // write new WiFi parameters provided in AP portal to EEPROM
      strlcpy(EE.mqttServer,   WiFiManMqttServer.getValue(),   sizeof(EE.mqttServer));
      if ((!strlen(WiFiManMqttPort.getValue())) || (sscanf(WiFiManMqttPort.getValue(), "%i", &EE.mqttPort) != 1)) {
        if (sscanf(MQTT_PORT, "%i", &EE.mqttPort) != 1) {
          EE.mqttPort = 1883;
        }
      }
      strlcpy(EE.mqttUser,     WiFiManMqttUser.getValue(),     sizeof(EE.mqttUser));
      strlcpy(EE.mqttPassword, WiFiManMqttPassword.getValue(), sizeof(EE.mqttPassword));
      if ((!strlen(WiFiMan_use_static_ip.getValue())) || (sscanf(WiFiMan_use_static_ip.getValue(), "%hhd", &EE.useStaticIP) != 1)) EE.useStaticIP = INIT_USE_STATIC_IP;
      strlcpy(EE.static_ip,   WiFiMan_static_ip.getValue(),   sizeof(EE.static_ip));
      strlcpy(EE.static_gw,   WiFiMan_static_gw.getValue(),   sizeof(EE.static_gw));
      strlcpy(EE.static_nm,   WiFiMan_static_nm.getValue(),   sizeof(EE.static_nm));

      saveEEPROM();

      if (EE.useStaticIP) {
        Serial_println(F("* [ESP] Restart ESP to set static IP"));
        saveRebootReason(REBOOT_REASON_STATICIP);
        delay(500);
        ESP.restart();
        delay(200);
      }

      delayedPrintfTopicS("-------");
      delayedPrintfTopicS("Wrote new WiFiManager-provided MQTT parameters to EEPROM");
      delayedPrintfTopicS("mqttUser/Password %s/%s", EE.mqttUser, EE.mqttPassword);
      delayedPrintfTopicS("mqtt server %s/%s", EE.mqttServer, EE.mqttPort);
      delayedPrintfTopicS("-------");
    }
#ifdef TELNET
    telnetSuccess = telnet.begin(telnet_port);
#endif
  }

  // setup uses mqtt_value to store string version op IPv4 address
  snprintf_P(mqtt_value, 16, PSTR("%d.%d.%d.%d"), local_ip[0], local_ip[1], local_ip[2], local_ip[3]);

  buildMqttTopic();

// MQTT client setup
  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  // mqttClient.onPublish(onMqttPublish);
  mqttClient.onMessage(onMqttMessage);
  mqttClient.setServer(EE.mqttServer, EE.mqttPort);
  mqttClient.setClientId(EE.mqttClientName);
  mqttClient.setCredentials((EE.mqttUser[0] == '\0') ? 0 : EE.mqttUser, (EE.mqttPassword[0] == '\0') ? 0 : EE.mqttPassword);

  topicCharSpecific('L');
  strlcpy(willTopic, mqttTopic, WILL_TOPIC_LEN);
  mqttClient.setWill(willTopic, MQTT_QOS_WILL, MQTT_RETAIN_WILL, "offline");

  Serial_print(F("* [ESP] Clientname ")); Serial_println(EE.mqttClientName);
  Serial_print(F("* [ESP] User ")); Serial_println(EE.mqttUser);
  // Serial_print(F("* [ESP] Password ")); Serial_println(EE.mqttPassword);
  Serial_print(F("* [ESP] Server ")); Serial_println(EE.mqttServer);
  Serial_print(F("* [ESP] Port ")); Serial_println(EE.mqttPort);

  delay(100);
  mqttClient.connect();
  delay(500);

  for (byte i = 0; i < 10; i++) { delay(50); if (mqttConnected == 2) break; }
  if (mqttConnected == 2) {
    reconnectTime = espUptime + 10;
  } else {
    if (fallback) {
      delayedPrintfTopicS("Reconnect to MQTT server " MQTT2_SERVER ":%i (user " MQTT2_USER "/password *) not successful yet, retrying in 10 seconds", MQTT2_PORT);
    } else {
      delayedPrintfTopicS("Reconnect to fallback MQTT server %s:%i (user %s/password *) not successful yet, retrying in 10 seconds", EE.mqttServer, EE.mqttPort, EE.mqttUser);
    }
    reconnectTime = espUptime + Mqtt_reconnectDelay;
  }

  if (mqttClient.connected()) {
    Serial_println(F("* [ESP] MQTT client connected"));
    delayedPrintfTopicS("MQTT client connected");
  } else {
    Serial_println(F("* [ESP] MQTT client connect failed, retrying for 10s"));
    delayedPrintfTopicS("MQTT client connect failed 1st time, retrying for 10s");
  }

#define MQTT_RETRIES 20 // 10s timeout
  byte connectTimeout = MQTT_RETRIES;
  while (!mqttClient.connected()) {
    mqttClient.connect();
    if (mqttClient.connected()) {
      Serial_println(F("* [ESP] MQTT client connected after retry"));
      break;
    } else {
      if (connectTimeout == MQTT_RETRIES) delayedPrintfTopicS("MQTT client connect failed, retrying in setup() until time-out ");
      Serial_print(F("* [ESP] MQTT client connect failed, retrying in setup() until time-out "));
      Serial_println(connectTimeout);
      if (!--connectTimeout) {
        delayedPrintfTopicS("Retrying failed - timeout");
        break;
      }
      delay(500);
    }
  }

  if (EE.deviceName[0] && EE.useDeviceNameInDeviceNameHA) {
    if (EE.bridgeName[0] && EE.useBridgeNameInDeviceNameHA) {
      snprintf_P(deviceNameHA, DEVICE_NAME_HA_LEN, PSTR("%s_%s"), EE.deviceName, EE.bridgeName);
    } else {
      snprintf_P(deviceNameHA, DEVICE_NAME_HA_LEN, PSTR("%s"), EE.deviceName);
    }
  } else {
    if (EE.bridgeName[0] && EE.useBridgeNameInDeviceNameHA) {
      snprintf_P(deviceNameHA, DEVICE_NAME_HA_LEN, PSTR("%s"), EE.bridgeName);
    } else {
      strlcpy(deviceNameHA, EE.deviceShortNameHA, DEVICE_NAME_HA_LEN);
    }
  }

  if (EE.deviceName[0] && EE.useDeviceNameInDeviceIdentity) {
    if (EE.bridgeName[0] && EE.useBridgeNameInDeviceIdentity) {
      snprintf_P(deviceUniqId, DEVICE_UNIQ_ID_LEN, PSTR("%s_%s"), EE.deviceName, EE.bridgeName);
    } else {
      snprintf_P(deviceUniqId, DEVICE_UNIQ_ID_LEN, PSTR("%s"), EE.deviceName);
    }
  } else {
    if (EE.bridgeName[0] && EE.useBridgeNameInDeviceIdentity) {
      snprintf_P(deviceUniqId, DEVICE_UNIQ_ID_LEN, PSTR("%s"), EE.bridgeName);
    } else {
      strlcpy(deviceUniqId, EE.deviceShortNameHA, DEVICE_UNIQ_ID_LEN);
    }
  }

  if (mqttClient.connected()) {
    clientPublishMqttChar('L', MQTT_QOS_WILL, MQTT_RETAIN_WILL, "online");
    printWelcome();
  }
  delay(200);
#ifdef ARDUINO_OTA
// OTA
  // port defaults to 8266
  // ArduinoOTA.setPort(8266);
  // Hostname defaults to esp8266-[ChipID], we use P1P2MQTT_[bridgename]
  ArduinoOTA.setHostname(EE.otaHostname); // TODO MDNS doesn't work due to webserver re-initing MDNS ?
  // No authentication by default
#ifdef OTA_PASSWORD
  ArduinoOTA.setPassword(EE.otaPassword);
#endif
  // Password "admin" can be set with its md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");
  ArduinoOTA.onStart([]() {
    OTAbusy = 1;
    prec_prev = 0;
    clientPublishMqttChar('L', MQTT_QOS_WILL, MQTT_RETAIN_WILL, "offline");
    if (ArduinoOTA.getCommand() == U_FLASH) {
      printfTopicS("Start OTA updating sketch");
    } else { // U_SPIFFS (not used here)
      printfTopicS("Start OTA updating filesystem");
    }
    delay(500);
    mqttClient.disconnect();
    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
  });
  ArduinoOTA.onEnd([]() {
    OTAbusy = 2;
    printfTopicS("OTA End, restarting");
    saveRebootReason(REBOOT_REASON_OTA);
    delay(300);
    ESP.restart();
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    byte prec = (progress / (total / 100));
    if (prec >= prec_prev + 10) {
      delayedPrintfTopicS("Progress: %u%%", prec);
      prec_prev = prec;
    }
  });
  ArduinoOTA.onError([](ota_error_t error) {
    clientPublishMqttChar('L', MQTT_QOS_WILL, MQTT_RETAIN_WILL, "online");
    switch (error) {
      case OTA_AUTH_ERROR    : printfTopicS("OTA Auth Failed");
                               break;
      case OTA_BEGIN_ERROR   : printfTopicS("OTA Begin Failed");
                               break;
      case OTA_CONNECT_ERROR : printfTopicS("OTA Connect Failed");
                               break;
      case OTA_RECEIVE_ERROR : printfTopicS("OTA Receive Failed");
                               break;
      case OTA_END_ERROR     : printfTopicS("OTA Begin Failed");
                               break;
      default                : printfTopicS("OTA unknown error");
                               break;
    }
    mqttClient.connect();
    ignoreRemainder = 2;
    OTAbusy = 0;
  });
#endif /* ARDUINO_OTA */

#ifdef ARDUINO_OTA
  ArduinoOTA.begin();
#endif /* ARDUINO_OTA */

#if defined AVRISP || defined WEBSERVER
  MDNS.begin(EE.mdnsName);
#endif
#ifdef AVRISP
// AVRISP
  // set RESET_PIN high, to prevent ESP8266AVRISP from resetting ATmega328P
  digitalWrite(RESET_PIN, HIGH);
  avrprog = new ESP8266AVRISP(avrisp_port, RESET_PIN, EE.ESPhwID ? SPI_SPEED_1 : SPI_SPEED_0);
  // set RESET_PIN back to INPUT mode
  pinMode(RESET_PIN, INPUT);
  MDNS.addService("avrisp", "tcp", avrisp_port);
  printfTopicS("AVRISP: ATmega programming: avrdude -c avrisp -p atmega328p -P net:%s:%i -t # or -U ...", mqtt_value, avrisp_port);
  // listen for avrdudes
  avrprog->begin();
#endif
#ifdef WEBSERVER
  httpUpdater.setup(&httpServer);
  httpServer.begin();
  MDNS.addService("http", "tcp", 80);
#endif /* WEBSERVER */
// Allow ATmega to enable serial input/output
  digitalWrite(ATMEGA_SERIAL_ENABLE, HIGH);
  pinMode(ATMEGA_SERIAL_ENABLE, OUTPUT);

  prevMillis = millis();

// Flush ATmega's serial input
  delay(200);
  ATmega_dummy_for_serial();

// Ready, report status
  if (telnetSuccess) {
    printfTopicS("Telnet setup OK");
  } else {
    printfTopicS("Telnet setup failed");
  }
  delay(500);

  configTZ();

  clientPublishMqttChar('Z', MQTT_QOS_CONFIG, MQTT_RETAIN_CONFIG, mqtt_value);
  printfTopicS("IPv4 address: %s", mqtt_value);

  checkParam();
  loadData();
  printfTopicS("Setup ready");
}

void process_for_mqtt(byte* rb, int n) {
  if (!mqttConnected) Mqtt_disconnectSkippedPackets++;
  if (mqttConnected || MQTT_DISCONNECT_CONTINUE) {
#ifdef EF_SERIES
    if (n == 3) bytes2keyvalue(rb[0], rb[2], EMPTY_PAYLOAD, rb + 3);
#endif /* EF_SERIES */
#ifdef MHI_SERIES
    for (byte i = 1; i < n; i++)
#else /* MHI_SERIES */
    for (byte i = 3; i < n; i++)
#endif /* MHI_SERIES */
    {
      if (!--throttle) throttle = THROTTLE_VALUE;
      if ((throttle >= throttleValue)
#ifdef E_SERIES
                                      || ((rb[0] == 0x00) && ((rb[2] == 0x31) || (rb[2] == 0x12)))               // time packets should be handled in right order
                                      || ((rb[0] == 0x40) && (rb[2] == 0xB8) && ((rb[3] & 0xFE) == 0x00))        // 0000B800/0000B801 should not be throttled
                                      || ((rb[0] == 0x40) && (rb[2] == 0x0F)                            )        // 40000F  should not be throttled
#endif /* E_SERIES */
                                                                                                 ) {
#ifdef MHI_SERIES
        byte doBits = bytes2keyvalue(rb[0],     0, i - 1, rb + 1);
#else /* MHI_SERIES */
        byte doBits = bytes2keyvalue(rb[0], rb[2], i - 3, rb + 3);
#endif /* MHI_SERIES */
        // returns which bits should be handled
#ifdef MHI_SERIES
        for (byte j = 0; j < 8; j++) if (doBits & (1 << j)) bits2keyvalue(rb[0],    0 , i - 1, rb + 1, j);
#else /* MHI_SERIES */
        for (byte j = 0; j < 8; j++) if (doBits & (1 << j)) bits2keyvalue(rb[0], rb[2], i - 3, rb + 3, j);
#endif /* MHI_SERIES */
      }
    }
  }
}

uint32_t espUptime_telnet = 0;
uint32_t espUptime_saveData = 0;
static bool wasConnected = false;

void loop() {

#ifdef ARDUINO_OTA
  // OTA
  ArduinoOTA.handle();
#endif /* ARDUINO_OTA */

#define TIMESTEP 5
  if ((EE.useTZ > 1) || ((EE.useTZ == 1) && EE.userTZ[0])) {
    time(&now);
    byte tm_min = tm.tm_min;
    localtime_r(&now, &tm);
    if (tm.tm_year != 70) {
      snprintf_P(sprint_value, TZ_PREFIX_LEN, PSTR("* [ESP] %04i-%02i-%02i %02i:%02i:%02i "), tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
      if (tm.tm_min != tm_min) {
        saveData();
        espUptime_saveData = espUptime + 60;
      }
      //TODO RTCdata above, switch from Daikin to TZ time?
      if ((M.R.hour <= 25) && (M.R.hour != tm.tm_hour)) {
        M.R.hour = tm.tm_hour;
        // new hour, once/hour activity
        if ((M.R.wday <= 25)  && (M.R.wday != tm.tm_wday)) {
          M.R.wday = tm.tm_wday;
          // new day, once/day activity
          if (tm.tm_wday == 1) {
            // new Monday, once/week activity
          }
          if ((M.R.month <= 25) && (M.R.month != tm.tm_mon)) {
            M.R.month = tm.tm_mon;
            // new month, once/month activity
          }
        }
      }
    }
  } else {
    snprintf_P(sprint_value, TZ_PREFIX_LEN, PSTR("* [ESP] ESPuptime%10d "), espUptime);
    if (espUptime >= espUptime_saveData) {
      espUptime_saveData = espUptime + 60;
      saveData();
    }
  }

#ifdef WEBSERVER
  httpServer.handleClient();
#endif /* WEBSERVER */

  // ESP-uptime and loop timing
  uint32_t currMillis = millis();
  uint16_t loopTime = currMillis - prevMillis;
  milliInc += loopTime;
  prevMillis = currMillis;
  while (milliInc >= 1000) {
    milliInc -= 1000;
    espUptime += 1;

#ifdef W_SERIES

    WiFiClient meterClient;
    HTTPClient httpMeter;
    httpMeter.begin(meterClient, EE.meterURL); // urlMeterURL); // ?? meterClient, serverPath.c_str());
    httpMeter.setTimeout(500);

    uint32_t currMillis1 = millis();
    int httpResponseCode = httpMeter.GET();
    uint32_t currMillis2 = millis();
    if (httpResponseCode > 0) {
      auto error = deserializeJson(electricityMeterJsonDocument, httpMeter.getString() /* .c_str() */);
      if (error) {
        // json deserialize failed
        electricityMeterDataValid = 0;
      } else {
        electricityMeterDataValid = 1;
        electricityMeterActivePower = electricityMeterJsonDocument["active_power_w"];
        electricityMeterTotal = ((double) electricityMeterJsonDocument["total_power_import_kwh"]) * 1000;
        // printfTopicS("EMETER payload %s Took %i active power %d total %ld", httpMeter.getString().c_str(), (int) currMillis2 - currMillis1, electricityMeterActivePower, electricityMeterTotal);
      }
    } else {
      printfTopicS("GET error %i", httpResponseCode);
      electricityMeterDataValid = 0;
    }

    httpMeter.end();
#endif /* W_SERIES */

    if (mqttClient.connected()) {
      Mqtt_disconnectTime = 0;
    } else {
      Mqtt_disconnectTimeTotal++;
      Mqtt_disconnectTime++;
    }
    if (milliInc < 1000) {
      if (throttleValue && (espUptime > throttleStepTime)) {
        throttleValue -= THROTTLE_STEP_P;
        throttleStepTime += THROTTLE_STEP_S;
        if (throttleValue) {
          printfTopicS("Throttling at %i", throttleValue);
        } else {
          printfTopicS("Ready throttling");
          pseudo0F = 9;
        }
      }

#define UPTIME_STEPSIZE 10
      if (espUptime >= espUptime_telnet + UPTIME_STEPSIZE) {
        espUptime_telnet = espUptime_telnet + UPTIME_STEPSIZE;
        if (!mqttClient.connected()) {
          printfTopicS("Uptime %i, MQTT is disconnected (%i s total %i s)", espUptime, Mqtt_disconnectTime, Mqtt_disconnectTimeTotal);
        } else {
          // printfTopicS_mqttserialonly("Uptime %i free %i", espUptime, ESP.getMaxFreeBlockSize());
          printfTopicS_mqttserialonly("Uptime %i", espUptime);
        }
      }
      pseudo0B++;
      pseudo0C++;
      pseudo0D++;
      pseudo0E++;
      pseudo0F++;

      if (!fallback && (Mqtt_disconnectTime > MQTT_DISCONNECT_TRY_FALLBACK)) {
        printfTopicS("Attempting fall back to fallback MQTT server");
        if (mqttDeleting) {
          printfTopicS("Stop mqtt delete action");
          mqttUnsubscribeToDelete();
          digitalWrite(ATMEGA_SERIAL_ENABLE, HIGH);
          ignoreRemainder = 2; // in view of change of ignoreSerial caused by mqttDeleting change
          mqttDeleting = 0;
        }
        // disconnect
        mqttClient.clearQueue();
        mqttClient.disconnect(true);
        delay(500);
        mqttClient.setServer(MQTT2_SERVER, MQTT2_PORT);
        mqttClient.setCredentials(MQTT2_USER, MQTT2_PASSWORD);
        printfTopicS("Trying to connect to fallback MQTT server (first attempt will fail)");
        throttleStepTime = espUptime + THROTTLE_STEP_S;
        throttleValue = THROTTLE_VALUE;
        pseudo0F = 9;
        resetDataStructures();
        delay(500);
        Mqtt_reconnectDelay = 0;
        reconnectTime = espUptime; // 1st attempt always fails (library issue?)
        fallback = 1;
      }

      if (Mqtt_disconnectTime > MQTT_DISCONNECT_RESTART) {
        printfTopicS("Restarting ESP to attempt to reconnect to Mqtt server");
        saveRebootReason(REBOOT_REASON_MQTT_FAILURE);
        delay(100);
        ESP.restart();
        delay(100);
      }
    }
  }

#ifdef TELNET
  // telnet
  telnet.loop();
#endif

#ifdef AVRISP
  // AVRISP
  static AVRISPState_t last_state = AVRISP_STATE_IDLE;
  AVRISPState_t new_state = avrprog->update();
  if (last_state != new_state) {
    switch (new_state) {
      case AVRISP_STATE_IDLE:    printfTopicS("AVR now idle");
                                 digitalWrite(RESET_PIN, LOW);
                                 pinMode(RESET_PIN, OUTPUT);
                                 delay(1);
                                 pinMode(RESET_PIN, INPUT);
                                 digitalWrite(RESET_PIN, HIGH);
                                 // enable ATmega to determine PB5=GPIO14/GPIO15 for v1.0/v1.1
                                 delay(1);
                                 // enable ATmega serial output on v1.2


                                 while ((c = Serial.read()) >= 0); // flush serial input



                                 pinMode(ATMEGA_SERIAL_ENABLE, OUTPUT); // not really needed
                                 digitalWrite(ATMEGA_SERIAL_ENABLE, HIGH);
                                 delay(100);
                                 ATmega_dummy_for_serial();
                                 ATmega_uptime_prev = 0;
                                 break;
      case AVRISP_STATE_PENDING: printfTopicS("AVR connection pending");
                                 pinMode(RESET_PIN, OUTPUT);
                                 break;
      case AVRISP_STATE_ACTIVE:  printfTopicS("AVR programming mode");
                                 pinMode(RESET_PIN, OUTPUT);
                                 break;
    }
    last_state = new_state;
  }
  // Serve the AVRISP client
  if (last_state != AVRISP_STATE_IDLE) avrprog->serve();
#endif /* AVRISP */

#if defined AVRISP || defined WEBSERVER
  MDNS.update();
#endif

  // network and mqtt connection check
  // if (WiFi.isConnected() || eth.connected()) {//} // TODO this does not work. eth.connected() becomes true after 4 minutes even if there is no ethernet cable attached
                                                  // and eth.connected() remains true even after disconnecting the ethernet cable (and perhaps also after 'D1' soft ESP.reset()
  // ethernetConnected = ???;                     // try to update ethernetConnected based on actual ethernet status
  if (WiFi.isConnected() || ethernetConnected) {  // for now: use initial ethernet connection status instead (ethernet cable disconnect is thus not detected)
    if (!mqttClient.connected()) {
      if (espUptime > reconnectTime) {
        mqttClient.connect();
        for (byte i = 0; i < 10; i++) { delay(50); if (mqttConnected == 2) break; }
        if (mqttConnected == 2) {
          reconnectTime = espUptime + 10;
        } else {
          if (fallback) {
            delayedPrintfTopicS("Reconnect to MQTT server " MQTT2_SERVER ":%i (user " MQTT2_USER "/password *) not successful yet, retrying in 10 seconds", MQTT2_PORT);
          } else {
            delayedPrintfTopicS("Reconnect to MQTT server %s:%i (user %s/password *) not successful yet, retrying in 10 seconds", EE.mqttServer, EE.mqttPort, EE.mqttUser);
          }
          reconnectTime = espUptime + Mqtt_reconnectDelay;
          Mqtt_reconnectDelay = 10;
        }
      }
    }
  } else {
    // clean up
    if (wasConnected) {
      Serial_println("* [ESP] WiFi/ethernet disconnected");
      delayedPrintfTopicS("WiFi/ethernet disconnected");
    }
    mqttClient.clearQueue();
    mqttClient.disconnect(true);
  }
  wasConnected = WiFi.isConnected() || ethernetConnected;

  if (mqttConnected == 2) {
    // mqtt (re)connected
    clientPublishMqttChar('L', MQTT_QOS_WILL, MQTT_RETAIN_WILL, "online");
    if (EE.outputMode & 0x0800) { // only if 0x0800
      if (mqttDeleting) {
        delayedPrintfTopicS("Mqtt reconnect - continue mqtt delete");
      } else {
        delayedPrintfTopicS("Restart data communication after Mqtt reconnect");
        resetDataStructures();
        throttleStepTime = espUptime + THROTTLE_STEP_S;
        throttleValue = THROTTLE_VALUE;
        pseudo0F = 9;
      }
    }
    mqttSubscribe();
    mqttConnected = 1;
  }

  if (!OTAbusy)  {

    // read serial input OR mqttBuffer input until and including '\n', but do not store '\n'
    // rb_buffer = readBuffer + serial_rb // +20 for timestamp
    // serial_rb is number of char received and stored in readBuffer
    // ignore first line from serial-input and too-long lines
    // readBuffer does not include '\n' but may include '\r' if Windows provides serial input
    // handle P1/P2 data from ATmega328P or from MQTT or from serial
    // also handles incoming command(s) and data for ESP from telnet or MQTT
    // use non-blocking serial input

    // read line from mqttBuffer but do not interrupt reading from serial input
    c = -1;
    bool mqttHexReceived = 0;
    if ((mqttConnected || telnetConnected) && !serial_rb) {
      while (((c = mqttBufferReadChar()) >= 0) && (c != '\n') && (serial_rb < RB)) {
        *rb_buffer++ = (char) c;
        serial_rb++;
      }
      if (c >= 0) {
        if (readBuffer[0] == '^') {
          printfTopicS("Mqtt input buffer overrun (%c)", readBuffer[1]);
          serial_rb = 0;
          rb_buffer = readBuffer;
          c = -1;
        } else {
          // handle as command
          *rb_buffer = '\0';
          if ((serial_rb > 0) && (*(rb_buffer - 1) == '\r')) {
            serial_rb--;
            *(--rb_buffer) = '\0';
          }
          handleCommand(readBuffer);
          serial_rb = 0;
          rb_buffer = readBuffer;
          c = -1;
        }
      } else {
        // no command to handle
        // check if MQTT delete needs restart or is finished
        if (mqttDeleting && (espUptime >= mqttUnsubscribeTime)) {
          // unsubscribe
          if (mqttUnsubscribeToDelete()) {
             delay(300);
             mqttSubscribeToDelete(deleteSpecific);
             mqttUnsubscribeTime = espUptime + DELETE_STEP;
          } else {
            // already done: mqttDeleting = 0;
            ignoreRemainder = 2; // in view of change of ignoreSerial caused by mqttDeleting change
            // create deleted messages
            snprintf_P(mqtt_value, 16, PSTR("%d.%d.%d.%d"), local_ip[0], local_ip[1], local_ip[2], local_ip[3]);
            clientPublishMqttChar('Z', MQTT_QOS_CONFIG, MQTT_RETAIN_CONFIG, mqtt_value);
            printfTopicS("Resetting data structures, throttle");
            throttleStepTime = espUptime + THROTTLE_STEP_S;
            throttleValue = THROTTLE_VALUE;
            pseudo0F = 9;
            clientPublishMqttChar('L', MQTT_QOS_WILL, MQTT_RETAIN_WILL, "online");
            resetDataStructures();
            digitalWrite(ATMEGA_SERIAL_ENABLE, HIGH);
          }
        }
      }
    }

    // read (partial) line from serial input unless ignoreSerial or if line was read from mqttBuffer (mqttHexReceived)
    if (!mqttHexReceived && !ignoreSerial) while (((c = Serial.read()) >= 0) && (c != '\n') && (serial_rb < RB)) {
      *rb_buffer++ = (char) c;
      // for first characters REDCc, insert time stamp
      if (!serial_rb) {
        if ((c == 'R') || (c == 'E') || (c == 'D') || (c == 'C') || (c == 'c')) {
          strncpy(readBuffer + 1, sprint_value + 7, 20);
          serial_rb += 20;
          rb_buffer += 20;
        }
      }
      serial_rb++;
    }

    // process message
    // ((c == '\n' and serial_rb >= 0))  and/or  serial_rb == RB)  or  c == -1
    if (c >= 0) {
      // something was read
      if ((c == '\n') && (serial_rb < RB)) {
        // c == '\n'
        *rb_buffer = '\0';
        // Remove \r before \n in DOS input
        if ((serial_rb > 0) && (*(rb_buffer - 1) == '\r')) {
          serial_rb--;
          *(--rb_buffer) = '\0';
        }
        if (!mqttHexReceived && (ignoreRemainder == 2)) {
          // printfTopicS("First line from ATmega ignored %s", readBuffer);
          ignoreRemainder = 0;
        } else if (ignoreRemainder == 1) {
          printfTopicS("Line from ATmega/mqtt too long, last part: ->%s<-", readBuffer);
          ignoreRemainder = 0;
        } else {
          if (!serial_rb) {
            // ignore empty line
          } else if ((readBuffer[0] == 'R') && (serial_rb > 32)){
            // read hex data, verify CRC or CS
            int rbp = 32; // 20-char TZ and 10-char relative time stamp
            int n, rbtemp;
            byte rh = 0;
            while (sscanf(readBuffer + rbp, "%2x%n", &rbtemp, &n) == 1) {
              if (rh < HB) readHex[rh] = rbtemp;
              rh++;
              rbp += n;
            }
            if (rh > HB) {
              printfTopicS("Unexpected input buffer full/overflow, received %i, ignoring remainder", rh);
              rh = RB;
            }
#ifdef MHI_SERIES
            if ((rh > 1) || (rh == 1) && !CS_GEN)
#else /* MHI_SERIES */
            if ((rh > 1) || (rh == 1) && !CRC_GEN)
#endif /* MHI_SERIES */
            {
#ifdef MHI_SERIES
              if (CS_GEN) rh--;
              // rh is packet length (not counting CS byte readHex[rh])
              uint8_t cs = 0;
              for (uint8_t i = 0; i < rh; i++) cs += readHex[i];
              if ((!CS_GEN) || (cs == readHex[rh]))
#else /* MHI_SERIES */
              if (CRC_GEN) rh--;
              // rh is packet length (not counting CRC byte readHex[rh])
              uint8_t crc = CRC_FEED;
              for (uint8_t i = 0; i < rh; i++) {
                uint8_t c = readHex[i];
                if (CRC_GEN != 0) for (uint8_t i = 0; i < 8; i++) {
                  crc = ((crc ^ c) & 0x01 ? ((crc >> 1) ^ CRC_GEN) : (crc >> 1));
                  c >>= 1;
                }
              }
              if ((!CRC_GEN) || (crc == readHex[rh]))
#endif /* MHI_SERIES */
              {
                if (((EE.outputMode & 0x0001) && (readBuffer[22] != 'P')) || ((EE.outputMode & 0x0004) && (readBuffer[22] == 'P')) ) {
                  clientPublishMqttChar('R', MQTT_QOS_HEX, MQTT_RETAIN_HEX, readBuffer);
                }
                if (EE.outputMode & 0x0010) printfTelnet_MON("R %s", readBuffer + 22);
                if ((EE.outputMode & 0x0022) && !mqttDeleting) process_for_mqtt(readHex, rh);
                if ((readHex[0] == 0x00) && (readHex[1] == 0x00) && (readHex[2] == 0x0E)) pseudo0B = pseudo0C = 9; // Insert pseudo packet 40000B/0C in output serial after 00000E
#ifndef W_SERIES
                if ((readHex[0] == 0x00) && (readHex[1] == 0x00) && (readHex[2] == 0x0F)) pseudo0D = pseudo0F = 9; // Insert pseudo packet 40000D/0F in output serial after 00000F
#endif /* W_SERIES */
                if ((readHex[0] == 0x00) && (readHex[1] == 0x00) && (readHex[2] == 0x0F)) {
#ifndef W_SERIES
                  pseudo0E = 9;                                                                         // Insert pseudo packet 40000E in output serial after 00000E
#define ATMEGA_UPTIME_LOC 17 // depends on pseudo msg format of P1P2Monitor
                  uint32_t ATmega_uptime = (readHex[ ATMEGA_UPTIME_LOC ] << 24) | (readHex[ ATMEGA_UPTIME_LOC + 1 ] << 16) | (readHex[ ATMEGA_UPTIME_LOC + 2 ] << 8) | readHex[ ATMEGA_UPTIME_LOC + 3 ];
                  if (ATmega_uptime < ATmega_uptime_prev) {
                    // unexpected ATmega reboot detected, flush ATmega's serial input
                    printfTopicS("ATmega reboot detected");
                    delay(200);
                    ATmega_dummy_for_serial();
                  }
                  ATmega_uptime_prev = ATmega_uptime;
#endif /* W_SERIES */
                }
              } else {
#ifdef MHI_SERIES
                printfTopicS("Serial input buffer overrun or CS error in R data:%s expected 0x%02X", readBuffer + 1, cs);
                if (ESP_serial_input_Errors_CS < 0xFF) ESP_serial_input_Errors_CS++;
#else /* MHI_SERIES */
                printfTopicS("Serial input buffer overrun or CRC error in R data:%s expected 0x%02X", readBuffer + 1, crc);
                if (ESP_serial_input_Errors_CRC < 0xFF) ESP_serial_input_Errors_CRC++;
#endif /* MHI_SERIES */
#ifdef H_SERIES
/* TODO implement Hitachi checksum tests previously on ATmega
                Serial_print(F(" CS1=")); // checksum starting at byte 1 (which is usually 0)
                if (cs_hitachi < 0x10) Serial_print(F("0"));
                Serial_print(cs_hitachi, HEX);
                cs_hitachi ^= RB[1];
                Serial_print(F(" CS2=")); // checksum starting at byte 2
                if (cs_hitachi < 0x10) Serial_print(F("0"));
                Serial_print(cs_hitachi, HEX);
*/
#endif
              }
            } else {
              printfTopicS("Not enough readable data in R line: ->%s<-", readBuffer + 1);
              if (ESP_serial_input_Errors_Data_Short < 0xFF) ESP_serial_input_Errors_Data_Short++;
            }
          } else if ((readBuffer[0] == 'C') || (readBuffer[0] == 'c')) {
            // timing info
            if (EE.outputMode & 0x0040) printfTelnet_MON("%c %s", readBuffer[0], readBuffer + 22);
            if (EE.outputMode & 0x1000) clientPublishMqttChar('R', MQTT_QOS_HEX, MQTT_RETAIN_HEX, readBuffer);
          } else if (readBuffer[0] == 'D') {
            // duplicated data (thus this is not pseudo data)
            if (EE.outputMode & 0x0001) clientPublishMqttChar('R', MQTT_QOS_HEX, MQTT_RETAIN_HEX, readBuffer);
          } else if (readBuffer[0] == 'E') {
            // data with errors
            printfTopicS_MON("E %s", readBuffer + 22);
            if (EE.outputMode & 0x2000) clientPublishMqttChar('R', MQTT_QOS_HEX, MQTT_RETAIN_HEX, readBuffer);
          } else if (readBuffer[0] == '*') {
            printfTopicS_MON("%s", readBuffer + 2);
          } else {
            printfTopicS_MON("%s", readBuffer);
            if (ESP_serial_input_Errors_Data_Short < 0xFF) ESP_serial_input_Errors_Data_Short++;
          }
        }
      } else {
        //  (c != '\n' ||  serial_rb == RB)
        char lst = *(rb_buffer - 1);
        *(rb_buffer - 1) = '\0';
        if (c != '\n') {
          printfTopicS("Line from ATmega too long, ignored, ignoring remainder: ->%s<-->%c<-->%c<-", readBuffer, lst, c);
          ignoreRemainder = 1;
        } else {
          printfTopicS("Line from ATmega too long, terminated, ignored: ->%s<-->%c<-", readBuffer, lst);
          ignoreRemainder = 0;
        }
      }
      rb_buffer = readBuffer;
      serial_rb = 0;
    } else {
      // wait for more serial input
    }
    if (pseudo0B > 5) {
      pseudo0B = 0;
      writePseudoSystemPacket0B();
    }
    if (pseudo0C > 5) {
      pseudo0C = 0;
      writePseudoSystemPacket0C();
    }
    if (pseudo0D > 5) {
      pseudo0D = 0;
      writePseudoSystemPacket0D();
    }
    if (pseudo0E > 5) {
      pseudo0E = 0;
      readHex[0] = 0x40;
      readHex[1] = 0x00;
      readHex[2] = 0x0E;
      readHex[3]  = SW_MAJOR_VERSION;
      readHex[4]  = SW_MINOR_VERSION;
      readHex[5]  = SW_PATCH_VERSION;
      readHex[6]  = doubleResetData & 0xFF; // nr of ESP restarts
      readHex[7]  = rebootReason();
      readHex[8]  = (EE.outputMode >> 24) & 0xFF;
      readHex[9]  = (EE.outputMode >> 16) & 0xFF;
      readHex[10] = (EE.outputMode >> 8) & 0xFF;
      readHex[11] = EE.outputMode & 0xFF;
      readHex[12] = EE.outputFilter;
      readHex[13] = EE.ESPhwID;
      readHex[14] = 0; // dummy for switches and buttons
#ifdef E_SERIES
      readHex[15] = (EE.RToffset >> 8) & 0xFF;
      readHex[16] = EE.RToffset & 0xFF;
      readHex[17] = EE.R1Toffset;
      readHex[18] = EE.R2Toffset;
      readHex[19] = EE.R4Toffset;
      writePseudoPacket(readHex, 20);
#else
      writePseudoPacket(readHex, 15);
#endif E_SERIES
    }
    if (pseudo0F > 5) {
      pseudo0F = 0;
      readHex[0]  = 0x40;
      readHex[1]  = 0x00;
      readHex[2] = 0x0F;
      readHex[3]  = (espUptime >> 24) & 0xFF;
      readHex[4]  = (espUptime >> 16) & 0xFF;
      readHex[5]  = (espUptime >> 8) & 0xFF;
      readHex[6]  = espUptime & 0xFF;
      readHex[7]  = (Mqtt_disconnectTimeTotal >> 8) & 0xFF;
      readHex[8]  = Mqtt_disconnectTimeTotal & 0xFF;
      readHex[9]  = (Mqtt_msgSkipNotConnected >> 8) & 0xFF;
      readHex[10] = Mqtt_msgSkipNotConnected & 0xFF;
      readHex[11] = ethernetConnected;
      readHex[12] = telnetConnected;
#ifdef E_SERIES
      readHex[13] = (EE_dirty ? 0 : 1) | (factoryReset ? 0x02 : 0x00) | (throttleValue ? 0x20 : 0x00) | (EE.D13 ? 0x04 : 0x00) | (EE.haSetup ? 0x08 : 0x00) | (M.R.heatingOnlyX10 ? 0x10 : 0x00);
#else /* E_SERIES */
      readHex[13] = (EE_dirty ? 0 : 1) | (factoryReset ? 0x02 : 0x00) | (throttleValue ? 0x20 : 0x00);
#endif /* E_SERIES */
      readHex[14] = 0;
      uint16_t m1 = ESP.getMaxFreeBlockSize();
      readHex[15] = (m1 >> 8) & 0xFF;
      readHex[16] = m1 & 0xFF;
      readHex[17] = ESP_serial_input_Errors_Data_Short;
#ifdef MHI_SERIES
      readHex[18] = ESP_serial_input_Errors_CS;
#else /* MHI_SERIES */
      readHex[18] = ESP_serial_input_Errors_CRC;
#endif /* MHI_SERIES */
      readHex[19] = WiFi.RSSI() & 0xFF;
      readHex[20] = WiFi.status() & 0xFF;
      readHex[21] = (Mqtt_msgSkipLowMem >> 8) & 0xFF;
      readHex[22] = Mqtt_msgSkipLowMem & 0xFF;
      writePseudoPacket(readHex, 21);
    }
  }
  saveRTC();
}
