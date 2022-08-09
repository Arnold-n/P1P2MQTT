/* P1P2Config.h
 *
 * Copyright (c) 2019-2022 Arnold Niessen, arnold.niessen -at- gmail-dot-com  - licensed under GPL v2.0 (see LICENSE)
 *
 * 20220808 v0.9.15 Added compile date/time to verbosity report
 * 20220802 v0.9.14 New parameter-write method 'e' (param write packet type 35-3D), error and write throttling, verbosity mode 2, EEPROM state, MCUSR reporting,
 *                  pseudo-packets, error counters, ...
 *                  Simplified code by removing packet interpretation, json, MQTT, UDP (all off-loaded to P1P2MQTT)
 *
 */

// json/MQTT support has been removed, as it is better to do so on a separate CPU
// Code/data size on an ATmega328P:

                         // prog-size data-size    Function
                         //      kB        kB
                         //    16.3       0.9      Basic functionality
#define MONITORCONTROL   //     2.6       0        enables P1P2 bus writing (as auxiliary controller and/or for requesting counters)
#define EEPROM_SUPPORT   //     0.5       0        adds EEPROM support to store verbose, counterrepeatingrequest, and CONTROL_ID
#define PSEUDO_PACKETS   //     0.9       0        adds pseudopacket to serial output with ATmega status info for P1P2-bridge-esp8266

                         // ------------------
                         //    20.3       0.9      ATmega328P/Arduino Uno

// Define serial speed
// Use 115200 for Arduino Uno/Mega2560 (serial over USB)
// Use 250000 for P1P2-ESP-interface, for direct serial connection between ATmega and ESP8266

//#define SERIALSPEED 115200
#define SERIALSPEED 250000

#define WELCOMESTRING "* P1P2Monitor-v0.9.15"

#define INIT_VERBOSE 3
// Set verbosity level
// verbose = 0: very limited reporting, raw data only (no R prefix); other data starts with *
// verbose = 1: interactive behavior, maximal reporting (*/R prefix)
// verbose = 2: status reports via pseudo packet, limited reporting, used in P1P2-ESP-interface (*/R prefix) for P1P2-bridge-esp8266/P1P2MQTT (default)
// verbose = 3: as verbose 2, but with timing info added (also works with P1P2-bridge-esp8266/P1P2MQTT) for real packets
//                   (format: 10-character "T 65.535: " for real packets and "P         " for pseudopackets)
// verbose = 4: no raw/pseudopacket data output, only maximal reporting

#define COUNTERREPEATINGREQUEST 0 // Change this to 1 to trigger a counter request cycle at the start of each minute
                                  //   By default this works only as, and only if there is no other, first auxiliary controller (F0)
				  //   The counter request is done after the (unanswered) 00F030*, unless KLICDA is defined
				  //   Requesting counters can also be done or changed manually using the 'C' command
				  //
//#define KLICDA                    // If KLICDA is defined,
                                  //   the counter request (if defined) is not done in the F0 pause but is done after the 400012* response
                                  //   This works for systems were the usual pause between 400012* and 000013* is around 47ms (+/- 20ms timer resolution)
				  //   So this may only work without bus collission on these systems, use with care!
#define KLICDA_DELAY 9            // If KLICDA is defined, the counter request is inserted at KLICDA_delay ms after the 400012* response
                                  //   This value needs to be selected carefully to avoid bus collission between the 4000B8* response and the next 000013* request
				  //   A value around 9ms works for my system; use with care, and check your logs for bus collissions or other errors

#define SERIAL_MAGICSTRING "1P2P" // Serial input line should start with SERIAL_MAGICSTRING, otherwise input line is ignored

// set CTRL adapter ID; if not used, installer mode becomes unavailable on main controller
#define CTRL_ID_1 0xB4 // LAN adapter ID in 0x31 payload byte 7
#define CTRL_ID_2 0x10 // LAN adapter ID in 0x31 payload byte 8

// Controller ID values
#define CONTROL_ID_NONE 0x00     // do not act as auxiliary controller
#define CONTROL_ID_0    0xF0     // first auxiliary controller address
#define CONTROL_ID_1    0xF1     // second auxiliary controller address
#define CONTROL_ID_DEFAULT CONTROL_ID_NONE   // by default, P1P2Monitor passively monitors.
                                             // If CONTROL_ID_DEFAULT is set to CONTROL_ID_0 (or _1) P1P2Monitor
                                             // will start to act as an auxiliary controller if no other controller is detected.

//#define REBOOT_RETRIES_WRITING // when too many errors are observed by P1P2Monitor (read errors in monitor mode and/or errors detected while writing),
                               // P1P2Monitor's control and counterrequesting functions are switched off
                               // if REBOOT_RETRIES_WRITING is defined, control and/or counterrequesting are switched back on (if they were on) after a reboot
                               // if REBOOT_RETRIES_WRITING is not defined, control and/or counterrequesting are not switched back on after a reboot (safest option,
                               //               but requires manual intervention to switch functionality on)

// EEPROM saves state of
// -CONTROL_ID (whether P1P2Monitor acts as auxiliary controller, and which one)
// -counterrepeatingrequest (whether P1P2Monitor repeatedly requests counters)
// -verbose (verbosity level)
//
// to reset EEPROM to settings in P1P2Config.h, either erase EEPROM, or change EEPROM_SIGNATURE in P1P2Config.h

