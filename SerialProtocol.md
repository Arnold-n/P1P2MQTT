**P1P2Monitor serial protocol description**

For the serial communication to the Arduino Uno running P1P2Monitor or P1P2Controller, the following configuration and packet-write commands are available :

For monitoring purposes (P1P2Monitor and P1P2Controller):

- W\<hex data\> Write packet (max 32 bytes) (no 0x prefix should be used for the hex bytes; hex bytes may be concatenated or separated by white space)
- Cx counterrequest triggers single cycle of 6 B8 packets to request counters from heat pump; temporarily blocks controller function
- Vx Sets reading mode verbose off/on
- Tx sets new delay value in ms, to be used for future packets (default 0)
- Ox sets new delay timeout value in ms, to be used immediately (default 2500)
- Xx calls P1PsSerial.SetEcho(x) to set Echo on/off
- Gx Sets crc_gen (default 0xD9)
- Hx Sets crc_feed (default 0x00)
- V  Display current verbose mode
- T  Display current delay value
- O  Display current delay timeout value
- X  Display current echo status
- G  Display current crc_gen value
- H  Display current crc_feed value
- \* comment lines starting with an asterisk are ignored (and are echoed in verbose mode)

And for controlling purposes (P1P2Controller only):
- Lx sets external controller mode and address: x=0: off, x=1: controller address 0xF0, x=2: controller address 0xF1
     (for EHYHBX08AAV3: use 1, cannot be combined with another auxiliary controller)
     (for EHVX08S23D6V: use 1 if you don't have another auxiliary controller, use 2 if you do)
- Yx sets DHW on/off (using parameter 0x40 from the set of parameters used in packet type 35)
- Zx sets heating/cooling on/off (using parameter 0x31 in packet type 35; for other devices this should be a different parameter)
- Px sets the parameter number in packet type 35 (0x00..0xFF) to use for heating/cooling on/off (default 0x31 forEHVX08S23D6V, use 0x2F for EHYHBX08AAV3)
- L reports current controller mode (0, 1, or 2)
- Y reports DHW status as reported by main controller
- Z reports heating/cooling status as reported by main controller
- P reports parameter number in packet type 35 used for cooling/heating on/off

These commands are case-insensitive. The maximum line length is 98 bytes (allowing "W 00 00 [..] 00\n" format for a 32-byte write packet). A line which is longer will be ignored. When a partial line is received with a break of >5ms, the partial line and anything following it until an EOL will be ignored. The first line will also be ignored for robustness reasons.

The above commands can also be used via the mqtt channel P1P2/W (response via P1P2/R) on the ESP running P1P2-bridge-esp8266.

In addition, the following commands are available to configure parameter conversion on the ESP or on the P1P2Monitor if JSON is compiled-in.
The ESP captures and handles these commands and does not forward them to the Arduino.

- Ux Sets mode off/on (0/1) whether to include (in json format and on ESP also on mqtt) unknown bits and bytes
- Sx Sets mode off/on (0/1) whether to include (in json format and on ESP also on mqtt) only changed parameters
- U  Display display-unknown mode
- S  Display changed-only mode
