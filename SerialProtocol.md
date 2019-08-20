**P1P2Monitor serial protocol description**

For the serial communication to the Arduino Uno running P1P2Monitor, the following configuration and packet-write commands are available :

- W\<hex data\> Write packet (max 32 bytes) (no 0x prefix should be used for the hex bytes; hex bytes may be concatenated or separated by white space)
- Vx Sets reading mode verbose off/on
- Tx sets new delay value in ms, to be used for future packets (default 0)
- Ox sets new delay timeout value in ms, to be used immediately (default 2500)
- Xx calls P1PsSerial.SetEcho(x) to set Echo on/off
- Gx Sets crc_gen (default 0xD9)
- Hx Sets crc_feed (default 0x00)
- V  Display current verbose mode
- G  Display current crc_gen value
- H  Display current crc_feed value
- X  Display current echo status
- T  Display current delay value
- O  Display current delay timeout value
- \* comment lines starting with an asterisk are ignored (and are echoed in verbose mode)

These commands are case-insensitive. The maximum line length is 98 bytes (allowing "W 00 00 [..] 00\n" format for a 32-byte write packet).

For the serial communication from Mqtt P1P2/W to the ESP running P1P2-bridge-esp8266, an additional set of commands is available to configure the ESP parameter conversion:

- Ux Sets mode whether to include (in json format and whether to publish on mqtt) unknown bits and bytes off/on
- Yx Sets mode whether to repeat (in json format and whether to publish on mqtt) unchanged parameters
- U  Display display-unknown mode
- Y  Display changed-only mode