#define EEPROM_SIGNATURE "P1P2SIG01" // change this every time you wish to re-init EEPROM settings to the settings in P1P2Config.h
#define EEPROM_ADDRESS_CONTROL_ID      0x00 // 1 byte for CONTROL_ID
#define EEPROM_ADDRESS_COUNTER_STATUS  0x01 // 1 byte for counterrepeatingreques
#define EEPROM_ADDRESS_VERBOSITY       0x02 // 1 byte for verbose
                                            // 0x03 .. 0x0F reserved
#define EEPROM_ADDRESS_SIGNATURE       0x10 // should be highest, in view of unspecified strlen(EEPROM_SIGNATURE)

// Write budget: thottle parameter writes to limit flash memory wear
#define TIME_WRITE_PERMISSION 3600 // on avg max one write per 3600s allowed
#define MAX_WRITE_PERMISSION 100   // budget never higher than 100 (don't allow to burn more than 100 writes at once) // 8-bit, so max 255
#define INIT_WRITE_PERMISSION 10   // initial write budget upon boot

// Error budget: P1P2Monitor should not see any errors except upon start falling into a packet
// so if P1P2Monitor sees see too many errors, it stops writing
//
#define TIME_ERRORS_PERMITTED 3600 // on avg max one error per 3600s allowed (otherwise writing will be switched off)
#define MAX_ERRORS_PERMITTED  20   // budget never higher than this (don't allow too many errors before we switch writing off) // 8-bit, so max 255
#define INIT_ERRORS_PERMITTED 10   // initial error budget upon boot
#define MIN_ERRORS_PERMITTED   5   // don't start control unless error budget is at least this value

// serial read buffer size for reading from serial port, max line length on serial input is 99 (3 characters per byte, plus 'W" and '\r\n')
#define RS_SIZE 99
// P1/P2 write buffer size for writing to P1P2bus, max packet size is 32 (have not seen anytyhing over 24 (23+CRC))
#define WB_SIZE 32
// P1/P2 read buffer size to store raw data and error codes read from P1P2bus; 1 extra for reading back CRC byte; 24 might be enough
#define RB_SIZE 33

#define WR_CNT 1            // number of write repetitions for writing a paramter. 1 should work reliably, no real need for higher value

#define INIT_ECHO 1         // defines whether written data is read back and verified against written data (advise to keep this 1)

#define INIT_SD 50        // (uint16_t) delay setting in ms for each manually instructed packet write
#define INIT_SDTO 2500    // (uint16_t) time-out delay in ms (applies both to manual instructed writes and controller writes)

// CRC settings, values can be changed via serial port
#define CRC_GEN 0xD9    // Default generator/Feed for CRC check; these values work at least for the Daikin hybrid
#define CRC_FEED 0x00   // Define CRC_GEN to 0x00 means no CRC is checked when reading or added when writing

// auxiliary controller timings
#define F030DELAY 100   // Time delay for in ms auxiliary controller simulation, should be larger than any response of other auxiliary controllers (which is typically 25-80 ms)
#define F03XDELAY  30   // Time delay for in ms auxiliary controller simulation, should preferably be a bit larger than any regular response from auxiliary controllers (which is typically 25 ms)
#define F0THRESHOLD 5   // Number of 00Fx30 messages to remain unanswered before we feel safe to act as auxiliary controller

#define SPRINT_VALUE_LEN 800 // for informational and debugging output over P1P2/S TODO

#define REVERSE_ENGINEER // adds extra info to mqtt_key fields TODO

// New parameter writing mode 'E' is generic write method for all parameters in all packet types 0x35-0x3D
//
// Auxiliary controller settings (see more writable parameters documented in Writable_Parameters.md and/or
// https://github.com/Arnold-n/Daikin-P1P2---UDP-Gateway/blob/main/Payload-data-write.md)
//
// For backwards compatibility, a limited number of specific write commands are still available:
//
// Parameter in packet type 35 (8-bit value) for switching cooling/heating on/off
//      0x31 works on EHVX08S23D6V
//      0x2F works on EHYHBX08AAV3	
//      0x2D works on EHVX08S26CA9W	
#define PARAM_HC_ONOFF 0x0031

// Parameter in packet type 35 for switching DHW on/of
//      0x40 works on EHVX08S23D6V 	
//      0x40 works on EHYHBX08AAV3	
//      0x3E works on EHVX08S26CA9W
#define PARAM_DHW_ONOFF 0x0040 // DHW on/off

// Other parameters in packet type 35 can be set using new 'E' command or using old 'P'/'Y' commands
//      0x48 works on EHVX08S26CB9W for DHW boost  (use 'P48' followed by 'Y1' or 'Y0' or use 'E 35 48 1' or 'E 35 48 0')

// Parameter in packet type 36 (16-bit value) for temperature settings
//      0x0003 works on EHYHBX08AAV3 for DHW temperature setting
#define PARAM_TEMP 0x0003 // DHW setpoint

// Packet type 3A has 16-bit parameter address and 8-bit values
// Parameters in packet type 3A (8-bit value) are used for various system settings
// such as holiday, schedule, cooling/heating
#define PARAM_SYS 0x005B // holiday schedule on/off
