**Release notes**

v0.9.57

- Daikin E: smart-grid entities
- Daikin F1/F2: initial version added
- all: support for P1P2MQTT bridge v1.3
- all: firmware built with modified version of WiFiManager

v0.9.54

- Buffered writes
- Daikin E: add Quiet_Mode_Active output
- Daikin E: switch bridge between user mode, advanced user mode, and installer mode
- Daikin E: simulate LCD backlight on bridge
- Daikin E: improve field setting handling
- Daikin F: preparing for masked writes
- Hitachi: more decoding, diversification
- HomeWizard: fix for 3-phase energy meter

v0.9.53 P1P2Monitor

- 'KLICDA'-style counter request by cycle stealing supported, configurable via 'q' command

v0.9.53 P1P2MQTT bridge

- tank-installed and cooling-capacity assumed by default until field setting is observed
- P48 option to update timestamps once/minute only
- V_Interface is now voltage
- pseudo hex data output is now outputmode 0x0004
- avoid double-reset AP after configuration in AP
- minor fixes

v0.9.52 P1P2MQTT bridge

- Increase time-out for external electricity meter input over MQTT
- Do not erase MQTT credentials in case of factory reset
- enable tank temperature setting default on
- support Src=0x41 for Daikin F series

v0.9.49 P1P2MQTT bridge

- increase max HA config message size

v0.9.48 P1P2MQTT bridge

- Daikin E: add external thermostat relay input decoding

v0.9.47 P1P2MQTT bridge

- add Hitachi packet 21001C decoding
- Hitachi: fix bcnt bitbasis decoding

v0.9.46 PP2Monitor/P1P2MQTT bridge

This version comes with a lot of changes, a few of which may break existing set-ups. Please let me know if you encounter any issue or bug.

- HA control via MQTT discovery: LWT/room temperature, DHW temperature, quiet levels, various settings, force defrost, Daikin restart, P1P2MQTT bridge control, various entity name changes
- new MQTT topic structure, including device name and bridge name
and also:
- improved NTP/TZ support
- P command for various configuration settings
- double reset restarts AP (with fixed password)
- supports fake temperature sensor
- missing WiFi restarts AP (with user-configurable password)
- Support for COP\_Realtime calculation based on external electricity meter input via MQTT
- Calculates COP\_lifetime based on Daikin internal counters, also calculate COP\_lifetime separately for periods before and after installing P1P2MQTT bridge
- Auto-detection of some Daikin F-series models
- remove bindata, remove json output, remove old commands ('Z')
- F-series fan speed 0/1/2 supported
- Adding hours\_drain\_pan\_heater, thanks to Pim Snoeks for finding this
- improves counter request mechanism
- Major/Minor/Patch information reporting
- improves ATmega behaviour during ESP reset

More detailed explanation for v0.9.46 [here](v0.9.46-releasenotes.md). See also [known issues](KnownIssues.md). For Home Assistant configuration, you can follow the [instructions for Home Assistant](HomeAssistant.md).

v0.9.43

- improves counter request mechanism

v0.9.42

- MDNS name changed to P1P2

v0.9.41

- adds option to restart after MQTT reconnect
- Eseries water pressure
- webserver for ESP update
- rewrite counter request mechanism
- reset data structures upon mqtt reconnect, homeassistant restart, or D3 command

v0.9.40

- adds NTP time stamps
- H-link2 decoding

v0.9.39

- P1P2Serial: H-link increased buffer size

v0.9.38

- P1P2Serial: H-link merge

v0.9.37

- support for V1.2 hardware

v0.9.36

- some thresholding added

v0.9.34

- adds AP timeout

v0.9.32

- resolves bug in HardwareSerial library

v0.9.31

- P1P2Monitor: fix nr_param check

v0.9.30

- switch to ESP_telnet v2.0.0

v0.9.29

- defrost info

v0.9.28 (November 12, 2022)

- adds static IPv4 option
- option to insert message in 40F030 time slot

v0.9.27 (November 12, 2022)

- fix to get Power\_\* visible in HA

v0.9.26 (November 9, 2022)

- fix MQTT password length to 80 in WiFiManager, clarify WiFiManager start screen

v0.9.25 (November 8, 2022)

- fix compilation issue for ETHERNET config

v0.9.24 (November 2, 2022)

- noWiFi option added

v0.9.23 (October 30, 2022)

- P1P2Serial and P1P2Monitor: ADC support
- P1P2-bridge-ESP8266: ADC, bit-banging SPI support, w5500 ethernet support
- See also [in which situations a few changes](OSS-dependencies/README.md) need to be made to the BSP and to the libraries

v0.9.22 (September 18, 2022)

- P1P2Serial: scopemode focus on errors, various lib changes, fake error generation option for testing, irq load measurement, removing OLDP1P2LIB, hwID
- P1P2Monitor: F-series control for FDY/FDYQ, scopemode improvements, additional L2/L3 modes (+experimental L5 mode for F-series)
- P1P2-bridge-ESP8266: minor changes, hwID, HA degree symbol

v0.9.21 (September 7, 2022)

- P1P2-bridge-ESP8266: maintain outputMode/outputFilter in EEPROM
- P1P2-bridge-ESP8266: #define MQTT_INPUT_HEXDATA/MQTT_INPUT_BINDATA enables data input from MQTT instead of serial input
- P1P2-bridge-ESP8266: reduce MQTT uptime and WiFi RSSI traffic

v0.9.20 (September 4, 2022)

- P1P2-bridge-ESP8266: F-series VRV decoding for FDY and FDYQ models

v0.9.19 (September 3, 2022)

- P1P2-bridge-ESP8266: MQTT user/password length 80/80

v0.9.18 (August 29, 2022)

- P1P2-bridge-ESP8266: state_class for HA energy overview

