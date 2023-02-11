/* P1P2_Config.h
 *
 * Copyright (c) 2019-2023 Arnold Niessen, arnold.niessen-at-gmail-dot-com - licensed under CC BY-NC-ND 4.0 with exceptions (see LICENSE.md)
 *
 * WARNING: P1P2-bridge-esp8266 is end-of-life, and will be replaced by P1P2MQTT
 *
 * Version history
 * 20230211 v0.9.33 0xA3 thermistor read-out F-series
 * 20230117 v0.9.32 centralize pseudopacket handling
 * 20230108 v0.9.31 sensor prefix, +2 valves in HA, fix bit history for 0x30/0x31, +pseudo controlLevel
 * 20221228 v0.9.30 switch from modified ESP_telnet library to ESP_telnet v2.0.0
 * 20221211 v0.9.29 misc fixes, defrost E-series
 * 20221116 v0.9.28 reset-line behaviour, IPv4 EEPROM init
 * 20221112 v0.9.27 static IP support, fix to get Power_* also in HA
 * 20221109 v0.9.26 clarify WiFiManager screen, fix to accept 80-char user/password also in WiFiManager
 * 20221108 v0.9.25 ADC report added to F-series
 * 20221102 v0.9.24 noWiFi option, w5500 reset added, fix switch to verbose=9, misc
 * 20221029 v0.9.23 ISPAVR over BB SPI, ADC, misc
 * 20220918 v0.9.22 degree symbol, hwID, 32-bit outputMode
 * 20220907 v0.9.21 outputMode/outputFilter status survive ESP reboot (EEPROM), added MQTT_INPUT_BINDATA/MQTT_INPUT_HEXDATA (see below), reduced uptime update MQTT traffic
 * 20220904 v0.9.20 added E_SERIES/F_SERIES defines, and F-series VRV reporting in MQTT for 2 models
 * 20220903 v0.9.19 longer MQTT user/password, ESP reboot reason (define REBOOT_REASON) added in reporting
 * 20220829 v0.9.18 state_class added in MQTT discovery enabling visibility in HA energy overview
 * 20220821 v0.9.17-fix1 corrected negative deviation temperature reporting, more temp settings reported
 * 20220817 v0.9.17 config file using COMBIBOARD, errors and scopemode-time info via P1P2/R/#
 * 20220808 v0.9.15 extended verbosity command, unique OTA hostname, minor fixes
 * 20220802 v0.9.14 AVRISP, wifimanager, mqtt settings, EEPROM, telnet, outputMode, outputFilter, ...
 * ..
 *  
 */

#ifndef P1P2_Config
#define P1P2_Config

// User configurable options: board, ethernet, hw_id, and model E_SERIES/F_SERIES

// Define one of these options below
#define P1P2_ESP_INTERFACE_250   // define this for regular operation of P1P2-ESP-Interface (ESP8266 + ATmega328P using 250kBaud)
//#define P1P2_ESP_INTERFACE_115 // define this for debugging P1P2-ESP-Interface, using 115.2kBaud for serial output debugging 
//                               // (in this case DO NOT connect to P1/P2), but power with 3V3 via ESP01-connector *or* with 16V DC power supply on P1/P2
//#define ARDUINO_COMBIBOARD     // define this for Arduino/ESP8266 combi-board, using 250kBaud between ESP8266 and ATmega238P
//#define ESP01S_MQTT            // define this for P1/P2 data input from P1P2/R MQTT topic, using 115.2kBaud for debugging over USB
//#define ESP01S_RX              // define this for P1P2 data input over 250kBaud serial, for debugging over MQTT

// Setting option NOWIFI to 1 (can be changed via 7th parameter of 'B' command) prevents WiFi use
// Only useful if ethernet adapter is installed, to prevent use of WiFi as fall-back when ethernet fails
#define INIT_NOWIFI 0

// Define P1P2-ESP-Interface version, set during EEPROM initialization only (can be changed with 6th parameter of 'B' command)
// To avoid erasing the ESP EEPROM and overwriting the hw-identifier, use "Sketch Only" when flashing over USB
#define INIT_HW_ID 1    // hardware identifier for P1P2-ESP-Interface, valid values are
                        //   0 for Arduino Uno/ESP combi-board, for ESP01_R, ESP01_RX (INIT_HW_ID is redefined below for these boards)
                        //   0 for P1P2-ESP-Interface v1.0, August 2022, ISPAVR over HSPI
                        //   1 for P1P2-ESP-Interface v1.1, October 2022, ISPAVR over BB SPI, ADC
                        //   (Note: if INIT_HW_ID is 1, BSP with modified library for ESP8266AVRISP library is required for updating firmware of ATmega328P)

