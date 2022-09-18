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
 * ESP_Telnet 1.3.1 by  Lennart Hennigs (installed using Arduino IDE)
 *
 * Version history
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

#ifdef AVRISP
#include <SPI.h>
#include <ESP8266AVRISP.h>
#endif /* AVRISP */

#include <ArduinoOTA.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <AsyncMqttClient.h> // should be included after include P1P2_Config which defines MQTT_MIN_FREE_MEMORY

AsyncMqttClient client;

uint32_t maxLoopTime = 0;
long int espUptime=0;
bool shouldSaveConfig = false;
static byte crc_gen = CRC_GEN;
static byte crc_feed = CRC_FEED;

#ifdef AVRISP
const char* avrisp_host = "esp8266-avrisp";
const uint16_t avrisp_port = 328;
ESP8266AVRISP avrprog(avrisp_port, RESET_PIN, 2e5); // default avrprog speed is 3e5, which is too high to be reliable
#endif

static byte outputFilter = INIT_OUTPUTFILTER;
static uint32_t outputMode = INIT_OUTPUTMODE;
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

#ifdef DEBUG_OVER_SERIAL
#define Serial_print(...) Serial.print(__VA_ARGS__)
#define Serial_println(...) Serial.println(__VA_ARGS__)
#else /* DEBUG_OVER_SERIAL */
#define Serial_print(...) {};
#define Serial_println(...) {};
#endif /* DEBUG_OVER_SERIAL */

bool telnetConnected = false;

#ifdef TELNET
ESPTelnet telnet;
uint16_t port=23;

