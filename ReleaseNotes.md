**Release notes**

v0.9.6

- Added documentation for x0Fx3x messages and external controller description
- Changed byte counter in documentation: now byte counting in packet starts at 0 to match C coding convention
- Moved generic protocol documentation to doc/README.md

v0.9.4

Added documentation and improved README.

**Applications**

v0.9.6

- P1P2Monitor has limited control functionality to switch DHW on/off and heating/cooling on/off. Tested on 2 products only.
- P1P2-bridge-esp savehistory functionality expanded for F035 messages
- P1P2-bridge-esp jsonstring buffer overflow handling improved

v0.9.5

- minor bug fixed in reporting bytes to be written (missing leading zero for hex values 0A..0F)

v0.9.4

The combined functions in P1P2Monitor turned out to be too much for an Arduino Uno, leading to memory shortage and/or instability.
In this version applications were separated into

for ESP8266:
- P1P2-bridge-esp8266 program for mqtt/json/serial-over-mqtt support to be used with P1P2Monitor (see SerialProtocol.md for serial protocol description)

for Arduino Uno:
- P1P2Monitor from/to serial port (see SerialProtocol.md for serial protocol description)
- P1P2LCD for showing data on LCD display
- P1P2json for outputting parameters in json format on serial out (no serial in/no bus writing support)
- P1P2HardwareTest for stand-alone hardware test


**P1P2Serial Library**

v0.9.6

- Added packetavailable() to avoid blocking in readpacket()

v0.9.5 

- Changed delay behaviour and added delay timeout parameter 

v0.9.4

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