// define only one of E_SERIES and F_SERIES:
//#define E_SERIES // for Daikin E* heat pumps
//#define F_SERIES // for Daikin F* VRV systems

// Other options below should be OK

// if ethernet adapter is detected, but does not connect (no cable/other issue), time-out in seconds until fall-back to WiFi (if NoWiFi = 0) or until restart (if NoWiFi = 1)
#define ETHERNET_CONNECTION_TIMEOUT 60

#ifdef P1P2_ESP_INTERFACE_250
#define SERIALSPEED 250000
#define SERIAL_MAGICSTRING "1P2P" // Each serial communication line to ATmega must start with SERIAL_MAGICSTRING, otherwise line is ignored
// ETHERNET must be defined if a W5500 adapter is used for ethernet (option for P1P2-ESP-Interface v1.1); otherwise ETHERNET is still optional, but please
// note that if W5500 adapter is absent, a BSP with modified w5500 library is required to avoid a WDT reboot when constructor hangs
// INIT_HW_ID = 0 does not require ethernet or BSP modification
#if INIT_HW_ID > 0
#define ETHERNET
#endif
#endif

#ifdef P1P2_ESP_INTERFACE_115
#define SERIALSPEED 115200 // for debugging, do not connect interface to P1/P2 when doing this
#define SERIAL_MAGICSTRING "[ESP-TX]" // Each serial communication line to ATmega must start with SERIAL_MAGICSTRING, otherwise line is ignored
#define DEBUG_OVER_SERIAL // send debugging output over serial for ESP01S_RX or USB serial interface
// ETHERNET must be defined if a W5500 adapter is used for ethernet; otherwise ETHERNET is optional, but please
// note that if W5500 adapter is absent, a BSP with modified w5500 library is required to avoid a WDT reboot when constructor hangs
#define ETHERNET
#endif

#ifdef ARDUINO_COMBIBOARD
#define SERIALSPEED 250000
#define SERIAL_MAGICSTRING "1P2P" // Each serial communication line to ATmega must start with SERIAL_MAGICSTRING, otherwise line is ignored
// no ethernet, no ESP8266 BSP modification needed
#undef INIT_HW_ID
#define INIT_HW_ID 0
#endif

#ifdef ESP01S_MQTT
#define SERIALSPEED 115200
#define DEBUG_OVER_SERIAL // send debugging output over serial (to be captured over USB)
#define MQTT_INPUT_HEXDATA // use raw data from P1P2/X/xxx instead of serial input (for ESP01S_MQTT)
//#define MQTT_INPUT_BINDATA // use raw data from P1P2/X/xxx instead of serial input (for ESP01S_MQTT)
// if MQTT_INPUT_HEXDATA and/or MQTT_INPUT_BINDATA is defined, the serial input to the ESP (from ATmega) is ignored and data from either or both of
// the defined R/X topics is used for processing (subject to (outputMode & 0x4000) and (outputMode & 0x8000) respectively):
// The 4th IPv4 byte of the topic to subscribe to can be set using 5th parameter in the 'B' command.
// no ethernet, no ESP8266 BSP modification needed
#undef INIT_HW_ID
#define INIT_HW_ID 0
#endif

#ifdef ESP01S_RX
#define SERIALSPEED 250000
// no ethernet, no ESP8266 BSP modification needed
#undef INIT_HW_ID
#define INIT_HW_ID 0
#endif

#define SAVEPARAMS
#define SAVEPACKETS
// to save memory to avoid ESP instability (until P1P2MQTT is released): do not #define SAVESCHEDULE // format of schedules will change to JSON format in P1P2MQTT

#define WELCOMESTRING "* [ESP] P1P2-bridge-esp8266 v0.9.33"
#define WELCOMESTRING_TELNET "P1P2-bridge-esp8266 v0.9.33"
#define HA_SW "0.9.33"

#define AVRISP // enables flashing ATmega by ESP on P1P2-ESP-Interface
#define SPI_SPEED_0 2e5 // for HSPI, default avrprog speed is 3e5, which is too high to be reliable; 2e5 works
#define SPI_SPEED_1   0 // for BB-SPI

//#define REBOOT_REASON // stores reboot reason in flash upon 'D0', 'D1' command, MQTT reconnect failure, nowifi change, ethernet failure, or wifimanager failure
                      // only use this on stable systems or if searching for reboot cause to avoid unnecessary flash writes

