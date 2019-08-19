/* P1P2-bridge-esp8266json: Host application for esp8266 to interface to P1P2Monitor on Arduino Uno,
 *                          supports mqtt/json/wifi
 *                          Includes wifimanager, OTA
 *                          For protocol description, see SerialProtocol.md and MqttProtocol.md
 *                     
 * Copyright (c) 2019 Arnold Niessen, arnold.niessen -at- gmail-dot-com  - licensed under GPL v2.0 (see LICENSE)
 *
 * Version history
 * 20190817 v0.9.4 Initial version
 *
 * P1P2erial is written and tested for the Arduino Uno and Mega.
 * It may or may not work on other hardware using a 16MHz clok.
 * As it is based on AltSoftSerial, the following pins should then work:
 *
 * Board          Transmit  Receive   PWM Unusable
 * -----          --------  -------   ------------
 * Arduino Uno        9         8         10
 * Arduino Mega      46        48       44, 45
 * Teensy 3.0 & 3.1  21        20         22
 * Teensy 2.0         9        10       (none)
 * Teensy++ 2.0      25         4       26, 27
 * Arduino Leonardo   5        13       (none)
 * Wiring-S           5         6          4
 * Sanguino          13        14         12
 */
 
// to allow larger json messages as mqtt payload, MQTT_MAX_PACKET_SIZE in PubSubClient.h must be increased
// For some reason, the definition below is not read before PubSubClient.h is included below - strange?
// So advise is to change MQTT_MAX_PACKET_SIZE in PubSubClient.h directly
#define MQTT_MAX_PACKET_SIZE 10000

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <PubSubClient.h>

// Include Daikin product-dependent header file for parameter conversion
//
// Note that this program does not use the (avr only) P1P2Serial library
// so we need to include the json header files with a relative reference below
//
static int changeonly = 1;    // whether json parameters are included if unchanged
static int outputunknown = 0; // whether json parameters include parameters for which we don't know function
#include "P1P2_Daikin_ParameterConversion_EHYHB.h"

// WiFimanager uses Serial output for debugging purposes - messages on serial output start with an asterisk
// these messages are routes to the Arduino Uno/P1P2Monitor
// but P1P2Monitor on Arduino/ATMega ignores any serial input starting with an asterisk

void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("* Entered config mode");
  Serial.print("*");  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.print("*");  Serial.println(myWiFiManager->getConfigPortalSSID());
}

char mqttServer[50] = "192.168.Your.Mqttserver"; // Configuration of mqtt parameters (one-time only) via WiFiManager set-up
char mqttPort[10] = "MqttPort";               // No re-configuration supported yet, unless wifi is switched off
int  mqttPortNr = 1234;
char mqttUser[20] = "MqttUser";
char mqttPassword[30] = "MqttPassword";
//emoncms upload via http-post not supported yet:
//const char* emonURL="URL";                  // Fill in YOUR emoncms URL TODO
//const char* emonAPI="APIkey";               // Fill in YOUR emoncms API key

static int jsonterm = 1;
int jsonstringp = 0;
char jsonstring[10000]={};

WiFiClient espClient;
PubSubClient client(espClient);

void setup() {
  Serial.begin(115200);
  
  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;
  //reset settings - for testing
  //wifiManager.resetSettings();
  //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  WiFiManagerParameter MqttServer("mqttserver", "MqttServer", mqttServer, 50);
  WiFiManagerParameter MqttPort("mqttport", "MqttPort", mqttPort, 10);
  WiFiManagerParameter MqttUser("mqttuser", "MqttUser", mqttUser, 20);
  WiFiManagerParameter MqttPassword("mqttpassword", "MqttPassword", mqttPassword, 30);
  wifiManager.addParameter(&MqttServer);
  wifiManager.addParameter(&MqttPort);
  wifiManager.addParameter(&MqttUser);
  wifiManager.addParameter(&MqttPassword);

  wifiManager.setAPCallback(configModeCallback);
  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  if(!wifiManager.autoConnect()) {
    Serial.println("*Failed to connect and hit timeout");
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(1000);
  } 
  //if you get here you have connected to the WiFi
  Serial.println("*Connected :)");
  Serial.println("*Ready");
  Serial.print("*IP address: ");
  Serial.println(WiFi.localIP());
  strcpy(mqttServer, MqttServer.getValue());
  strcpy(mqttPort, MqttPort.getValue());
  strcpy(mqttUser, MqttUser.getValue());
  strcpy(mqttPassword, MqttPassword.getValue());
  Serial.print("*Mqtt Server IP: ");
  Serial.println(mqttServer);
  Serial.print("*Mqtt Port: ");
  sscanf(MqttPort.getValue(),"%i",&mqttPortNr); // note: no error testing done, fall-back to 1234 as portnr as defined above
  Serial.println(mqttPortNr);
  Serial.print("*Mqtt User: ");
  Serial.println(mqttUser);
  Serial.print("*Mqtt Password: ");
  Serial.println(mqttPassword);
  
  // OTA 
  // port defaults to 8266
  // ArduinoOTA.setPort(8266);
  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname("esp8266-P1P2");
  // No authentication by default
  // ArduinoOTA.setPassword("admin");
  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_SPIFFS
      type = "filesystem";
    }
    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    //Serial.println("*Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("*"); Serial.println("*End");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("*Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("*Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("*Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("*Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("*Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("*Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("*End Failed");
    }
  });
  ArduinoOTA.begin();

  // Set up mqtt connection
  client.setServer(mqttServer, mqttPortNr);
  client.setCallback(callback);
  if (client.connect("P1P2", mqttUser, mqttPassword )) {
    Serial.println("*Connected to mqtt server");  
  } else {
    Serial.print("*Mqtt server connection failed with state ");
    Serial.print(client.state());
    delay(2000);
  }
  client.subscribe("P1P2/W");
}

