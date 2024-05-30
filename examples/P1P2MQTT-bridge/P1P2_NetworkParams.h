/* P1P2_NetworkParams.h
 *
 * Copyright (c) 2019-2024 Arnold Niessen, arnold.niessen-at-gmail-dot-com - licensed under CC BY-NC-ND 4.0 with exceptions (see LICENSE.md)
 *
 * Version history
 * 20240515 v0.9.46 double-reset WiFi manager AP support
 * 20221112 v0.9.27 static IP support
 * 20220817 v0.9.17 license change
 * 20220808 v0.9.15 extended verbosity command, unique OTA hostname, minor fixes
 * 20220802 v0.9.14 AVRISP, wifimanager, mqtt settings, EEPROM, telnet, outputMode, outputFilter, ...
 * ..
 *
 */

#ifndef P1P2_NetworkParams
#define P1P2_NetworkParams

// AP for WiFiManager configuration during first boot, or if WiFi connection fails (password can be changed via P command)
#ifdef W_SERIES
#define WIFIMAN_SSID1 "P1P2MQTT-W"
#else /* W_SERIES */
#define WIFIMAN_SSID1 "P1P2MQTT"
#endif /* W_SERIES */
#define WIFIMAN_SSID_LEN (32+1)

// AP for WiFiManager configuration after double reset, cannot be changed
#ifdef W_SERIES
#define WIFIMAN_SSID2 "P1P2MQTT-W-2x"
#else /* W_SERIES */
#define WIFIMAN_SSID2 "P1P2MQTT-2x"
#endif /* W_SERIES */

#define WIFIMAN_PASSWORD "P1P2P3P4" // configurable for WIFIMAN_SSID1 via P command, fixed for WIFIMAN_SSID2 (double-reset-button AP)
#define WIFIMAN_PASSWORD_LEN (32+1)

// STATIC IP
#define INIT_USE_STATIC_IP 0            // 0 for DHCP, 1 for static IP
#define INIT_USE_STATIC_IP_STRING "0"   // same, for WiFiManager input field
#define STATIC_IP "192.168.4.11"        // only relevant for static IP use
#define STATIC_GW "192.168.4.1"
#define STATIC_NM "255.255.255.0"
#define STATIC_IP_LEN (15+1)

// TELNET
#define TELNET_MAGICWORD "_" // Telnet magic word to unlock telnet functionality, can be set with P command
#define TELNET_MAGICWORD_LEN (20+1)

// DEVICE / BRIDGE names, used for MDNS, OTA, MQTT client name, and MQTT topics and HA configuration
#ifdef W_SERIES
#define DEVICE_NAME "meter"
#else /* W_SERIES */
#define DEVICE_NAME "P1P2MQTT"
#endif /* W_SERIES */
#define DEVICE_NAME_LEN (8+1)

#define BRIDGE_NAME "bridge0"
#define BRIDGE_NAME_LEN (8+1)

// MQTT
#define MQTT_SERVER "192.168.4.12"       // required
#define MQTT_PORT "1883"                 // string, defaults to 1883 if wrongly formatted
#define MQTT_USER "P1P2"                 // optional, defaults to none if undefined
#define MQTT_PASSWORD "P1P2"             // optional, defaults to none if undefined
#define MQTT_SERVER_LEN (15+1)
#define MQTT_USER_LEN (80+1)
#define MQTT_PASSWORD_LEN (80+1)
#define MQTT_CLIENTNAME DEVICE_NAME "_" BRIDGE_NAME
#define MQTT_CLIENTNAME_LEN ( DEVICE_NAME_LEN + BRIDGE_NAME_LEN )

// MQTT topics for external electricity meter(s) and/or gas
#define MQTT_ELECTRICITY_POWER     "P1P2/P/meter/U/9/Electricity_Power"
#define MQTT_ELECTRICITY_TOTAL     "P1P2/P/meter/U/9/Electricity_Total"
#define MQTT_ELECTRICITY_BUH_POWER "P1P2/P/meter/U/9/Electricity_BUH_Power"
#define MQTT_ELECTRICITY_BUH_TOTAL "P1P2/P/meter/U/9/Electricity_BUH_Total"
#define POWER_TIME_OUT 60   // in s, minimum update frequency external meter input for realtime power consumption
#define ENERGY_TIME_OUT 300 // in s, minimum update frequency external meter input for total energy consumed

#define MQTT_INPUT_TOPIC_LEN (80+1)

// 2nd MQTT server (fallback)

#define MQTT2_SERVER "192.168.4.12"
#define MQTT2_PORT 1883 // TODO nr iso string
#define MQTT2_USER "P1P2"
#define MQTT2_PASSWORD "P1P2"
#define MQTT_DISCONNECT_TRY_FALLBACK 60 // time-out on 1st MQTT server before switching to fall-back server

// OTA
#define OTA_HOSTNAME DEVICE_NAME "_" BRIDGE_NAME
#define OTA_HOSTNAME_LEN ( DEVICE_NAME_LEN + BRIDGE_NAME_LEN )

#define OTA_PASSWORD "P1P2MQTT"
#define OTA_PASSWORD_LEN (16+1)

// MDNS
#define MDNS_NAME DEVICE_NAME "_" BRIDGE_NAME
#define MDNS_NAME_LEN ( DEVICE_NAME_LEN + BRIDGE_NAME_LEN )

// MQTT delete
#define DELETE_STEP 2 // seconds between MQTT delete-steps

// MQTT topics
#define MQTT_PREFIX "P1P2"                      // prefix for all MQTT topics
#define MQTT_PREFIX_LEN (16+1)

#define WILL_TOPIC_LEN (MQTT_PREFIX_LEN + DEVICE_NAME_LEN + BRIDGE_NAME_LEN + 2) // prefix/L/devicename/bridgename

#endif /* P1P2_NetworkParams */