#define TELNET // define to allow telnet access (no password protection!) for monitoring and control
               // the telnet library provides no authentication for telnet
               // Unfortunately telnet output is synchronized, which may trigger some issues only when telnet is being used
               // undefine on open networks or if you experience problems

#define PSEUDO_PACKETS // define to have P1P2-bridge-esp8266 output additional P1P2-like packets with internal state information

// home assistant (including MQTT discovery)
#define HA_PREFIX "homeassistant/sensor"   // homeassistant MQTT discovery prefix
char haDeviceName[9] = "P1P2-xxx";         // becomes device name in HA
#define HA_DEVICE_NAME_PREFIX 5
char haDeviceID[10] = "P1P2IDxxx";         // uniq device_id. Currently all sensors in one device_ID
#define HA_DEVICE_ID_PREFIX 6
#define HA_DEVICE_MODEL "P1P2MQTT_bridge"  // shows up as Device Info in HA
#define HA_MF "NPC"
#define HA_KEY_LEN 100
#define HA_VALUE_LEN 600
char haPostfix[5] = "_xxx";
#define HA_POSTFIX_PREFIX 1
#define HA_SENSOR_PREFIX "P1P2_"
#define INIT_USE_SENSOR_PREFIX_HA 0

// MQTT topics
#define MQTT_KEY_PREFIXIP   7
char mqttHexdata[11]     = "P1P2/R/xxx";
char mqttBindata[11]     = "P1P2/X/xxx";
char mqttSignal[11]      = "P1P2/S/xxx";
char mqttCommands[11]    = "P1P2/W/xxx";
char mqttCommandsNoIP[7] = "P1P2/W";
char mqttJsondata[11]    = "P1P2/J/xxx";
char mqttKeyPrefix[16]   = "P1P2/P/xxx/M/0/";
#ifdef MQTT_INPUT_HEXDATA
char mqttInputHexData[11]= "P1P2/R";  // default accepts input from any P1P2/R/#; can be changed to P1P2/R/xxx via 'B' command
#endif
#ifdef MQTT_INPUT_BINDATA
char mqttInputBinData[11]= "P1P2/X";  // default accepts input from any P1P2/X/#; can be changed to P1P2/X/xxx via 'B' command
#endif
#define MQTT_KEY_PREFIXCAT 11 // location in prefix for category identification
#define MQTT_KEY_PREFIXSRC 13 // location in prefix for source identification
#define MQTT_KEY_PREFIXLEN 15 // length of mqttKeyPrefix (excl '\0')

// To avoid Mqtt/CPU overload and message loss, throttle mqt traffic after reboot
#define THROTTLE_VALUE 5 // start with 20% parameter coverage, increasing in (THROTTLE_VALUE-1)*(THROTTLE_STEP) seconds to 100%
#define THROTTLE_STEP 4  // in seconds

// #define MQTT_PUBLISH_NONHA 1 // this is now online configuration option as (outputMode & 0x10000)

#define MQTT_DISCONNECT_CONTINUE 0 // 0 pauses processing packets if mqtt is disconnected (to avoid that changes are lost)
                                   // Set to 1 to continue (in case you have no mqtt of want to see changes via telnet or so)
#define MQTT_DISCONNECT_RESTART 150 // Restart ESP if Mqtt disconnect time larger than this value in seconds (because after WiFi interruption, Mqtt may not reconnect reliably)

#define MQTT_RETAIN true // all messages (ha and parameter values) retained
//#define MQTT_RETAIN false // no messages (ha and parameter values) retained

#define INIT_OUTPUTFILTER 1    // outputfilter determines which parameters to report, can be changed run-time using 'S'/'s' command
                               // 0 all
                               // 1 only changed parameters
                               // 2 only changed parameters except measurements (temperature, flow)
                               // 3 only changed parameters except measurements (temperature, flow) and except date/time
                               // first-time parameters are always reported

