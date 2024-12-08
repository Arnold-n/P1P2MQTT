/* P1P2_Config.h
 *
 * Copyright (c) 2019-2024 Arnold Niessen, arnold.niessen-at-gmail-dot-com - licensed under CC BY-NC-ND 4.0 with exceptions (see LICENSE.md)
 *
 * Version history
 * 20240605 v0.9.53 V_Interface entity is voltage
 * 20240519 v0.9.51 onMqtt reception buffer size reduced
 * 20240519 v0.9.50 make factory reset and production/consumption counter update only available if haSetup is active
 * 20240519 v0.9.49 fix haConfigMsg max length
 * 20240518 v0.9.47 fix bcnt code
 * 20240515 v0.9.46 HA control, improve TZ, remove bindata and json output format, rewrote mqtt/telnet/buffering, remove MQTT_INPUT_HEXDATA
 * 20230806 v0.9.41 restart after MQTT reconnect, Eseries water pressure, Fseries name fix, web server for ESP update
 * 20230702 v0.9.40 add NTP-based time stamps, add H-link2 decoding
 * 20230611 v0.9.38 H-link2 branch merge into main
 * 20230604 v0.9.37 support for P1P2MQTT bridge v1.2
 * 20230526 v0.9.36 remove several pseudo parameters from HA, unless j-mask includes 0x20000, and adds thresholds to reduce traffic
 * 20230422 v0.9.35 (version skipped)
 * 20230322 v0.9.34 AP timeout
 * 20230211 v0.9.33a 0xA3 thermistor read-out F-series
 * 20230412 v0.9.33 MHI data interpretation added
 * 20230117 v0.9.32 centralize pseudopacket handling, MHI data analysis
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
 */

#ifndef P1P2_Config
#define P1P2_Config

// User configurable options: board, ethernet, hw_id, and model *_SERIES

// Define one of these options below
//#define P1P2MQTT_BRIDGE    // define this for regular operation of P1P2MQTT bridge (ESP8266 + ATmega328 using 250kBaud)
//#define ARDUINO_COMBIBOARD // define this for Arduino/ESP8266 combi-board, using 250kBaud between ESP8266 and ATmega328P (no longer supported by library)

// Setting option NOWIFI to 1 (can be changed via 7th parameter of 'B' command) prevents WiFi use
// Only useful if ethernet adapter is installed, to prevent use of WiFi as fall-back when ethernet fails
#define INIT_NOWIFI 0

// Define timeout for AP, to restart ESP after power outage when AP starts if WiFi is not immediately available
#define WIFIPORTAL_TIMEOUT 180

#define MY_NTP_SERVER "europe.pool.ntp.org"

// Define P1P2-ESP-Interface version, set during EEPROM initialization only (can be changed with 6th parameter of 'B' command)
// To avoid erasing the ESP EEPROM and overwriting the hw-identifier, use "Sketch Only" when flashing over USB
#define INIT_ESP_HW_ID 1 // hardware identifier for ESP side of P1P2-ESP-Interface, valid values are
                         //   0 for Arduino Uno/ESP combi-board, for ESP01_R, ESP01_RX (INIT_ESP_HW_ID is redefined below for these boards) (no longer supported by library)
                         //   0 for P1P2-ESP-Interface v1.0, August 2022, ISPAVR over HSPI
                         //   1 for P1P2-ESP-Interface v1.1, October 2022, ISPAVR over BB SPI, ADC (+ not used: serial suppression signal)
                         //   1 for P1P2-ESP-Interface v1.2, October 2022, ISPAVR over BB SPI, ADC, serial suppression signal
                         //   (Note: if INIT_ESP_HW_ID is 1, BSP with modified ESP8266AVRISP library required for updating firmware of ATmega328P)

// define only one of *_SERIES:
//#define E_SERIES // for Daikin E* heat pumps
//#define F_SERIES // for Daikin F* VRV systems
//#define H_SERIES // for Hitachi H-LINK2 systems
//#define M_SERIES // for Mitsubishi systems (not yet supported)
//#define MHI_SERIES // for Mitsubishi Heavy Industries systems
//#define S_SERIES // for Sanyo systems (not yet supported)
//#define T_SERIES // for Toshiba TCC-link systems
//#define W_SERIES // for homewizard2mqtt systems