v0.9.17 (August 17, 2022)

- License change to Creative Commons Attribution-NonCommercial-NoDerivatives 4.0 International license [(CC BY-NC-ND 4.0)](https://creativecommons.org/licenses/by-nc-nd/4.0/) (with exceptions)
- P1P2Serial, P1P2Monitor, P1P2Serial: software-oscilloscope timing info ("U1"/"U0") added, available via R topic (outputmode 0x1000)
- P1P2Serial: rare read error in spike suppression bug fixed (OLDP1P2LIB)
- P1P2Serial: incorrect read-back-verification error report fixed (OLDP1P2LIB en NEWP1P2LIB/16MHz)

v0.9.16 (August 11, 2022)

- P1P2Serial: #define S_TIMER switch to make TIMER0 use (for uptime_sec) optional
- P1P2Monitor: change upt\* from uint32_t to int32_t to catch uptime_sec returning -1

v0.9.15 (August 8, 2022)

- P1P2Serial: all LEDs on until P1/P2 bus traffic
- P1P2Monitor: extended verbosity command output with compile date/time
- P1P2-bridge-esp8266 (still end-of-life): extended verbosity command output with compile date/time, OTA hostname unique, few minor fixes

v0.9.14 (August 2, 2022)

- P1P2Serial: major rewrite of library, send and receive methodology spreads CPU load better over time, allowing 8MHz operation on ATmega328P. Old library version still available as fall-back (#define OLDP1P2LIB),
- P1P2Monitor: program supports generic parameter writing ('E' command) to packet types 0x35-0x3D. Other improvements include error and write throttling, error counters, EEPROM state maintenance, self-reset command, and communication via pseudo-packets: packets that look like P1/P2 packets, but which carry information generated by P1P2Monitor, and which just like real packets generate json/MQTT parameter packets and homeassistant MQTT discovery packets. Json/mqtt support has been removed from the ATmega code as it doesn't fit: it is better handled on a separate CPU (such as the ESP8266),
- P1P2-bridge-esp8266: code has been extended with lots of improvements: telnet, schedule decoding, field setting decoding, boot process analysis, self-reset and ATmega-reset, ATmega-AVRISP, improved OTA, binary output over MQTT, configurable output modes ('J' command), configurable output filter ('S' command), home assistant MQTT discovery, state maintenance in flash and more. P1P2-bridge-esp8266 is becoming too big for an ESP8266 (schedule decoding is disabled for that reason). This is the last version of P1P2-bridge-esp8266, please don't use it for further develoment. It will be replaced by P1P2MQTT.
- P1P2MQTT: will replace P1P2-bridge-esp8266, work-in-progress, waiting for release

v0.9.13

- P1P2Monitor: no longer using millis(), reducing blocking due to serial input, correcting 'Z' handling to hexadecimal, and more

v0.9.12

- P1P2Monitor: Parameter support for F036 parameters
- P1P2serial: various minor bug fixes

v0.9.11

- P1P2Serial library: suppress end-of-block detection in case of short pauses between bytes in a package (for Zennio's KLIC-DA)
- P1P2Monitor: extended counter request functionality for KLIC-DA

v0.9.10

- P1P2Serial library: upon bus collision detection, write buffer is emptied

v0.9.9

- P1P2Monitor: autodetection of external controller slave address (thanks to the sugestion of Krakra)
- P1P2Monitor: reduced bus collision situations
- P1P2Serial library: added writeready() function

v0.9.8

- [+20190911] amended savehistory to include coverage for packet type 16; added P1P2Convert for Linux/RPi postprocessing of logs
- Added hysteresis functionality in temperature reporting
- Errors returned in errorbuf by readpacket() on longer contain EOB flag, only real error flags

v0.9.7

- Fixed hang/hick-up bug (switch from TIMER0 to TIMER2)
- Improved reliability of serial input to P1P2Monitor
- Ignore packets if CRC error detected
- Misc improvements
- Added counter request functionality to P1P2Monitor
- Rewrote release note format

v0.9.6

- !! P1P2Monitor has limited control functionality to switch DHW on/off and heating/cooling on/off. Tested on 2 products only.
- json functionality merged into P1P2Monitor
- udp functionality added to P1P2Monitor
- P1P2-bridge-esp8266 savehistory functionality expanded for F035 messages
- P1P2-bridge-esp8266 jsonstring buffer overflow handling improved
- Reduced Serialprint clutter from ESP via P1P2Monitor to Mqtt
- Added packetavailable() to avoid blocking in readpacket()
- Added documentation for x0Fx3x messages and external controller description
- Changed byte counter in documentation: now byte counting in packet starts at 0 to match C coding convention
- Moved generic protocol documentation to doc/README.md

v0.9.5

- Changed delay behaviour and added delay timeout parameter
- minor bug fixed in reporting bytes to be written (missing leading zero for hex values 0A..0F)

v0.9.4

- Added documentation and improved README.
- The combined functions in P1P2Monitor turned out to be too much for an Arduino Uno, leading to memory shortage and/or instability - use separate example applications per function
- new header files for LCD and json/mqtt conversion functions
- Library clean up, deleted unused functions, deleted relation to Stream
- several bug fixes
- improved ms counter, and reset of prescaler added
- time measurement changed
- delta/error reporting separated
- return value of readpacket changed from #bytes stored to #bytes received
- increased RX_BUFFER_SIZE to reduce risk of receiving buffer overruns

v0.9.3

- changed error handling
- corrected deltabuf type in readpacket

v0.9.2

- added setEcho()
- added readpacket(), writepacket()

v0.9.1

- added setDelay()

v0.9.0

- improved reading, writing, meta-data
- added support for timed writings
- added collission detection
- added stand-alon hardware debug mode
