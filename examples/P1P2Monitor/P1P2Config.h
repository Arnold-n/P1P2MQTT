/* P1P2Config.h
 *
 * Copyright (c) 2019-2024 Arnold Niessen, arnold.niessen-at-gmail-dot-com - licensed under CC BY-NC-ND 4.0 with exceptions (see LICENSE.md)
 *
 * 20241113 v0.9.56 support for V1.3
 * 20241012 v0.9.55 cleanup, fix insert message, fix L99 in L0 mode
 * 20240605 v0.9.53 replace KLICDA by configurable counterCycleStealDelay
 * 20240512 v0.9.46 simultaneous writes, (fake) room temperature sensor, MHI increased buffer size, check-sum, and MHI-protocol data conversion, removed OLD_COMMAND support
 * 20230702 v0.9.40 increase buffer size for Hitachi
 * 20230611 v0.9.39 do init Daikin fields in H-link2 version too
 * 20230611 v0.9.38 H-link2 branch merged into main branch
 * 20230604 v0.9.37 support for ATmega serial enable/disable via PD4, required for P1P2MQTT bridge v1.2
 * 20230423 v0.9.35 (skipped)
 * 20230211 v0.9.33 added ENABLE_INSERT_MESSAGE_3x, use with care!
 * 20230117 v0.9.32 check CONTROL_ID for write commands
 * 20230108 v0.9.31 fix nr_param check
 * 20221224 v0.9.30 expand F series write possibilities
 * 20221211 v0.9.29 add control_ID in bool in pseudopacket, fix 3+4-byte writes, FXMQ support
 * 20221129 v0.9.28 option to insert message in 40F030 time slot for restart or user-defined write message
 * 20221102 v0.9.24 suppress repeated "too long" warnings
 * 20221029 v0.9.23 ADC code, fix 'W' command, misc
 * 20220918 v0.9.22 scopemode for writes and focused on actual errors, fake error generation for test purposes, control for FDY/FDYQ (#define F_SERIES), L2/L3/L5 mode
 * 20220903 v0.9.19 minor change in serial output
 * 20220830 v0.9.18 version alignment for firmware image release
 * 20220819 v0.9.17-fix fixed non-functioning 'E'/'n' write commands
 * 20220817 v0.9.17 minor changes, fixed 'W' command handling magic string prefix, SERIALSPEED/OLDP1P2LIB in P1P2Config.h now depends on F_CPU/COMBIBOARD selection
 * 20220808 v0.9.15 Added compile date/time to verbosity report
 * 20220802 v0.9.14 New parameter-write method 'e' (param write packet type 35-3D), error and write throttling, verbosity mode 2, EEPROM state, MCUSR reporting,
 *                  pseudo-packets, error counters, ...
 *                  Simplified code by removing packet interpretation, json, MQTT, UDP (all off-loaded to P1P2MQTT)
 *
 */

#ifndef MHI_SERIES
#define PSEUDO_PACKETS   // adds pseudopacket to serial output with ATmega status info
#endif

//#include "Arduino.h"

// Define serial speed
// Use 115200 for Arduino Uno/Mega2560 (serial over USB)
// Use 250000 for P1P2MQTT bridge, for direct serial connection between ATmega and ESP8266
//
#define COMBIBOARD // define this for Uno+ESP combiboard, determines SERIALSPEED (250k instead of 115k2) and SERIAL_MAGICSTRING. Ignored if F_CPU=8MHz (as used in P1P2MQTT bridge)

// define only one of *_SERIES:
//#define E_SERIES   // for Daikin E* heat pumps
//#define F_SERIES   // for Daikin F* VRV systems
//#define H_SERIES   // for Hitachi H-link2 systems
//#define M_SERIES   // for Mitsubishi systems (not supported yet)
//#define MHI_SERIES // for MHI (Mitsubishi Heavy Industries) systems
//#define S_SERIES   // for Sanyo systems (not supported yet)
//#define T_SERIES   // for Toshiba systems

#if (defined E_SERIES || defined F_SERIES)
#define EF_SERIES
#define MONITORCONTROL   // enables P1P2 bus writing (as auxiliary controller and/or for requesting counters)
#endif /* (defined E_SERIES || defined F_SERIES) */

