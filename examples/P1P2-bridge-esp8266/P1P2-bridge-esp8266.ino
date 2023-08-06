/* P1P2-bridge-esp8266json: Host application for esp8266 to interface to P1P2Monitor on Arduino Uno,
 *                          supports mqtt/json/wifi
 *                          Includes wifimanager, OTA
 *                          For protocol description, see SerialProtocol.md and MqttProtocol.md
 *
 * Copyright (c) 2019-2022 Arnold Niessen, arnold.niessen-at-gmail-dot-com - licensed under CC BY-NC-ND 4.0 with exceptions (see LICENSE.md)
 *
 * WARNING: P1P2-bridge-esp8266 is end-of-life, and will be replaced by P1P2MQTT
 *
 * Requires board support package:
 * http://arduino.esp8266.com/stable/package_esp8266com_index.json
 *
 * Requires libraries:
 * WiFiManager 0.16.0 by tzapu (installed using Arduino IDE)
 * AsyncMqttClient 0.9.0 by Marvin Roger obtained with git clone or as ZIP from https://github.com/marvinroger/async-mqtt-client
 * ESPAsyncTCP 2.0.1 by Phil Bowles obtained with git clone or as ZIP from https://github.com/philbowles/ESPAsyncTCP/
 * ESP_Telnet 2.0.0 by  Lennart Hennigs (installed using Arduino IDE)
 *
 * Version history
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
 * 20220918 v0.9.22 degree symbol, hwID, 32-bit outputMode
 * 20220907 v0.9.21 outputMode/outputFilter status survive ESP reboot (EEPROM), added MQTT_INPUT_BINDATA/MQTT_INPUT_HEXDATA (see P1P2_Config.h), reduced uptime update MQTT traffic
 * 20220904 v0.9.20 added E_SERIES/F_SERIES defines, and F-series VRV reporting in MQTT for 2 models
 * 20220903 v0.9.19 longer MQTT user/password, ESP reboot reason (define REBOOT_REASON) added in reporting
 * 20220829 v0.9.18 state_class added in MQTT discovery enabling visibility in HA energy overview
 * 20220817 v0.9.17 handling telnet welcomestring, error/scopemode time info via P1P2/R/#, v9=ignoreserial
 * 20220808 v0.9.15 extended verbosity command, unique OTA hostname, minor fixes
 * 20220802 v0.9.14 AVRISP, wifimanager, mqtt settings, EEPROM, telnet, outputMode, outputFilter, ...
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
#include "P1P2_NetworkParams.h"
#include "P1P2_Config.h"
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <EEPROM.h>
#include <time.h>
#include <TZ.h>

// WEBSERVER
#include "P1P2_ESP8266HTTPUpdateServer/P1P2_ESP8266HTTPUpdateServer.h"
ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;

//const char* MY_TZ[2] = {TZ_Europe_Amsterdam, TZ_Europe_London};
const char* MY_TZ[2] = { "CET-1CEST,M3.5.0/02,M10.5.0/03"   , "GMT0BST,M3.5.0/1,M10.5.0" };
time_t now;
tm tm;

#ifdef AVRISP
#include <SPI.h>
#include <ESP8266AVRISP.h>
#endif /* AVRISP */

#include <ArduinoOTA.h>
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

// EEPROM values for EEPROM_SIGNATURE_OLD1
typedef struct EEPROMSettingsOld {
  char signature[10];
  char mqttUser[20];
  char mqttPassword[40];
  char mqttServer[20];
  int  mqttPort;
};

#define NR_RESERVED 46

typedef struct EEPROMSettingsNew {
  // EEPROM values for EEPROM_SIGNATURE_OLD2
  char signature[10];
  char mqttUser[81];     // will be overwritten in case of shouldSaveConfig
  char mqttPassword[81]; // will be overwritten in case of shouldSaveConfig
  char mqttServer[20];   // will be overwritten in case of shouldSaveConfig
  int  mqttPort;
  byte rebootReason;
  // EEPROM values added for EEPROM_SIGNATURE_OLD3
  uint32_t outputMode;
  byte outputFilter;
  byte mqttInputByte4;
  // EEPROM values added for EEPROM_SIGNATURE_NEW
  byte ESPhwID;
  byte EEPROM_version;
  byte noWiFi;
  byte useStaticIP;
  char static_ip[16];
  char static_gw[16];
  char static_nm[16];
  byte useSensorPrefixHA;
  byte reserved[NR_RESERVED];
};

union EEPROMSettingsUnion {
  EEPROMSettingsOld EEPROMold;
  EEPROMSettingsNew EEPROMnew;
};

EEPROMSettingsUnion EEPROM_state;

bool ethernetConnected  = false;
static uint32_t noWiFi = INIT_NOWIFI;
static uint32_t useStaticIP = INIT_USE_STATIC_IP;
static uint32_t useSensorPrefixHA = INIT_USE_SENSOR_PREFIX_HA;

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

// #define _ASYNC_MQTT_LOGLEVEL_               1
#define ETH_CS_PIN        4  // GPIO4
#define ETH_RESET_PIN     16 // GPIO16
#define SHIELD_TYPE       "ESP8266_W5500 Ethernet"

#ifndef AVRISP
#include <SPI.h>
#endif

#include "W5500lwIP.h"
Wiznet5500lwIP eth(ETH_CS_PIN);

#include <WiFiClient.h> // WiFiClient (-> TCPClient), WiFiServer (->TCPServer)
 using TCPClient = WiFiClient;
 using TCPServer = WiFiServer;

bool initEthernet()
{
  SPI.begin();
  SPI.setClockDivider(SPI_CLOCK_DIV4); // 4 MHz
  SPI.setBitOrder(MSBFIRST);
  SPI.setDataMode(SPI_MODE0);
  eth.setDefault();
  delay(100);
  // see also https://github.com/esp8266/Arduino/pull/8395 and https://github.com/esp8266/Arduino/issues/8144
  if (EEPROM_state.EEPROMnew.useStaticIP) {
    Serial.println(F("* [ESP] Using static IP"));
    IPAddress _ip, _gw, _nm;
    _ip.fromString(EEPROM_state.EEPROMnew.static_ip);
    _gw.fromString(EEPROM_state.EEPROMnew.static_gw);
    _nm.fromString(EEPROM_state.EEPROMnew.static_nm);
    eth.config(_ip, _gw, _nm, _gw);
  }
  // eth.setDefault();
  if (!eth.begin()) {
    Serial.println("* [ESP] No Ethernet hardware detected");
    return false;
  } else {
    Serial_println("* [ESP] Connecting to network");
    int timeoutcnt = 0;
    while (!eth.connected()) {
      if (++timeoutcnt > ETHERNET_CONNECTION_TIMEOUT) {
        if (noWiFi) {
          Serial.println(F("* [ESP] Ethernet failed, restart"));
#ifdef REBOOT_REASON
          EEPROM_state.EEPROMnew.rebootReason = REBOOT_REASON_ETH;
          EEPROM.put(0, EEPROM_state);
          EEPROM.commit();
#endif /* REBOOT_REASON */
          delay(500);
          ESP.restart();
          delay(100);
        } else {
          Serial.println(F("* [ESP] Ethernet failed, trying WiFi"));
          return false;
        }
      }
      Serial.print("* [ESP] Waiting for ethernet connection... ");
      Serial.println(timeoutcnt);
      delay(1000);
    }
    Serial_println(F("* [ESP] Ethernet connected"));
    return true;
  }
}
#endif /* ETHERNET */

AsyncMqttClient mqttClient;

uint32_t maxLoopTime = 0;
long int espUptime=0;
bool shouldSaveConfig = false;
static byte crc_gen = CRC_GEN;
static byte crc_feed = CRC_FEED;

#ifdef AVRISP
const char* avrisp_host = "esp8266-avrisp";
const uint16_t avrisp_port = 328;
ESP8266AVRISP* avrprog;
#endif

static byte outputFilter = INIT_OUTPUTFILTER;
static uint32_t outputMode = INIT_OUTPUTMODE;
static uint32_t ESPhwID = INIT_ESP_HW_ID;
#define outputUnknown (outputMode & 0x0008)

const byte Compile_Options = 0 // multi-line statement
#ifdef SAVEPARAMS
+0x01
#endif
#ifdef SAVESCHEDULE
+0x02
#endif
#ifdef SAVEPACKETS
+0x04
#endif
#ifdef TELNET
+0x08
#endif
;

bool telnetConnected = false;

#ifdef TELNET
ESPTelnet telnet;
uint16_t telnet_port=23;

void onTelnetConnect(String ip) {
  Serial_print(F("- Telnet: "));
  Serial_print(ip);
  Serial_println(F(" connected"));
  telnet.print("\nWelcome " + telnet.getIP() + " to ");
  telnet.print(F(WELCOMESTRING_TELNET));
  telnet.print(" compiled ");
  telnet.print(__DATE__);
  telnet.print(" ");
  telnet.print(__TIME__);
#ifdef E_SERIES
  telnet.println(" for E-Series");
#endif /* E_SERIES */
#ifdef F_SERIES
  telnet.println(" for F-Series");
#endif /* F_SERIES */
#ifdef H_SERIES
  telnet.println(" for H-Series");
#endif /* H_SERIES */
#ifdef MHI_SERIES
  telnet.println(" for MHI-Series");
#endif /* MHI_SERIES */
  telnet.println(F("(Use ^] + q  to disconnect.)"));
  telnetConnected = true;
}

void onTelnetDisconnect(String ip) {
  Serial_print(F("- Telnet: "));
  Serial_print(ip);
  Serial_println(F(" disconnected"));
  telnetConnected = false;
}

void onTelnetReconnect(String ip) {
  Serial_print(F("- Telnet: "));
  Serial_print(ip);
  Serial_println(F(" reconnected"));
  telnetConnected = true;
}

void onTelnetConnectionAttempt(String ip) {
  Serial_print(F("- Telnet: "));
  Serial_print(ip);
  Serial_println(F(" tried to connect"));
}
#endif

#ifndef MQTT_PORT
#define MQTT_PORT 1883
#endif
#ifndef MQTT_PORT_STRING
#define MQTT_PORT_STRING "1883"
#endif
#ifndef MQTT_USER
#define MQTT_USER ""
#endif
#ifndef MQTT_PASSWORD
#define MQTT_PASSWORD ""
#endif

#define EMPTY_PAYLOAD 0xFF

uint32_t MQTT_waitCounter = 0;

void client_publish_serial(char* key, char* value) {
  Serial_print(F("** "));
  Serial_print(key);
  Serial_print(F(" "));
  Serial_println(value);
}

bool mqttConnected = false;
bool mqttSetupReady = false;
uint16_t Mqtt_msgSkipLowMem = 0;
uint16_t Mqtt_msgSkipNotConnected = 0;
uint16_t Mqtt_disconnects = 0;
uint16_t Mqtt_disconnectSkippedPackets = 0;
uint16_t Mqtt_disconnectTime = 0;
uint16_t Mqtt_disconnectTimeTotal = 0;
uint32_t Mqtt_acknowledged = 0; // does not increment in case of QOS=0
uint32_t Mqtt_gap = 0;          // does not increment in case of QOS=0
uint16_t prevpacketId = 0;

#define MAXWAIT 20

uint32_t mqttPublished = 0;
bool ignoreSerial = false;

void client_publish_mqtt(char* key, char* value, bool retain = MQTT_RETAIN) {
  if (mqttConnected) {
    byte i = 0;
    while ((ESP.getMaxFreeBlockSize() < MQTT_MIN_FREE_MEMORY) && (i++ < MAXWAIT)) {
      MQTT_waitCounter++;
      delay(5);
    }
    if (ESP.getMaxFreeBlockSize() >= MQTT_MIN_FREE_MEMORY) {
      mqttPublished++;
      mqttClient.publish(key, MQTT_QOS, retain, value);
    } else {
      Mqtt_msgSkipLowMem++;
    }
  } else {
    Mqtt_msgSkipNotConnected++;
  }
}

void client_publish_telnet(char* key, char* value) {
  if (telnetConnected) {
    telnet.println((String) key + ' ' + (String) value);
    telnet.loop();
  }
}

char sprint_value[SPRINT_VALUE_LEN];
uint32_t Sprint_buffer_overflow = 0;

#define Sprint_P(pubSerial, pubMqtt, pubTelnet, ...) { \
  if (snprintf_P(sprint_value, SPRINT_VALUE_LEN, __VA_ARGS__) > SPRINT_VALUE_LEN - 2) { \
    Sprint_buffer_overflow++; \
  }; \
  if (pubSerial) client_publish_serial(mqttSignal, sprint_value);\
  if (pubMqtt) client_publish_mqtt(mqttSignal, sprint_value);\
  if (pubTelnet) client_publish_telnet(mqttSignal, sprint_value);\
};