void onTelnetConnect(String ip) {
  Serial_print(F("- Telnet: "));
  Serial_print(ip);
  Serial_println(F(" connected"));
  telnet.print("\nWelcome " + telnet.getIP() + " to ");
  telnet.print(WELCOMESTRING_TELNET);
  telnet.print(" compiled ");
  telnet.print(__DATE__);
  telnet.print(" ");
  telnet.print(__TIME__);
#ifdef E_SERIES
  telnet.println(" for E-Series");
#endif
#ifdef F_SERIES
  telnet.println(" for F-Series");
#endif
  telnet.println("(Use ^] + q  to disconnect.)");
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
      client.publish(key, MQTT_QOS, retain, value);
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

// Include Daikin product-dependent header file for parameter conversion
// As this program does not use the (avr only) P1P2Serial library,
// we need to include the json header file with a relative reference below
// include here such that Sprint_P(PSTR()) is available in header file code

#ifdef E_SERIES
#include "P1P2_Daikin_ParameterConversion_EHYHB.h"
#endif
#ifdef F_SERIES
#include "P1P2_Daikin_ParameterConversion_F.h"
#endif

static byte throttle = 1;
static byte throttleValue = THROTTLE_VALUE;

void onMqttConnect(bool sessionPresent) {
  Serial.println(F("* [ESP] Connected to MQTT server"));
  int result = client.subscribe(mqttCommands, MQTT_QOS);
  Sprint_P(true, false, false, PSTR("* [ESP] Subscribed to %s with result %i"), mqttCommands, result);
  result = client.subscribe(mqttCommandsNoIP, MQTT_QOS);
  Sprint_P(true, false, false, PSTR("* [ESP] Subscribed to %s with result %i"), mqttCommandsNoIP, result);
#ifdef MQTT_INPUT_BINDATA
  //result = client.subscribe("P1P2/X/#", MQTT_QOS);
  //Sprint_P(true, false, false, PSTR("* [ESP] Subscribed to X/# with result %i"), result);
  if (!mqttInputBinData[MQTT_KEY_PREFIXIP - 1]) {
    mqttInputBinData[MQTT_KEY_PREFIXIP - 1] = '/';
    mqttInputBinData[MQTT_KEY_PREFIXIP] = '#';
    mqttInputBinData[MQTT_KEY_PREFIXIP + 1] = '\0';
  }
  result = client.subscribe(mqttInputBinData, MQTT_QOS);
  Sprint_P(true, false, false, PSTR("* [ESP] Subscribed to %s with result %i"), mqttInputBinData, result);
  mqttInputBinData[MQTT_KEY_PREFIXIP - 1] = '\0';
#endif
#ifdef MQTT_INPUT_HEXDATA
  //result = client.subscribe("P1P2/R/#", MQTT_QOS);
  //Sprint_P(true, false, false, PSTR("* [ESP] Subscribed to R/# with result %i"), result);
  if (!mqttInputHexData[MQTT_KEY_PREFIXIP - 1]) {
    mqttInputHexData[MQTT_KEY_PREFIXIP - 1] = '/';
    mqttInputHexData[MQTT_KEY_PREFIXIP] = '#';
    mqttInputHexData[MQTT_KEY_PREFIXIP + 1] = '\0';
  }
  result = client.subscribe(mqttInputHexData, MQTT_QOS);
  Sprint_P(true, false, false, PSTR("* [ESP] Subscribed to %s with result %i"), mqttInputHexData, result);
  mqttInputHexData[MQTT_KEY_PREFIXIP - 1] = '\0';
#endif
  throttleValue = THROTTLE_VALUE;
  mqttConnected = true;
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Serial_print(F("* [ESP] Disconnected from MQTT server "));
  Serial.println(Mqtt_disconnects);
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

typedef struct EEPROMSettingsOld {
  char signature[10];
  char mqttUser[20];
  char mqttPassword[40];
  char mqttServer[20];
  int  mqttPort;
};

#define NR_RESERVED 99

typedef struct EEPROMSettingsNew {
  char signature[10];
  char mqttUser[81];
  char mqttPassword[81];
  char mqttServer[20];
  int  mqttPort;
  byte rebootReason;
// additional values with newest signature:
  uint32_t outputMode;
  byte outputFilter;
  byte mqttInputByte4;
  byte reserved[NR_RESERVED];
};


union EEPROMSettingsUnion {
  EEPROMSettingsOld EEPROMold;
  EEPROMSettingsNew EEPROMnew;
};

EEPROMSettingsUnion EEPROM_state;

void ATmega_dummy_for_serial() {
  Sprint_P(true, true, true, PSTR("* [ESP] Two dummy lines to ATmega."));
  Serial.print(F(SERIAL_MAGICSTRING));
  Serial.println(F("* Dummy line 1."));
  Serial.print(F(SERIAL_MAGICSTRING));
  Serial.println(F("* Dummy line 2."));
  Serial.print(F(SERIAL_MAGICSTRING));
  Serial.println('V');
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
static uint8_t serial_rb = 0;
static int c;
static byte ESP_serial_input_Errors_Data_Short = 0;
static byte ESP_serial_input_Errors_CRC = 0;

static uint32_t ATmega_uptime_prev = 0;

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
  IPAddress local_ip = WiFi.localIP();
  switch (cmdString[0]) {
    case 'a': // reset ATmega
    case 'A': Sprint_P(true, true, true, PSTR("* [ESP] Hard resetting ATmega..."));
              pinMode(RESET_PIN, OUTPUT);
              digitalWrite(RESET_PIN, LOW);
              delay(1);
              digitalWrite(RESET_PIN, HIGH);
              pinMode(RESET_PIN, INPUT);
              delay(200);
              ATmega_dummy_for_serial();
              ATmega_uptime_prev = 0;
              break;
    case 'b': // display or set MQTT settings
    case 'B': if ((n = sscanf((const char*) (cmdString + 1), "%19s %i %80s %80s %i", &EEPROM_state.EEPROMnew.mqttServer, &EEPROM_state.EEPROMnew.mqttPort, &EEPROM_state.EEPROMnew.mqttUser, &EEPROM_state.EEPROMnew.mqttPassword, &mqttInputByte4)) > 0) {
                Sprint_P(true, true, true, PSTR("* [ESP] Writing new MQTT settings to EEPROM"));
                if (n > 4) EEPROM_state.EEPROMnew.mqttInputByte4 = mqttInputByte4;
                EEPROM.put(0, EEPROM_state);
                EEPROM.commit();
              } 
              if (n > 0) {
                Sprint_P(true, true, true, PSTR("* [ESP] EEPROM_state set to %s"), EEPROM_state.EEPROMnew.mqttServer);
              } else {
                Sprint_P(true, true, true, PSTR("* [ESP] EEPROM_state %s"), EEPROM_state.EEPROMnew.mqttServer);
              }
              if (n > 1) {
                Sprint_P(true, true, true, PSTR("* [ESP] MQTT_port set to %i"), EEPROM_state.EEPROMnew.mqttPort);
              } else {
                Sprint_P(true, true, true, PSTR("* [ESP] MQTT_port %i"), EEPROM_state.EEPROMnew.mqttPort);
              }
              if (n > 2) {
                Sprint_P(true, true, true, PSTR("* [ESP] MQTT_user set to %s"), EEPROM_state.EEPROMnew.mqttUser);
              } else {
                Sprint_P(true, true, true, PSTR("* [ESP] MQTT_user %s"), EEPROM_state.EEPROMnew.mqttUser);
              }
              if (n > 3) {
                Sprint_P(true, true, true, PSTR("* [ESP] MQTT_password set to %s"), EEPROM_state.EEPROMnew.mqttPassword);
              } else {
                Sprint_P(true, true, true, PSTR("* [ESP] MQTT_password %s"), EEPROM_state.EEPROMnew.mqttPassword);
              }
              Sprint_P(true, true, true, PSTR("* [ESP] Local IP address: %i.%i.%i.%i"), local_ip[0], local_ip[1], local_ip[2], local_ip[3]);
              if (n > 0) {
                if (client.connected()) {
                  Sprint_P(true, true, true, PSTR("* [ESP] MQTT Client connected, disconnect"));
                  // unsubscribe topics here before disconnect?
                  client.disconnect();
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
                if (client.connected()) {
                  Sprint_P(true, true, true, PSTR("* [ESP] MQTT Client still connected ?"));
                  delay(500);
                } else {
                  Sprint_P(true, true, true, PSTR("* [ESP] MQTT Client disconnected"));
                }
                client.setServer(EEPROM_state.EEPROMnew.mqttServer, EEPROM_state.EEPROMnew.mqttPort);
                client.setCredentials((EEPROM_state.EEPROMnew.mqttUser[0] == '\0') ? 0 : EEPROM_state.EEPROMnew.mqttUser, (EEPROM_state.EEPROMnew.mqttPassword[0] == '\0') ? 0 : EEPROM_state.EEPROMnew.mqttPassword);
                Sprint_P(true, true, true, PSTR("* [ESP] MQTT Try to connect"));
                Mqtt_disconnectTime = 0;
                client.connect();
                delay(500);
                if (client.connected()) {
                  Sprint_P(true, true, true, PSTR("* [ESP] MQTT client connected with EEPROMnew settings"));
                } else {
                  Sprint_P(true, true, true, PSTR("* [ESP] MQTT connection failed, retrying in 5 seconds"));
                  reconnectTime = espUptime + 5;
                }
              }
              break;
    case 'd': // reset ESP
    case 'D': if (sscanf((const char*) (cmdString + 1), "%d", &temp) == 1) {
                if (temp > 2) temp = 2;
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
                  case 2 : Sprint_P(true, true, true, PSTR("* [ESP] Resetting maxLoopTime")); maxLoopTime = 0; break;
                  default : break;
                }
              } else {
                Sprint_P(true, true, true, PSTR("* [ESP] Specify D0=RESTART-ESP D1=RESET-ESP D2=RESET-maxLoopTime"));
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
                Sprint_P(true, true, true, PSTR("* [ESP] %ix 0x1000 to output timing data also over P1P2/R/xxx (prefix: C)"), (outputMode >> 12) & 0x01);
                Sprint_P(true, true, true, PSTR("* [ESP] %ix 0x2000 to output error data also over P1P2/R/xxx (prefix: *)"), (outputMode >> 13) & 0x01);
                Sprint_P(true, true, true, PSTR("* [ESP] %ix 0x4000 to use P1P2/R/xxx as input (requires MQTT_INPUT_HEXDATA)"), (outputMode >> 14) & 0x01);
                Sprint_P(true, true, true, PSTR("* [ESP] %ix 0x8000 to use P1P2/X/xxx as input (requires MQTT_INPUT_BINDATA)"), (outputMode >> 15) & 0x01);
                Sprint_P(true, true, true, PSTR("* [ESP] %ix 0x10000 to include non-HACONFIG parameters in P1P2/P/# "), (outputMode >> 16) & 0x01);
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
#endif
#ifdef F_SERIES
    Sprint_P(true, true, true, PSTR("* [ESP] F-Series"));
#endif
                          if (sscanf((const char*) (cmdString + 1), "%2x", &temp) == 1) {
                            if (temp < 2) {
                              Sprint_P(true, true, true, PSTR("* [ESP] Warning: verbose < 2 not supported by P1P2-bridge-esp8266, changing to verbosity mode 3"));
                              cmdString[1] = '3';
                              cmdString[2] = '\n';
                            }
                            // TODO perhaps catch v > 4 too
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
    default : Sprint_P(true, true, true, PSTR("* [ESP] To ATmega: ->%s<-"), cmdString);
              Serial.print(F(SERIAL_MAGICSTRING));
              Serial.println((char *) cmdString);
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
        for (uint8_t i = 0; i < len; i++) MQTT_readBuffer_writeHex(payload[i]);
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
        for (uint8_t i = 0; i < len; i++) MQTT_readBuffer_writeChar(payload[i]);
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
static byte saveRebootReason = REBOOT_REASON_UNKNOWN;

void setup() {
// RESET_PIN (input mode unless reset ('A' command) actively pulls it down)
  pinMode(RESET_PIN, INPUT);

// Set up Serial from/to P1P2Monitor on ATmega (250kBaud); or serial from/to USB debugging (115.2kBaud);
  delay(100);
  Serial.setRxBufferSize(RX_BUFFER_SIZE); // default value is too low for ESP taking long pauses at random moments...
  Serial.begin(SERIALSPEED);
  while (!Serial);      // wait for Arduino Serial Monitor to open
  Serial_println(F("*"));        // this line is copied back by ATmega as "first line ignored"
  Serial_println(WELCOMESTRING);
  Serial_print(F("* Compiled "));
  Serial_print(__DATE__);
  Serial_print(' ');
  Serial_println(__TIME__);

// WiFiManager setup
  // Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;
  // Set config save notify callback
  wifiManager.setSaveConfigCallback(WiFiManagerSaveConfigCallback);
  // Customize AP
  WiFiManagerParameter custom_text("<p>P1P2-ESP-Interface</p>");
  wifiManager.addParameter(&custom_text);
  // Debug info?
#ifdef DEBUG_OVER_SERIAL
  wifiManager.setDebugOutput(true);
#endif
  // add 4 MQTT settings to WiFiManager, with default settings preconfigured in NetworkParams.h
  WiFiManagerParameter WiFiManMqttServer("mqttserver", "MqttServer (IPv4, required)", MQTT_SERVER, 19);
  WiFiManagerParameter WiFiManMqttPort("mqttport", "MqttPort (optional, default 1883)", MQTT_PORT_STRING, 9);
  WiFiManagerParameter WiFiManMqttUser("mqttuser", "MqttUser (optional)", MQTT_USER, 19);
  WiFiManagerParameter WiFiManMqttPassword("mqttpassword", "MqttPassword (optional)", MQTT_PASSWORD, 39);
  wifiManager.addParameter(&WiFiManMqttServer);
  wifiManager.addParameter(&WiFiManMqttPort);
  wifiManager.addParameter(&WiFiManMqttUser);
  wifiManager.addParameter(&WiFiManMqttPassword);

  // reset WiFiMangager settings - for testing only;
  // wifiManager.resetSettings();

  // Set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  wifiManager.setAPCallback(WiFiManagerConfigModeCallback);
  // Custom static IP for AP may be configured here
  // wifiManager.setAPStaticIPConfig(IPAddress(10,0,1,1), IPAddress(10,0,1,1), IPAddress(255,255,255,0));
  // Custom static IP for client may be configured here
  // wifiManager.setSTAStaticIPConfig(IPAddress(192,168,0,99), IPAddress(192,168,0,1), IPAddress(255,255,255,0)); // optional DNS 4th argument

// get EEPROM data and rebootReason
  EEPROM.begin(sizeof(EEPROMSettingsUnion));
  EEPROM.get(0, EEPROM_state);
  saveRebootReason = REBOOT_REASON_NOTSUPPORTED;
#ifdef REBOOT_REASON
  if (!strcmp(EEPROM_state.EEPROMnew.signature, EEPROM_SIGNATURE_NEW)) {
    saveRebootReason = EEPROM_state.EEPROMnew.rebootReason;
    if (saveRebootReason != REBOOT_REASON_UNKNOWN) {
      EEPROM_state.EEPROMnew.rebootReason = REBOOT_REASON_UNKNOWN;
      EEPROM.put(0, EEPROM_state);
      EEPROM.commit();
    }
  } else {
    saveRebootReason = REBOOT_REASON_NOTSTORED;
  }
#endif /* REBOOT_REASON */

// WiFiManager start
  // Fetches ssid, password, and tries to connect.
  // If it does not connect it starts an access point with the specified name,
  // and goes into a blocking loop awaiting configuration.
  // First parameter is name of access point, second is the password.
  Serial_println(F("* [ESP] Trying autoconnect"));
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
  IPAddress local_ip = WiFi.localIP();
  Serial_print(F("* [ESP] Connected, IP address: "));
  Serial_println(local_ip);

//Silence WiFiManager from here on
  wifiManager.setDebugOutput(false);

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

  Sprint_P(true, false, false, PSTR("* [ESP] ESP reboot reason %i"), saveRebootReason);

// Store/retrieve MQTT settings entered in WiFiManager portal, and save to EEPROM
  if (shouldSaveConfig) {
    Serial_println(F("* [ESP] Writing new MQTT parameters to EEPROM"));
    strlcpy(EEPROM_state.EEPROMnew.signature,    EEPROM_SIGNATURE_NEW, sizeof(EEPROM_state.EEPROMnew.signature));
    strlcpy(EEPROM_state.EEPROMnew.mqttServer,   WiFiManMqttServer.getValue(),   sizeof(EEPROM_state.EEPROMnew.mqttServer));
    if ((!strlen(WiFiManMqttPort.getValue())) || (sscanf(WiFiManMqttPort.getValue(), "%i", &EEPROM_state.EEPROMnew.mqttPort) != 1)) EEPROM_state.EEPROMnew.mqttPort = MQTT_PORT;
    strlcpy(EEPROM_state.EEPROMnew.mqttUser,     WiFiManMqttUser.getValue(),     sizeof(EEPROM_state.EEPROMnew.mqttUser));
    strlcpy(EEPROM_state.EEPROMnew.mqttPassword, WiFiManMqttPassword.getValue(), sizeof(EEPROM_state.EEPROMnew.mqttPassword));
    EEPROM_state.EEPROMnew.rebootReason = REBOOT_REASON_UNKNOWN;
    EEPROM_state.EEPROMnew.outputMode = INIT_OUTPUTMODE;
    EEPROM_state.EEPROMnew.outputFilter = INIT_OUTPUTFILTER;
    EEPROM_state.EEPROMnew.mqttInputByte4 = 0;
    for (int i = 0; i < NR_RESERVED; i++) EEPROM_state.EEPROMnew.reserved[i] = 0;
    EEPROM.put(0, EEPROM_state);
    EEPROM.commit();
  }

  EEPROM.get(0, EEPROM_state);
  Serial_println(F("* [ESP] Check EEPROM for old1 signature"));
  if (!strcmp(EEPROM_state.EEPROMold.signature, EEPROM_SIGNATURE_OLD1)) {
    Serial_println(F("* [ESP] Old1 signature match, need to update EEPROM"));
    Sprint_P(true, true, true, PSTR("* [ESP] Old1 signature match, need to update EEPROM"));
    strlcpy(EEPROM_state.EEPROMnew.signature,    EEPROM_SIGNATURE_OLD2, sizeof(EEPROM_state.EEPROMnew.signature));
    // order is important, don't overwrite old data before reading it
    EEPROM_state.EEPROMnew.rebootReason = REBOOT_REASON_UNKNOWN;
    EEPROM_state.EEPROMnew.mqttPort = EEPROM_state.EEPROMold.mqttPort;
    strlcpy(EEPROM_state.EEPROMnew.mqttServer,   EEPROM_state.EEPROMold.mqttServer,    sizeof(EEPROM_state.EEPROMnew.mqttServer));
    strlcpy(EEPROM_state.EEPROMnew.mqttPassword, EEPROM_state.EEPROMold.mqttPassword,  sizeof(EEPROM_state.EEPROMnew.mqttPassword));
    strlcpy(EEPROM_state.EEPROMnew.mqttUser,     EEPROM_state.EEPROMold.mqttUser,      sizeof(EEPROM_state.EEPROMnew.mqttUser));
    EEPROM.put(0, EEPROM_state);
    EEPROM.commit();
    EEPROM.get(0, EEPROM_state);
  }

  Serial_println(F("* [ESP] Check EEPROM for old2 signature"));
  if (!strcmp(EEPROM_state.EEPROMold.signature, EEPROM_SIGNATURE_OLD2)) {
    Serial_println(F("* [ESP] Old2 signature match, need to initiate outputMode and zero more reserved bytes"));
    Sprint_P(true, true, true, PSTR("* [ESP] Old2 signature match, need to initiate outputMode and zero more reserved bytes"));
    strlcpy(EEPROM_state.EEPROMnew.signature,    EEPROM_SIGNATURE_NEW, sizeof(EEPROM_state.EEPROMnew.signature));
    EEPROM_state.EEPROMnew.outputMode = INIT_OUTPUTMODE;
    EEPROM_state.EEPROMnew.outputFilter = INIT_OUTPUTFILTER;
    EEPROM_state.EEPROMnew.mqttInputByte4 = 0;
    for (int i = 0; i < NR_RESERVED; i++) EEPROM_state.EEPROMnew.reserved[i] = 0;
    EEPROM.put(0, EEPROM_state);
    EEPROM.commit();
    EEPROM.get(0, EEPROM_state);
  }

  Serial_println(F("* [ESP] Check EEPROM signature, read EEPROM"));
  if (strcmp(EEPROM_state.EEPROMnew.signature, EEPROM_SIGNATURE_NEW)) {
    Serial_println(F("* [ESP] Signature mismatch, need to init EEPROM"));
    Sprint_P(true, true, true, PSTR("* [ESP] Signature mismatch, need to init EEPROM"));
    strlcpy(EEPROM_state.EEPROMnew.signature,    EEPROM_SIGNATURE_NEW, sizeof(EEPROM_state.EEPROMnew.signature));
    strlcpy(EEPROM_state.EEPROMnew.mqttUser,     MQTT_USER,          sizeof(EEPROM_state.EEPROMnew.mqttUser));
    strlcpy(EEPROM_state.EEPROMnew.mqttPassword, MQTT_PASSWORD,      sizeof(EEPROM_state.EEPROMnew.mqttPassword));
    strlcpy(EEPROM_state.EEPROMnew.mqttServer,   MQTT_SERVER,        sizeof(EEPROM_state.EEPROMnew.mqttServer));
    EEPROM_state.EEPROMnew.mqttPort = MQTT_PORT;
    EEPROM_state.EEPROMnew.rebootReason = REBOOT_REASON_UNKNOWN;
    EEPROM_state.EEPROMnew.outputMode = INIT_OUTPUTMODE;
    EEPROM_state.EEPROMnew.outputFilter = INIT_OUTPUTFILTER;
    EEPROM_state.EEPROMnew.mqttInputByte4 = 0;
    for (int i = 0; i < NR_RESERVED; i++) EEPROM_state.EEPROMnew.reserved[i] = 0;
    EEPROM.put(0, EEPROM_state);
    EEPROM.commit();
  } else {
    Serial_println(F("* [ESP] New EEPROM signature match, use EEPROM settings"));
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
  Serial_print(F("* [ESP] outputMode = ")); Serial.println(outputMode);
  Serial_print(F("* [ESP] outputFilter = ")); Serial.println(outputFilter);

// MQTT client setup
  client.onConnect(onMqttConnect);
  client.onDisconnect(onMqttDisconnect);
  client.onPublish(onMqttPublish);
  client.onMessage(onMqttMessage);
  client.setServer(EEPROM_state.EEPROMnew.mqttServer, EEPROM_state.EEPROMnew.mqttPort);

  MQTT_CLIENTNAME[MQTT_CLIENTNAME_IP]     = (local_ip[3] / 100) + '0';
  MQTT_CLIENTNAME[MQTT_CLIENTNAME_IP + 1] = (local_ip[3] % 100) / 10 + '0';
  MQTT_CLIENTNAME[MQTT_CLIENTNAME_IP + 2] = (local_ip[3] % 10) + '0';

  client.setClientId(MQTT_CLIENTNAME);
  client.setCredentials((EEPROM_state.EEPROMnew.mqttUser[0] == '\0') ? 0 : EEPROM_state.EEPROMnew.mqttUser, (EEPROM_state.EEPROMnew.mqttPassword[0] == '\0') ? 0 : EEPROM_state.EEPROMnew.mqttPassword);
  // client.setWill(MQTT_WILL_TOPIC, MQTT_WILL_QOS, MQTT_WILL_RETAIN, MQTT_WILL_PAYLOAD); // TODO

  Serial_print(F("* [ESP] Clientname ")); Serial_println(MQTT_CLIENTNAME);
  Serial_print(F("* [ESP] User ")); Serial_println(EEPROM_state.EEPROMnew.mqttUser);
  // Serial_print(F("* [ESP] Password ")); Serial_println(EEPROM_state.EEPROMnew.mqttPassword);
  Serial_print(F("* [ESP] Server ")); Serial_println(EEPROM_state.EEPROMnew.mqttServer);
  Serial_print(F("* [ESP] Port ")); Serial_println(EEPROM_state.EEPROMnew.mqttPort);

  delay(100);
  client.connect();
  delay(500);

  if (client.connected()) {
    Serial.println(F("* [ESP] MQTT client connected on first try"));
  } else {
    Serial.println(F("* [ESP] MQTT client connect failed"));
  }

  byte connectTimeout = 10; // 50s timeout
  while (!client.connected()) {
    client.connect();
    if (client.connected()) {
      Serial.println(F("* [ESP] MQTT client connected"));
      break;
    } else {
      Serial.print(F("* [ESP] MQTT client connect failed, retrying in setup() until time-out "));
      Serial.println(connectTimeout);
      if (!--connectTimeout) break;
      delay(5000);
    }
  }
  if (client.connected()) {
    Sprint_P(true, true, true, PSTR(WELCOMESTRING));
#ifdef E_SERIES
    Sprint_P(true, true, true, PSTR("* [ESP] E-Series"));
#endif
#ifdef F_SERIES
    Sprint_P(true, true, true, PSTR("* [ESP] F-Series"));
#endif
    Sprint_P(true, true, true, PSTR("* [ESP] Connected to MQTT server"));
  }
  delay(200);

#ifdef TELNET
// setup telnet
  // passing on functions for various telnet events
  telnet.onConnect(onTelnetConnect);
  telnet.onConnectionAttempt(onTelnetConnectionAttempt);
  telnet.onReconnect(onTelnetReconnect);
  telnet.onDisconnect(onTelnetDisconnect);
  telnet.onInputReceived(onInputReceived);
  bool telnetSuccess = telnet.begin(port);
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
  // Password "admin" can be set with it's md5 value as well
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
    Serial_println(F("* [ESP] OTA End"));
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
  avrprog.setReset(false);  // let the AVR run
  MDNS.begin(avrisp_host);
  MDNS.addService("avrisp", "tcp", avrisp_port);
  Sprint_P(true, true, true, PSTR("* [ESP] AVRISP: ATmega programming: avrdude -c avrisp -p atmega328p -P net:%i.%i.%i.%i:%i -t # or -U ..."), local_ip[0], local_ip[1], local_ip[2], local_ip[3], avrisp_port);
  // listen for avrdudes
  avrprog.begin();
#endif

  prevMillis = millis();

// Flush ATmega's serial input
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
}

static int jsonTerm = 1; // indicates whether json output was terminated
int jsonStringp = 0;
char jsonString[10000];

void process_for_mqtt_json(byte* rb, int n) {
  char mqtt_value[MQTT_VALUE_LEN] = "\0";
  char mqtt_key[MQTT_KEY_LEN + MQTT_KEY_PREFIXLEN]; // = mqttKeyPrefix;
  if (!mqttConnected) Mqtt_disconnectSkippedPackets++;
  if (mqttConnected || MQTT_DISCONNECT_CONTINUE) {
    if (n == 3) bytes2keyvalue(rb[0], rb[2], EMPTY_PAYLOAD, rb + 3, mqtt_key, mqtt_value);
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

byte timeStamp = 10; // TODO change this for TIMESTRING

void writePseudoPacket(byte* WB, byte rh)
// rh is packet size (without CRC byte)
{
  char pseudoWriteBuffer[RB];
  snprintf(pseudoWriteBuffer, 13, "R P         ");
// if (outputMode & ??) add timestring TODO to pseudoWriteBuffer snprintf(pseudoWriteBuffer, 13, "R TIMESTRING P         ");
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
  if ((outputMode & 0x0800) && (mqttConnected)) client.publish((const char*) mqttBindata, MQTT_QOS, false, (const char*) WB, rh + 1);
#endif /* MQTT_INPUT_BINDATA || MQTT_INPUT_HEXDATA */
  if (outputMode & 0x0666) process_for_mqtt_json(WB, rh);
}

uint32_t espUptime_telnet = 0;

void loop() {
  byte readHex[HB];
  // OTA
  ArduinoOTA.handle();

  // ESP-uptime and loop timing
  uint32_t currMillis = millis();
  uint16_t loopTime = currMillis - prevMillis;
  milliInc += loopTime;
  if (loopTime > maxLoopTime) maxLoopTime = loopTime;
  prevMillis = currMillis;
  while (milliInc >= 1000) {
    milliInc -= 1000;
    espUptime += 1;
    if (!client.connected()) {
      Mqtt_disconnectTimeTotal++;
      Mqtt_disconnectTime++;
    } else {
      Mqtt_disconnectTime = 0;
    }
    if (milliInc < 1000) {
      if ((throttleValue > 1) && (espUptime > (THROTTLE_VALUE - throttleValue + 1) * THROTTLE_STEP)) throttleValue--;
      Sprint_P(true, true, false, PSTR("* [ESP] Uptime %li"), espUptime);
      if (espUptime >= espUptime_telnet + 10) {
        espUptime_telnet = espUptime_telnet + 10;
        Sprint_P(false, false, true, PSTR("* [ESP] Uptime %li"), espUptime);
      }
      if (!client.connected()) Sprint_P(true, false, true, PSTR("* [ESP] MQTT is disconnected (%i s total %i s)"), Mqtt_disconnectTime, Mqtt_disconnectTimeTotal);
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
  AVRISPState_t new_state = avrprog.update();
  if (last_state != new_state) {
    switch (new_state) {
      case AVRISP_STATE_IDLE:    Sprint_P(true, true, true, PSTR("* [ESP-AVRISP] now idle"));           pinMode(RESET_PIN, INPUT); delay (200); ATmega_dummy_for_serial(); break;
      case AVRISP_STATE_PENDING: Sprint_P(true, true, true, PSTR("* [ESP-AVRISP] connection pending")); pinMode(RESET_PIN, OUTPUT); break;
      case AVRISP_STATE_ACTIVE:  Sprint_P(true, true, true, PSTR("* [ESP-AVRISP] programming mode"));   pinMode(RESET_PIN, OUTPUT); break;
    }
    last_state = new_state;
  }
  // Serve the AVRISP client
  if (last_state != AVRISP_STATE_IDLE) avrprog.serve();
  if (WiFi.status() == WL_CONNECTED) MDNS.update();
#endif /* AVRISP */

  // mqtt connection check
  if (!client.connected()) {
    if (espUptime > reconnectTime) {
      Sprint_P(true, false, true, PSTR("* [ESP] MQTT Try to reconnect to MQTT server %s:%i (%s/%s)"), EEPROM_state.EEPROMnew.mqttServer, EEPROM_state.EEPROMnew.mqttPort, EEPROM_state.EEPROMnew.mqttUser, EEPROM_state.EEPROMnew.mqttPassword);
      client.connect();
      delay(500);
      if (client.connected()) {
        Sprint_P(true, false, true, PSTR("* [ESP] Mqtt reconnected"));
        Sprint_P(true, false, true, PSTR(WELCOMESTRING));
      } else {
        Sprint_P(true, false, true, PSTR("* [ESP] Reconnect failed, retrying in 5 seconds"));
        Serial.println(F("* [ESP] Reconnect to MQTT failed, see telnet for details"));
        reconnectTime = espUptime + 5;
      }
    }
    // else wait before trying reconnection
  }

  if (!OTAbusy)  {
    // Non-blocking serial input
    // serial_rb is number of char received and stored in RB
    // rb_buffer = readBuffer + serial_rb
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
      serial_rb++;
    }
#else
    if (!ignoreSerial) {
       while (((c = Serial.read()) >= 0) && (c != '\n') && (serial_rb < RB)) {
        *rb_buffer++ = (char) c;
        serial_rb++;
      }
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
          Sprint_P(true, true, true, PSTR("* [MON] First ATmega input line ignored %s"), readBuffer);
          ignoreremainder = 0;
        } else if (ignoreremainder == 1) {
          Sprint_P(true, true, true, PSTR("* [MON] Line too long, last part: ->%s<-"), readBuffer);
          ignoreremainder = 0;
        } else {
          if (readBuffer[0] == 'R') {
            int rbp = 1;
            int n, rbtemp;
            byte rh = 0;
            if ((serial_rb > 12) && ((readBuffer[2] == 'T') || (readBuffer[2] == 'P') || (readBuffer[2] == 'X'))) {
              // skip 10-character time stamp
              rbp = 12;
              timeStamp = 10;
            } else {
              timeStamp = 0;
            }
            while ((rh < HB) && (sscanf(readBuffer + rbp, "%2x%n", &rbtemp, &n) == 1)) {
              readHex[rh++] = rbtemp;
              rbp += n;
            }
            if (rh == HB) Sprint_P(true, true, true, PSTR("* [MON] Buffer full, overflow not checked"));
            if (rh > 1) {
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
                if (outputMode & 0x0001) client_publish_mqtt(mqttHexdata, readBuffer);
#endif /* MQTT_INPUT_BINDATA || MQTT_INPUT_HEXDATA */
                if (outputMode & 0x0010) client_publish_telnet(mqttHexdata, readBuffer);
                if (outputMode & 0x0100) client_publish_serial(mqttHexdata, readBuffer);
#if !((defined MQTT_INPUT_BINDATA) || (defined MQTT_INPUT_HEXDATA))
                if ((outputMode & 0x0800) && (mqttConnected)) client.publish((const char*) mqttBindata, MQTT_QOS, false, (const char*) readHex, rh + 1);
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
                Sprint_P(true, true, true, PSTR("* [MON] Serial input buffer overrun or CRC error in R data:%s expected 0x%02X"), readBuffer + 1, crc);
                if (ESP_serial_input_Errors_CRC < 0xFF) ESP_serial_input_Errors_CRC++;
              }
            } else {
              Sprint_P(true, true, true, PSTR("* [MON] Not enough readable data in R line: ->%s<-"), readBuffer + 1);
              if (ESP_serial_input_Errors_Data_Short < 0xFF) ESP_serial_input_Errors_Data_Short++;
            }
          } else if ((readBuffer[0] == 'C') || (readBuffer[0] == 'c')) {
            // timing info
            if (outputMode & 0x1000) client_publish_mqtt(mqttHexdata, readBuffer);
          } else if (readBuffer[0] == 'E') {
            // data with errors
            readBuffer[0] = '*'; // backwards output report compatibility
            Sprint_P(true, true, true, PSTR("* [MON]%s"), readBuffer + 1);
            if (outputMode & 0x2000) client_publish_mqtt(mqttHexdata, readBuffer);
          } else if (readBuffer[0] == '*') {
            Sprint_P(true, true, true, PSTR("* [MON]%s"), readBuffer + 1);
          } else {
            Sprint_P(true, true, true, PSTR("* [MON]:%s"), readBuffer);
            if (ESP_serial_input_Errors_Data_Short < 0xFF) ESP_serial_input_Errors_Data_Short++;
          }
        }
      } else {
        //  (c != '\n' ||  serial_rb == RB)
        char lst = *(rb_buffer - 1);
        *(rb_buffer - 1) = '\0';
        if (c != '\n') {
          Sprint_P(true, true, true, PSTR("* [MON] Line too long, ignored, ignoring remainder: ->%s<-->%c%c<-"), readBuffer, lst, c);
          ignoreremainder = 1;
        } else {
          Sprint_P(true, true, true, PSTR("* [MON] Line too long, terminated, ignored: ->%s<-->%c<-"), readBuffer, lst);
          ignoreremainder = 0;
        }
      }
      rb_buffer = readBuffer;
      serial_rb = 0;
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
      for (int i = 19; i <= 22; i++) readHex[i]  = 0x00; // reserved for future use
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
      readHex[17] = 0; // old 16-bit outputMode;
      readHex[18] = 0; // old 16-bit outputMode;
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
