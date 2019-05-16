**P1P2Serial library and P1P2Monitor example application**

The P1P2Serial library and P1P2Monitor enable reading and writing data on the Daikin/Rotex P1/P2 serial interface using an Arduino Uno (or a similar board) and a small P1P2 adapter circuit using an MM1192 IC. This combination translates the P1/P2 signals into a regular serial protocol over USB which can be read by a system such as a Raspberry Pi. The P1P2Monitor program currently supports two operation modes:
(1) raw data out over USB serial, just copying each byte to the serial line and vice versa. Timing information is only implicitly available. Information on collission detection, buffer overrun, parity error, and CRC error detection is lost, and
(2) hexadecimally coded data in ASCII out over USB serial. Optionally, each data block optionally starts on a new line (a data block is defined as a number of consecutively sent bytes on the interface), and the time between data blocks can be shown in milliseconds at the atart of each line. If a parity error is detected, a byte is prefixed with "-PE:". If a buffer overrun is detected, a byte is prefixed with "-OR:". If data is read as a result of a write by the monitor itself, and it differs from the byte written, it is prefixed with "-XX:" as it likely signals a collision on the interface. Optionally the last byte of each line, which is a CRC byte, is verified for correctness.
Translating the data into human-readable form is planned, likely as a host-based application. As different heat pumps use very different protocol data formats, this will be limited to certain product ranges for which users have contributed data and/or descriptions.

**How is the software licensed?**

The software is licensed under GPL V2.0. Please comply with the license conditions. If you improve or add, I would be interested to hear from you.

**What can I do with the P1/P2 adapter?**

For now, you can only read and write the data on the P1/P2 bus. Write support in version 0.9.1 includes a basic bus collision detection, which operates on a byte basis (and not on a bit basis). Even if a collission is detected, writing will continue. You can use this library to view and study the data on the bus, and to monitor various temperatures, flow data, energy consumption, and to send messages to the heat pump and/or to the thermostat. This would for example enable the calculation of COP values or switching certain functions. You can power the P1/P2 bus if you use the P1/P2 adapter with a P1/P2 device that requires a power feed.

Data is output to an OLED display connected to the Arduino Uno, and to the serial over USB connection. Support for conversion to json/emoncms format for upload into emoncms (openenergymonitor.org) is in preparation.

**Why did you build this?**

My heat pump is a 2014 model Daikin hybrid heat pump/natural gas boiler combination. This is a very efficient combination: a heat pump is used in low power demand situations, whereas in high power demand situations a natural gas after-burner is also used in cascade operation (still allowing the heat pump to operate at low LWT temperature). This reduces natural gas usage by approximately 75% and results in a heat pump achieving, in our case, a year average COP of 3.6. Unfortunately the software in this system is very basic, and it behaves oddly. It often defrosts when totally unneeded, and in other cases not at all when air flow is entirely blocked at the outside unit (the COP will then drop dramatically towards 1.0, the heat pump could detect this situation by internally measuring the COP, but it does not seem to notice at all). It often sets the heat pump at a power setting that leaves the natural gas boiler to operate below its minimum power capability, resulting in an error message on the gas boiler and a far too low LWT, resulting in insufficient heating. Unfortunately, Daikin did not respond to or resolve these complaints. Therefore I want to understand and improve the behaviour myself via the P1/P2 bus.

**What is the Daikin P1/P2 bus?**

Daikin (or Rotex) uses various communication standards between thermostats and heat pumps. The P1/P2 standard is one of them (others are F1/F2, Modbus, BACnet, which are not supported here). The P1/P2 protocol is a proprietary standard. At the lowest level it is a two-wire 9600 baud serial-like interface based on the Japanese Home Bus System (ET-2101). Some technical details of this standard can be found in chapter 4 of the https://echonet.jp/wp/wp-content/uploads/pdf/General/Standard/Echonet/Version_2_11_en/spec_v211e_3.pdf.

**Which interface is supported?**

This project was started for the Daikin P1/P2 bus. However the underlying electrical HBS format is used by many heat pump / air conditioning manufacturers, so far we have indications that the following buses are based on the HBS format: Daikin P1/P2, Daikin F1/F2 (DIII-Net), Mitsubishi M-Net, Toshiba TCC-Link, Hitachi H-link, Panasonic/Sanyo SIII-Net, perhaps also products from Haier and York. The logical format will likely differ. For example, Len Shustek made an impressive OPAMP-based reading circuit for the Mitsubishi M-Net and documented his protocol observations on https://github.com/LenShustek/M-NET-Sniffer. The logical format is clearly different from the Daikin format, but the P1P2Serial library and adapter will likely work for reading and writing M-Net too.

**How do I build a P1/P2 adapter circuit?**