#if (defined T_SERIES || defined H_SERIES)
#define TH_SERIES
#endif /* (defined T_SERIES || defined H_SERIES) */

#if F_CPU > 8000000L
// Assume Arduino Uno (or Mega) hardware; use 115200 Baud for USB or 250000 Baud for combi-board
#ifdef COMBIBOARD
#define SERIALSPEED 250000
#define SERIAL_MAGICSTRING "1P2P" // Serial input line should start with SERIAL_MAGICSTRING, otherwise input line is ignored
#else
#define SERIALSPEED 115200
// do not #define SERIAL_MAGICSTRING
#endif
#else /* F_CPU <= 8MHz */
// Assume on P1P2MQTT bridge hardware, 8MHz ATmega328P(B), use 250000 Baud and SERIAL_MAGICSTRING
#define SERIALSPEED 250000
#define SERIAL_MAGICSTRING "1P2P" // Serial input line should start with SERIAL_MAGICSTRING, otherwise input line is ignored
#endif /* F_CPU */

#define WELCOMESTRING "P1P2Monitor v0.9.56"
#define SW_PATCH_VERSION 56
#define SW_MINOR_VERSION 9
#define SW_MAJOR_VERSION 0

#ifdef MHI_SERIES
#define INIT_MHI_FORMAT 1 // 1 (default) for conversion from/to 3-bit-per-byte format with check-sum, 0 for raw data
#endif /* MHI_SERIES */

#define COUNTERREPEATINGREQUEST 0 // Change this to 1 to trigger a counter request cycle at the start of each minute
                                  //   By default this works only as, and only if there is no other, first auxiliary controller (F0)
				  //   The counter request is done after the (unanswered) 00F030*, if this slot is available
				  //   Counter configuration setting can be done 'C' command
				  //
				  //   Alternatively, if counter cycle steal delay ('Q' command) is not zero,
                                  //   the counter request is done in the pause after the 400012* response
                                  //   This works for systems were the usual pause between 400012* and 000013* is around 47ms (+/- 20ms timer resolution)
				  //   So this may only work without bus collission on these systems, use with care!
				  //   The counter request is inserted at counterCycleRequestDelay ms after the 400012* response
                                  //   The delay value needs to be selected carefully to avoid bus collission between the 4000B8* response and the next 000013* request
				  //   A value around 9ms works for my system; use with care, and check your logs for bus collissions or other errors
                                  //   (in case of bus collision or read errors, the bridge will switch off cycle stealing mode and will switch off control mode)

// set CTRL adapter ID; if not used, installer mode becomes unavailable on main controller
#define CTRL_ID_2 0x10 // Value for packet 0x31 payload byte 5

// Controller ID values
#define CONTROL_ID_NONE 0x00     // do not act as auxiliary controller
#define CONTROL_ID_0    0xF0     // first auxiliary controller address
#define CONTROL_ID_1    0xF1     // second auxiliary controller address

// EEPROM saves state of
// -Daikin CONTROL_ID (whether P1P2Monitor acts as auxiliary controller, and which one) (Daikin only)
// -Daikin counterrepeatingrequest (whether P1P2Monitor repeatedly requests counters) (Daikin E_SERIES only)
// -verbose (verbosity level)
// -MHI format conversion on/off
// -MHI inter-byte pause allowance
// -brand  0=unknown
//         1=Daikin (E_SERIES and F_SERIES)
//         2=Hitachi (H_SERIES)
//         3=Mitsubishi (M_SERIES)
//         4=MHI (MHI_SERIES)
//         5=Panasonic (P_SERIES)
//         6=Sanyo (S_SERIES)
//         7=Toshiba (T_SERIES)
// -model (currently for Daikin only):
//         0=undefined/unknown
//         1=F-series generic,
//         2=EKH/Altherma, EKH, EWYQ?
//         3=major versions CA/CB/Altherma 3
//         4-9 reserved for other major versions
//         10=F-series, major version B,C,L(some)
//         11=F-series, major version L(some),P,A
//         12=F-series, major version M
//         20-29 reserved for EKH*/Altherma 2/ perhaps EWYQ,EWAQ
//         30-39 reserved for Altherma 3 models
//
// to reset EEPROM to settings in P1P2Config.h, either erase EEPROM, or change EEPROM_SIGNATURE in P1P2Config.h
#ifdef E_SERIES
#define EEPROM_SIGNATURE "P1P2SIG01"
#elif defined F_SERIES
#define EEPROM_SIGNATURE "P1P2-F"
#elif defined H_SERIES
#define EEPROM_SIGNATURE "P1P2-H"
#elif defined M_SERIES
#define EEPROM_SIGNATURE "P1P2-M"
#elif defined MHI_SERIES
#define EEPROM_SIGNATURE "P1P2-MHI"
#elif defined S_SERIES
#define EEPROM_SIGNATURE "P1P2-S"
#elif defined T_SERIES
#define EEPROM_SIGNATURE "P1P2-T"
#else
#define EEPROM_SIGNATURE "P1P2OTHER"
#endif