#ifdef ESP01S_MQTT
#define INIT_OUTPUTMODE 0x1C003
#else
#define INIT_OUTPUTMODE 0x13003 // outputmode determines output content and channels, can be changed run-time using 'J'/'j' command
#endif
                               // outputmode is sum of:
                               // 0x0001 to output raw packet data (including pseudo-packets) over mqtt P1P2/R/xxx
                               // 0x0002 to output mqtt individual parameter data over mqtt P1P2/P/xxx/#
                               // 0x0004 to output json data over mqtt P1P2/J/xxx
                               // 0x0008 to have mqtt/json include parameters even if functionality is unknown, warning: easily overloads ATmega/ESP (best to combine this with outputfilter >=1)
                               // 0x0010 ESP to output raw data over telnet
                               // 0x0020 to output mqtt individual parameter data over telnet
                               // 0x0040 to output json data over telnet
                               // 0x0080 (reserved for time string in R output)
                               // 0x0100 ESP to output raw data over serial
                               // 0x0200 to output mqtt individual parameter data over serial
                               // 0x0400 to output json data over serial
                               // 0x0800 to output raw bin data over P1P2/X/xxx
                               // 0x1000 to output scope-mode output also over P1P2/R/xxx (prefix: C)
                               // 0x2000 to output error data also over P1P2/R/xxx (prefix: *)
                               // 0x4000 to use P1P2/R/xxx as input (requires MQTT_INPUT_HEXDATA)
                               // 0x8000 to use P1P2/X/xxx as input (requires MQTT_INPUT_BINDATA)
                               // 0x10000 to include non-HACONFIG parameters in P1P2/P/#

// no need to change these:
#define RESET_PIN 5 // GPIO_5 on ESP-12F pin 20 connected to ATmega328P's reset line
#define RX_BUFFER_SIZE 2048 // to avoid serial buffer overruns (512 is too small)
#define MQTT_MIN_FREE_MEMORY 6000 // Must likely be more than 4kB, MQTT messages will not be transmitted if available memory is below this value
#define MQTT_QOS 0 // QOS = 1 is too slow
#define SERIAL_MAGICSTRING "1P2P" // Serial input of ATmega should start with SERIAL_MAGICSTRING, otherwise lines line is ignored by P1P2Monitor
#define CRC_GEN 0xD9    // Default generator/Feed for CRC check; these values work at least for the Daikin hybrid
#define CRC_FEED 0x00   // Define CRC_GEN to 0x00 means no CRC is checked when reading or added when writing
#define SPRINT_VALUE_LEN 800 // max message length for informational and debugging output over P1P2/S, telnet, or serial
#define MQTT_KEY_LEN 100
#ifdef SAVESCHEDULE
#define MQTT_VALUE_LEN 630 // max length: Program 6* (6+8)*6*7+3*7+2 = 611
#else
#define MQTT_VALUE_LEN 400
#endif
#define MAX_COMMAND_LENGTH 252 // B command can be long
#define RB 1000     // max size of readBuffer (serial input from Arduino) (was 400, changed for long-scope-mode to 1000)
#define HB 33      // max size of hexbuf, same as P1P2Monitor (model-dependent? 24 might be sufficient)
#define MQTT_RB 1024 // size of ring buffer for MQTT_INPUT_HEXDATA/MQTT_INPUT_BINDATA

#define REBOOT_REASON_NOTSTORED 0xFE
#define REBOOT_REASON_NOTSUPPORTED 0xFF
#define REBOOT_REASON_UNKNOWN 0x00          // could be power-down or crash
#define REBOOT_REASON_WIFIMAN 0x01          // wifiManager time-out
#define REBOOT_REASON_MQTT 0x02             // MQTT reconnect time-out
#define REBOOT_REASON_OTA 0x03              // OTA restart
#define REBOOT_REASON_BCMD 0x04             // hwID/noWiFi/useSensorPrefixHA change restart
#define REBOOT_REASON_ETH 0x06              // ethernet failure restart (only if noWiFi)
#define REBOOT_REASON_STATICIP 0x06         // staticIP setting restart
#define REBOOT_REASON_D0 0xD0               // manual restart
#define REBOOT_REASON_D1 0xD1               // manual reset

#define EEPROM_SIGNATURE_OLD1 "P1P2sig" // old signature for EEPROM initialization (user/password length 19/39)
#define EEPROM_SIGNATURE_OLD2 "P1P2sih" // old signature for EEPROM initialization (user/password length 80/80, rebootReason)
#define EEPROM_SIGNATURE_OLD3 "P1P2sii" // old signature for EEPROM initialization (user/password length 80/80, rebootReason, outputMode, outputFilter, mqttInputByte4, reserved-data)
#define EEPROM_SIGNATURE_NEW  "P1P2sij" // new signature for EEPROM initialization (user/password length 80/80, rebootReason, outputMode, outputFilter, mqttInputByte4, hwId, EEPROM_version, reserved-data)

#endif /* P1P2_Config */