#if (defined E_SERIES || defined F_SERIES || defined W_SERIES)
#define EF_SERIES
#endif /* (defined E_SERIES || defined F_SERIES || W_SERIES) */
#if (defined T_SERIES || defined H_SERIES)
#define TH_SERIES
#endif /* (defined T_SERIES || defined H_SERIES) */

// Other options below should be OK

// if ethernet adapter is detected, but does not connect (no cable/other issue), time-out in seconds until fall-back to WiFi (if NoWiFi = 0) or until restart (if NoWiFi = 1)
#define ETHERNET_CONNECTION_TIMEOUT 60

#ifdef P1P2MQTT_BRIDGE
#define SERIAL_MAGICSTRING "1P2P" // Each serial communication line to ATmega must start with SERIAL_MAGICSTRING, otherwise line is ignored
// ETHERNET must be defined if a W5500 adapter is used for ethernet (option for P1P2-ESP-Interface v1.1); otherwise ETHERNET is still optional, but please
// note that if W5500 adapter is absent, a BSP with modified w5500 library is required to avoid a WDT reboot when constructor hangs
// INIT_ESP_HW_ID = 0 does not require ethernet or BSP modification
#if INIT_ESP_HW_ID > 0
#define ETHERNET
#endif /* INIT_ESP_HW_ID */
#endif /* P1P2_ESP_INTERFACE_250 */

#ifdef ARDUINO_COMBIBOARD
#define SERIAL_MAGICSTRING "1P2P" // Each serial communication line to ATmega must start with SERIAL_MAGICSTRING, otherwise line is ignored
// no ethernet, no ESP8266 BSP modification needed
#undef INIT_ESP_HW_ID
#define INIT_ESP_HW_ID 0
#endif

#define SAVEPARAMS
#define SAVEPACKETS
// to save memory to avoid ESP instability (until P1P2MQTT is released): do not #define SAVESCHEDULE // format of schedules will change to JSON format in P1P2MQTT

#define WELCOMESTRING "P1P2MQTT bridge v0.9.58rc4"
#define HA_SW "0.9.58rc4"
#define SW_PATCH_VERSION 58
#define SW_MINOR_VERSION 9
#define SW_MAJOR_VERSION 0

#define ARDUINO_OTA
#define WEBSERVER // adds webserver to update firmware of ESP
#define AVRISP // enables flashing ATmega by ESP on P1P2-ESP-Interface
#define SPI_SPEED_0 2e5 // for HSPI, default avrprog speed is 3e5, which is too high to be reliable; 2e5 works
#define SPI_SPEED_1   0 // for BB-SPI

//#define DEBUG_OVER_SERIAL
#ifdef DEBUG_OVER_SERIAL
#define SERIALSPEED 115200 // for debugging only
#endif

#ifndef SERIALSPEED
#define SERIALSPEED 250000
#endif

#define TELNET // define to allow telnet access (no password protection!) for monitoring and control
               // the telnet library provides no authentication for telnet
               // Unfortunately telnet output is synchronized, which may trigger some issues only when telnet is being used
               // undefine on open networks or if you experience problems

// To avoid Mqtt/CPU overload and message loss, throttle mqt traffic after reboot
#if defined S_SERIES || defined MHI_SERIES
#define THROTTLE_STEP_S 2  // in seconds (higher=slower)
#define THROTTLE_STEP_P 1 // 10  // in percentage (higher=faster)
#define THROTTLE_VALUE 0 // start with 100% skipping, linearly decreasing 1% each THROTTLE_STEP seconds down to 0% skipping, must be multiple of THROTTLE_STEP_P. 0 = no throttling.
#elif defined H_SERIES
#define THROTTLE_STEP_S 1  // in seconds (higher=slower)
#define THROTTLE_STEP_P 10  // in percentage (higher=faster)
#define THROTTLE_VALUE 100 // start with 100% skipping, linearly decreasing 1% each THROTTLE_STEP seconds down to 0% skipping, must be multiple of THROTTLE_STEP_P
#elif defined F1F2_SERIES
#define THROTTLE_STEP_S 1  // in seconds (higher=slower)
#define THROTTLE_STEP_P 50  // in percentage (higher=faster)
#define THROTTLE_VALUE 100 // start with 100% skipping, linearly decreasing 1% each THROTTLE_STEP seconds down to 0% skipping, must be multiple of THROTTLE_STEP_P
#elif defined F_SERIES
#define THROTTLE_STEP_S 1  // in seconds (higher=slower)
#define THROTTLE_STEP_P 1  // in percentage (higher=faster)
#define THROTTLE_VALUE 100 // start with 100% skipping, linearly decreasing 1% each THROTTLE_STEP seconds down to 0% skipping, must be multiple of THROTTLE_STEP_P
#else
#define THROTTLE_STEP_S 1  // in seconds (higher=slower)
#define THROTTLE_STEP_P 1 // in percentage (higher=faster)
#define THROTTLE_VALUE 100 // start with 100% skipping, linearly decreasing 1% each THROTTLE_STEP seconds down to 0% skipping, must be multiple of THROTTLE_STEP_P
#endif /* H_SERIES */

