**P1P2Monitor serial protocol description**

For the serial communication to the Arduino Uno running P1P2Monitor or P1P2Controller, the following configuration and packet-write commands are available :

For monitoring purposes (P1P2Monitor and P1P2Controller):

- W\<hex data\> Write packet (max 32 bytes) (no 0x prefix should be used for the hex bytes; hex bytes may be concatenated or separated by white space)
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
- Wx sets DHW on/off (using parameter 0x40 from the set of parameters used in packet type 35)
- Zx sets heating/cooling on/off (using parameter 0x31 in packet type 35; for other devices this should be a different parameter)
- Px sets the parameter number in packet type 35 (0x00..0xFF) to use for heating/cooling on/off (default 0x31 forEHVX08S23D6V, use 0x2E for EHYHBX08AAV3)
- L reports current controller mode (0, 1, or 2)
- W reports DHW status as reported by main controller
- Z reports heating/cooling status as reported by main controller
- P reports parameter number in packet type 35 used for cooling/heating on/off

These commands are case-insensitive. The maximum line length is 98 bytes (allowing "W 00 00 [..] 00\n" format for a 32-byte write packet).

The above commands can also be used via the mqtt channel P1P2/W (response via P1P2/R) on the ESP running P1P2-bridge-esp8266. In addition, the following commands are 
available to configure the ESP parameter conversion. These commands are captured and handled by the ESP and not forwarded to the Arduino:

- Ux Sets mode whether to include (in json format and whether to publish on mqtt) unknown bits and bytes off/on
- Sx Sets mode whether to repeat (in json format and whether to publish on mqtt) unchanged parameters
- U  Display display-unknown mode
- S  Display changed-only mode

