/* P1P2_NetworkParams.h 
 *
 * Copyright (c) 2019-2022 Arnold Niessen, arnold.niessen-at-gmail-dot-com - licensed under CC BY-NC-ND 4.0 with exceptions (see LICENSE.md)
 *
 * Version history
 * 20221112 v0.9.27 static IP support
 * 20220817 v0.9.17 license change
 * 20220808 v0.9.15 extended verbosity command, unique OTA hostname, minor fixes
 * 20220802 v0.9.14 AVRISP, wifimanager, mqtt settings, EEPROM, telnet, outputMode, outputFilter, ...
 * ..
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
char MQTT_CLIENTNAME[9] = "P1P2_xxx"; // Must be unique, include 4th byte of IPv4 address
#define MQTT_CLIENTNAME_IP 5          // start of xxx for ip nr in MQTT_CLIENTNAME

// OTA
char OTA_HOSTNAME[13] = "P1P2MQTT_xxx";
#define OTA_HOSTNAME_PREFIXIP 9        // start of xxx for ip nr in OTA_HOSTNAME
#define OTA_PASSWORD "P1P2MQTT"

// STATIC IP
#define INIT_USE_STATIC_IP 0           // 0 for DHCP, 1 for static IP
#define INIT_USE_STATIC_IP_STRING "0"  // same, for WiFiManager input field
#define STATIC_IP "192.168.4.11"       // only relevant for static IP use
#define STATIC_GW "192.168.4.1"
#define STATIC_NM "255.255.255.0"

#endif /* P1P2_NetworkParams */