#define MQTT_DISCONNECT_CONTINUE 0 // 0 pauses processing packets if mqtt is disconnected (to avoid that changes are lost)
                                   // Set to 1 to continue (in case you have no mqtt of want to see changes via telnet or so)
#define MQTT_DISCONNECT_RESTART 150 // Restart ESP if Mqtt disconnect time larger than this value in seconds (because after WiFi interruption, Mqtt may not reconnect reliably)

#define MQTT_RETAIN_DATA true        // retain parameter value messages
#define MQTT_RETAIN_WILL true   // retain birth and will messages
#define MQTT_RETAIN_CONFIG true // retain HA config messages
#define MQTT_RETAIN_DELETE true
#define MQTT_RETAIN_SIGNAL false // do not retain informational messages
#define MQTT_RETAIN_HEX    false // do not retain hex data messages

#define INIT_OUTPUTFILTER 1    // outputfilter determines which parameters to report, can be changed run-time using 'S'/'s' command
                               // 0 all
                               // 1 only changed parameters
                               // 2 only changed parameters except measurements (temperature, flow)
                               // 3 only changed parameters except measurements (temperature, flow) and except date/time
                               // first-time parameters are always reported

#define INIT_OUTPUTMODE 0x0002 // outputmode determines output content and channels, can be changed run-time using 'J'/'j' command, 0x0002=MQTT only, 0x0003=MQTT+raw-hex
#ifdef MHI_SERIES
#define INIT_OUTPUTMODE 0x1303B // normally 13003, but 1303B for initial MHI analysis // outputmode determines output content and channels, can be changed run-time using 'J'/'j' command
#endif /* MHI_SERIES */

// no need to change these:
#define RESET_PIN 5 // GPIO_5 on ESP-12F pin 20 connected to ATmega328P's reset line
#define RX_BUFFER_SIZE 4096 // to avoid serial buffer overruns (512 is too small)
#define MQTT_MIN_FREE_MEMORY 6000 // Must be more than 4kB, MQTT messages will not be transmitted if available memory is below this value
#define MQTT_QOS_HEX 0 // QOS = 1 is not needed for hex data messages
#define MQTT_QOS_DATA 0 // QOS = 1 is too slow for regular data messages, only use for certain messages related to HA controls
#define MQTT_QOS_SIGNAL 0 // QOS = 1 is not needed for textual messages
#define MQTT_QOS_WILL 1
#define MQTT_QOS_CONFIG 0
#define MQTT_QOS_DELETE MQTT_QOS_CONFIG
#define MQTT_QOS_CONTROL 1
#define MQTT_QOS_EMETER 0
#define MQTT_QOS_HEX_IN 0
#ifndef SERIAL_MAGICSTRING
#define SERIAL_MAGICSTRING "1P2P" // Serial input of ATmega should start with SERIAL_MAGICSTRING, otherwise lines line is ignored by P1P2Monitor
#endif /* SERIAL_MAGICSTRING */

