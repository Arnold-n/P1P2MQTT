**Release notes**

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