#define HA_KEY snprintf_P(ha_mqttKey, HA_KEY_LEN, PSTR("%s/%s/%s%c%c_%s/config"), HA_PREFIX, haDeviceID, useSensorPrefixHA ? HA_SENSOR_PREFIX : "", mqtt_key[-4], mqtt_key[-2], mqtt_key);
#define HA_VALUE snprintf_P(ha_mqttValue, HA_VALUE_LEN, PSTR("{\"name\":\"%c%c_%s\",\"stat_t\":\"%s\",%s\"uniq_id\":\"%c%c_%s%s\",%s%s%s\"dev\":{\"name\":\"%s\",\"ids\":[\"%s\"],\"mf\":\"%s\",\"mdl\":\"%s\",\"sw\":\"%s\"}}"),\
  /* name */         mqtt_key[-4], mqtt_key[-2], mqtt_key, \
  /* stat_t */       mqtt_key - MQTT_KEY_PREFIXLEN,\
  /* state_class */  ((stateclass == 2) ? "\"state_class\":\"total_increasing\"," : ((stateclass == 1) ? "\"state_class\":\"measurement\"," : "")),\
  /* uniq_id */      mqtt_key[-4], mqtt_key[-2], mqtt_key, haPostfix, \
  /* dev_cla */      ((stateclass == 2) ? "\"dev_cla\":\"energy\"," : ""),\
  /* unit_of_meas */ ((uom == 13) ? "bar" : ((uom == 12) ? "\"unit_of_meas\":\"%\"," :  ((uom == 11) ? "\"unit_of_meas\":\"Hz\"," :  ((uom == 10) ? "\"unit_of_meas\":\"A\"," :  ((uom == 9) ? "\"unit_of_meas\":\"events\"," : ((uom == 8) ? "\"unit_of_meas\":\"byte\"," : ((uom == 7) ? "\"unit_of_meas\":\"ms\"," : ((uom == 6) ? "\"unit_of_meas\":\"s\"," : ((uom == 5) ? "\"unit_of_meas\":\"hours\"," : ((uom == 4) ? "\"unit_of_meas\":\"kWh\"," : ((uom == 3) ? "\"unit_of_meas\":\"l/min\"," : ((uom == 2) ? "\"unit_of_meas\":\"kW\"," : ((uom == 1) ? "\"unit_of_meas\":\"°C\"," : ""))))))))))))),\
  /* ic */ ((uom == 13) ? "\"ic\":\"mdi:water\"," : ((uom == 12) ? "\"ic\":\"mdi:valve\"," :  ((uom == 11) ? "\"ic\":\"mdi:heat-pump\"," :  ((uom == 10) ? "\"ic\":\"mdi:heat-pump\"," : ((uom == 9) ? "\"ic\":\"mdi:counter\"," : ((uom == 8) ? "\"ic\":\"mdi:memory\"," : ((uom == 7) ? "\"ic\":\"mdi:clock-outline\"," : ((uom == 6) ? "\"ic\":\"mdi:clock-outline\"," : ((uom == 5) ? "\"ic\":\"mdi:clock-outline\"," : ((uom == 4) ? "\"ic\":\"mdi:transmission-tower\"," : ((uom == 3) ? "\"ic\":\"mdi:water-boiler\"," : ((uom == 2) ? "\"ic\":\"mdi:transmission-tower\"," : ((uom == 1) ? "\"ic\":\"mdi:coolant-temperature\"," : ""))))))))))))),\
  /* device */       \
    /* name */         haDeviceName,\
    /* ids */\
      /* id */         haDeviceID,\
    /* mf */           HA_MF,\
    /* mdl */          HA_DEVICE_MODEL,\
    /* sw */           HA_SW);

// Include Daikin product-dependent header file for parameter conversion
// include here such that Sprint_P(PSTR()) is available in header file code

#ifdef E_SERIES
#include "P1P2_Daikin_ParameterConversion_EHYHB.h"
#endif /* E_SERIES */
#ifdef F_SERIES
#include "P1P2_Daikin_ParameterConversion_F.h"
#endif /* F_SERIES */
#ifdef H_SERIES
#include "P1P2_Hitachi_ParameterConversion_HLINK2.h"
#endif /* H_SERIES */
#ifdef MHI_SERIES
#include "P1P2_Hitachi_ParameterConversion_HLINK2.h" // for now TODO
#endif /* MHI_SERIES */
#ifdef T_SERIES
#include "P1P2_Hitachi_ParameterConversion_HLINK2.h" // for now TODO
#endif /* T_SERIES */

static byte throttle = 1;
static byte throttleValue = THROTTLE_VALUE;

void onMqttConnect(bool sessionPresent) {
  Serial.println(F("* [ESP] Connected to MQTT server"));
  int result = mqttClient.subscribe(mqttCommands, MQTT_QOS);
  Sprint_P(true, false, false, PSTR("* [ESP] Subscribed to %s with result %i"), mqttCommands, result);
  result = mqttClient.subscribe(mqttCommandsNoIP, MQTT_QOS);
  Sprint_P(true, false, false, PSTR("* [ESP] Subscribed to %s with result %i"), mqttCommandsNoIP, result);
  result = mqttClient.subscribe("homeassistant/status", MQTT_QOS);
  Sprint_P(true, false, false, PSTR("* [ESP] Subscribed to homeassistant/status with result %i"), result);
#ifdef MQTT_INPUT_BINDATA
  //result = mqttClient.subscribe("P1P2/X/#", MQTT_QOS);
  //Sprint_P(true, false, false, PSTR("* [ESP] Subscribed to X/# with result %i"), result);
  if (!mqttInputBinData[MQTT_KEY_PREFIXIP - 1]) {
    mqttInputBinData[MQTT_KEY_PREFIXIP - 1] = '/';
    mqttInputBinData[MQTT_KEY_PREFIXIP] = '#';
    mqttInputBinData[MQTT_KEY_PREFIXIP + 1] = '\0';
  }
  result = mqttClient.subscribe(mqttInputBinData, MQTT_QOS);
  Sprint_P(true, false, false, PSTR("* [ESP] Subscribed to %s with result %i"), mqttInputBinData, result);
  mqttInputBinData[MQTT_KEY_PREFIXIP - 1] = '\0';
#endif
#ifdef MQTT_INPUT_HEXDATA
  //result = mqttClient.subscribe("P1P2/R/#", MQTT_QOS);
  //Sprint_P(true, false, false, PSTR("* [ESP] Subscribed to R/# with result %i"), result);
  if (!mqttInputHexData[MQTT_KEY_PREFIXIP - 1]) {
    mqttInputHexData[MQTT_KEY_PREFIXIP - 1] = '/';
    mqttInputHexData[MQTT_KEY_PREFIXIP] = '#';
    mqttInputHexData[MQTT_KEY_PREFIXIP + 1] = '\0';
  }
  result = mqttClient.subscribe(mqttInputHexData, MQTT_QOS);
  Sprint_P(true, false, false, PSTR("* [ESP] Subscribed to %s with result %i"), mqttInputHexData, result);
  mqttInputHexData[MQTT_KEY_PREFIXIP - 1] = '\0';
#endif
  mqttConnected = true;
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Serial_print(F("* [ESP] Disconnected from MQTT server "));
  Serial_println(Mqtt_disconnects);
  Sprint_P(true, false, false, PSTR("* [ESP] Disconnected from MQTT."));
  Mqtt_disconnects++;
  mqttConnected = false;
}

void onMqttPublish(uint16_t packetId) {
  Sprint_P(true, false, false, PSTR("* [ESP] Publish acknowledged of packetId %i"), packetId);
  Mqtt_acknowledged++;
  if (packetId != prevpacketId + 1) Mqtt_gap++;
  prevpacketId = packetId;
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
  shouldSaveConfig = true;
}

uint32_t milliInc = 0;
uint32_t prevMillis = 0; //millis();
static uint32_t reconnectTime = 0;
static uint32_t throttleStart = 0;

void ATmega_dummy_for_serial() {
  Sprint_P(true, true, true, PSTR("* [ESP] Two dummy lines to ATmega."));
  Serial.print(F(SERIAL_MAGICSTRING));
  Serial.println(F("* Dummy line 1."));
  Serial.print(F(SERIAL_MAGICSTRING));
  Serial.println(F("* Dummy line 2."));
}

bool MQTT_commandReceived = false;
char MQTT_commandString[MAX_COMMAND_LENGTH];
bool telnetCommandReceived = false;
char telnetCommandString[MAX_COMMAND_LENGTH];
char readBuffer[RB];
#if defined MQTT_INPUT_BINDATA || defined MQTT_INPUT_HEXDATA
char MQTT_readBuffer[MQTT_RB];
volatile uint16_t MQTT_readBufferH = 0;
volatile uint16_t MQTT_readBufferT = 0;
#endif /* MQTT_INPUT_BINDATA || MQTT_INPUT_HEXDATA */
static char* rb_buffer = readBuffer;
static uint16_t serial_rb = 0;
static int c;
static byte ESP_serial_input_Errors_Data_Short = 0;
static byte ESP_serial_input_Errors_CRC = 0;

static uint32_t ATmega_uptime_prev = 0;
static byte saveRebootReason = REBOOT_REASON_UNKNOWN;

WiFiManager wifiManager;