#ifdef TH_SERIES
#define CRC_GEN 0x00    // No CRC check for Toshiba/Hitachi
#endif /* TH_SERIES */
#if defined EF_SERIES
#define CRC_GEN 0xD9    // Default generator/Feed for CRC check; these values work at least for the Daikin hybrid
#endif /* EF_SERIES || W_SERIES */
#ifdef F1F2_SERIES
#define CRC_GEN 0
#endif /* F1F2_SERIES */
#ifdef MHI_SERIES
#define CS_GEN 1  // Summation checksum on/off
#endif /* MHI_SERIES */
#define CRC_FEED 0x00   // Define CRC_GEN to 0x00 means no CRC is checked when reading or added when writing
#define SPRINT_VALUE_LEN 1000 // max message length for informational and debugging output over P1P2/S, telnet, or serial
#define MQTT_KEY_LEN 100
#define MQTT_VALUE_LEN 1000
#define RB 1000     // max size of readBuffer (serial input from Arduino) (was 400, changed for long-scope-mode to 1000)
#ifdef F1F2_SERIES
#define HB 80
#elif not (defined TH_SERIES || defined MHI_SERIES)
#define HB 24      // max size of hexbuf, same as P1P2Monitor (model-dependent? 24 might be sufficient)
#else /* MHI_SERIES || TH_SERIES */
#define HB 65      // max size of hexbuf, same as P1P2Monitor (model-dependent? 24 might be sufficient)
#endif /* MHI_SERIES || TH_SERIES */
#define MQTT_BUFFER_SIZE 2048 // size of ring buffer for MQTT/telnet input handling
#define MQTT_BUFFER_SPARE 256  // keep a part of buffer reserved for MQTT topic W and telnet input and clean-up
#define MQTT_BUFFER_SPARE2 128 // during clean-up, keep a part of buffer reserved for MQTT topic W and telnet input
#define MQTT_PAYLOAD_LEN 1024 // max length of MQTT message that can be received; should be at least 2*MAX_SAVE_BLOCK_SIZE
#define MQTT_SAVE_BLOCK_SIZE 512 // length of P1P2/M/# save-data messages, should not be more than (MQTT_PAYLOAD_LEN >> 1)
#define MQTT_CMDBUFFER_MINFREE 200 // incoming R messages should respect min buffer for commands

#define REBOOT_REASON_UNKNOWN 0x00      // reset button / power-up / crash
#define REBOOT_REASON_WIFIMAN1 0x01     // wifiManager time-out (WiFiManager in case of double reset)
#define REBOOT_REASON_WIFIMAN2 0x02     // wifiManager time-out (wifiManager if WiFi connect fails)
#define REBOOT_REASON_MQTT_FAILURE 0x03 // MQTT reconnect time-out
#define REBOOT_REASON_OTA 0x04          // OTA restart
#define REBOOT_REASON_ETH 0x05          // ethernet failure restart (only if noWiFi)
#define REBOOT_REASON_STATICIP 0x06     // staticIP setting restart
#define REBOOT_REASON_D0 0x07           // manual restart
#define REBOOT_REASON_D1 0x08           // manual reset

// EEPROM signature
#define EEPROM_SIGNATURE_LEN (9+1)

#define EEPROM_SIGNATURE_COMMON "P1P2" // to maintain mqtt server details
#define EEPROM_SIGNATURE_OLD4 "P1P2sij"
#ifdef E_SERIES
#define EEPROM_SIGNATURE_NEW  "P1P2sEz" // new signature for EEPROM initialization (user/password length 80/80, rebootReason, outputMode, outputFilter, mqttInputByte4, hwId, EEPROM_version, reserved-data)
#elif defined F1F2_SERIES
#define EEPROM_SIGNATURE_NEW  "P1P2sF2"
#elif defined F_SERIES
#define EEPROM_SIGNATURE_NEW  "P1P2sFz"
#elif defined M_SERIES
#define EEPROM_SIGNATURE_NEW  "P1P2sMz"
#elif defined H_SERIES
#define EEPROM_SIGNATURE_NEW  "P1P2sHz"
#elif defined MHI_SERIES
#define EEPROM_SIGNATURE_NEW  "P1P2sMz"
#elif defined S_SERIES
#define EEPROM_SIGNATURE_NEW  "P1P2sSz"
#elif defined T_SERIES
#define EEPROM_SIGNATURE_NEW  "P1P2sTz"
#elif defined W_SERIES
#define EEPROM_SIGNATURE_NEW  "P1P2sWz"
#else
#error
#endif

#define BLUELED_PIN 2      // pin 17 GPIO 2

#endif /* P1P2_Config */
