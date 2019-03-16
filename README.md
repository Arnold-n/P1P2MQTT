**P1P2 serial monitor**

The P1P2Serial library and P1P2monitor enable reading data on the Daikin/Rotex P1/P2 serial interface using an Arduino Uno (or a similar board) and a small adapter circuit. This combination translates the P1/P2 signals into a regular serial protocol over USB which can be read by a system such as a Raspberry Pi. The P1P2Monitor program currently supports two operation modes:
(1) raw data out over USB serial, just copying each byte to the serial line (timing information is implicit; but parity error detection is lost), or
(2) hexadecimally coded data in ASCII out over USB serial, each data block starts on a new line (a data block is defined as a number of consecutively sent bytes on the interface). Optionally, the time between data blocks can be shown in milliseconds at the strat of each line. If a parity error is detected, a byte is prefixed with "-PE:". Optionally the last byte of each line, which is a CRC byte, is verified for correctness (this will not produce any output unless a CRC error is actually detected).
Planned is a third operation mode, in which the relevant data items are presented in ASCII human-readable form. As different heat pumps use very different protocol data formats, this will be limited to certain product ranges for which users have contributed data and/or descriptions.

**How is the software licensed?**

The software is licensed under GPL V2.0. Please comply with the license conditions. If you improve or add, I would be interested to hear from you.

**What can I do with it?**

For now, you can only read the data on the P1/P2 bus. Preliminary write support is included but not tested yet. Writing to the bus may be a challenge for the more complex heat pumps. Write support in this version does not yet contain bus usage detection or colission detection. In this stage, you can use this library to view and study the data on the bus, and to monitor various temperatures, flow data, energy consumption, etcetera. This would for example enable the calculation of COP values.

**Why did you build this?**

My heat pump is a 2014 model Daikin hybrid heat pump/natural gas boiler combination. This is a very efficient combination: a heat pump is used in low power demand situations, whereas in high power demand situations a natural gas after-burner is also used in cascade operation (still allowing the heat pump to operate at low LWT temperature). This reduces natural gas usage by approximately 75% and results in a heat pump achieving, in our case, a year average COP of 3.6. Unfortunately the software in this system is very basic, and it behaves oddly. It often defrosts when totally unneeded, and in other cases not at all when air flow is entirely blocked at the outside unit (the COP will then drop dramatically towards 1.0, the heat pump could detect this situation by internally measuring the COP, but it does not seem to notice at all). It often sets the heat pump at a power setting that leaves the natural gas boiler to operate below its minimum power capability, resulting in an error message on the gas boiler and a far too low LWT, resulting in insufficient heating. Unfortunately, Daikin did not respond to or resolve these complaints. Therefore I want to understand and improve the behaviour myself via the P1/P2 bus.

**Which interface is supported?**

Daikin (or Rotex) uses various communication standards between thermostats and heat pumps. The P1/P2 standard is one of them. It is a proprietary standard. At the lowest level it is a 9600 Baud serial-like interface based on the Japanese Home Bus System (ET-2101). Technical details of this standard can be found in chapter 4 of the https://echonet.jp/wp/wp-content/uploads/pdf/General/Standard/Echonet/Version_2_11_en/spec_v211e_3.pdf.

**What does the P1/P2 adapter circuit look like?**

