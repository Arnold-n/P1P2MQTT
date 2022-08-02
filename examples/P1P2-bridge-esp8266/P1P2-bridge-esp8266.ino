/* P1P2-bridge-esp8266json: Host application for esp8266 to interface to P1P2Monitor on Arduino Uno,
 *                          supports mqtt/json/wifi
 *                          Includes wifimanager, OTA
 *                          For protocol description, see SerialProtocol.md and MqttProtocol.md
 *
 * Copyright (c) 2019-2022 Arnold Niessen, arnold.niessen -at- gmail-dot-com  - licensed under GPL v2.0 (see LICENSE)
 *
 * 20220802 v0.9.14 AVRISP, wifimanager, mqtt settings, EEPROM, telnet, outputMode, outputFilter, ...
 *
 * WARNING: P1P2-bridge-esp8266 is end-of-life, and will be replaced by P1P2MQTT
 *
 * Requires board support package:
 * http://arduino.esp8266.com/stable/package_esp8266com_index.json
 *
 * Requires libraries:
 * WiFiManager 0.16.0 by tzapu (installed using Arduino IDE)
 * AsyncMqttClient 0.9.0 by Marvin Roger obtained as ZIP from https://github.com/marvinroger/async-mqtt-client
 * ESPAsyncTCP 1.2.2 by Khoi Hoang obtained as ZIP from https://github.com/me-no-dev/ESPAsyncTCP/
 * ESP_Telnet 1.3.1 by  Lennart Hennigs (installed using Arduino IDE)
 *
 * Version history
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
#include <AsyncMqttClient.h>

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
static uint16_t outputMode = INIT_OUTPUTMODE;
#define outputUnknown (outputMode & 0x08)

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
  telnet.print("\nWelcome " + telnet.getIP() + " to P1P2-bridge-esp8266 v0.9.14 compiled ");
  telnet.print(__DATE__);
  telnet.print(" ");
  telnet.println(__TIME__);
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

#define RB 400     // max size of readBuffer (serial input from Arduino)
#define HB 33      // max size of hexbuf, same as P1P2Monitor (model-dependent? 24 might be sufficient)

char readBuffer[RB];
byte readHex[HB];

static uint8_t rb=0; // MOVE inside routine
static char* rb_buffer = readBuffer;
static int c;
static byte ESP_serial_input_Errors_Data_Short = 0;
static byte ESP_serial_input_Errors_CRC = 0;

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
uint16_t Mqtt_msgSkipLowMem = 0;
uint16_t Mqtt_msgSkipNotConnected = 0;
uint16_t Mqtt_disconnects = 0;
uint16_t Mqtt_disconnectSkippedPackets = 0;
uint16_t Mqtt_disconnectTime = 0;
uint32_t Mqtt_acknowledged = 0; // does not increment in case of QOS=0
uint32_t Mqtt_gap = 0;          // does not increment in case of QOS=0
uint16_t prevpacketId = 0;

#define MAXWAIT 20


uint32_t mqttPublished = 0;


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

#define SPRINT_VALUE_LEN 200
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
#include "P1P2_Daikin_ParameterConversion_EHYHB.h"

static byte throttle = 1;
static byte throttleValue = THROTTLE_VALUE;

void onMqttConnect(bool sessionPresent) {
  Sprint_P(true, false, false, PSTR("* [ESP] Connected to MQTT server"));
  int result = client.subscribe(mqttCommands, MQTT_QOS);
  Sprint_P(true, false, false, PSTR("* [ESP] Subscribed to %s with result %i"), mqttCommands, result);
  result = client.subscribe(mqttCommandsNoIP, MQTT_QOS);
  Sprint_P(true, false, false, PSTR("* [ESP] Subscribed to %s with result %i"), mqttCommandsNoIP, result);
  throttleValue = THROTTLE_VALUE;
  mqttConnected = true;
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Serial_println("* [ESP] onMqttDisconnect");
  Sprint_P(true, false, false, PSTR("* [ESP] Disconnected from MQTT."));
  Mqtt_disconnects++; // does not seem to work
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


typedef struct MQTTSettings {
  char signature[10];
  char mqttUser[20];
  char mqttPassword[40];
  char mqttServer[20];
  int  mqttPort;
};

MQTTSettings MQTT_server;

void ATmega_dummy_for_serial() {
  Sprint_P(true, true, true, PSTR("* [ESP] Two dummy lines to ATmega."));
  Serial.print(SERIAL_MAGICSTRING);
  Serial.println(F("* Dummy line 1."));
  Serial.print(SERIAL_MAGICSTRING);
  Serial.println(F("* Dummy line 2."));
}

#define MAX_COMMAND_LENGTH 102
bool MQTT_commandReceived = false;
char MQTT_commandString[MAX_COMMAND_LENGTH];
bool telnetCommandReceived = false;
char telnetCommandString[MAX_COMMAND_LENGTH];

static uint32_t ATmega_uptime_prev = 0;

void handleCommand(const char* cmdString) {
// handles a single command (not necessarily '\n'-terminated) received via telnet or MQTT (P1P2/W)
// most of these messages are fowarded over serial to P1P2Monitor on ATmega
// some messages are (also) handled on the ESP
  int temp = 0;
  byte temphex = 0;
  Serial_print(F("* [ESP] handleCommand cmdString ->"));
  Serial_print((char*) cmdString);
  Serial_print(F("<-"));
  Serial_println();
  int n;
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
    case 'B': if ((n = sscanf((const char*) (cmdString + 1), "%19s %i %19s %39s", &MQTT_server.mqttServer, &MQTT_server.mqttPort, &MQTT_server.mqttUser, &MQTT_server.mqttPassword)) > 0) {
                EEPROM.put(0, MQTT_server);
                EEPROM.commit();
              }
              if (n > 0) {
                Sprint_P(true, true, true, PSTR("* [ESP] MQTT_server set to %s"), MQTT_server.mqttServer);
              } else {
                Sprint_P(true, true, true, PSTR("* [ESP] MQTT_server %s"), MQTT_server.mqttServer);
              }
              if (n > 1) {
                Sprint_P(true, true, true, PSTR("* [ESP] MQTT_port set to %i"), MQTT_server.mqttPort);
              } else {
                Sprint_P(true, true, true, PSTR("* [ESP] MQTT_port %i"), MQTT_server.mqttPort);
              }
              if (n > 2) {
                Sprint_P(true, true, true, PSTR("* [ESP] MQTT_user set to %s"), MQTT_server.mqttUser);
              } else {
                Sprint_P(true, true, true, PSTR("* [ESP] MQTT_user %s"), MQTT_server.mqttUser);
              }
              if (n > 3) {
                Sprint_P(true, true, true, PSTR("* [ESP] MQTT_password set to %s"), MQTT_server.mqttPassword);
              } else {
                Sprint_P(true, true, true, PSTR("* [ESP] MQTT_password %s"), MQTT_server.mqttPassword);
              }
              Sprint_P(true, true, true, PSTR("* [ESP] Local IP address: %i.%i.%i.%i"), local_ip[0], local_ip[1], local_ip[2], local_ip[3]);
              if (n > 0) {
                if (client.connected()) {
                  Sprint_P(true, true, true, PSTR("* [ESP] MQTT Client connected, disconnect"));
                  client.disconnect();
                  delay(100);
                }
                if (client.connected()) {
                  Sprint_P(true, true, true, PSTR("* [ESP] MQTT Client still connected ?"));
                  delay(500);
                } else {
                  Sprint_P(true, true, true, PSTR("* [ESP] MQTT Client disconnected"));
                }
                client.setServer(MQTT_server.mqttServer, MQTT_server.mqttPort);
                client.setCredentials((MQTT_server.mqttUser == '\0') ? 0 : MQTT_server.mqttUser, (MQTT_server.mqttPassword == '\0') ? 0 : MQTT_server.mqttPassword);
                Sprint_P(true, true, true, PSTR("* [ESP] MQTT Try to connect"));
                client.connect();
                delay(500);
                if (client.connected()) {
                  Sprint_P(true, true, true, PSTR("* [ESP] MQTT client connected with new settings"));
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
                  case 0 : Sprint_P(true, true, true, PSTR("* [ESP] Restarting ESP...")); ESP.restart(); delay(100); break;
                  case 1 : Sprint_P(true, true, true, PSTR("* [ESP] Resetting ESP...")); ESP.reset(); delay(100); break;
                  case 2 : Sprint_P(true, true, true, PSTR("* [ESP] Resetting maxLoopTime")); maxLoopTime = 0; break;
                  default : break;
                }
              } else {
                Sprint_P(true, true, true, PSTR("* [ESP] Specify D0=RESTART-ESP D1=RESET-ESP D2=RESET-maxLoopTime"));
              }
              break;
    case 'j': // OutputMode
    case 'J': if (sscanf((const char*) (cmdString + 1), "%x", &temp) == 1) {
                if (temp > 0xFFF) temp = 0xFFF;
                outputMode = temp;
                Sprint_P(true, true, true, PSTR("* [ESP] Outputmode set to 0x%02X"), outputMode);
              } else {
                Sprint_P(true, true, true, PSTR("* [ESP] Outputmode 0x%02X"), outputMode);
              }
              break;
    case 's': // OutputFilter
    case 'S': if (sscanf((const char*) (cmdString + 1), "%d", &temp) == 1) {
                if (temp > 3) temp = 3;
                outputFilter = temp;
              } else {
                temp = 99;
              }
              switch (outputFilter) {
                case 0 : Sprint_P(true, true, true, PSTR("* [ESP] Outputfilter %s 0 all parameters, no filter"), (temp != 99) ? "set to" : ""); break;
                case 1 : Sprint_P(true, true, true, PSTR("* [ESP] Outputfilter %s 1 only changed data"), (temp != 99) ? "set to" : ""); break;
                case 2 : Sprint_P(true, true, true, PSTR("* [ESP] Outputfilter %s 2 only changed data, excluding temp/flow"), (temp != 99) ? "set to" : ""); break;
                case 3 : Sprint_P(true, true, true, PSTR("* [ESP] Outputfilter %s 3 only changed data, excluding temp/flow/time"), (temp != 99) ? "set to" : ""); break;
                default: break;
              }
              break;
    case '\0':break;
    case '*': break;
    case 'g':
    case 'G': if (sscanf((const char*) (cmdString + 1), "%2x", &temphex) == 1) {
                crc_gen = temphex;
                Sprint_P(true, true, true, PSTR("* [ESP] CRC_gen set to 0x%02X"), crc_gen);
              } else {
                Sprint_P(true, true, true, PSTR("* [ESP] CRC_gen 0x%02X"), crc_gen);
              }
              // fallthrough
    case 'h':
    case 'H': if ((cmdString[0] != 'g') && (cmdString[0] != 'G')) {
                if (sscanf((const char*) (cmdString + 1), "%2x", &temphex) == 1) {
                  crc_feed = temphex;
                  Sprint_P(true, true, true, PSTR("* [ESP] CRC_feed set to 0x%02X"), crc_feed);
                } else {
                  Sprint_P(true, true, true, PSTR("* [ESP] CRC_feed 0x%02X"), crc_feed);
                }
              }
              // fallthrough
    default : Sprint_P(true, true, true, PSTR("* [ESP] To ATmega: ->%s<-"), cmdString);
              Serial.print(SERIAL_MAGICSTRING);
              Serial.println((char *) cmdString);
              break;
  }
}

bool MQTT_readBufferReceived = false;
int MQTT_rh;
byte MQTT_rbp = 0;

void onMqttMessage(char* topic, char* payload, const AsyncMqttClientMessageProperties& properties,
                   const size_t& len, const size_t& index, const size_t& total) {
  (void) payload;
  if (!len) {
    Serial_println(F("* [ESP] Empty MQTT payload received and ignored"));
  } else if (len > 0xFF) {
    Serial_println(F("* [ESP] Received MQTT payload too long"));
  } else if ((!strcmp(topic, mqttCommands)) || (!strcmp(topic, mqttCommandsNoIP))) {
    if (MQTT_commandReceived) {
      Serial_println(F("* [ESP] Ignoring MQTT command, previous command is still being processed"));
    } else {
      if (len + 1 >= MAX_COMMAND_LENGTH) {
        Serial_println(F("* [ESP] Received MQTT command too long"));
      } else {
        strlcpy(MQTT_commandString, payload, len + 1); // create null-terminated copy
        MQTT_commandString[len] = '\0';
        MQTT_commandReceived = true;
      }
    }
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
      telnetCommandString[str.length() + 1] = '\0';
      telnetCommandReceived = true;
    }
  }
}
#endif

bool OTAbusy = 0;
static byte ignoreremainder = 2; // first line ignored - robustness

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

// WiFiManager start
  // Fetches ssid, password, and tries to connect.
  // If it does not connect it starts an access point with the specified name,
  // and goes into a blocking loop awaiting configuration.
  // First parameter is name of access point, second is the password.
  Serial_println(F("* [ESP] Trying autoconnect"));
  if (!wifiManager.autoConnect(WIFIMAN_SSID, WIFIMAN_PASSWORD)) {
    Serial_println(F("* [ESP] Failed to connect and hit timeout, resetting"));
    // Reset and try again
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

// Store/retrieve MQTT settings entered in WiFiManager portal, and save to EEPROM
  EEPROM.begin(sizeof(MQTTSettings));
  if (shouldSaveConfig) {
    Serial_println(F("* [ESP] Writing new parameters to EEPROM"));
    strlcpy(MQTT_server.signature,    MQTT_SIGNATURE, sizeof(MQTT_server.mqttUser));
    strlcpy(MQTT_server.mqttServer,   WiFiManMqttServer.getValue(),   sizeof(MQTT_server.mqttServer));
    if ((!strlen(WiFiManMqttPort.getValue())) || (sscanf(WiFiManMqttPort.getValue(), "%i", &MQTT_server.mqttPort) != 1)) MQTT_server.mqttPort = MQTT_PORT;
    strlcpy(MQTT_server.mqttUser,     WiFiManMqttUser.getValue(),     sizeof(MQTT_server.mqttUser));
    strlcpy(MQTT_server.mqttPassword, WiFiManMqttPassword.getValue(), sizeof(MQTT_server.mqttPassword));
    EEPROM.put(0, MQTT_server);
    EEPROM.commit();
  }
  EEPROM.get(0, MQTT_server);
  Serial_println(F("* [ESP] Check EEPROM signature, read EEPROM"));
  if (strcmp(MQTT_server.signature, MQTT_SIGNATURE)) {
    Serial_println(F("* [ESP] Signature mismatch, need to init EEPROM"));
    Sprint_P(true, true, true, PSTR("* [ESP] Signature mismatch, need to init EEPROM"));
    strlcpy(MQTT_server.signature,    MQTT_SIGNATURE, sizeof(MQTT_server.signature));
    strlcpy(MQTT_server.mqttUser,     MQTT_USER,      sizeof(MQTT_server.mqttUser));
    strlcpy(MQTT_server.mqttPassword, MQTT_PASSWORD,  sizeof(MQTT_server.mqttPassword));
    strlcpy(MQTT_server.mqttServer,   MQTT_SERVER,    sizeof(MQTT_server.mqttServer));
    MQTT_server.mqttPort = MQTT_PORT;
    EEPROM.put(0, MQTT_server);
    EEPROM.commit();
  }

// MQTT client setup
  client.onConnect(onMqttConnect);
  client.onDisconnect(onMqttDisconnect);
  client.onPublish(onMqttPublish);
  client.onMessage(onMqttMessage);
  client.setServer(MQTT_server.mqttServer, MQTT_server.mqttPort);

  MQTT_CLIENTNAME[MQTT_CLIENTNAME_IP]     = (local_ip[3] / 100) + '0';
  MQTT_CLIENTNAME[MQTT_CLIENTNAME_IP + 1] = (local_ip[3] % 100) / 10 + '0';
  MQTT_CLIENTNAME[MQTT_CLIENTNAME_IP + 2] = (local_ip[3] % 10) + '0';

  client.setClientId(MQTT_CLIENTNAME);
  client.setCredentials((MQTT_server.mqttUser == '\0') ? 0 : MQTT_server.mqttUser, (MQTT_server.mqttPassword == '\0') ? 0 : MQTT_server.mqttPassword);
  // client.setWill(MQTT_WILL_TOPIC, MQTT_WILL_QOS, MQTT_WILL_RETAIN, MQTT_WILL_PAYLOAD); // TODO

  Serial_print(F("* [ESP] Clientname ")); Serial_println(MQTT_CLIENTNAME);
  Serial_print(F("* [ESP] User ")); Serial_println(MQTT_server.mqttUser);
  // Serial_print(F("* [ESP] Password ")); Serial_println(MQTT_server.mqttPassword);
  Serial_print(F("* [ESP] Server ")); Serial_println(MQTT_server.mqttServer);
  Serial_print(F("* [ESP] Port ")); Serial_println(MQTT_server.mqttPort);

  delay(100);
  client.connect();
  delay(500);

  if (client.connected()) {
    Serial_println(F("* [ESP] MQTT client connected on first try"));
  } else {
    Serial_println(F("* [ESP] MQTT client connect failed"));
  }

  byte connectTimeout = 10; // 50s timeout
  while (!client.connected()) {
    client.connect();
    if (client.connected()) {
      Serial_println(F("* [ESP] MQTT client connected"));
      break;
    } else {
      Serial_println(F("* [ESP] MQTT client connect failed"));
      if (!--connectTimeout) break;
      delay(5000);
    }
  }
  if (client.connected()) {
    Sprint_P(true, true, true, PSTR(WELCOMESTRING));
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
  if (telnet.begin(port)) {
    Sprint_P(true, false, false, PSTR("* [ESP] Telnet running on %i.%i.%i.%i"), local_ip[0], local_ip[1], local_ip[2], local_ip[3]);
  } else {
    Sprint_P(true, false, false, PSTR("* [ESP] Telnet error."));
  }
#endif

// OTA
  // port defaults to 8266
  // ArduinoOTA.setPort(8266);
  // Hostname defaults to esp8266-[ChipID]
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
  Sprint_P(true, true, true, PSTR("* [ESP] TELNET: telnet %i.%i.%i.%i"), local_ip[0], local_ip[1], local_ip[2], local_ip[3]);
  delay(1000);
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
        // returns 1 if a new mqtt_key,mqtt_value should be output
        // returns 8 if byte should be treated per bit
        // returns 9 if json string should be terminated
        if (kvrbyte == 9) {
          if (jsonTerm == 0) {
            // only terminate json string and publish if at least one parameter was written
            jsonTerm = 1;
            jsonString[jsonStringp++] = '}';
            jsonString[jsonStringp++] = '\0';
            if (outputMode & 0x004) client_publish_mqtt(mqttJsondata, jsonString);
            if (outputMode & 0x040) client_publish_telnet(mqttJsondata, jsonString);
            if (outputMode & 0x400) client_publish_serial(mqttJsondata, jsonString);
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
              if (outputMode & 0x002) client_publish_mqtt(mqtt_key, mqtt_value);
              if (outputMode & 0x020) client_publish_telnet(mqtt_key, mqtt_value);
              if (outputMode & 0x200) client_publish_serial(mqtt_key, mqtt_value);
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

byte timeStamp = 10;

void writePseudoPacket(byte* WB, byte rh, char* readBuffer)
// rh is packet size (without CRC byte)
{
  snprintf(readBuffer, 13, "R P         ");
  uint8_t crc = crc_feed;
  for (uint8_t i = 0; i < rh; i++) {
    uint8_t c = WB[i];
    snprintf(readBuffer + 2 + timeStamp + (i << 1), 3, "%02X", readHex[i]);
    if (crc_gen != 0) for (uint8_t i = 0; i < 8; i++) {
      crc = ((crc ^ c) & 0x01 ? ((crc >> 1) ^ crc_gen) : (crc >> 1));
      c >>= 1;
    }
  }
  WB[rh] = crc;
  if (crc_gen) snprintf(readBuffer + 2 + timeStamp + (rh << 1), 3, "%02X", crc);
  if (outputMode & 0x001) client_publish_mqtt(mqttHexdata, readBuffer);
  if (outputMode & 0x010) client_publish_telnet(mqttHexdata, readBuffer);
  if (outputMode & 0x100) client_publish_serial(mqttHexdata, readBuffer);
  if ((outputMode & 0x800) && (mqttConnected)) client.publish((const char*) mqttBindata, MQTT_QOS, false, (const char*) WB, rh + 1);
  if (outputMode & 0x666) process_for_mqtt_json(WB, rh);
}

static byte pseudo0D = 0;
static byte pseudo0E = 0;
static byte pseudo0F = 0;

uint32_t espUptime_telnet = 0;

void loop() {
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
    if (!client.connected()) Mqtt_disconnectTime++;
    if (milliInc < 1000) {
      if ((throttleValue > 1) && (espUptime > (THROTTLE_VALUE - throttleValue + 1) * THROTTLE_STEP)) throttleValue--;
      Sprint_P(true, true, false, PSTR("* [ESP] Uptime %li"), espUptime);
      if (espUptime >= espUptime_telnet + 10) {
        espUptime_telnet = espUptime_telnet + 10;
        Sprint_P(false, false, true, PSTR("* [ESP] Uptime %li"), espUptime);
      }
      if (!client.connected()) Sprint_P(true, false, true, PSTR("* [ESP] MQTT is disconnected (total %i s)"), Mqtt_disconnectTime);
      pseudo0D++;
      pseudo0E++;
      pseudo0F++;
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
      Sprint_P(true, false, true, PSTR("* [ESP] MQTT Try to reconnect to MQTT server %s:%i (%s/%s)"), MQTT_server.mqttServer, MQTT_server.mqttPort, MQTT_server.mqttUser, MQTT_server.mqttPassword);
      client.connect();
      delay(500);
      if (client.connected()) {
        Sprint_P(true, false, true, PSTR("* [ESP] Mqtt reconnected"));
        Sprint_P(true, false, true, PSTR(ESPWELCOMESTRING));
      } else {
        Sprint_P(true, false, true, PSTR("* [ESP] Reconnect failed, retrying in 5 seconds"));
        reconnectTime = espUptime + 5;
      }
    } 
    // else wait before trying reconnection
  }

  if (!OTAbusy)  {
    // Non-blocking serial input
    // rb is number of char received and stored in RB
    // rb_buffer = readBuffer + rb
    // ignore first line and too-long lines
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
    while (((c = Serial.read()) >= 0) && (c != '\n') && (rb < RB)) {
      *rb_buffer++ = (char) c;
      rb++;
    }
    // ((c == '\n' and rb > 0))  and/or  rb == RB)  or  c == -1
    if (c >= 0) {
      if ((c == '\n') && (rb < RB)) {
        // c == '\n'
        *rb_buffer = '\0';
        // Remove \r before \n in DOS input
        if ((rb > 0) && (*(rb_buffer - 1) == '\r')) {
          rb--;
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
            if ((rb > 12) && ((readBuffer[2] == 'T') || (readBuffer[2] == 'P'))) {
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
              if ((!crc_gen) || (crc == readHex[rh])) {
                if (outputMode & 0x001) client_publish_mqtt(mqttHexdata, readBuffer);
                if (outputMode & 0x010) client_publish_telnet(mqttHexdata, readBuffer);
                if (outputMode & 0x100) client_publish_serial(mqttHexdata, readBuffer);
                if ((outputMode & 0x800) && (mqttConnected)) client.publish((const char*) mqttBindata, MQTT_QOS, false, (const char*) readHex, rh + 1);
                if (outputMode & 0x666) process_for_mqtt_json(readHex, rh);
#ifdef PSEUDO_PACKETS
                if ((readHex[0] == 0x00) && (readHex[1] == 0x00) && (readHex[2] == 0x0D)) pseudo0D = 5; // Insert pseudo packet 40000D in output serial after 00000D
                if ((readHex[0] == 0x00) && (readHex[1] == 0x00) && (readHex[2] == 0x0F)) pseudo0F = 5; // Insert pseudo packet 40000F in output serial after 00000F
#endif
                if ((readHex[0] == 0x00) && (readHex[1] == 0x00) && (readHex[2] == 0x0E)) {
#ifdef PSEUDO_PACKETS
                  pseudo0E = 5; // Insert pseudo packet 40000E in output serial after 00000E
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
                Sprint_P(true, true, true, PSTR("* [MON] CRC error in R data:%s expected 0x%02X"), readBuffer + 1, crc);
                if (ESP_serial_input_Errors_CRC < 0xFF) ESP_serial_input_Errors_CRC++;
              }
            } else {
              Sprint_P(true, true, true, PSTR("* [MON] Not enough readable data in R line: ->%s<-"), readBuffer + 1);
              if (ESP_serial_input_Errors_Data_Short < 0xFF) ESP_serial_input_Errors_Data_Short++;
            }
          } else if (readBuffer[0] == '*') {
            Sprint_P(true, true, true, PSTR("* [MON]%s"), readBuffer + 1);
          } else {
            Sprint_P(true, true, true, PSTR("* [MON]:%s"), readBuffer);
            if (ESP_serial_input_Errors_Data_Short < 0xFF) ESP_serial_input_Errors_Data_Short++;
          }
        }
      } else {
        //  (c != '\n' ||  rb == RB)
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
      rb = 0;
      MQTT_readBufferReceived = false;
    } else {
      // wait for more serial input, or just nothing to do
    }
#ifdef PSEUDO_PACKETS
    if (pseudo0D > 4) {
      pseudo0D = 0;
      readHex[0] = 0x40;
      readHex[1] = 0x00;
      readHex[2] = 0x0D;
      readHex[3] = Compile_Options;
      readHex[4] = (Mqtt_msgSkipNotConnected >> 8) & 0xFF;
      readHex[5] = Mqtt_msgSkipNotConnected & 0xFF;
      readHex[6]  = (mqttPublished >> 24) & 0xFF;
      readHex[7]  = (mqttPublished >> 16) & 0xFF;
      readHex[8]  = (mqttPublished >> 8) & 0xFF;
      readHex[9]  = mqttPublished & 0xFF;
      for (int i = 10; i <= 22; i++) readHex[i]  = 0x00; // reserved for future use
      writePseudoPacket(readHex, 23, readBuffer);
    }
    if (pseudo0E > 4) {
      pseudo0E = 0;
      readHex[0]  = 0x40;
      readHex[1]  = 0x00;
      readHex[2]  = 0x0E;
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
      readHex[17] = (outputMode >> 8) & 0xFF;
      readHex[18] = outputMode & 0xFF;
      readHex[19] = (maxLoopTime >> 24) & 0xFF;
      readHex[20] = (maxLoopTime >> 16) & 0xFF;
      readHex[21] = (maxLoopTime >> 8) & 0xFF;
      readHex[22] = maxLoopTime & 0xFF;
      writePseudoPacket(readHex, 23, readBuffer);
    }
    if (pseudo0F > 4) {
      pseudo0F = 0;
      readHex[0] = 0x40;
      readHex[1] = 0x00;
      readHex[2] = 0x0F;
      readHex[3]  = telnetConnected;
      readHex[4]  = outputFilter;
      uint16_t m1 = ESP.getMaxFreeBlockSize();
      readHex[5]  = (m1 >> 8) & 0xFF;
      readHex[6]  = m1 & 0xFF;
      readHex[7]  = (Mqtt_disconnectTime >> 8) & 0xFF;
      readHex[8]  = Mqtt_disconnectTime & 0xFF;
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
      writePseudoPacket(readHex, 23, readBuffer);
    }
#endif
  }
}