#define EEPROM_ADDRESS_CONTROL_ID          0x00 // 1 byte for CONTROL_ID (Daikin-specific)
#define EEPROM_ADDRESS_COUNTER_STATUS      0x01 // 1 byte for counterrepeatingrequest (Daikin-specific)
#define EEPROM_ADDRESS_BRAND               0x03 // 1 byte for brand
#define EEPROM_ADDRESS_MODEL               0x04 // 1 byte for model
#define EEPROM_ADDRESS_ERROR_MASK          0x05 // 1 byte for error mask
#if defined MHI_SERIES || defined TH_SERIES
#define EEPROM_ADDRESS_ALLOW               0x06 // 1 byte for MHI max inter-byte pause
#endif /* MHI_SERIES || TH_SERIES */
#ifdef MHI_SERIES
#define EEPROM_ADDRESS_MHI_FORMAT          0x07 // 1 byte for raw/MHI setting
#endif /* MHI_SERIES */
#ifdef EF_SERIES
#define EEPROM_ADDRESS_WRITE_BUDGET_PERIOD  0x06 // 1 byte for write budget period
#define EEPROM_ADDRESS_INITIAL_WRITE_BUDGET 0x07 // 1 byte for initial write budget
#define EEPROM_ADDRESS_COUNTER_CYCLE_STEAL_DELAY 0x08 // 1 byte for counter cycle stealing delay (0=off, 9 seems to work om some models, use with care!)
// skip 0x09 for EEPROM_ADDRESS_VERSION
#ifdef E_SERIES
#define EEPROM_ADDRESS_ROOM_TEMPERATURE_LSB 0x0A // roomTemperature & 0xFF
#define EEPROM_ADDRESS_ROOM_TEMPERATURE_MSB 0x0B // (roomTemperature >> 8) & 0x7F | (insertRoomTemperature ? 0x80 : 0x00)
#define INIT_ROOM_TEMPERATURE_LSB 200 // 20.0C
#define INIT_ROOM_TEMPERATURE_MSB 0   // off
#endif /* E_SERIES */
#endif /* EF_SERIES */


#define EEPROM_ADDRESS_VERSION             0x09
#define EEPROM_ADDRESS_SIGNATURE           0x10 // should be last, in view of unspecified strlen(EEPROM_SIGNATURE)

#ifdef E_SERIES
#define INIT_BRAND 1
#define INIT_MODEL 3
#endif
#ifdef F_SERIES
#define INIT_BRAND 1
#define INIT_MODEL 1
#endif
#ifdef H_SERIES
#define INIT_BRAND 2
#define INIT_MODEL 0
#endif
#ifdef T_SERIES
#define INIT_BRAND 6
#define INIT_MODEL 0
#endif
#ifdef MHI_SERIES
#define INIT_BRAND 4
#define INIT_MODEL 0
#endif

#ifdef EF_SERIES
// Write budget: thottle parameter writes to limit flash memory wear
#define INIT_WRITE_BUDGET_PERIOD 60 // on avg max one write per 60m allowed
#define MAX_WRITE_BUDGET        100 // budget never incremented beyond this value (so don't allow to burn more than 100 writes at once)
#define INIT_WRITE_BUDGET        10 // initial write budget upon boot (255 = unlimited; recommended: 10)
#define WR_CNT 1                    // number of write repetitions for writing a paramter. 1 should work reliably, no real need for higher value