The P1/P2 bus is a two-wire circuit. The easiest way to make an adapter for this bus is to use the MM1192 HBS-Compatible Driver and Receiver (http://pdf.datasheetcatalog.com/datasheet_pdf/mitsumi-electric/MM1192XD.pdf) and an Arduino Uno (or Teensy or other device). The MM1192 data sheet does not provide information how to build a working circuit, but the data sheet for the MM1007 (https://www.digchip.com/datasheets/parts/datasheet/304/MM1007.php) and the XL1192 (http://www.xlsemi.com/datasheet/XL1192%20datasheet-English.pdf) show how to build it. Unfortunately, these schematics did not work for me as the MM1192 detected a lot of spurious edges in the noise P1/P2 signal. This was due to the relatively high amplitude of signal and noise, and to the common-mode distortion of the signal. I had to make three modifications to resolve that: (1) both resistors between the MM1192 and the P1/P2 lines were changed from 33kOhm to 100kOhm; (2) two resistors of 33 kOhm between the MM1192 input pins (15 and 16) and ground were added; and (3) one 1.5 kOhm resistor was added between ground and P1, and one 1.5 kOhm resistor was added between ground and P2 (but please note next question on the consequences of this third modification!). A fourth modification, the addition of a 680pF capacitor between pin 15 and pin 16 of the MM1192, reduces the detection of spurious edges further. A schematic diagram of the circuit will be added later.

**Is there any galvanic isolation? Is there any risk?**

PLEASE NOTE that the third modification removes the galvanic isolation between the P1/P2 bus and the Arduino! Use at your own risk! This should be OK as long as the Arduino and subsequent circuits are not connected to any other circuitry. Do not use any power supply which has built-in capacitors between the mains and the low voltage side. Again: apply these modifications at your own risk.

**What does the data on the P1/P2 bus look like, at the physical level?**

The P1/P2 bus uses the Home Bus System protocol, which is a serial protocol. It is variation of bipolar encoding (alternate mark inversion, https://en.wikipedia.org/wiki/Bipolar_encoding), but the pulses only take half of the bit time. So every falling edge in the signal represents a 0, and every "missing" falling edge represents a 1. Every byte is coded as a start bit (0), 8 data bits (LSB first), 1 parity bit (even), and 1 stop bit (1). Oscilloscope pictures are available on http://www.grix.it/forum/forum_thread.php?ftpage=1&id_forum=1&id_thread=519140&tbackto=/forum/forum_discussioni.php?id_forum=1. As it is a two-wire interface, devices should not write on the bus if any other device is writing. The HBS standard has an advanced collision detection and priority mechanism. In a 2-device set up of a Daikin heat pump and a thermostat, the devices seem to simply alternate turns in writing to the bus.

**What does the data on the P1/P2 bus look like, at the logical level?**

The data does not follow the HBS packet format or HBS timing specification. Instead, the data is transmitted in short packets of 4-24 bytes each, with alternating packets sent by the thermostat and by the heat pump. The Daikin hybrid uses a protocol where a set of 13, sometimes more, packets are communicated in sequence. We will call this set of packets a package. The thermostat sends the first packet and the last packet in each package. Further exceptions apply. The first few bytes of each packet may indicate source, desination, and/or packet type number. After that the payload follows, followed by a CRC byte. The CRC algorithm for the Daikin hybrid is a simple 8-bit LFSR with a feed of 0x00 and a generator value of 0xD9. It may be different for other products.

**What does the payload look like?**

The payload contains various data items, like temperatures, data flow, software serial number, operating mode, number of operating hours, energy consumed, etcetera. Temperatures and flows can be coded as value\*10, so with an accuracy of one digit after the comma: a value of 0x01B6 (hex value for 438, means 43.8). Other values may be encoded as two bytes, one byte for the value before the comma, the other behind (for example a room temperature is encoded as 0x14 0x18, or 20 24, meaning 20 24/256 or 20.1 degrees).

**Which settings can be tuned in the Monitor?**

Please see the comments in the P1P2Monitor.ino file. If RAWMONITOR is defined, bytes are simply copied 1-by-1 from the P1/P2 bus to the USB serial port. If RAWMONITOR is not defined, bytes are translated into human-readable ASCII hex. If a parity error is detected, the received byte is prefixed with the string "-PE:". After a pause of MINDELTA milliseconds in time between bytes, a new line is started to signal the start of a packet (unless the first byte after a pause results in a parity error, in which case the pause time field is overwritten to signal the parity error). If PRINTDELTA is defined, the time in milliseconds between the current and previous byte is shown before the packet (but if the break is longer than 255ms, it will be reported as 255ms). If CRC_GEN is CRC_FEED are defined, a CRC check is performed and an error message is printed if the CRC byte does not match its packet.

**Which files are included?**

- LICENSE: GPL-v2.0 license for P1P2Monitor and P1P2Serial
- P1P2Monitor.ino: (GPL-licensed) monitor program on Arduino, uses P1P2Serial library
- P1P2Serial.cpp and P1P2Serial.h: (GPL-licensed) P1P2Serial library, based on AltSoftSerial, and uses AltSoftSerial configuration files
- config/AltSoftSerial_Boards.h and AltSoftSerial_Timers.h: (MIT-licensed) AltSoftSerial configuration files
- usb2console.pi: simple python program to copy non-RAWMOMITOR USB serial input to stdout for Raspberry Pi or other host
- usb2console-raw.pi: simple python program for use with RAWMONITOR mode to copy USB serial raw input to stdout for Raspberry Pi or other host
- doc/Daikin-protocol\*: observations of protocol data for various heat pumps (work-in-progress)
- README.md: this file

**Where can I buy a MM1192?**

It is difficult to obtain. So far only a few sellers on ebay and aliexpress are selling the MM1192 and MM1007. I could not find a seller of the XL1192. The circuit could be re-built using a few opamps.

**Do you plan to offer pre-soldered or DIY adapter circuit kits?**

Perhaps, if there is interest. Please contact me if you are interested.

**Acknowledgements**

Many thanks to the following persons:
- a user called "donato35" for discovering that the P1/P2 bus is using HBS adapters (http://www.grix.it/forum/forum_thread.php?ftpage=2)
- a user called "Krakra" published a link to a description of the HBS protocol in the Echonet standard (https://community.openenergymonitor.org/t/hack-my-heat-pump-and-publish-data-onto-emoncms/2551/43)
- Bart Ellast for sharing his P1/P2 protocol data and analysis
- Paul Stoffregen for publishing the AltSoftSerial library, on which the P1P2Serial library is heavily based.
