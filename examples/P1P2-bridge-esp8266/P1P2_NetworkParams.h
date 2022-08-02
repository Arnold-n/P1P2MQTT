/* P1P2_NetworkParams.h 
 *
 * Copyright (c) 2019-2022 Arnold Niessen, arnold.niessen -at- gmail-dot-com  - licensed under GPL v2.0 (see LICENSE)
 *
 * 20220802 v0.9.14 AVRISP, wifimanager, mqtt settings, EEPROM, telnet, outputMode, outputFilter, ...
 *
 * WARNING: P1P2-bridge-esp8266 is end-of-life, and will be replaced by P1P2MQTT
 *
 */

#ifndef P1P2_NetworkParams
#define P1P2_NetworkParams

// AP for WiFiManager configuration during first boot
#define WIFIMAN_SSID "P1P2MQTT"
#define WIFIMAN_PASSWORD "P1P2P3P4"

// MQTT
#define MQTT_SERVER "192.168.4.12"    // required
#define MQTT_PORT 1883                // optional, defaults to 1883 if undefined, no ""
#define MQTT_PORT_STRING "1883"       // optional, defaults to "1883" if undefined
#define MQTT_USER "P1P2"              // optional, defaults to none if undefined
#define MQTT_PASSWORD "P1P2"          // optional, defaults to none if undefined
#define MQTT_SIGNATURE "P1P2sig"      // signature for EEPROM initialization
char MQTT_CLIENTNAME[9] = "P1P2_xxx"; // Must be unique, include 4th byte of IPv4 address
#define MQTT_CLIENTNAME_IP 5          // start of xxx for ip nr in MQTT_CLIENTNAME

// OTA
#define OTA_HOSTNAME "P1P2MQTT"
//#define OTA_PASSWORD "P1P2MQTT"

#endif /* P1P2_NetworkParams */
