# P1P2Monitor commands and serial communication format

This document describes the serial interface to the ATmega328P or Arduino Uno. It is only relevant if you interface directly to P1P2Monitor. If you use P1P2-bridge-esp8266 on the P1P2-ESP-interface, this information is not relevant for you, in that case please read the [MQTT protocol information](https://github.com/Arnold-n/P1P2Serial/blob/main/MqttProtocol.md).

## Serial output

Unless you set verbosity to 0, each P1P2Monitor serial output line always starts 
- with "R " for errorfree raw data read directly from the P1/P2 bus, or 
- with "\* " for any other (human-readable) output (including raw data with errors).
The serial output is forwarded by P1P2-bridge-esp8266 via MQTTT topics P1P2/R and P1P2/S, respectively.  

In addition, if JSON and OUTPUTSERIAL are defined in P1P2Monitor, json parameter blocks are output over serial on lines starting with "J ". If MQTTTOPICS and OUTPUTSERIAL are defined, mqtt-like parameters are output on lines starting with "P ". The ATmega328P is limited in capacity so json/mqtt functionality is limited. It is strongly advised to do the raw data interpretation to paramter values on a different device, such as an ESP running P1P2-bridge-esp8266.

If PSEUDO_PACKETS is defined, P1P2Monitor outputs additional information on its own status over the serial line in a format as if the data originates from the P1/P2 bus, using packet types 0x0A-0x0F. These messages do not originate from the P1/P2 bus, but P1P2-bridge-esp8266 will handle interpretation of this data like it interprets P1/P2 bus data.

## Serial input

The serial input is used for providing commands to P1P2Monitor. P1P2Monitor commands are case-insensitive. The maximum command line length is 98 bytes, longer lines will be entirely ignored.

*Warning: The first command line received over serial by P1P2Monitor after its reboot will - on purpose - be ignored for robustness reasons. If you communicate directly over serial (not via MQTT), a good habit is to send a dummy line "\*" as first command. If you use P1P2-bridge-esp8266, this warning is not applicable as P1P2-bridge-esp8266 takes care of the extra line.*

If P1P2-bridge-esp8266 is used in combination with P1P2Monitor, these commands are forwarded from MQTT channel P1P2/W to the serial input of P1P2Monitor.

The serial input is also used to communicate actual power consumption to P1P2Monitor, using the F command.

### Basic commands

The most useful commands:
 - L1 to start P1P2Monitor acting as an auxiliary controller
 - C2 to start requesting counters every minute
 - E for parameter writing (not released yet)

The 'E' parameter writing may take one of the following formats
- E <packet_type> <param_nr> <new param_val>
- E XX[ ]YYYY[ ]Z[..] where XX is a 2-digit hex encoding of the (1-byte) packettype, YYYY is a 4-digit hex encoding of the (2-byte) parameter number, and Z is the hex-encoding (1-8 hex digits) of the new parameter value to be written (1, 2, 3, or 4 bytes depending on the packet type: 35, 3A: 1-byte, 36, 3B: 2-byte, 37 and 3C: 3-byte, and 38, 39, and 3D: 4-byte).
For example, to switch heating on (value 01) by writing parameter number 002F in packet type 35, you can issue
- E 35 2F 1
- E 35 002F 01
- E 35002F01
or
- E 35002F1

A few pre-defined parameter writing actions are still available from earlier P1P2Monitor versions, for example:
- Z to write parameter <PARAM_HC_ONOFF> (defined in P1P2Config.h) in packet type 35:
 - Z0 switches heating(/cooling) off
 - Z1 switches heating(/cooling) on
- R3C set DHW temperature to 0x3C = 60 degrees

### Monitor commands:

- V  Show verbosity mode (default 1)
- Vx Sets verbosity mode (0 minimal, 1 traditional, 2 for P1P2-ESP-interface)
- Ux Sets mode off/on (0/1) whether to include (in json format and on ESP also on mqtt) unknown bits and bytes (default off, configurable in P1P2Config.h) (to be replaced in newer version by J:)
- U Display output-unknown mode
- Jx (soon to be released:) Sets output mode, sum of
  - 1 to output raw data (also requires DATASERIAL to be defined)
  - 2 to have mqtt output its data (also requires MQTTTOPICS to be defined)
  - 4 to have json output data (also requires JSON to be defined)
  - 8 to have mqtt/json include parameters even if functionality is unknown, warning: easily overloads ATmega
  - configurable in P1P2Config.h, default 0x01 on ATmega, default 0x0A on ESP
- J  Display output mode

#define OUTPUTMODE 0    // Sum of (can be changed run-time using 'U'/'u' command on ATmega (equivalent of 'J'/j' on ESP))

- Sx Sets mode off/on (0/1) whether to report parameters only if changed (default on, configurable in P1P2Config.h, setting off may overload ATmega's CPU/bandwidth) (to be replaced in newer version by:)
- Sx (to be released:) Set filter level
  - 0 show and repeat all parameters, even if unchanged
  - 1 show each parameter at least once, then only if changed
  - 2 idem, but omit temperatures
  - 3 idem, but also omit timestamps and uptime reports
- S  Display changed-only mode
- S  (to be released:) Display filter mode
- \* comment lines starting with an asterisk are ignored (and echoed in verbose mode)

## Auxiliary controller commands:

- L1 sets auxiliary controller mode on (controller address (0xF0 or 0xF1) is auto-detected after check whether another auxiliary controller is present or not) (can also be set in P1P2Config.h) (controller ID is saved in EEPROM, so this command remains effective after a restart)
- L0 sets auxiliary controller mode off
- L  displays current controller_id (0x00 = off; 0xF0/0xF1 is first/secondary auxiliary controller)
- Zx sets heating(/cooling) on/off (1/0) (function can be changed using Px command below to set any 8-bit parameter in packet type 35)
- Rx sets DHW temperature (function can be changed using Qx command below to set any 16-bit parameter in packet type 36)
- Nx sets 8-bit value of 8-bit parameter selected by Q command) in packet type 3A
- Px sets (16-bit) parameter number in packet type 35 to use for Zx command (default PARAM_HC_ONOFF)
- Qx sets (16-bit) parameter number in packet type 36 to use for Rx command (default PARAM_TEMP)
- Mx sets (16-bit) parameter number in packet type 3A to use for Nx command (default PARAM_SYS)
- C1 triggers single cycle of 6 B8 packets to request (energy/operation/starts) counters from heat pump
- C2 like C1, but repeating every new minute
- C0 Stop requesting counters
- C  Show counterrepeatingrequest status

## Raw data commands:

Commands for raw data writing to the bus (only for reverse engineering purposes), avoid these unless you know what you do:
- W\<hex data\> Write packet (max 32 bytes (as defined by WB_SIZE)) (no 0x prefix should be used for the hex bytes; hex bytes may be concatenated or separated by white space)
- T  Display current delay value (a packet will be written after exactly <delay> ms after the latest start bit, or if the bus has been silent for <delaytimeout> ms)
- Tx sets new delay value in ms, to be used for future packets (default 50 (older versions: 0))
- O  Display current delaytimeout value
- Ox sets new delay timeout value in ms, to be used immediately (default 2500)

## Miscellaneous

Supported but advised not to use, not really needed (some may be removed in a future version):
- G  Display current crc_gen value
- Gx Sets crc_gen (default 0xD9) (we have not seen any other values so no need to change)
- H  Display current crc_feed value
- Hx Sets crc_feed (default 0x00) (we have not seen any other values so no need to change)
- X  Display current echo status (determines whether bytes written will be echoed on the serial line, and also whether bus errors will be detected during writing)
- Xx sets echo status on/off (recommended to keep on to detect bus errors)
- Yx sets DHW on/off (use Px/Rx commands instead)
- P reports parameter number in packet type 35 used for cooling/heating on/off
- P reports parameter number in packet type 35 used for cooling/heating on/off
- P reports parameter number in packet type 35 used for cooling/heating on/off
- A instruct ESP to reset ATmega328P via reset line (to be released soon)
- D0 instruct ESP to reset itself (to be released soon)
- D1 instruct ESP to restart itself (to be released soon)
- K instruct ATmega328P to reset itself (to be released soon)
- B reserved for ESP for MQTT network settings (to be released soon)
- I reserved