The easiest way to make an adapter for this bus is to use the MM1192 HBS-Compatible Driver and Receiver (http://pdf.datasheetcatalog.com/datasheet_pdf/mitsumi-electric/MM1192XD.pdf) and an Arduino Uno (or Teensy or other device). 

Several adapter circuit schemas for use as an Arduino Uno hat are provided in PDF form:
 - version 1 bare-bones version without galvanic isolation (use at your own risk)
 - version 2 with galvanic isolation and an isolated DC-DC converter
 - version 3 with galvanic isolation, adds bus-powered and/or bus-powering options to version 2
 - version 3-selection, the basis for the SMD PCB prototype version as shown in the pictures

The MM1192 data sheet does not provide information how to build a working circuit, but the data sheet for the MM1007 (https://www.digchip.com/datasheets/parts/datasheet/304/MM1007.php) and the XL1192 (http://www.xlsemi.com/datasheet/XL1192%20datasheet-English.pdf) show how to build it. Unfortunately, these schematics did not work for me as the MM1192 detected a lot of spurious edges in the noise P1/P2 signal. This was due to the relatively high amplitude of signal and noise, and to the common-mode distortion of the signal. I had to make two modifications to resolve that: (1) both resistors between the MM1192 and the P1/P2 lines were changed from 33kOhm to 150kOhm; and (2) one 1.5 kOhm resistor was added between ground and P1, and one 1.5 kOhm resistor was added between ground and P2 (but please note next question on the consequences of this third modification!). Modification 2 is only needed if the adapter is not powered by the bus itself. A further modification, the addition of a 470pF capacitor between pin 15 and pin 16 of the MM1192, reduces the detection of spurious edges further. Line termination (a 10uF capacitor and a 200Ohm resistor in series between P1 and P2) may also improve the quality of the incoming signal. 

Version 1 is the bare-bones version - as stated this version does not provide galvanic isolation between the P1/P2 bus and the Arduino (but if only reading circuitry is implemented this risk is limited because the resistors between the P1/P2 bus and the MM1192 are 150k Ohm; however if modification 2 is implemented the resistance is lowered to around 1k Ohm; and if write circuitry is added there really is isolation at all. Use at your own risk!

Version 2 provides galvanic isolation. The MM1192 is powered via an isolated DC-DC 5V-5V converter.

Version 3 adds various power options to version 2. It adds a bus-powered 5V supply (as an alternative to the DC-DC 5V-5V converter), and optionally a bus power feed. If you want to interface to the heat pump, which operates as a master and provides a power feed on the bus, the power on the bus can be used to power the P1/P2 adapter. If you want to interface to a user interface or thermostat, which operates as a slave and requires a power feed, the adapter now can provide 15V on the P1/P2 bus. The schematic shows two options to power the P1/P2 bus: using a DC-DC 5V-15V converter, or using an external 15V power supply. This circuit functions properly in many cases, but not always. The circuit that uses bus power also influences the signal shape, depending on other devices and wire length this may result in miscommunications. Proper line termination may reduce this impact on the bus. However, for maximum reliability, I advise to use version 2, and only if you want to power the bus, add the two external 10mH inductors to do so.

**There is no galvanic isolation in adapter version 1! What is the risk?**

Using a circuitry without galvanic isolation should be OK as long as the Arduino and subsequent circuits are not connected to any other circuitry. Do not use any power supply which has built-in capacitors between the mains and the low voltage side. We don't know whether the P1/P2 bus is galvanically isolated on the heat pump side, so be careful: use of version 1 is at your own risk.

**What does the data on the P1/P2 bus look like, at the physical level?**

The P1/P2 bus uses the Home Bus System protocol, which is a serial protocol. It is variation of bipolar encoding (alternate mark inversion, https://en.wikipedia.org/wiki/Bipolar_encoding), but the pulses only take half of the bit time. So every falling edge in the signal represents a 0, and every "missing" falling edge represents a 1. Every byte is coded as a start bit (0), 8 data bits (LSB first), 1 parity bit (even), and 1 stop bit (1). Oscilloscope pictures are available on http://www.grix.it/forum/forum_thread.php?ftpage=1&id_forum=1&id_thread=519140&tbackto=/forum/forum_discussioni.php?id_forum=1. As it is a two-wire interface, devices should not write on the bus if any other device is writing. The HBS standard has an advanced collision detection and priority mechanism. In a 2-device set up of a Daikin heat pump and a thermostat, the devices seem to simply alternate turns in writing to the bus.

**What does the data on the P1/P2 bus look like, at the logical level?**

The data does not follow the HBS packet format or HBS timing specification. Instead, the data is transmitted in short packets of 4-24 bytes each, with alternating packets sent by the thermostat and by the heat pump. The Daikin hybrid uses a protocol where a set of 13, sometimes more, packets are communicated in sequence. We will call this set of packets a package. The thermostat sends the first packet and the last packet in each package. Further exceptions apply. The first few bytes of each packet may indicate source, desination, and/or packet type number. After that the payload follows, followed by a CRC byte. The CRC algorithm for the Daikin hybrid is a simple 8-bit LFSR with a feed of 0x00 and a generator value of 0xD9. It may be different for other products.

**What does the payload look like?**

The payload contains various data items, like temperatures, data flow, software serial number, operating mode, number of operating hours, energy consumed, etcetera. Temperatures and flows can be coded as value\*10, so with an accuracy of one digit after the comma: a value of 0x01B6 (hex value for 438, means 43.8). Other values may be encoded as two bytes, one byte for the value before the comma, the other behind (for example a room temperature is encoded as 0x14 0x18, or 20 24, meaning 20 24/256 or 20.1 degrees).

**Which settings can be tuned in the P1P2Monitor?**

Please see the comments in the P1P2Monitor.ino file. If RAWMONITOR is defined, bytes are simply copied 1-by-1 from the P1/P2 bus to the USB serial port and vice-versa. If RAWMONITOR is not defined, bytes are translated into human-readable ASCII hex. If DEBUG_HARDWARE is defined, a stand-alone self-test is performed by writing simple packets. If PRINTERRORS is defined, errors will be identified in the output: if a parity error is detected, the received byte is prefixed with the string "-PE:". After any pause in data received, a new line is started to signal the start of a packet (unless the first byte after a pause results in an error, in which case the pause time field is overwritten to signal the parity error). If PRINTDELTA is defined, the time in millisecond resolution between the current and previous byte is shown before the packet. If CRC_GEN and CRC_FEED are defined, a CRC check is performed and an error message is printed if the CRC byte does not match its packet.

**How to avoid bus collissions when writing?**

The simple answer is: write when others don't. In practice communication seems to be initiated by the thermostat(s), and parties alternate turns. The heat pump seems to answer each request after a fixed time (25ms in my situation). P1P2Serial implements a setDelay(t) function which ensures a write does not happen until t milliseconds after the last start bit falling edge detected. Calling setDelay(2) before starting a (block)transmission is a safe way to start any block of communication. If imitating a heat pump, calling setDelay(25) would make the adapter answer 25ms after the last byte received. Use of setDelay(1) is discouraged: a byte transmission takes slightly longer than 1 ms. A setDelay(1) will set transmission to 1.146ms, the time it takes to transmit 1 byte (11 bits at 9600 baud), but this still creates a bus collision if the other party transmitting has not finished. This may be detected by a parity error or read-back verification error, but bus collision avoidance is not implemented.

**Which files are included?**

- LICENSE: GPL-v2.0 license for P1P2Monitor and P1P2Serial
- P1P2Serial.cpp and P1P2Serial.h: (GPL-licensed) P1P2Serial library, based on AltSoftSerial, and uses AltSoftSerial configuration files
- examples/P1P2Monitor/P1P2Monitor.ino: (GPL-licensed) monitor program on Arduino, uses P1P2Serial library. Shows data on P1/P2 bus, and enables writing data. Reading/writing packets of data and CRC-byte generation/verification is supported. Using the u8g2 library, support for an additional SPI 128x64 display is provided (display connected directly to the Arduino Uno pins). This works for the Daikin hybrid model, it may work (partially) for other models.
- examples/P1P2Monitor/usb2console.py: simple python program to copy non-RAWMOMITOR USB serial input to stdout for Raspberry Pi or other host
- examples/P1P2Monitor/usb2console-raw.py: simple python program for use with RAWMONITOR mode to copy USB serial raw input to stdout for Raspberry Pi or other host
- examples/P1P2AdapterTest: program to test communication between two P1/P2 adapters
- doc/Daikin-protocol\*: observations of protocol data for various heat pumps (work-in-progress, contributions welcome)
- circuits/\*: P1/P2 adapter schematics and pictures of prototype board.
- config/AltSoftSerial_Boards.h and AltSoftSerial_Timers.h: (MIT-licensed) AltSoftSerial configuration files
- README.md: this file

**Where can I buy a MM1192?**

Only a few sellers on ebay and aliexpress are selling the MM1192 and MM1007. I could not find a seller of the XL1192. The read circuit could also be re-built using a few opamps as demonstrated by https://github.com/LenShustek/M-NET-Sniffer.

**Do you plan to offer pre-soldered or DIY adapter circuit kits?**

Yes, please contact me if you are interested, my e-mail is in the source code header. The adapter circuit is a 0.5" x 2" PCB for the Arduino Uno, and connects to GND, 5V, and to digital pins 8 and 9. It is based on version 2 (but without the LEDs). It is a single-sided PCB with SMD components (0603/0805 components, MM1192 SOP-16, SI8621 SOIC-8, and a Murata SMD-mounted 5V/5V converter). Pictures will follow.

 Images of the first prototype based on version 3 can be found in the circuits directory. However as version 2 is much more reliable I am currently offerinthe The final circuit will be based on the "version3-selection" schematics. 

**Acknowledgements**

Many thanks to the following persons:
- a user called "donato35" for discovering that the P1/P2 bus is using HBS adapters (http://www.grix.it/forum/forum_thread.php?ftpage=2)
- a user called "Krakra" published a link to a description of the HBS protocol in the Echonet standard (https://community.openenergymonitor.org/t/hack-my-heat-pump-and-publish-data-onto-emoncms/2551/43)
- Bart Ellast for sharing his P1/P2 protocol data and analysis
- designer2k2 for sharing his EWYQ005ADVP protocol data and analysis
- Paul Stoffregen for publishing the AltSoftSerial library, on which the P1P2Serial library is heavily based.