void callback(char* topic, byte* payload, unsigned int length) {
// any message received on P1P2/W will be copied directly to P1P2Monitor via serial out
// unless the message starts with a 'U' or 'Y', in which case we decode the message on the ESP
  int temp;
  char value[98];
  switch (payload[0]) {
    case 'u' :
    case 'U': if (sscanf((const char*) payload + 1, "%d", &temp) == 1) {
                if (temp) temp = 1;
                outputunknown = temp;
                snprintf(value, sizeof(value), "* [ESP] OutputUnknown set to %i", outputunknown);
                client.publish("P1P2/R", value);
              } else {
                snprintf(value, sizeof(value), "* [ESP] OutputUnknown %i", outputunknown);
                client.publish("P1P2/R", value);
              }
              break;
    case 'y' :
    case 'Y': if (sscanf((const char*) (payload + 1), "%d", &temp) == 1) {
                if (temp) temp = 1;
                changeonly = temp;
                snprintf(value, sizeof(value), "* [ESP] ChangeOnly set to %i", changeonly);
                client.publish("P1P2/R", value);
              } else {
                snprintf(value, sizeof(value), "* [ESP] ChangeOnly %i", changeonly);
                client.publish("P1P2/R", value);
              }
              break;
    default : strlcpy(value, (char*) payload, (length + 1) > sizeof(value) ? sizeof(value) : (length + 1)); // create null-terminated copy of payload
              Serial.println((char*) value); 
              break;
  }
}

void process_for_mqtt_json(byte* rb, int n) {
  char value[KEYLEN];    
  char key[KEYLEN + 7]; // 7 character prefix for topic
  int rv;
  strcpy(key,"P1P2/P/");
  for (byte i = 3; i < n; i++) {
    int kvrbyte = bytes2keyvalue(rb, i, 8, key + 7, value);
    // returns 0 if byte does not trigger any output
    // returns 1 if a new value should be output
    // returns 8 if byte should be treated per bit
    // returns 9 if json string should be terminated
    if (kvrbyte == 9) {
      // only terminate json string if there is any parameter written
      if (jsonterm == 0) {
        jsonterm = 1;
        jsonstring[jsonstringp++] = '}';
        jsonstring[jsonstringp++] = '\0';
        client.publish("P1P2/J", jsonstring);
        // TODO HTTPPOST jsonstring to emoncms
        jsonstringp = 0;
      }
    } else {
      for (byte j = 0; j < kvrbyte; j++) {
        int kvr= (kvrbyte==8) ? bytes2keyvalue(rb, i, j, key + 7, value) : kvrbyte;
        if (kvr) {
          client.publish(key, value);
          if (jsonterm) {
            jsonstring[jsonstringp++] = '{';
            jsonterm = 0;
          } else {
            jsonstring[jsonstringp++] = ',';
          }
          jsonstring[jsonstringp++] = '"';

          rv = strlcpy(jsonstring + jsonstringp, key + 7, sizeof(jsonstring) - jsonstringp);
          if (rv >= (sizeof(jsonstring) - jsonstringp)) jsonstringp = sizeof(jsonstring); else jsonstringp += rv;

          jsonstring[jsonstringp++] = '"';
          jsonstring[jsonstringp++] = ':';
          
          rv = strlcpy(jsonstring + jsonstringp, value, sizeof(jsonstring) - jsonstringp);
          if (rv >= (sizeof(jsonstring) - jsonstringp)) jsonstringp = sizeof(jsonstring); else jsonstringp += rv;
        }
      }
    }
  }
  savehistory(rb,n);
}

#define RB 200     // max size of readbuf (serial input from Arduino)
#define HB 32      // max size of hexbuf, same as P1P2Monitor
char readbuf[RB];
byte readhex[HB];
  
void loop() {
  ArduinoOTA.handle();
  client.loop(); 
  if (!client.connected()) {
    Serial.println("*Reconnecting");
    if (client.connect("P1P2", mqttUser, mqttPassword )) {
        Serial.println("*Reconnected");
    } else {
      Serial.println("*Not reconnected");
      delay(1000);
    }
  } else {
    if (byte rb = Serial.readBytesUntil('\n', readbuf, RB)) {
      if (readbuf[rb-1] == '\r') rb--;
      if (rb < RB) readbuf[rb++] = '\0'; else readbuf[RB-1] = '\0';
      client.publish("P1P2/R", readbuf, rb);
      int rbp = 0;
      int n, rbtemp;
      if ((rb >= 10) && (readbuf[7] == ':')) rbp=9;
      byte rh=0;
      // dirty trick: check for space in serial input to avoid that " CRC" in input is recognized as hex value 0x0C...
      while ((rh < HB) && (readbuf[rbp] != ' ') && (sscanf(&readbuf[rbp], "%2x%n", &rbtemp, &n) == 1)) {
        readhex[rh++] = rbtemp;
        rbp += n;
      }
      process_for_mqtt_json(readhex, rh);
    } 
  }
}