// Error budget: P1P2Monitor should not see any errors except upon start falling into a packet
// so if P1P2Monitor sees see too many errors, it stops writing
//
#define TIME_ERRORS_PERMITTED 3600 // on avg max one error per 3600s allowed (otherwise writing will be switched off)
#define MAX_ERRORS_PERMITTED    20 // budget never higher than this (don't allow too many errors before we switch writing off) // 8-bit, so max 255
#define INIT_ERRORS_PERMITTED   10 // initial error budget upon boot
#define MIN_ERRORS_PERMITTED     2 // don't start control unless error budget is at least this value

// serial read buffer size for reading from serial port, max line length on serial input is 99 (3 characters per byte, plus 'W" and '\r\n')
#define RS_SIZE 99
// P1/P2 write buffer size for writing to P1P2bus, max packet size is 24 (have not seen anytyhing over 24 (23+CRC))
#define WB_SIZE 25
// P1/P2 read buffer size to store raw data and error codes read from P1P2bus; 1 extra for reading back CRC byte; 24 might be enough
#define RB_SIZE 33
#endif /* EF_SERIES */

#ifdef MHI_SERIES
// serial read buffer size for reading from serial port, max line length on serial input is 150 (2 characters per byte, plus some)
#define RS_SIZE 110
// MHI: should be >22, tbd
#define WB_SIZE 41
// MHI: should be >22, tbd
#define RB_SIZE 41
#endif /* MHI_SERIES */

#ifdef TH_SERIES
// serial read buffer size for reading from serial port, max line length on serial input is 150 (2 characters per byte, plus some)
#define RS_SIZE 150
// read/write buffer size for writing to P1P2bus, max packet size is 64
#define WB_SIZE 65
#define RB_SIZE 65
#endif /* H_SERIES */

#define INIT_ECHO 1         // defines whether written data is read back and verified against written data (advise to keep this 1)
#define INIT_SCOPE 0        // defines whether scopemode, recording timing info, is on/off at start (advise to keep this 0)

#define INIT_SD 50        // (uint16_t) delay setting in ms for each manually instructed packet write
#define INIT_SDTO 2500    // (uint16_t) time-out delay in ms (applies both to manual instructed writes and controller writes)

// CRC settings, values can be changed via serial port
#ifdef EF_SERIES
#define CRC_GEN 0xD9    // Default generator/Feed for CRC check; these values work at least for the Daikin hybrid
#else /* EF_SERIES */
#define CRC_GEN 0x00    // Default generator/Feed for CRC check; these values work at least for the Daikin hybrid; no CRC check for Hitachi/Toshiba (CS not checked in lib)
#endif /* EF_SERIES */
#define CRC_CS_FEED 0x00   // Define CRC_GEN to 0x00 means no CRC is checked when reading or added when writing (in case of Hitachi/Toshiba/MHI)

#ifdef MHI_SERIES
#define CS_GEN 1        // Use (addition) checksum generator/verification for MHI
#else
#define CS_GEN 0        // Use (addition) checksum generator/verification for MHI
#endif /* MHI_SERIES */

#ifdef EF_SERIES
// auxiliary controller timings
#define F030DELAY 100   // Time delay for in ms auxiliary controller simulation, should be larger than any response of other auxiliary controllers (which is typically 25-80 ms)
#define F03XDELAY  30   // Time delay for in ms auxiliary controller simulation, should preferably be a bit larger than any regular response from auxiliary controllers (which is typically 25 ms)
#define F0THRESHOLD 5   // Number of 00Fx30 messages to remain unanswered before we feel safe to act as auxiliary controller

// L99 command inserts messages to restart Daikin system
// Wxxxxxx command inserts user-specified message in 40Fx3x time slot
#define F030DELAY_INSERT 25    // Time delay for inserted message; 25 seems OK, but if bus collisions occur, try 5
#define RESTART_NR_MESSAGES 2 // nr of restart messages to be sent (1 is sometimes not enough, 2 OK?)
#define RESTART_PACKET_TYPE 0x12
#define RESTART_PACKET_PAYLOAD_BYTE 12
#define RESTART_PACKET_BYTE 0x20

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
#endif /* EF_SERIES */