void handleCommand(char* cmdString) {
// handles a single command (not necessarily '\n'-terminated) received via telnet or MQTT (P1P2/W)
// most of these messages are fowarded over serial to P1P2Monitor on ATmega
// some messages are (also) handled on the ESP
  int temp = 0;
  byte temphex = 0;

  Serial_print(F("* [ESP] handleCommand cmdString ->"));
  Serial_print((char*) cmdString);
  Serial_print(F("<-"));
  Serial_println();
  int mqttInputByte4 = 0;
  int n;
  int result = 0;
  IPAddress local_ip;
#ifdef ETHERNET
  if (ethernetConnected) {
    local_ip = eth.localIP();
  } else {
#else
  {
#endif
    local_ip = WiFi.localIP();
  }
  switch (cmdString[0]) {
    case 'a': // reset ATmega
    case 'A': Sprint_P(true, true, true, PSTR("* [ESP] Hard resetting ATmega..."));
              digitalWrite(RESET_PIN, LOW);
              pinMode(RESET_PIN, OUTPUT);
              delay(1);
              pinMode(RESET_PIN, INPUT);
              digitalWrite(RESET_PIN, HIGH);
              delay(200);
              ATmega_dummy_for_serial();
              ATmega_uptime_prev = 0;
              break;
    case 'b': // display or set MQTT settings
    case 'B': if ((n = sscanf((const char*) (cmdString + 1), "%19s %i %80s %80s %i %i %i %i", &EEPROM_state.EEPROMnew.mqttServer, &EEPROM_state.EEPROMnew.mqttPort, &EEPROM_state.EEPROMnew.mqttUser, &EEPROM_state.EEPROMnew.mqttPassword, &mqttInputByte4, &ESPhwID, &noWiFi, &useSensorPrefixHA)) > 0) {
                Sprint_P(true, true, true, PSTR("* [ESP] Writing new MQTT settings to EEPROM"));
                if (n > 4) EEPROM_state.EEPROMnew.mqttInputByte4 = mqttInputByte4;
                if ((n > 5) && (ESPhwID != EEPROM_state.EEPROMnew.ESPhwID)) {
                  Sprint_P(true, true, true, PSTR("* [ESP] Reboot required to change ESPhwID"));
#ifdef REBOOT_REASON
                  EEPROM_state.EEPROMnew.rebootReason = REBOOT_REASON_BCMD;
#endif /* REBOOT_REASON */
                  EEPROM_state.EEPROMnew.ESPhwID = ESPhwID;
                }
                if ((n > 6) && (EEPROM_state.EEPROMnew.noWiFi != noWiFi)) {
                  Sprint_P(true, true, true, PSTR("* [ESP] Reboot required to change noWiFi"));
#ifdef REBOOT_REASON
                  EEPROM_state.EEPROMnew.rebootReason = REBOOT_REASON_BCMD;
#endif /* REBOOT_REASON */
                  EEPROM_state.EEPROMnew.noWiFi = noWiFi;
                }
                if ((n > 7)  && (useSensorPrefixHA != EEPROM_state.EEPROMnew.useSensorPrefixHA)) {
                  Sprint_P(true, true, true, PSTR("* [ESP] Reboot required to rediscover new sensor names"));
#ifdef REBOOT_REASON
                  EEPROM_state.EEPROMnew.rebootReason = REBOOT_REASON_BCMD;
#endif /* REBOOT_REASON */
                  EEPROM_state.EEPROMnew.useSensorPrefixHA = useSensorPrefixHA;
                }
              }
              EEPROM.put(0, EEPROM_state);
              EEPROM.commit();
              if (n > 0) {
                Sprint_P(true, true, true, PSTR("* [ESP] MQTT_server set to %s"), EEPROM_state.EEPROMnew.mqttServer);
              } else {
                Sprint_P(true, true, true, PSTR("* [ESP] MQTT_server is %s"), EEPROM_state.EEPROMnew.mqttServer);
              }
              if (n > 1) {
                Sprint_P(true, true, true, PSTR("* [ESP] MQTT_port set to %i"), EEPROM_state.EEPROMnew.mqttPort);
              } else {
                Sprint_P(true, true, true, PSTR("* [ESP] MQTT_port is %i"), EEPROM_state.EEPROMnew.mqttPort);
              }
              if (n > 2) {
                Sprint_P(true, true, true, PSTR("* [ESP] MQTT_user set to %s"), EEPROM_state.EEPROMnew.mqttUser);
              } else {
                Sprint_P(true, true, true, PSTR("* [ESP] MQTT_user is %s"), EEPROM_state.EEPROMnew.mqttUser);
              }
              if (n > 3) {
                Sprint_P(true, true, true, PSTR("* [ESP] New MQTT_password set") /*, EEPROM_state.EEPROMnew.mqttPassword */);
              } else {
                // Sprint_P(true, true, true, PSTR("* [ESP] MQTT_password is %s"), EEPROM_state.EEPROMnew.mqttPassword);
              }
              if (n > 4) {
                Sprint_P(true, true, true, PSTR("* [ESP] mqttInputByte4 set to %i"), EEPROM_state.EEPROMnew.mqttInputByte4);
              } else {
                Sprint_P(true, true, true, PSTR("* [ESP] mqttInputByte4 is %i"), EEPROM_state.EEPROMnew.mqttInputByte4);
              }
              if (n > 5) {
                Sprint_P(true, true, true, PSTR("* [ESP] ESPhwID set to %i"), EEPROM_state.EEPROMnew.ESPhwID);
              } else {
                Sprint_P(true, true, true, PSTR("* [ESP] ESPhwID %i"), EEPROM_state.EEPROMnew.ESPhwID);
              }
              if (n > 6) {
                Sprint_P(true, true, true, PSTR("* [ESP] noWiFi set to %i"), EEPROM_state.EEPROMnew.noWiFi);
              } else {
                Sprint_P(true, true, true, PSTR("* [ESP] noWiFi is %i"), EEPROM_state.EEPROMnew.noWiFi);
              }
              if (n > 7) {
                Sprint_P(true, true, true, PSTR("* [ESP] useSensorPrefixHA set to %i"), EEPROM_state.EEPROMnew.useSensorPrefixHA);
              } else {
                Sprint_P(true, true, true, PSTR("* [ESP] useSensorPrefixHA is %i"), EEPROM_state.EEPROMnew.useSensorPrefixHA);
              }
              if ((n > 5) || (n && (outputMode & 0x40000))) {
                Sprint_P(true, true, true, PSTR("* [ESP] Restart ESP"));
                delay(100);
                ESP.restart();
                delay(100);
              }
              Sprint_P(true, true, true, PSTR("* [ESP] Local IP address: %i.%i.%i.%i"), local_ip[0], local_ip[1], local_ip[2], local_ip[3]);
              // TODO enable in future WiFiManager version: if (WiFi.isConnected()) Sprint_P(true, true, true, PSTR("* [ESP] Connected to WiFi SSID %s"), wifiManager.getWiFiSSID());
              // Sprint_P(true, true, true, PSTR("* [ESP] Pass of AP is %s"), wifiManager.getWiFiPass());
              if (n > 0) {
                if (mqttClient.connected()) {
                  Sprint_P(true, true, true, PSTR("* [ESP] MQTT Client connected, disconnect"));
                  // unsubscribe topics here before disconnect?
                  mqttClient.disconnect();
                  delay(1000);
                }
              }
#ifdef MQTT_INPUT_BINDATA
              if (n > 4) {
                mqttInputBinData[MQTT_KEY_PREFIXIP - 1] = mqttInputByte4 ? '/' : '\0';
                mqttInputBinData[MQTT_KEY_PREFIXIP] = (EEPROM_state.EEPROMnew.mqttInputByte4 / 100) + '0';
                mqttInputBinData[MQTT_KEY_PREFIXIP + 1] = (EEPROM_state.EEPROMnew.mqttInputByte4 % 100) / 10 + '0';
                mqttInputBinData[MQTT_KEY_PREFIXIP + 2] = (EEPROM_state.EEPROMnew.mqttInputByte4 % 10) + '0';
                Sprint_P(true, true, true, PSTR("* [ESP] mqttInputBinData set to %s"), mqttInputBinData);
              } else {
                Sprint_P(true, true, true, PSTR("* [ESP] mqttInputBinData %s"), mqttInputBinData);
              }
#endif
#ifdef MQTT_INPUT_HEXDATA
              if (n > 4) {
                mqttInputHexData[MQTT_KEY_PREFIXIP - 1] = mqttInputByte4 ? '/' : '\0';
                mqttInputHexData[MQTT_KEY_PREFIXIP] = (EEPROM_state.EEPROMnew.mqttInputByte4 / 100) + '0';
                mqttInputHexData[MQTT_KEY_PREFIXIP + 1] = (EEPROM_state.EEPROMnew.mqttInputByte4 % 100) / 10 + '0';
                mqttInputHexData[MQTT_KEY_PREFIXIP + 2] = (EEPROM_state.EEPROMnew.mqttInputByte4 % 10) + '0';
                Sprint_P(true, true, true, PSTR("* [ESP] mqttInputHexData set to %s"), mqttInputHexData);
              } else {
                Sprint_P(true, true, true, PSTR("* [ESP] mqttInputHexData %s"), mqttInputHexData);
              }
#endif
              if (n > 0) {
                if (mqttClient.connected()) {
                  Sprint_P(true, true, true, PSTR("* [ESP] MQTT Client still connected ?"));
                  delay(500);
                } else {
                  Sprint_P(true, true, true, PSTR("* [ESP] MQTT Client disconnected"));
                }
                mqttClient.setServer(EEPROM_state.EEPROMnew.mqttServer, EEPROM_state.EEPROMnew.mqttPort);
                mqttClient.setCredentials((EEPROM_state.EEPROMnew.mqttUser[0] == '\0') ? 0 : EEPROM_state.EEPROMnew.mqttUser, (EEPROM_state.EEPROMnew.mqttPassword[0] == '\0') ? 0 : EEPROM_state.EEPROMnew.mqttPassword);
                Sprint_P(true, true, true, PSTR("* [ESP] MQTT Try to connect"));
                Mqtt_disconnectTime = 0;
                mqttClient.connect();
                delay(500);
                if (mqttClient.connected()) {
                  Sprint_P(true, true, true, PSTR("* [ESP] MQTT client connected with EEPROMnew settings"));
                  reconnectTime = espUptime;
                  throttleStart = espUptime;
                  throttleValue = THROTTLE_VALUE;
                } else {
                  Sprint_P(true, true, true, PSTR("* [ESP] MQTT connection failed, retrying in 5 seconds"));
                  reconnectTime = espUptime + 5;
                }
              }
              break;
    case 'd': // reset ESP
    case 'D': if (sscanf((const char*) (cmdString + 1), "%d", &temp) == 1) {
                switch (temp) {
                  case 0 : Sprint_P(true, true, true, PSTR("* [ESP] Restarting ESP..."));
#ifdef REBOOT_REASON
                           EEPROM_state.EEPROMnew.rebootReason = REBOOT_REASON_D0;
                           EEPROM.put(0, EEPROM_state);
                           EEPROM.commit();
#endif /* REBOOT_REASON */
                           delay(100);
                           ESP.restart();
                           delay(100);
                           break;
                  case 1 : Sprint_P(true, true, true, PSTR("* [ESP] Resetting ESP..."));
#ifdef REBOOT_REASON
                           EEPROM_state.EEPROMnew.rebootReason = REBOOT_REASON_D1;
                           EEPROM.put(0, EEPROM_state);
                           EEPROM.commit();
#endif /* REBOOT_REASON */
                           delay(100);
                           ESP.reset();
                           delay(100);
                           break;
                  case 2 : Sprint_P(true, true, true, PSTR("* [ESP] Resetting maxLoopTime"));
                           maxLoopTime = 0;
                           break;
                  case 3 : Sprint_P(true, true, true, PSTR("* [ESP] Resetting data structures"));
                           throttleStart = espUptime;
                           throttleValue = THROTTLE_VALUE;
                           resetDataStructures();
                           break;
                  default: Sprint_P(true, true, true, PSTR("* [ESP] Specify D0=RESTART-ESP D1=RESET-ESP D2=RESET-maxLoopTime D3=reset-data-structures (w throttling)"));
                }
              } else {
                Sprint_P(true, true, true, PSTR("* [ESP] Specify D0=RESTART-ESP D1=RESET-ESP D2=RESET-maxLoopTime"));
              }
              break;
    case 'i': // static IP address (for now, WiFi only) //  TODO ethernet
    case 'I': if ((n = sscanf((const char*) (cmdString + 1), "%i %15s %15s %15s %15s", &useStaticIP, &EEPROM_state.EEPROMnew.static_ip, &EEPROM_state.EEPROMnew.static_gw, &EEPROM_state.EEPROMnew.static_nm)) > 0) {
                Sprint_P(true, true, true, PSTR("* [ESP] Writing new static ip settings to EEPROM"));
                EEPROM_state.EEPROMnew.useStaticIP = useStaticIP;
#ifdef REBOOT_REASON
                EEPROM_state.EEPROMnew.rebootReason = REBOOT_REASON_STATICIP;
#endif /* REBOOT_REASON */
                EEPROM.put(0, EEPROM_state);
                EEPROM.commit();
              }
              if (n > 0) {
                Sprint_P(true, true, true, PSTR("* [ESP] useStaticIP set to %i"), EEPROM_state.EEPROMnew.useStaticIP);
              } else {
                Sprint_P(true, true, true, PSTR("* [ESP] useStaticIP is %i"), EEPROM_state.EEPROMnew.useStaticIP);
              }
              if (n > 1) {
                Sprint_P(true, true, true, PSTR("* [ESP] static IPv4 address set to %s"), EEPROM_state.EEPROMnew.static_ip);
              } else {
                Sprint_P(true, true, true, PSTR("* [ESP] static IPv4 address is %s"), EEPROM_state.EEPROMnew.static_ip);
              }
              if (n > 2) {
                Sprint_P(true, true, true, PSTR("* [ESP] static IPv4 gateway set to %s"), EEPROM_state.EEPROMnew.static_gw);
              } else {
                Sprint_P(true, true, true, PSTR("* [ESP] static IPv4 gateway is %s"), EEPROM_state.EEPROMnew.static_gw);
              }
              if (n > 3) {
                Sprint_P(true, true, true, PSTR("* [ESP] static IPv4 netmask set to %s"), EEPROM_state.EEPROMnew.static_nm);
              } else {
                Sprint_P(true, true, true, PSTR("* [ESP] static IPv4 netmask is %s"), EEPROM_state.EEPROMnew.static_nm);
              }
              if (n > 0) {
                Sprint_P(true, true, true, PSTR("* [ESP] Restart required to use new settings"));
                delay(500);
                ESP.restart();
                delay(100);
              }
              break;
    case 'j': // OutputMode
    case 'J': if (sscanf((const char*) (cmdString + 1), "%x", &temp) == 1) {
                outputMode = temp;
                if (outputMode != EEPROM_state.EEPROMnew.outputMode) {
                  EEPROM_state.EEPROMnew.outputMode = outputMode;
                  EEPROM.put(0, EEPROM_state);
                  EEPROM.commit();
                }
                Sprint_P(true, true, true, PSTR("* [ESP] Outputmode set to 0x%04X"), outputMode);
              } else {
                Sprint_P(true, true, true, PSTR("* [ESP] Outputmode 0x%04X is sum of"), outputMode);
                Sprint_P(true, true, true, PSTR("* [ESP] %ix 0x0001 to output raw packet data (including pseudo-packets) over mqtt P1P2/R/xxx"), outputMode  & 0x01);
                Sprint_P(true, true, true, PSTR("* [ESP] %ix 0x0002 to output mqtt individual parameter data over mqtt P1P2/P/xxx/#"), (outputMode >> 1) & 0x01);
                Sprint_P(true, true, true, PSTR("* [ESP] %ix 0x0004 to output json data over mqtt P1P2/J/xxx"), (outputMode >> 2) & 0x01);
                Sprint_P(true, true, true, PSTR("* [ESP] %ix 0x0008 to have mqtt/json include parameters even if functionality is unknown, warning: easily overloads ATmega/ESP (best to combine this with outputfilter >=1)"), (outputMode >> 3) & 0x01);
                Sprint_P(true, true, true, PSTR("* [ESP] %ix 0x0010 ESP to output raw data over telnet"), (outputMode >> 4) & 0x01);
                Sprint_P(true, true, true, PSTR("* [ESP] %ix 0x0020 to output mqtt individual parameter data over telnet"), (outputMode >> 5) & 0x01);
                Sprint_P(true, true, true, PSTR("* [ESP] %ix 0x0040 to output json data over telnet"), (outputMode >> 6) & 0x01);
                Sprint_P(true, true, true, PSTR("* [ESP] %ix 0x0080 (reserved for adding time string in R output)"), (outputMode >> 7) & 0x01);
                Sprint_P(true, true, true, PSTR("* [ESP] %ix 0x0100 ESP to output raw data over serial"), (outputMode >> 8) & 0x01);
                Sprint_P(true, true, true, PSTR("* [ESP] %ix 0x0200 to output mqtt individual parameter data over serial"), (outputMode >> 9) & 0x01);
                Sprint_P(true, true, true, PSTR("* [ESP] %ix 0x0400 to output json data over serial"), (outputMode >> 10) & 0x01);
                Sprint_P(true, true, true, PSTR("* [ESP] %ix 0x0800 to output raw bin data over P1P2/X/xxx"), (outputMode >> 11) & 0x01);
                Sprint_P(true, true, true, PSTR("* [ESP] %ix 0x1000 to output timing data also over P1P2/R/xxx (prefix: C) and via telnet"), (outputMode >> 12) & 0x01);
                Sprint_P(true, true, true, PSTR("* [ESP] %ix 0x2000 to output error data also over P1P2/R/xxx (prefix: *)"), (outputMode >> 13) & 0x01);
                Sprint_P(true, true, true, PSTR("* [ESP] %ix 0x4000 to use P1P2/R/xxx as input (requires MQTT_INPUT_HEXDATA)"), (outputMode >> 14) & 0x01);
                Sprint_P(true, true, true, PSTR("* [ESP] %ix 0x8000 to use P1P2/X/xxx as input (requires MQTT_INPUT_BINDATA)"), (outputMode >> 15) & 0x01);
                Sprint_P(true, true, true, PSTR("* [ESP] %ix 0x10000 to include non-HACONFIG parameters in P1P2/P/# "), (outputMode >> 16) & 0x01);
                Sprint_P(true, true, true, PSTR("* [ESP] %ix 0x20000 to add all pseudo parameters to HA in P1P2/P/# "), (outputMode >> 17) & 0x01);
                Sprint_P(true, true, true, PSTR("* [ESP] %ix 0x40000 to restart ESP after MQTT reconnect "), (outputMode >> 18) & 0x01);
                Sprint_P(true, true, true, PSTR("* [ESP] %ix 0x80000 to restart data communication after MQTT reconnect "), (outputMode >> 19) & 0x01);
              }
              break;
    case 's': // OutputFilter
    case 'S': if (sscanf((const char*) (cmdString + 1), "%d", &temp) == 1) {
                if (temp > 3) temp = 3;
                outputFilter = temp;
                if (outputFilter != EEPROM_state.EEPROMnew.outputFilter) {
                  EEPROM_state.EEPROMnew.outputFilter = outputFilter;
                  EEPROM.put(0, EEPROM_state);
                  EEPROM.commit();
                }
              } else {
                temp = 99;
              }
              switch (outputFilter) {
                case 0 : Sprint_P(true, true, true, PSTR("* [ESP] Outputfilter %s0 all parameters, no filter"), (temp != 99) ? "set to " : ""); break;
                case 1 : Sprint_P(true, true, true, PSTR("* [ESP] Outputfilter %s1 only changed data"), (temp != 99) ? "set to " : ""); break;
                case 2 : Sprint_P(true, true, true, PSTR("* [ESP] Outputfilter %s2 only changed data, excluding temp/flow"), (temp != 99) ? "set to " : ""); break;
                case 3 : Sprint_P(true, true, true, PSTR("* [ESP] Outputfilter %s3 only changed data, excluding temp/flow/time"), (temp != 99) ? "set to " : ""); break;
                default: Sprint_P(true, true, true, PSTR("* [ESP] Outputfilter illegal state %i"), outputFilter); break;
              }
              break;
    case '\0':break;
    case '*': break;
    case 'v':
    case 'V':
    case 'g':
    case 'G':
    case 'h':
    case 'H': // commands v/g/h handled both by P1P2-bridge-esp8266 and P1P2Moniotr
              switch (cmdString[0]) {
                case 'v':
                case 'V': Sprint_P(true, true, true, PSTR(WELCOMESTRING));
                          Sprint_P(true, true, true, PSTR("* [ESP] Compiled %s %s"), __DATE__, __TIME__);
#ifdef E_SERIES
                          Sprint_P(true, true, true, PSTR("* [ESP] E-Series"));
#endif /* E_SERIES */
#ifdef F_SERIES
                          Sprint_P(true, true, true, PSTR("* [ESP] F-Series"));
#endif /* F_SERIES */
#ifdef H_SERIES
                          Sprint_P(true, true, true, PSTR("* [ESP] H-Series"));
#endif /* H_SERIES */
#ifdef MHI_SERIES
                          Sprint_P(true, true, true, PSTR("* [ESP] MHI-Series"));
#endif /* MHI_SERIES */
                          Sprint_P(true, true, true, PSTR("* [ESP] ESP_hw_identifier %i"), ESPhwID);
                          if (mqttClient.connected()) {
                            Sprint_P(true, true, true, PSTR("* [ESP] Connected to MQTT server"));
                          } else {
                            Sprint_P(true, true, true, PSTR("* [ESP] Warning: not connected to MQTT server"));
                          }
                          Sprint_P(true, true, true, PSTR("* [ESP] MQTT Clientname = %s"), MQTT_CLIENTNAME);
                          Sprint_P(true, true, true, PSTR("* [ESP] MQTT User = %s"), EEPROM_state.EEPROMnew.mqttUser);
                          // Sprint_P(true, true, true, PSTR("* [ESP] MQTT Password = %s"), EEPROM_state.EEPROMnew.mqttPassword);
                          Sprint_P(true, true, true, PSTR("* [ESP] MQTT Server = %s"), EEPROM_state.EEPROMnew.mqttServer);
                          Sprint_P(true, true, true, PSTR("* [ESP] MQTT Port = %i"), EEPROM_state.EEPROMnew.mqttPort);
                          Sprint_P(true, true, true, PSTR("* [ESP] ESP reboot reason = 0x%2X"), saveRebootReason);
                          Sprint_P(true, true, true, PSTR("* [ESP] outputMode = 0x%X"), outputMode);
                          Sprint_P(true, true, true, PSTR("* [ESP] outputFilter = %i"), outputFilter);
                          Sprint_P(true, true, true, PSTR("* [ESP] mqttInputByte4 = %i"), mqttInputByte4);
                          Sprint_P(true, true, true, PSTR("* [ESP] EEPROM version = %i"), EEPROM_state.EEPROMnew.EEPROM_version);
                          if (sscanf((const char*) (cmdString + 1), "%2x", &temp) == 1) {
                            if (temp < 2) {
                              Sprint_P(true, true, true, PSTR("* [ESP] Warning: verbose < 2 not supported by P1P2-bridge-esp8266, changing to verbosity mode 3"));
                              cmdString[1] = '3';
                              cmdString[2] = '\n';
                            }
                            ignoreSerial = (temp == 9); // TODO document or remove
                            if (ignoreSerial) {
                              rb_buffer = readBuffer;
                              serial_rb = 0;
                            }
                          }
                          break;
                case 'g':
                case 'G': if (sscanf((const char*) (cmdString + 1), "%2x", &temphex) == 1) {
                            crc_gen = temphex;
                            Sprint_P(true, true, true, PSTR("* [ESP] CRC_gen set to 0x%02X"), crc_gen);
                          } else {
                            Sprint_P(true, true, true, PSTR("* [ESP] CRC_gen 0x%02X"), crc_gen);
                          }
                          break;
                case 'h':
                case 'H': if (sscanf((const char*) (cmdString + 1), "%2x", &temphex) == 1) {
                            crc_feed = temphex;
                            Sprint_P(true, true, true, PSTR("* [ESP] CRC_feed set to 0x%02X"), crc_feed);
                          } else {
                            Sprint_P(true, true, true, PSTR("* [ESP] CRC_feed 0x%02X"), crc_feed);
                          }
                          break;
                default : break;
              }
              // fallthrough for v/g/h commands handled both by P1P2-bridge-esp8266 and P1P2Monitor
              // 'c' 'C' 'p' 'P' 'e' 'E' 'f' 'F' 't' 'T" 'o' 'O" 'u' 'U" 'x' 'X" 'w' 'W" 'k' 'K' 'l' 'L' 'q' 'Q' 'm' 'M' 'z' 'Z' 'r' 'R' 'n' 'N' 'y' 'Y' handled by P1P2Monitor (except for 'f' 'F" for H-link)
    default : Sprint_P(true, true, true, PSTR("* [ESP] To ATmega: ->%s<-"), cmdString);
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

static byte pseudo0D = 0;
static byte pseudo0E = 0;
static byte pseudo0F = 0;

#if defined MQTT_INPUT_BINDATA || defined MQTT_INPUT_HEXDATA
void MQTT_readBuffer_writeChar (const char c) {
  MQTT_readBuffer[MQTT_readBufferH] = c;
  if (++MQTT_readBufferH >= MQTT_RB) MQTT_readBufferH = 0;
}

void MQTT_readBuffer_writeHex (byte c) {
  byte p = c >> 4;
  MQTT_readBuffer[MQTT_readBufferH] = (p < 10) ? '0' + p : 'A' + p - 10;
  if (++MQTT_readBufferH >= MQTT_RB) MQTT_readBufferH = 0;
  p = c & 0x0F;
  MQTT_readBuffer[MQTT_readBufferH] = (p < 10) ? '0' + p : 'A' + p - 10;
  if (++MQTT_readBufferH >= MQTT_RB) MQTT_readBufferH = 0;
}

int16_t MQTT_readBuffer_readChar (void) {
// returns -1 if buffer empty, or c otherwise
  if (MQTT_readBufferT == MQTT_readBufferH) return -1;
  char c = MQTT_readBuffer[MQTT_readBufferT];
  if (++MQTT_readBufferT >= MQTT_RB) MQTT_readBufferT = 0;
  return c;
}
#endif

byte haOnline = 0;
#define MAX_HASTATUS_LENGTH 10
char MQTT_haStatus[MAX_HASTATUS_LENGTH];

void onMqttMessage(char* topic, char* payload, const AsyncMqttClientMessageProperties& properties,
                   const size_t& len, const size_t& index, const size_t& total) {
  static bool MQTT_drop = false;
  (void) payload;
  if (!len) {
    Serial_println(F("* [ESP] Empty MQTT payload received and ignored"));
  } else if (index + len > 0xFF) {
    Serial_println(F("* [ESP] Received MQTT payload too long"));
  } else if ((!strcmp(topic, mqttCommands)) || (!strcmp(topic, mqttCommandsNoIP))) {
    if (MQTT_commandReceived) {
      Serial_println(F("* [ESP] Ignoring MQTT command, previous command is still being processed"));
    } else {
      if (total + 1 >= MAX_COMMAND_LENGTH) {
        Serial_println(F("* [ESP] Received MQTT command too long"));
      } else {
        strlcpy(MQTT_commandString + index, payload, len + 1); // create null-terminated copy
        MQTT_commandString[index + len] = '\0';
        if (index + len == total) MQTT_commandReceived = true;
      }
    }
  } else if (!strcmp(topic, "homeassistant/status")) {
    if (total + 1 >= MAX_HASTATUS_LENGTH) {
      Serial_println(F("* [ESP] Received homeassistant/status message too long"));
    } else {
      strlcpy(MQTT_haStatus + index, payload, len + 1); // create null-terminated copy
      MQTT_haStatus[index + len] = '\0';
      if (index + len == total) {
        if (!strcmp(MQTT_haStatus, "online")) {
          Serial_println(F("* [ESP] Detected homeassistant/status online"));
          haOnline = 2;
          throttleStart = espUptime;
          throttleValue = THROTTLE_VALUE;
          resetDataStructures();
        } else if (!strcmp(MQTT_haStatus, "offline")) {
          Serial_println(F("* [ESP] Detected homeassistant/status offline"));
          haOnline = 0;
        } else {
          haOnline = 9;
        }
      }
    }
#ifdef MQTT_INPUT_BINDATA
  } else if ((!strcmp(topic, mqttInputBinData)) || (!mqttInputBinData[MQTT_KEY_PREFIXIP - 1]) && !strncmp(topic, mqttInputBinData, MQTT_KEY_PREFIXIP - 1)) {
    if (index == 0) MQTT_drop = false;
    if ((outputMode & 0x8000) && mqttSetupReady) {
      uint16_t MQTT_readBufferH_new = MQTT_readBufferH + 3 + (total << 1); // 2 for "R ", 2 per byte, 1 for '\n'
      if (((MQTT_readBufferH_new >= MQTT_RB)  && ((MQTT_readBufferT > MQTT_readBufferH) || (MQTT_readBufferT <= (MQTT_readBufferH_new - MQTT_RB)))) ||
          ((MQTT_readBufferH_new  < MQTT_RB)  &&  (MQTT_readBufferT > MQTT_readBufferH) && (MQTT_readBufferT <= MQTT_readBufferH_new))) {
        // buffer overrun
        Sprint_P(true, false, false, PSTR("* [MON2] Mqtt packet input buffer overrun, dropped, index %i len %i total %i"), index, len, total);
        MQTT_drop = true;
      } else if (!MQTT_drop) {
        if (index == 0) {
          MQTT_readBuffer_writeChar('R');
          MQTT_readBuffer_writeChar(' ');
        }
        for (uint16_t i = 0; i < len; i++) MQTT_readBuffer_writeHex(payload[i]);
        if (index + len == total) MQTT_readBuffer_writeChar('\n');
      }
    } else {
      // ignore based on outputMode
    }
#endif /* MQTT_INPUT_BINDATA */
#ifdef MQTT_INPUT_HEXDATA
  } else if ((!strcmp(topic, mqttInputHexData)) || (!mqttInputHexData[MQTT_KEY_PREFIXIP - 1]) && !strncmp(topic, mqttInputHexData, MQTT_KEY_PREFIXIP - 1)) {
    if (index == 0) MQTT_drop = false;
    if ((outputMode & 0x4000) && mqttSetupReady) {
      uint16_t MQTT_readBufferH_new = MQTT_readBufferH + total + 1; // 1 for '\n'
      if (((MQTT_readBufferH_new >= MQTT_RB)  && ((MQTT_readBufferT > MQTT_readBufferH) || (MQTT_readBufferT <= (MQTT_readBufferH_new - MQTT_RB)))) ||
          ((MQTT_readBufferH_new  < MQTT_RB)  &&  (MQTT_readBufferT > MQTT_readBufferH) && (MQTT_readBufferT <= MQTT_readBufferH_new))) {
        // buffer overrun
        Sprint_P(true, false, false, PSTR("* [MON2] Mqtt packet input buffer overrun, dropped, index %i len %i total %i"), index, len, total);
        MQTT_drop = true;
      } else if (!MQTT_drop) {
        for (uint16_t i = 0; i < len; i++) MQTT_readBuffer_writeChar(payload[i]);
        if (index + len == total) MQTT_readBuffer_writeChar('\n');
      }
    } else {
      // ignore based on outputMode
    }
#endif /* MQTT_INPUT_HEXDATA */
  } else {
    Serial_println(F("Unknown MQTT topic received"));
  }
}

#ifdef TELNET
void onInputReceived (String str) {
  if (telnetCommandReceived) {
    Serial_println(F("* [ESP] Ignoring telnet command, previous command is still being processed"));
  } else {
    if (str.length() + 1 >= MAX_COMMAND_LENGTH) {
      Serial_println(F("* [ESP] Received MQTT command too long"));
    } else {
      strlcpy(telnetCommandString, str.c_str(), str.length() + 1); // create null-terminated copy
      telnetCommandString[str.length()] = '\0';
      telnetCommandReceived = true;
    }
  }
}
#endif

bool OTAbusy = 0;
static byte ignoreremainder = 2; // first line ignored - robustness
#ifdef REBOOT_REASON
uint8_t delayedRebootReasonReset = 0;
#endif

void setup() {
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

  // get EEPROM data and rebootReason
  EEPROM.begin(sizeof(EEPROMSettingsUnion));
  EEPROM.get(0, EEPROM_state);
  saveRebootReason = REBOOT_REASON_NOTSUPPORTED;

  Serial_println(F("* [ESP] Check for old1 signature"));
  if (!strcmp(EEPROM_state.EEPROMold.signature, EEPROM_SIGNATURE_OLD1)) {
    Serial_println(F("* [ESP] Old1 signature match, need to update EEPROM"));
    // order is important, don't overwrite old data before reading it
    EEPROM_state.EEPROMnew.rebootReason = REBOOT_REASON_UNKNOWN;
    EEPROM_state.EEPROMnew.mqttPort = EEPROM_state.EEPROMold.mqttPort;
    strlcpy(EEPROM_state.EEPROMnew.mqttServer,   EEPROM_state.EEPROMold.mqttServer,    sizeof(EEPROM_state.EEPROMnew.mqttServer));
    strlcpy(EEPROM_state.EEPROMnew.mqttPassword, EEPROM_state.EEPROMold.mqttPassword,  sizeof(EEPROM_state.EEPROMnew.mqttPassword));
    strlcpy(EEPROM_state.EEPROMnew.mqttUser,     EEPROM_state.EEPROMold.mqttUser,      sizeof(EEPROM_state.EEPROMnew.mqttUser));
    EEPROM_state.EEPROMnew.rebootReason = REBOOT_REASON_UNKNOWN;
    EEPROM_state.EEPROMnew.outputMode = INIT_OUTPUTMODE;
    EEPROM_state.EEPROMnew.outputFilter = INIT_OUTPUTFILTER;
    EEPROM_state.EEPROMnew.mqttInputByte4 = 0;
    EEPROM_state.EEPROMnew.ESPhwID = 0;
    EEPROM_state.EEPROMnew.EEPROM_version = 0;
    EEPROM_state.EEPROMnew.noWiFi = INIT_NOWIFI;
    EEPROM_state.EEPROMnew.useStaticIP = INIT_USE_STATIC_IP;
    EEPROM_state.EEPROMnew.static_ip[0] = '\0';
    EEPROM_state.EEPROMnew.static_gw[0] = '\0';
    EEPROM_state.EEPROMnew.static_nm[0] = '\0';
    for (int i = 0; i < NR_RESERVED; i++) EEPROM_state.EEPROMnew.reserved[i] = 0;
    strlcpy(EEPROM_state.EEPROMnew.signature,    EEPROM_SIGNATURE_NEW, sizeof(EEPROM_state.EEPROMnew.signature));
    EEPROM.put(0, EEPROM_state);
    EEPROM.commit();
  }

  Serial_print(F("* [ESP] Signature: "));
  Serial_println(EEPROM_state.EEPROMnew.signature);

  Serial_println(F("* [ESP] Check for old2 signature"));
  if (!strcmp(EEPROM_state.EEPROMnew.signature, EEPROM_SIGNATURE_OLD2)) {
    Serial_println(F("* [ESP] Old2 signature match, need to initiate outputMode, outputFilter, mqttInputByte4 and zero more reserved bytes"));
    EEPROM_state.EEPROMnew.outputMode = INIT_OUTPUTMODE;
    EEPROM_state.EEPROMnew.outputFilter = INIT_OUTPUTFILTER;
    EEPROM_state.EEPROMnew.mqttInputByte4 = 0;
    EEPROM_state.EEPROMnew.ESPhwID = 0;
    EEPROM_state.EEPROMnew.EEPROM_version = 0;
    EEPROM_state.EEPROMnew.noWiFi = INIT_NOWIFI;
    EEPROM_state.EEPROMnew.useStaticIP = INIT_USE_STATIC_IP;
    EEPROM_state.EEPROMnew.static_ip[0] = '\0';
    EEPROM_state.EEPROMnew.static_gw[0] = '\0';
    EEPROM_state.EEPROMnew.static_nm[0] = '\0';
    for (int i = 0; i < NR_RESERVED; i++) EEPROM_state.EEPROMnew.reserved[i] = 0;
    strlcpy(EEPROM_state.EEPROMnew.signature,    EEPROM_SIGNATURE_NEW, sizeof(EEPROM_state.EEPROMnew.signature));
    EEPROM.put(0, EEPROM_state);
    EEPROM.commit();
  }

  Serial_println(F("* [ESP] Check for old3 signature"));
  if (!strcmp(EEPROM_state.EEPROMnew.signature, EEPROM_SIGNATURE_OLD3)) {
    Serial_println(F("* [ESP] Old3 signature match, need to upgrade signature and set ESPhwID and EEPROM_version to 0"));
    // EEPROM_state.EEPROMnew.ESPhwID = 0; // was 0 already
    // EEPROM_state.EEPROMnew.EEPROM_version = 0; //was 0 already
    EEPROM_state.EEPROMnew.noWiFi = INIT_NOWIFI;
    EEPROM_state.EEPROMnew.useStaticIP = INIT_USE_STATIC_IP;
    EEPROM_state.EEPROMnew.static_ip[0] = '\0';
    EEPROM_state.EEPROMnew.static_gw[0] = '\0';
    EEPROM_state.EEPROMnew.static_nm[0] = '\0';
    strlcpy(EEPROM_state.EEPROMnew.signature,    EEPROM_SIGNATURE_NEW, sizeof(EEPROM_state.EEPROMnew.signature));
    EEPROM.put(0, EEPROM_state);
    EEPROM.commit();
  }

  Serial_println(F("* [ESP] Check new EEPROM signature, init if not present"));
  if (strcmp(EEPROM_state.EEPROMnew.signature, EEPROM_SIGNATURE_NEW)) {
    Serial_println(F("* [ESP] No new ignature found, need to init EEPROM"));
    strlcpy(EEPROM_state.EEPROMnew.signature,    EEPROM_SIGNATURE_NEW, sizeof(EEPROM_state.EEPROMnew.signature));
    strlcpy(EEPROM_state.EEPROMnew.mqttUser,     MQTT_USER,          sizeof(EEPROM_state.EEPROMnew.mqttUser));
    strlcpy(EEPROM_state.EEPROMnew.mqttPassword, MQTT_PASSWORD,      sizeof(EEPROM_state.EEPROMnew.mqttPassword));
    strlcpy(EEPROM_state.EEPROMnew.mqttServer,   MQTT_SERVER,        sizeof(EEPROM_state.EEPROMnew.mqttServer));
    EEPROM_state.EEPROMnew.mqttPort = MQTT_PORT;
    EEPROM_state.EEPROMnew.rebootReason = REBOOT_REASON_UNKNOWN;
    EEPROM_state.EEPROMnew.outputMode = INIT_OUTPUTMODE;
    EEPROM_state.EEPROMnew.outputFilter = INIT_OUTPUTFILTER;
    EEPROM_state.EEPROMnew.mqttInputByte4 = 0;
    EEPROM_state.EEPROMnew.ESPhwID = INIT_ESP_HW_ID;
    EEPROM_state.EEPROMnew.EEPROM_version = 0; // use counter instead of new signature in future
    EEPROM_state.EEPROMnew.noWiFi = INIT_NOWIFI;
    EEPROM_state.EEPROMnew.useStaticIP = INIT_USE_STATIC_IP;
    EEPROM_state.EEPROMnew.static_ip[0] = '\0';
    EEPROM_state.EEPROMnew.static_gw[0] = '\0';
    EEPROM_state.EEPROMnew.static_nm[0] = '\0';
    for (int i = 0; i < NR_RESERVED; i++) EEPROM_state.EEPROMnew.reserved[i] = 0;
    EEPROM.put(0, EEPROM_state);
    EEPROM.commit();
  } else {
    Serial_println(F("* [ESP] New EEPROM signature found, using stored EEPROM settings"));
  }

#ifdef REBOOT_REASON
  if (!strcmp(EEPROM_state.EEPROMnew.signature, EEPROM_SIGNATURE_NEW)) {
    saveRebootReason = EEPROM_state.EEPROMnew.rebootReason;
    if (saveRebootReason != REBOOT_REASON_UNKNOWN) {
      if ((saveRebootReason == REBOOT_REASON_WIFIMAN) || (saveRebootReason == REBOOT_REASON_ETH)) {
        // avoid writing to EEPROM here, resulting in 2 writes per cycle, if ESP cycles/restarts due to WiFiMan or ethernet problem
        delayedRebootReasonReset = 1;
      } else if (saveRebootReason == REBOOT_REASON_MQTT) {
        // avoid writing to EEPROM here, resulting in 2 writes per cycle, if ESP cycles/restarts due to MQTT not connecting
        delayedRebootReasonReset = 2;
      } else {
        EEPROM_state.EEPROMnew.rebootReason = REBOOT_REASON_UNKNOWN;
        EEPROM.put(0, EEPROM_state);
        EEPROM.commit();
      }
    }
  } else {
    saveRebootReason = REBOOT_REASON_NOTSTORED;
  }
  Serial.print(F("* [ESP] ESP reboot reason 0x"));
  Serial.println(saveRebootReason, HEX);
#endif /* REBOOT_REASON */

  IPAddress local_ip;

#ifdef TELNET
// setup telnet
  // passing on functions for various telnet events
  telnet.onConnect(onTelnetConnect);
  telnet.onConnectionAttempt(onTelnetConnectionAttempt);
  telnet.onReconnect(onTelnetReconnect);
  telnet.onDisconnect(onTelnetDisconnect);
  telnet.onInputReceived(onInputReceived);
  bool telnetSuccess = false;
#endif TELNET

#ifdef ETHERNET
  Serial.println(F("* [ESP] Trying initEthernet"));
  if (ethernetConnected = initEthernet()) {
    telnetSuccess = telnet.begin(telnet_port, false); // change in v0.9.30 to official unmodified ESP_telnet v2.0.0
    local_ip = eth.localIP();
    Serial.print(F("* [ESP] Connected to ethernet, IP address: "));
    Serial.println(local_ip);
  } else
#endif
  {
    // WiFiManager setup
    // Set config save notify callback
    wifiManager.setSaveConfigCallback(WiFiManagerSaveConfigCallback);
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
    WiFiManagerParameter WiFiManMqttServer("mqttserver", "MqttServer (IPv4, required)", MQTT_SERVER, 19);
    WiFiManagerParameter WiFiManMqttPort("mqttport", "MqttPort (optional, default 1883)", MQTT_PORT_STRING, 9);
    WiFiManagerParameter WiFiManMqttUser("mqttuser", "MqttUser (optional)", MQTT_USER, 80);
    WiFiManagerParameter WiFiManMqttPassword("mqttpassword", "MqttPassword (optional)", MQTT_PASSWORD, 80);

    // add 4 static IP settings to WiFiManager, with default settings preconfigured in NetworkParams.h
    WiFiManagerParameter WiFiMan_use_static_ip("use_static_ip", "0=DHCP 1=staticIP", INIT_USE_STATIC_IP_STRING, 1);
    WiFiManagerParameter WiFiMan_static_ip("static_ip", "Static IPv4 (optional)", STATIC_IP, 15);
    WiFiManagerParameter WiFiMan_static_gw("static_gw", "Static GW (optional)", STATIC_GW, 15);
    WiFiManagerParameter WiFiMan_static_nm("static_nm", "Static NM (optional)", STATIC_NM, 15);

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
    if (EEPROM_state.EEPROMnew.useStaticIP) {
      IPAddress _ip, _gw, _nm;
      _ip.fromString(EEPROM_state.EEPROMnew.static_ip);
      _gw.fromString(EEPROM_state.EEPROMnew.static_gw);
      _nm.fromString(EEPROM_state.EEPROMnew.static_nm);
      wifiManager.setSTAStaticIPConfig(_ip, _gw, _nm);
    }
// WiFiManager start
    // Fetches ssid, password, and tries to connect.
    // If it does not connect it starts an access point with the specified name,
    // and goes into a blocking loop awaiting configuration.
    // First parameter is name of access point, second is the password.
    Serial.println(F("* [ESP] Trying autoconnect"));
#ifdef WIFIPORTAL_TIMEOUT
    wifiManager.setConfigPortalTimeout(WIFIPORTAL_TIMEOUT);
#endif /* WIFIPORTAL_TIMEOUT */
    if (!wifiManager.autoConnect(WIFIMAN_SSID, WIFIMAN_PASSWORD)) {
      Serial_println(F("* [ESP] Failed to connect and hit timeout, resetting"));
      // Reset and try again
#ifdef REBOOT_REASON
      if (!strcmp(EEPROM_state.EEPROMnew.signature, EEPROM_SIGNATURE_NEW)) {
        EEPROM_state.EEPROMnew.rebootReason = REBOOT_REASON_WIFIMAN;
        EEPROM.put(0, EEPROM_state);
        EEPROM.commit();
      }
#endif /* REBOOT_REASON */
      ESP.reset();
      delay(2000);
    }
    // Connected to WiFi
    local_ip = WiFi.localIP();
//Silence WiFiManager from here on
    wifiManager.setDebugOutput(false);
    Serial_print(F("* [ESP] Connected to WiFi, IP address: "));
    Serial_println(local_ip);
    // Serial_print(F("* SSID of WiFi is "));
    // Serial_println(wifiManager.getWiFiSSID()); // TODO enable in future WiFiManager version
    // Serial_print(F("* Pass of AP is "));
    // Serial_println(wifiManager.getWiFiPass());

    if (shouldSaveConfig) {
      // write new WiFi parameters provided in AP portal to EEPROM
      Sprint_P(true, true, true, PSTR("* [ESP] Writing new WiFiManager-provided MQTT parameters to EEPROM"));
      strlcpy(EEPROM_state.EEPROMnew.mqttServer,   WiFiManMqttServer.getValue(),   sizeof(EEPROM_state.EEPROMnew.mqttServer));
      if ((!strlen(WiFiManMqttPort.getValue())) || (sscanf(WiFiManMqttPort.getValue(), "%i", &EEPROM_state.EEPROMnew.mqttPort) != 1)) EEPROM_state.EEPROMnew.mqttPort = MQTT_PORT;
      strlcpy(EEPROM_state.EEPROMnew.mqttUser,     WiFiManMqttUser.getValue(),     sizeof(EEPROM_state.EEPROMnew.mqttUser));
      strlcpy(EEPROM_state.EEPROMnew.mqttPassword, WiFiManMqttPassword.getValue(), sizeof(EEPROM_state.EEPROMnew.mqttPassword));
      if ((!strlen(WiFiMan_use_static_ip.getValue())) || (sscanf(WiFiMan_use_static_ip.getValue(), "%i", &useStaticIP) != 1)) useStaticIP = INIT_USE_STATIC_IP;
      EEPROM_state.EEPROMnew.useStaticIP = useStaticIP;
      strlcpy(EEPROM_state.EEPROMnew.static_ip,   WiFiMan_static_ip.getValue(),   sizeof(EEPROM_state.EEPROMnew.static_ip));
      strlcpy(EEPROM_state.EEPROMnew.static_gw,   WiFiMan_static_gw.getValue(),   sizeof(EEPROM_state.EEPROMnew.static_gw));
      strlcpy(EEPROM_state.EEPROMnew.static_nm,   WiFiMan_static_nm.getValue(),   sizeof(EEPROM_state.EEPROMnew.static_nm));
      EEPROM.put(0, EEPROM_state);
      EEPROM.commit();
      if (useStaticIP) {
        Sprint_P(true, true, true, PSTR("* [ESP] Restart ESP to set static IP"));
        delay(500);
        ESP.restart();
        delay(200);
      }
    }
#ifdef TELNET
    telnetSuccess = telnet.begin(telnet_port);
#endif
  }

#ifdef REBOOT_REASON
  if (delayedRebootReasonReset == 1) {
    EEPROM_state.EEPROMnew.rebootReason = REBOOT_REASON_UNKNOWN;
    EEPROM.put(0, EEPROM_state);
    EEPROM.commit();
  }
#endif

// Fill in 4th byte of IPv4 address to MQTT topics
  mqttKeyPrefix[MQTT_KEY_PREFIXIP] = (local_ip[3] / 100) + '0';
  mqttKeyPrefix[MQTT_KEY_PREFIXIP + 1] = (local_ip[3] % 100) / 10 + '0';
  mqttKeyPrefix[MQTT_KEY_PREFIXIP + 2] = (local_ip[3] % 10) + '0';
  mqttCommands[MQTT_KEY_PREFIXIP] = (local_ip[3] / 100) + '0';
  mqttCommands[MQTT_KEY_PREFIXIP + 1] = (local_ip[3] % 100) / 10 + '0';
  mqttCommands[MQTT_KEY_PREFIXIP + 2] = (local_ip[3] % 10) + '0';
  mqttHexdata[MQTT_KEY_PREFIXIP] = (local_ip[3] / 100) + '0';
  mqttHexdata[MQTT_KEY_PREFIXIP + 1] = (local_ip[3] % 100) / 10 + '0';
  mqttHexdata[MQTT_KEY_PREFIXIP + 2] = (local_ip[3] % 10) + '0';
  mqttBindata[MQTT_KEY_PREFIXIP] = (local_ip[3] / 100) + '0';
  mqttBindata[MQTT_KEY_PREFIXIP + 1] = (local_ip[3] % 100) / 10 + '0';
  mqttBindata[MQTT_KEY_PREFIXIP + 2] = (local_ip[3] % 10) + '0';
  mqttJsondata[MQTT_KEY_PREFIXIP] = (local_ip[3] / 100) + '0';
  mqttJsondata[MQTT_KEY_PREFIXIP + 1] = (local_ip[3] % 100) / 10 + '0';
  mqttJsondata[MQTT_KEY_PREFIXIP + 2] = (local_ip[3] % 10) + '0';
  mqttSignal[MQTT_KEY_PREFIXIP] = (local_ip[3] / 100) + '0';
  mqttSignal[MQTT_KEY_PREFIXIP + 1] = (local_ip[3] % 100) / 10 + '0';
  mqttSignal[MQTT_KEY_PREFIXIP + 2] = (local_ip[3] % 10) + '0';
  haDeviceName[HA_DEVICE_NAME_PREFIX] = (local_ip[3] / 100) + '0';
  haDeviceName[HA_DEVICE_NAME_PREFIX + 1] = (local_ip[3] % 100) / 10 + '0';
  haDeviceName[HA_DEVICE_NAME_PREFIX + 2] = (local_ip[3] % 10) + '0';
  haDeviceID[HA_DEVICE_ID_PREFIX] = (local_ip[3] / 100) + '0';
  haDeviceID[HA_DEVICE_ID_PREFIX + 1] = (local_ip[3] % 100) / 10 + '0';
  haDeviceID[HA_DEVICE_ID_PREFIX + 2] = (local_ip[3] % 10) + '0';
  haPostfix[HA_POSTFIX_PREFIX] = (local_ip[3] / 100) + '0';
  haPostfix[HA_POSTFIX_PREFIX + 1] = (local_ip[3] % 100) / 10 + '0';
  haPostfix[HA_POSTFIX_PREFIX + 2] = (local_ip[3] % 10) + '0';

  Sprint_P(true, false, false, PSTR("* [ESP] ESP reboot reason 0x%2X"), saveRebootReason);

// MQTT client setup
  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onPublish(onMqttPublish);
  mqttClient.onMessage(onMqttMessage);
  mqttClient.setServer(EEPROM_state.EEPROMnew.mqttServer, EEPROM_state.EEPROMnew.mqttPort);

  MQTT_CLIENTNAME[MQTT_CLIENTNAME_IP]     = (local_ip[3] / 100) + '0';
  MQTT_CLIENTNAME[MQTT_CLIENTNAME_IP + 1] = (local_ip[3] % 100) / 10 + '0';
  MQTT_CLIENTNAME[MQTT_CLIENTNAME_IP + 2] = (local_ip[3] % 10) + '0';

  mqttClient.setClientId(MQTT_CLIENTNAME);
  mqttClient.setCredentials((EEPROM_state.EEPROMnew.mqttUser[0] == '\0') ? 0 : EEPROM_state.EEPROMnew.mqttUser, (EEPROM_state.EEPROMnew.mqttPassword[0] == '\0') ? 0 : EEPROM_state.EEPROMnew.mqttPassword);
  // mqttClient.setWill(MQTT_WILL_TOPIC, MQTT_WILL_QOS, MQTT_WILL_RETAIN, MQTT_WILL_PAYLOAD); // TODO

  Serial_print(F("* [ESP] Clientname ")); Serial_println(MQTT_CLIENTNAME);
  Serial_print(F("* [ESP] User ")); Serial_println(EEPROM_state.EEPROMnew.mqttUser);
  // Serial_print(F("* [ESP] Password ")); Serial_println(EEPROM_state.EEPROMnew.mqttPassword);
  Serial_print(F("* [ESP] Server ")); Serial_println(EEPROM_state.EEPROMnew.mqttServer);
  Serial_print(F("* [ESP] Port ")); Serial_println(EEPROM_state.EEPROMnew.mqttPort);

  delay(100);
  mqttClient.connect();
  delay(500);

  if (mqttClient.connected()) {
    Serial.println(F("* [ESP] MQTT client connected on first try"));
#ifdef REBOOT_REASON
    if (delayedRebootReasonReset == 2) {
      EEPROM_state.EEPROMnew.rebootReason = REBOOT_REASON_UNKNOWN;
      EEPROM.put(0, EEPROM_state);
      EEPROM.commit();
      delayedRebootReasonReset = 0;
    }
#endif
  } else {
    Serial.println(F("* [ESP] MQTT client connect failed"));
  }

  byte connectTimeout = 20; // 10s timeout
  while (!mqttClient.connected()) {
    mqttClient.connect();
    if (mqttClient.connected()) {
      Serial.println(F("* [ESP] MQTT client connected"));
#ifdef REBOOT_REASON
      if (delayedRebootReasonReset == 2) {
        EEPROM_state.EEPROMnew.rebootReason = REBOOT_REASON_UNKNOWN;
        EEPROM.put(0, EEPROM_state);
        EEPROM.commit();
        delayedRebootReasonReset = 0;
      }
#endif
      break;
    } else {
      Serial.print(F("* [ESP] MQTT client connect failed, retrying in setup() until time-out "));
      Serial.println(connectTimeout);
      if (!--connectTimeout) break;
      delay(500);
    }
  }

#ifdef MQTT_INPUT_BINDATA
  if (EEPROM_state.EEPROMnew.mqttInputByte4) {
    mqttInputBinData[MQTT_KEY_PREFIXIP - 1] = '/';
    mqttInputBinData[MQTT_KEY_PREFIXIP] = (EEPROM_state.EEPROMnew.mqttInputByte4 / 100) + '0';
    mqttInputBinData[MQTT_KEY_PREFIXIP + 1] = (EEPROM_state.EEPROMnew.mqttInputByte4 % 100) / 10 + '0';
    mqttInputBinData[MQTT_KEY_PREFIXIP + 2] = (EEPROM_state.EEPROMnew.mqttInputByte4 % 10) + '0';
  }
#endif
#ifdef MQTT_INPUT_HEXDATA
  if (EEPROM_state.EEPROMnew.mqttInputByte4) {
    mqttInputHexData[MQTT_KEY_PREFIXIP - 1] = '/';
    mqttInputHexData[MQTT_KEY_PREFIXIP] = (EEPROM_state.EEPROMnew.mqttInputByte4 / 100) + '0';
    mqttInputHexData[MQTT_KEY_PREFIXIP + 1] = (EEPROM_state.EEPROMnew.mqttInputByte4 % 100) / 10 + '0';
    mqttInputHexData[MQTT_KEY_PREFIXIP + 2] = (EEPROM_state.EEPROMnew.mqttInputByte4 % 10) + '0';
  }
#endif

  outputFilter = EEPROM_state.EEPROMnew.outputFilter;
  outputMode = EEPROM_state.EEPROMnew.outputMode;
  ESPhwID = EEPROM_state.EEPROMnew.ESPhwID;
  noWiFi = EEPROM_state.EEPROMnew.noWiFi;
  useStaticIP = EEPROM_state.EEPROMnew.useStaticIP;

  if (mqttClient.connected()) {
    Sprint_P(true, true, true, PSTR(WELCOMESTRING));
#ifdef E_SERIES
    Sprint_P(true, true, true, PSTR("* [ESP] E-Series"));
#endif /* E_SERIES */
#ifdef F_SERIES
    Sprint_P(true, true, true, PSTR("* [ESP] F-Series"));
#endif /* F_SERIES */
#ifdef H_SERIES
    Sprint_P(true, true, true, PSTR("* [ESP] H-Series"));
#endif /* H_SERIES */
#ifdef MHI_SERIES
    Sprint_P(true, true, true, PSTR("* [ESP] MHI-Series"));
#endif /* MHI_SERIES */
    Sprint_P(true, true, true, PSTR("* [ESP] ESP_hw_identifier %i"), ESPhwID);
    Sprint_P(true, true, true, PSTR("* [ESP] noWiFi %i"), noWiFi);
    Sprint_P(true, true, true, PSTR("* [ESP] useStaticIP %i"), useStaticIP);
    Sprint_P(true, true, true, PSTR("* [ESP] Connected to MQTT server"));
    Sprint_P(true, true, true, PSTR("* [ESP] MQTT Clientname = %s"), MQTT_CLIENTNAME);
    Sprint_P(true, true, true, PSTR("* [ESP] MQTT User = %s"), EEPROM_state.EEPROMnew.mqttUser);
    // Sprint_P(true, true, true, PSTR("* [ESP] MQTT Password = %s"), EEPROM_state.EEPROMnew.mqttPassword);
    Sprint_P(true, true, true, PSTR("* [ESP] MQTT Server = %s"), EEPROM_state.EEPROMnew.mqttServer);
    Sprint_P(true, true, true, PSTR("* [ESP] MQTT Port = %i"), EEPROM_state.EEPROMnew.mqttPort);
    Sprint_P(true, true, true, PSTR("* [ESP] ESP reboot reason = 0x%2X"), saveRebootReason);
    Sprint_P(true, true, true, PSTR("* [ESP] outputMode = 0x%X"), outputMode);
    Sprint_P(true, true, true, PSTR("* [ESP] outputFilter = %i"), outputFilter);
    // Sprint_P(true, true, true, PSTR("* [ESP] mqttInputByte4 = %i"), mqttInputByte4);
    Sprint_P(true, true, true, PSTR("* [ESP] EEPROM version = %i"), EEPROM_state.EEPROMnew.EEPROM_version);
  }
  delay(200);

#ifdef TELNET
  if (telnetSuccess) {
    Sprint_P(true, false, false, PSTR("* [ESP] Telnet running on %i.%i.%i.%i"), local_ip[0], local_ip[1], local_ip[2], local_ip[3]);
  } else {
    Sprint_P(true, false, false, PSTR("* [ESP] Telnet error."));
  }
#endif

// OTA
  // port defaults to 8266
  // ArduinoOTA.setPort(8266);
  // Hostname defaults to esp8266-[ChipID]
  OTA_HOSTNAME[OTA_HOSTNAME_PREFIXIP] = (local_ip[3] / 100) + '0';
  OTA_HOSTNAME[OTA_HOSTNAME_PREFIXIP + 1] = (local_ip[3] % 100) / 10 + '0';
  OTA_HOSTNAME[OTA_HOSTNAME_PREFIXIP + 2] = (local_ip[3] % 10) + '0';
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  // No authentication by default
#ifdef OTA_PASSWORD
  ArduinoOTA.setPassword(OTA_PASSWORD);
#endif
  // Password "admin" can be set with its md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");
  ArduinoOTA.onStart([]() {
    OTAbusy = 1;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      Sprint_P(true, true, true, PSTR("* [ESP] Start OTA updating sketch"));
    } else { // U_SPIFFS (not used here)
      Sprint_P(true, true, true, PSTR("* [ESP] Start OTA updating filesystem"));
    }
    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
  });
  ArduinoOTA.onEnd([]() {
    OTAbusy = 2;
    Sprint_P(true, true, true, PSTR("* [ESP] OTA End"));
    Sprint_P(true, true, true, PSTR("* [ESP] Disconnect and stop telnet"));
#ifdef REBOOT_REASON
    EEPROM_state.EEPROMnew.rebootReason = REBOOT_REASON_OTA;
    EEPROM.put(0, EEPROM_state);
    EEPROM.commit();
#endif /* REBOOT_REASON */
    delay(300);
    ESP.restart();
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Sprint_P(true, false, false, PSTR("* [ESP] Progress: %u%%\r"), (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Sprint_P(true, false, false, PSTR("* [ESP] Error[%u]: "), error);
    Sprint_P(true, false, false, PSTR("* Error[%u]: "), error);
    if (error == OTA_AUTH_ERROR) {
      Serial_println(F("* Auth Failed"));
      Sprint_P(true, true, true, PSTR("* [ESP] OTA Auth Failed"));
    } else if (error == OTA_BEGIN_ERROR) {
      Serial_println(F("* Begin Failed"));
      Sprint_P(true, true, true, PSTR("* [ESP] OTA Begin Failed"));
    } else if (error == OTA_CONNECT_ERROR) {
      Serial_println(F("* Connect Failed"));
      Sprint_P(true, true, true, PSTR("* [ESP] OTA Connect Failed"));
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial_println(F("* Receive Failed"));
      Sprint_P(true, true, true, PSTR("* [ESP] OTA Receive Failed"));
    } else if (error == OTA_END_ERROR) {
      Serial_println(F("* End Failed"));
      Sprint_P(true, true, true, PSTR("* [ESP] OTA End Failed"));
    }
    ignoreremainder = 2;
    OTAbusy = 0;
  });
  ArduinoOTA.begin();

#ifdef AVRISP
// AVRISP
  // set RESET_PIN high, to prevent ESP8266AVRISP from resetting ATmega328P
  digitalWrite(RESET_PIN, HIGH);
  avrprog = new ESP8266AVRISP(avrisp_port, RESET_PIN, ESPhwID ? SPI_SPEED_1 : SPI_SPEED_0);
  // set RESET_PIN back to INPUT mode
  pinMode(RESET_PIN, INPUT);
  MDNS.begin(avrisp_host);
  MDNS.addService("avrisp", "tcp", avrisp_port);


// WEBSERVER
  httpUpdater.setup(&httpServer);
  httpServer.begin();
  MDNS.addService("http", "tcp", 80);


  Sprint_P(true, true, true, PSTR("* [ESP] AVRISP: ATmega programming: avrdude -c avrisp -p atmega328p -P net:%i.%i.%i.%i:%i -t # or -U ..."), local_ip[0], local_ip[1], local_ip[2], local_ip[3], avrisp_port);
  // listen for avrdudes
  avrprog->begin();
#endif
// Allow ATmega to enable serial input/output
  digitalWrite(ATMEGA_SERIAL_ENABLE, HIGH);
  pinMode(ATMEGA_SERIAL_ENABLE, OUTPUT);

  prevMillis = millis();

// Flush ATmega's serial input
  delay(200);
  ATmega_dummy_for_serial();

// Ready, report status
  Sprint_P(true, true, true, PSTR("* [ESP] Setup ready"));
  if (telnetSuccess) {
    Sprint_P(true, true, true, PSTR("* [ESP] TELNET: telnet %i.%i.%i.%i"), local_ip[0], local_ip[1], local_ip[2], local_ip[3]);
  } else {
    Sprint_P(true, true, true, PSTR("* [ESP] Telnet setup failed."));
  }
  delay(1000);
  mqttSetupReady = true;
  configTime(MY_TZ[0], MY_NTP_SERVER);
}

static int jsonTerm = 1; // indicates whether json output was terminated
int jsonStringp = 0;
char jsonString[10000];

void process_for_mqtt_json(byte* rb, int n) {
  char mqtt_value[MQTT_VALUE_LEN] = "\0";
  char mqtt_key[MQTT_KEY_LEN + MQTT_KEY_PREFIXLEN]; // = mqttKeyPrefix;
  if (!mqttConnected) Mqtt_disconnectSkippedPackets++;
  if (mqttConnected || MQTT_DISCONNECT_CONTINUE) {
#ifdef EF_SERIES
    if (n == 3) bytes2keyvalue(rb[0], rb[2], EMPTY_PAYLOAD, rb + 3, mqtt_key, mqtt_value);
#endif /* EF_SERIES */
    for (byte i = 3; i < n; i++) {
      if (!--throttle) {
        throttle = throttleValue;
        int kvrbyte = bytes2keyvalue(rb[0], rb[2], i - 3, rb + 3, mqtt_key, mqtt_value);
        // returns 0 if byte does not trigger any output
        // returns 1 if new mqtt_key,mqtt_value should be output
        // returns 8 if byte should be treated per bit
        // returns 9 if json string should be terminated
        if (kvrbyte == 9) {
          if (jsonTerm == 0) {
            // only terminate json string and publish if at least one parameter was written
            jsonTerm = 1;
            jsonString[jsonStringp++] = '}';
            jsonString[jsonStringp++] = '\0';
            if (outputMode & 0x0004) client_publish_mqtt(mqttJsondata, jsonString);
            if (outputMode & 0x0040) client_publish_telnet(mqttJsondata, jsonString);
            if (outputMode & 0x0400) client_publish_serial(mqttJsondata, jsonString);
            jsonStringp = 1;
          }
        } else {
          if (kvrbyte > 9) {
            kvrbyte = 0;
            Sprint_P(true, true, true, PSTR("* [ESP] Warning: kvrbyte > 9 Src 0x%02X Tp 0x%02X Byte %i"),rb[0], rb[2], i);
          }
          for (byte j = 0; j < kvrbyte; j++) {
            int kvr = (kvrbyte == 8) ? bits2keyvalue(rb[0], rb[2], i - 3, rb + 3, mqtt_key, mqtt_value, j) : kvrbyte;
            if (kvr) {
              if (outputMode & 0x0002) client_publish_mqtt(mqtt_key, mqtt_value);
              if (outputMode & 0x0020) client_publish_telnet(mqtt_key, mqtt_value);
              if (outputMode & 0x0200) client_publish_serial(mqtt_key, mqtt_value);
              // don't add another parameter if remaining space is not enough for mqtt_key, mqtt_value, and ',' '"', '"', ':', '}', and '\0'
              if (jsonStringp + strlen(mqtt_value) + strlen(mqtt_key + MQTT_KEY_PREFIXLEN) + 6 <= sizeof(jsonString)) {
                if (jsonTerm) {
                  jsonString[jsonStringp++] = '{';
                  jsonTerm = 0;
                } else {
                  jsonString[jsonStringp++] = ',';
                }
                jsonString[jsonStringp++] = '"';
                strcpy(jsonString + jsonStringp, mqtt_key + MQTT_KEY_PREFIXLEN);
                jsonStringp += strlen(mqtt_key + MQTT_KEY_PREFIXLEN);
                jsonString[jsonStringp++] = '"';
                jsonString[jsonStringp++] = ':';
                strcpy(jsonString + jsonStringp, mqtt_value);
                jsonStringp += strlen(mqtt_value);
              }
            }
          }
        }
      }
    }
  }
}

byte timeStamp = 30; // TODO

void writePseudoPacket(byte* WB, byte rh)
// rh is packet size (without CRC byte)
{
  char pseudoWriteBuffer[RB];
  time(&now);
  localtime_r(&now, &tm);
  snprintf(pseudoWriteBuffer, 33, "R %04i-%02i-%02i_%02i:%02i:%02i P         ", tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
  uint8_t crc = crc_feed;
  for (uint8_t i = 0; i < rh; i++) {
    uint8_t c = WB[i];
    snprintf(pseudoWriteBuffer + 2 + timeStamp + (i << 1), 3, "%02X", c);
    if (crc_gen != 0) for (uint8_t i = 0; i < 8; i++) {
      crc = ((crc ^ c) & 0x01 ? ((crc >> 1) ^ crc_gen) : (crc >> 1));
      c >>= 1;
    }
  }
  WB[rh] = crc;
  if (crc_gen) snprintf(pseudoWriteBuffer + 2 + timeStamp + (rh << 1), 3, "%02X", crc);
#if !((defined MQTT_INPUT_BINDATA) || (defined MQTT_INPUT_HEXDATA))
  if (outputMode & 0x0001) client_publish_mqtt(mqttHexdata, pseudoWriteBuffer);
#endif /* MQTT_INPUT_BINDATA || MQTT_INPUT_HEXDATA */
  if (outputMode & 0x0010) client_publish_telnet(mqttHexdata, pseudoWriteBuffer);
  if (outputMode & 0x0100) client_publish_serial(mqttHexdata, pseudoWriteBuffer);
#if !((defined MQTT_INPUT_BINDATA) || (defined MQTT_INPUT_HEXDATA))
  if ((outputMode & 0x0800) && (mqttConnected)) mqttClient.publish((const char*) mqttBindata, MQTT_QOS, false, (const char*) WB, rh + 1);
#endif /* MQTT_INPUT_BINDATA || MQTT_INPUT_HEXDATA */
  if (outputMode & 0x0666) process_for_mqtt_json(WB, rh);
}

uint32_t espUptime_telnet = 0;
static bool wasConnected = false;

void loop() {
  byte readHex[HB];
  // OTA
  ArduinoOTA.handle();

  httpServer.handleClient();

  // ESP-uptime and loop timing
  uint32_t currMillis = millis();
  uint16_t loopTime = currMillis - prevMillis;
  milliInc += loopTime;
  if (loopTime > maxLoopTime) maxLoopTime = loopTime;
  prevMillis = currMillis;
  while (milliInc >= 1000) {
    milliInc -= 1000;
    espUptime += 1;
    if (mqttClient.connected()) {
#ifdef REBOOT_REASON
      if (delayedRebootReasonReset == 2) {
        EEPROM_state.EEPROMnew.rebootReason = REBOOT_REASON_UNKNOWN;
        EEPROM.put(0, EEPROM_state);
        EEPROM.commit();
        delayedRebootReasonReset = 0;
      }
#endif
      Mqtt_disconnectTime = 0;
    } else {
      Mqtt_disconnectTimeTotal++;
      Mqtt_disconnectTime++;
    }
    if (milliInc < 1000) {
      if ((throttleValue > 1) && ((espUptime - throttleStart) > (THROTTLE_VALUE - throttleValue + 1) * THROTTLE_STEP)) throttleValue--;
        Sprint_P(true, true, false, PSTR("* [ESP] Uptime %li"), espUptime);
        // alternative, more info: Sprint_P(true, true, false, PSTR("* [ESP] %d RSSI %d heap %d maxfreeblocksize %d Uptime %li disconnects %d WiFi %d ethernet %d status %d"), espUptime, WiFi.RSSI(), ESP.getFreeHeap(), ESP.getMaxFreeBlockSize(), espUptime, Mqtt_disconnects, WiFi.isConnected(), eth.connected(), eth.status());
      if (espUptime >= espUptime_telnet + 10) {
        espUptime_telnet = espUptime_telnet + 10;
        time(&now);
        localtime_r(&now, &tm);
        Sprint_P(false, false, true, PSTR("* [ESP] Uptime %li at %04i%02i%02i_%02i:%02i:%02i"), espUptime, tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
      }
      if (!mqttClient.connected()) Sprint_P(true, false, true, PSTR("* [ESP] MQTT is disconnected (%i s total %i s)"), Mqtt_disconnectTime, Mqtt_disconnectTimeTotal);
      pseudo0D++;
      pseudo0E++;
      pseudo0F++;
      if (Mqtt_disconnectTime > MQTT_DISCONNECT_RESTART) {
        Sprint_P(true, true, true, PSTR("* [ESP] Restarting ESP to attempt to reconnect Mqtt"));
#ifdef REBOOT_REASON
        EEPROM_state.EEPROMnew.rebootReason = REBOOT_REASON_MQTT;
        EEPROM.put(0, EEPROM_state);
        EEPROM.commit();
#endif /* REBOOT_REASON */
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
      case AVRISP_STATE_IDLE:    Sprint_P(true, true, true, PSTR("* [ESP-AVRISP] now idle"));
                                 digitalWrite(RESET_PIN, LOW);
                                 pinMode(RESET_PIN, OUTPUT);
                                 delay(1);
                                 pinMode(RESET_PIN, INPUT);
                                 digitalWrite(RESET_PIN, HIGH);
                                 // enable ATmega to determine PB5=GPIO14/GPIO15 for v1.0/v1.1
                                 delay(1);
                                 // enable ATmega serial output on v1.2
                                 pinMode(ATMEGA_SERIAL_ENABLE, OUTPUT); // not really needed
                                 digitalWrite(ATMEGA_SERIAL_ENABLE, HIGH);
                                 delay(100);
                                 ATmega_dummy_for_serial();
                                 ATmega_uptime_prev = 0;
                                 break;
      case AVRISP_STATE_PENDING: Sprint_P(true, true, true, PSTR("* [ESP-AVRISP] connection pending"));
                                 pinMode(RESET_PIN, OUTPUT);
                                 break;
      case AVRISP_STATE_ACTIVE:  Sprint_P(true, true, true, PSTR("* [ESP-AVRISP] programming mode"));
                                 pinMode(RESET_PIN, OUTPUT);
                                 break;
    }
    last_state = new_state;
  }
  // Serve the AVRISP client
  if (last_state != AVRISP_STATE_IDLE) avrprog->serve();
  MDNS.update();
#endif /* AVRISP */

  // network and mqtt connection check
  // if (WiFi.isConnected() || eth.connected()) {//} // TODO this does not work. eth.connected() becomes true after 4 minutes even if there is no ethernet cable attached
                                                  // TODO and eth.connected() remains true even after disconnecting the ethernet cable (and perhaps also after 'D1' soft ESP.reset()
  // ethernetConnected = ???;                     // TODO try to update ethernetConnected based on actual ethernet status
  if (WiFi.isConnected() || ethernetConnected) {  // TODO for now: use initial ethernet connection status instead (ethernet cable disconnect is thus not detected)
    if (!mqttClient.connected()) {
      if (espUptime > reconnectTime) {
        // Sprint_P(true, false, true, PSTR("* [ESP] MQTT Try to reconnect to MQTT server %s:%i (user %s/password %s)"), EEPROM_state.EEPROMnew.mqttServer, EEPROM_state.EEPROMnew.mqttPort, EEPROM_state.EEPROMnew.mqttUser, EEPROM_state.EEPROMnew.mqttPassword);
        Sprint_P(true, false, true, PSTR("* [ESP] MQTT Try to reconnect to MQTT server %s:%i (user %s/password *)"), EEPROM_state.EEPROMnew.mqttServer, EEPROM_state.EEPROMnew.mqttPort, EEPROM_state.EEPROMnew.mqttUser);
        mqttClient.connect();
        delay(500);
        if (mqttClient.connected()) {
          Sprint_P(true, true, true, PSTR("* [ESP] Mqtt reconnected"));
          if (outputMode & 0x40000) {
            Sprint_P(true, true, true, PSTR("* [ESP] Restart ESP after Mqtt reconnect (outputMode 0x40000)"));
#ifdef REBOOT_REASON
            EEPROM_state.EEPROMnew.rebootReason = REBOOT_REASON_MQTT;
            EEPROM.put(0, EEPROM_state);
            EEPROM.commit();
#endif /* REBOOT_REASON */
            delay(100);
            ESP.restart();
            delay(100);
          }
          Sprint_P(true, true, true, PSTR(WELCOMESTRING));
          reconnectTime = espUptime;
          if (outputMode & 0x80000) {
            Sprint_P(true, true, true, PSTR("* [ESP] Restart data communication after Mqtt reconnect (outputMode 0x80000)"));
            throttleStart = espUptime;
            throttleValue = THROTTLE_VALUE;
            resetDataStructures();
          }
        } else {
          Sprint_P(true, false, true, PSTR("* [ESP] Reconnect failed, retrying in 5 seconds"));
          Serial.println(F("* [ESP] Reconnect to MQTT failed, see telnet or serial output for details"));
          reconnectTime = espUptime + 5;
        }
      }
    }
  } else {
    // clean up
    if (wasConnected) {
      Serial.println("* [ESP] WiFi/ethernet disconnected");
    }
    mqttClient.clearQueue();        // needed ? TODO
    mqttClient.disconnect(true);    // needed ? TODO
  }
  wasConnected = WiFi.isConnected() || ethernetConnected;

  // handle incoming command
  if (!OTAbusy)  {
    // Non-blocking serial input
    // serial_rb is number of char received and stored in RB
    // rb_buffer = readBuffer + serial_rb // +20 for timestamp
    // ignore first line and too-long lines
    // readBuffer does not include '\n' but may include '\r' if Windows provides serial input
    if (MQTT_commandReceived) {
      Serial_print(F("* [ESP] handleCommand MQTT commandString ->"));
      Serial_print((char*) MQTT_commandString);
      Serial_print(F("<-"));
      Serial_println();
      handleCommand(MQTT_commandString);
      MQTT_commandReceived = false;
    }
    if (telnetCommandReceived) {
      Serial_print(F("* [ESP] handleCommand telnet commandString ->"));
      Serial_print((char*) telnetCommandString);
      Serial_print(F("<-"));
      Serial_println();
      handleCommand(telnetCommandString);
      telnetCommandReceived = false;
    }

    // read serial input OR MQTT_readBuffer input until and including '\n', but do not store '\n'
#if defined MQTT_INPUT_BINDATA || defined MQTT_INPUT_HEXDATA
    while (((c = MQTT_readBuffer_readChar()) >= 0) && (c != '\n') && (serial_rb < RB)) {
      *rb_buffer++ = (char) c;
      if (serial_rb) {
        serial_rb++;
      } else {
        // add timestamp TZ
        time(&now);
        localtime_r(&now, &tm);
        snprintf(&readBuffer[1], 22, " %04i-%02i-%02i_%02i:%02i:%02i ", tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
        serial_rb += 21;
        rb_buffer += 20;
      }
    }
#else
    if (!ignoreSerial) {
      while (((c = Serial.read()) >= 0) && (c != '\n') && (serial_rb < RB)) {
        *rb_buffer++ = (char) c;
        if (serial_rb) {
          serial_rb++;
        } else {
          // add timestamp TZ
          time(&now);
          localtime_r(&now, &tm);
          snprintf(&readBuffer[1], 22, " %04i-%02i-%02i_%02i:%02i:%02i ", tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
          serial_rb += 21;
          rb_buffer += 20;
        }
      }
    } else {
      c = -1;
    }
#endif
    // ((c == '\n' and serial_rb > 0))  and/or  serial_rb == RB)  or  c == -1
    if (c >= 0) {
      if ((c == '\n') && (serial_rb < RB)) {
        // c == '\n'
        *rb_buffer = '\0';
        // Remove \r before \n in DOS input
        if ((serial_rb > 0) && (*(rb_buffer - 1) == '\r')) {
          serial_rb--;
          *(--rb_buffer) = '\0';
        }
        if (ignoreremainder == 2) {
          Sprint_P(true, true, true, PSTR("* [ESP] First line from ATmega ignored %s"), readBuffer);
          ignoreremainder = 0;
        } else if (ignoreremainder == 1) {
          Sprint_P(true, true, true, PSTR("* [ESP] Line from ATmega too long, last part: ->%s<-"), readBuffer);
          ignoreremainder = 0;
        } else {
          if (readBuffer[0] == 'R') {
            int rbp = 21; // TODO +20 added
            int n, rbtemp;
            byte rh = 0;
            if ((serial_rb > 32) && ((readBuffer[22] == 'T') || (readBuffer[22] == 'P') || (readBuffer[22] == 'X'))) { // TODO +20 -> 32
              // skip 10-character time stamp
              rbp = 32;
              timeStamp = 30; // TODO ????
            } else {
              timeStamp = 20; // TODO ???
            }
            while ((rh < HB) && (sscanf(readBuffer + rbp, "%2x%n", &rbtemp, &n) == 1)) { // TODO
              readHex[rh++] = rbtemp;
              rbp += n;
            }
            if (rh == HB) Sprint_P(true, true, true, PSTR("* [MON] Buffer full, overflow not checked"));
            if ((rh > 1) || (rh == 1) && !crc_gen) {
              if (crc_gen) rh--;
              // rh is packet length (not counting CRC byte readHex[rh])
              uint8_t crc = crc_feed;
              for (uint8_t i = 0; i < rh; i++) {
                uint8_t c = readHex[i];
                if (crc_gen != 0) for (uint8_t i = 0; i < 8; i++) {
                  crc = ((crc ^ c) & 0x01 ? ((crc >> 1) ^ crc_gen) : (crc >> 1));
                  c >>= 1;
                }
              }
// if (outputMode & ??) add timestring TODO to readBuffer
              if ((!crc_gen) || (crc == readHex[rh])) {
#if !((defined MQTT_INPUT_BINDATA) || (defined MQTT_INPUT_HEXDATA))
                if (outputMode & 0x0001) client_publish_mqtt(mqttHexdata, readBuffer); // TODO
#endif /* MQTT_INPUT_BINDATA || MQTT_INPUT_HEXDATA */
                if (outputMode & 0x0010) client_publish_telnet(mqttHexdata, readBuffer); // TODO
                if (outputMode & 0x0100) client_publish_serial(mqttHexdata, readBuffer); // TODO
#if !((defined MQTT_INPUT_BINDATA) || (defined MQTT_INPUT_HEXDATA))
                if ((outputMode & 0x0800) && (mqttConnected)) mqttClient.publish((const char*) mqttBindata, MQTT_QOS, false, (const char*) readHex, rh + 1);
#endif /* MQTT_INPUT_BINDATA || MQTT_INPUT_HEXDATA */
                if (outputMode & 0x0666) process_for_mqtt_json(readHex, rh);
#ifdef PSEUDO_PACKETS
                if ((readHex[0] == 0x00) && (readHex[1] == 0x00) && (readHex[2] == 0x0D)) pseudo0D = 9; // Insert pseudo packet 40000D in output serial after 00000D
                if ((readHex[0] == 0x00) && (readHex[1] == 0x00) && (readHex[2] == 0x0F)) pseudo0F = 9; // Insert pseudo packet 40000F in output serial after 00000F
#endif
                if ((readHex[0] == 0x00) && (readHex[1] == 0x00) && (readHex[2] == 0x0E)) {
#ifdef PSEUDO_PACKETS
                  pseudo0E = 9; // Insert pseudo packet 40000E in output serial after 00000E
#endif
                  uint32_t ATmega_uptime = (readHex[3] << 24) || (readHex[4] << 16) || (readHex[5] << 8) || readHex[6];
                  if (ATmega_uptime < ATmega_uptime_prev) {
                    // unexpected ATmega reboot detected, flush ATmega's serial input
                    delay(200);
                    ATmega_dummy_for_serial();
                  }
                  ATmega_uptime_prev = ATmega_uptime;
                }
              } else {
                Sprint_P(true, true, true, PSTR("* [MON] Serial input buffer overrun or CRC error in R data:%s expected 0x%02X"), readBuffer + 1, crc); // TODO
                if (ESP_serial_input_Errors_CRC < 0xFF) ESP_serial_input_Errors_CRC++;
              }
            } else {
              Sprint_P(true, true, true, PSTR("* [MON] Not enough readable data in R line: ->%s<-"), readBuffer + 1); // TODO
              if (ESP_serial_input_Errors_Data_Short < 0xFF) ESP_serial_input_Errors_Data_Short++;
            }
          } else if ((readBuffer[0] == 'C') || (readBuffer[0] == 'c')) { // TODO
            // timing info
            if (outputMode & 0x1000) {
              client_publish_mqtt(mqttHexdata, readBuffer); // TODO
              client_publish_telnet(mqttHexdata, readBuffer); // TODO
            }
          } else if (readBuffer[0] == 'E') { // TODO
            // data with errors
            readBuffer[0] = '*'; // backwards output report compatibility // TODO
            Sprint_P(true, true, true, PSTR("* [MON]%s"), readBuffer + 1); // TODO
            if (outputMode & 0x2000) client_publish_mqtt(mqttHexdata, readBuffer); // TODO
          } else if (readBuffer[0] == '*') { // TODO
            Sprint_P(true, true, true, PSTR("* [MON]%s"), readBuffer + 1); // TODO
          } else {
            Sprint_P(true, true, true, PSTR("* [MON]:%s"), readBuffer); // TODO
            if (ESP_serial_input_Errors_Data_Short < 0xFF) ESP_serial_input_Errors_Data_Short++;
          }
        }
      } else {
        //  (c != '\n' ||  serial_rb == RB) // TODO
        char lst = *(rb_buffer - 1);
        *(rb_buffer - 1) = '\0';
        if (c != '\n') {
          Sprint_P(true, true, true, PSTR("* [MON] Line from ATmega too long, ignored, ignoring remainder: ->%s<-->%c<-->%c<-"), readBuffer, lst, c); // TODO
          ignoreremainder = 1;
        } else {
          Sprint_P(true, true, true, PSTR("* [MON] Line from ATmega too long, terminated, ignored: ->%s<-->%c<-"), readBuffer, lst); // TODO
          ignoreremainder = 0;
        }
      }
      rb_buffer = readBuffer; // TODO
      serial_rb = 0; // TODO
    } else {
      // wait for more serial input
    }
#ifdef PSEUDO_PACKETS
    if (pseudo0D > 5) {
      pseudo0D = 0;
      readHex[0] = 0x40;
      readHex[1] = 0x00;
#if defined MQTT_INPUT_BINDATA || defined MQTT_INPUT_HEXDATA
      readHex[2] = 0x09;
#else
      readHex[2] = 0x0D;
#endif
      readHex[3] = Compile_Options;
      readHex[4] = (Mqtt_msgSkipNotConnected >> 8) & 0xFF;
      readHex[5] = Mqtt_msgSkipNotConnected & 0xFF;
      readHex[6]  = (mqttPublished >> 24) & 0xFF;
      readHex[7]  = (mqttPublished >> 16) & 0xFF;
      readHex[8]  = (mqttPublished >> 8) & 0xFF;
      readHex[9]  = mqttPublished & 0xFF;
      readHex[10] = (Mqtt_disconnectTime >> 8) & 0xFF;
      readHex[11] = Mqtt_disconnectTime & 0xFF;
      readHex[12] = WiFi.RSSI() & 0xFF;
      readHex[13] = WiFi.status() & 0xFF;
      readHex[14] = saveRebootReason;
      readHex[15] = (outputMode >> 24) & 0xFF;
      readHex[16] = (outputMode >> 16) & 0xFF;
      readHex[17] = (outputMode >> 8) & 0xFF;
      readHex[18] = outputMode & 0xFF;
      readHex[19] = haOnline;
      for (int i = 20; i <= 22; i++) readHex[i]  = 0x00; // reserved for future use
      writePseudoPacket(readHex, 23);
    }
    if (pseudo0E > 5) {
      pseudo0E = 0;
      readHex[0]  = 0x40;
      readHex[1]  = 0x00;
#if defined MQTT_INPUT_BINDATA || defined MQTT_INPUT_HEXDATA
      readHex[2] = 0x0A;
#else
      readHex[2] = 0x0E;
#endif
      readHex[3]  = (espUptime >> 24) & 0xFF;
      readHex[4]  = (espUptime >> 16) & 0xFF;
      readHex[5]  = (espUptime >> 8) & 0xFF;
      readHex[6]  = espUptime & 0xFF;
      readHex[7]  = (Mqtt_acknowledged >> 24) & 0xFF;
      readHex[8]  = (Mqtt_acknowledged >> 16) & 0xFF;
      readHex[9]  = (Mqtt_acknowledged >> 8) & 0xFF;
      readHex[10] = Mqtt_acknowledged & 0xFF;
      readHex[11] = (Mqtt_gap >> 24) & 0xFF;
      readHex[12] = (Mqtt_gap >> 16) & 0xFF;
      readHex[13] = (Mqtt_gap >> 8) & 0xFF;
      readHex[14] = Mqtt_gap & 0xFF;
      readHex[15] = (MQTT_MIN_FREE_MEMORY >> 8) & 0xFF;
      readHex[16] = MQTT_MIN_FREE_MEMORY & 0xFF;
      readHex[17] = ESPhwID;           // previously 16-bit outputMode;
      readHex[18] = ethernetConnected; // previously 16-bit outputMode;
      readHex[19] = (maxLoopTime >> 24) & 0xFF;
      readHex[20] = (maxLoopTime >> 16) & 0xFF;
      readHex[21] = (maxLoopTime >> 8) & 0xFF;
      readHex[22] = maxLoopTime & 0xFF;
      writePseudoPacket(readHex, 23);
    }
    if (pseudo0F > 5) {
      pseudo0F = 0;
      readHex[0] = 0x40;
      readHex[1] = 0x00;
#if defined MQTT_INPUT_BINDATA || defined MQTT_INPUT_HEXDATA
      readHex[2] = 0x0B;
#else
      readHex[2] = 0x0F;
#endif
      readHex[3]  = telnetConnected;
      readHex[4]  = outputFilter;
      uint16_t m1 = ESP.getMaxFreeBlockSize();
      readHex[5]  = (m1 >> 8) & 0xFF;
      readHex[6]  = m1 & 0xFF;
      readHex[7]  = (Mqtt_disconnectTimeTotal >> 8) & 0xFF;
      readHex[8]  = Mqtt_disconnectTimeTotal & 0xFF;
      readHex[9]  = (Sprint_buffer_overflow >> 8) & 0xFF;
      readHex[10] = Sprint_buffer_overflow & 0xFF;
      readHex[11] = ESP_serial_input_Errors_Data_Short;
      readHex[12] = ESP_serial_input_Errors_CRC;
      readHex[13] = (MQTT_waitCounter >> 24) & 0xFF;
      readHex[14] = (MQTT_waitCounter >> 16) & 0xFF;
      readHex[15] = (MQTT_waitCounter >> 8) & 0xFF;
      readHex[16] = MQTT_waitCounter & 0xFF;
      readHex[17] = (Mqtt_disconnects >> 8) & 0xFF;
      readHex[18] = Mqtt_disconnects & 0xFF;
      readHex[19] = (Mqtt_disconnectSkippedPackets >> 8) & 0xFF;
      readHex[20] = Mqtt_disconnectSkippedPackets & 0xFF;
      readHex[21] = (Mqtt_msgSkipLowMem >> 8) & 0xFF;
      readHex[22] = Mqtt_msgSkipLowMem & 0xFF;
      writePseudoPacket(readHex, 23);
    }
#endif
  }
}
