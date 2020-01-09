**Summary**

The P1P2Monitor program and the P1P2Serial library read and write raw data packets on Daikin P1/P2 two-wire interface, and similar interfaces, using an Arduino Uno and a P1P2Serial shield. There is preliminary support for some Daikin P1/P2 based devices for parameter interpretation, json, and mqtt. WiFi can be added using an additional ESP8266. Preliminary support to switch basic functions (DHW, heating/cooling) on/off is confirmed to work on 3 specific models. Automatic counter requests (for electricity consumed) is supported.

**No liability, no warranty**

Any use is at your own risk (GPL sections 11 and 12 apply).

**New in v0.9.6-v0.9.11**

(See also release notes)
- 0.9.11 Allow pause between bytes in a package, for Zennio's KLIC-DA device, to avoid detecting each byte as individual packet
- 0.9.10 Upon bus collision detection, write buffer is emptied
- 0.9.9 Improved write behaviour and reduced bus collision situations, added auto-detection of controller address, added writeready() function
- 0.9.8 Added hysteresis functionality in temperature reporting
- 0.9.8 Removed EOB from errorbuf as returned by readpacket; ignore packets with errors; ignore first serial line as it may be a partial line
- 0.9.7 Improved reliability and fixed hang/hick-up bug (switch from TIMER0 to TIMER2, improved serial input handling, ignore packets with CRC error)
- 0.9.7 Added counter request functionality to P1P2Monitor
- 0.9.6 Control of DHW and heating/cooling: possibility to switch these functions on and off. Parameter reporting of the 35 packet type used to communicate between the main controller and auxiliary controllers.
 
**New in v0.9.4/v0.9.5/v0.9.5+controller**

ESP8266 support to interface Arduino Uno serial output from/to mqtt/json over WiFi. This works nicely on an Arduino-Uno/ESP8266 combination. Removed raw data in/out support. Separated the previously all-in-one P1P2Monitor functionality (LCD, monitor, test) into different example programs. Added limited control function.

In v0.9.5 delay behaviour when writing has been changed, with a timeout function, to avoid bus collissions. Instead of writing a packet when a pause was *at least* a certain length, a packet is written when a pause took *exactly* a certain length, or alternatively, a timeout occured.

**Set-up based on Arduino Uno/ESP8266/wifi**

P1/P2 2-wire interface
      
      P1P2Adapter Shield
      (pin 8, 9)
      Arduino Uno running P1P2Monitor (with JSON undefined)

Serial interface

      ESP8266 running P1P2-bridge-esp
      with wifi-manager and OTA firmware upgrade capability

Serial/json/mqtt over WiFi

The above set-up works nicely on a combi-board such as available from https://robotdyn.com/catalog/arduino/boards/uno-wifi-r3-atmega328p-esp8266-32mb-flash-usb-ttl-ch340g-micro-usb.html.
In order to program this board, the following steps are needed:

- enter your mqtt server details in P1P2-bridge-esp8266.ino
- program P1P2Monitor via USB on Arduino ATMega processor (set dip switches 3 and 4 on, others off)
- program P1P2-bridge-esp8266 via USB on ESP8266 (install ESP8266 in Arduino environment, select "Generic ESP8266 Module" as board, select 4MB as available memory) (set dip switches 5, 6, and 7 on, others off)
- connect Atmega and ESP8266 via serial interface: dip switches 1 and 2 on, others off. Optionally, also set dip switch 3 or  5 (or both) on to eavesdrop on the serial communication (in one direction only) via USB
- connect to the ESP WiFi AP on 192.168.4.1
- select your local wifi network, enter password, and have ESP8266 connect to your WiFi
- now, and upon each reboot, the ESP8266 connects to the MQTT server
- the ESP8266 should also become visible in the Arduino IDE as a network port, and can be reprogrammed OTA

**Set-up based on Arduino Uno/Raspberry Pi/LAN, wifi**

P1/P2 2-wire interface
      
      P1P2Adapter Shield
      (pin 8, 9)
      Arduino Uno running P1P2Monitor (with JSON undefined)

Serial interface over USB

      Raspberry Pi running [ tbd ]

mqtt/.. over WiFi or LAN

The above set-up works nicely on an original Arduino Uno, and has the advantage that the Arduino can be flashed without dip switch settings.

**Set-up based on Arduino Mega/W5500**

P1/P2 2-wire interface
      
      P1P2Adapter Shield
      (pin 48, 46 (not: 8, 9))
      Arduino Mega running P1P2Monitor with JSON and JSONUDP defined

      W5500 ethernet shield

json over UDP via LAN

**P1P2Serial library and P1P2Monitor example application**

The P1P2Serial library and P1P2Monitor enable reading and writing raw data packets on the Daikin/Rotex P1/P2 serial interface using an Arduino Uno (or a similar board) and a small P1P2Serial adapter circuit using an MM1192 IC. This combination translates the P1/P2 signals into a regular serial protocol over USB which can be read by a system such as a Raspberry Pi. The P1P2Monitor program converts bus data from/to ASCII-formatted HEX data. Certain parameters can be adjusted via the serial interface. Each packet (data block of consecutive bytes) starts on a new line. In verbose mode, the time between data blocks is shown in seconds (in ms resolution) at the atart of each line. Also in verbose mode, if an error is detected, a byte is prefixed with "-PE:" in case of a parity error, a byte is postfixed with ":OR-" in case of a read buffer overrun, a byte is prefixed with "-XX:" if a byte read during a write by the monitor itself differs from the byte written (works only if echo mode is on), as it likely signals a collision on the interface. Optionally the last byte of each line, which is usually a CRC byte, is verified for correctness.

Other example programs translate the data packets to parameter format in json and mqtt format.

**How is the software licensed?**

The software is licensed under GPL V2.0. Please comply with the license conditions. If you improve or add, I would be interested to hear from you.

**What can I do with the P1/P2 adapter?**

For now, you can only read and write raw (byte) data on the P1/P2 bus. Write support includes basic bus collision detection, which operates on a byte basis (and not on a bit basis - even if a collission is detected, writing the current byte will continue). You can use this library to view and study the data on the bus, and to monitor various temperatures, flow data, energy consumption.  This would for example enable the calculation of COP values (if you know the electric power consumption). So far I have not detected the power consumption information in the P1/P2 bus data. A separate signal from an electricity meter (P1 signal or blinking LED) may be needed to calculate the COP values.

The data observed on the bus is output to the serial over USB connection, and optionally to an OLED display connected to the Arduino Uno (this requires the u8g2 library to be installed). Support for conversion to json/emoncms format for upload into emoncms (openenergymonitor.org) is in preparation.

In principle code can be added to send messages to the heat pump and/or to the controller. For example one could send a message to request certain parameters that are normally not requested by the controller and thus normally not provided by the heat pump, such as energy consumption. In principle one could try to control the heat pump also by writing to the bus, but if the controller is also connected it may overrule such control in its communications. Taking full control of the heat pump is not supported and will be a real challenge. One option would be to disconnect the controller once the P1P2 adapter takes over control. Another approach would be to have a man-in-the-middle approach with two adapters.

**Why did you build this?**

My heat pump is a 2014 model Daikin hybrid heat pump/natural gas boiler combination. This is a very efficient combination: a heat pump is used in low power demand situations, whereas in high power demand situations a natural gas after-burner is also used in cascade operation (still allowing the heat pump to operate at low LWT temperature where its efficiency is higher). This reduces natural gas usage by approximately 75% and results in a heat pump achieving, in our case, a year average COP of 3.6. Unfortunately the software in this system is very basic, and it behaves oddly. It often defrosts when totally unneeded, and in other cases not at all when air flow is entirely blocked at the outside unit (the COP will then drop dramatically towards 1.0, the heat pump could detect this situation by internally measuring the COP, but it does not seem to notice at all). It often sets the heat pump at a power setting that leaves the natural gas boiler to operate below its minimum power capability, resulting in an error message on the gas boiler and a far too low LWT, resulting in insufficient heating. Unfortunately, Daikin did not respond to or resolve these complaints. Therefore I want to understand and improve the behaviour myself via the P1/P2 bus.

**What is the Daikin P1/P2 bus?**

Daikin (or Rotex) uses various communication standards between thermostats and heat pumps. The P1/P2 standard is one of them (others are F1/F2, Modbus, BACnet, which are not supported here). The P1/P2 protocol is a proprietary standard. At the lowest level it is a two-wire 9600 baud serial-like interface based on the Japanese Home Bus System (ET-2101). Some technical details of this standard can be found in chapter 4 of the https://echonet.jp/wp/wp-content/uploads/pdf/General/Standard/Echonet/Version_2_11_en/spec_v211e_3.pdf.

**Which interface is supported?**

This project was started for the Daikin P1/P2 bus. However the underlying electrical HBS format is used by many heat pump / air conditioning manufacturers, so far we have indications that the following buses are based on the HBS format: Daikin P1/P2, Daikin F1/F2 (DIII-Net), Mitsubishi M-NET, Toshiba TCC-Link, Hitachi H-link, Panasonic/Sanyo SIII-Net, perhaps also products from Haier and York. The logical format will likely differ. For example, Len Shustek made an impressive OPAMP-based reading circuit for the Mitsubishi M-NET and documented his protocol observations on https://github.com/LenShustek/M-NET-Sniffer. The logical format is clearly different from the Daikin format, but the P1P2Serial library and adapter will likely work for reading and writing M-NET too as the physical format is the same or similar (though the Mitsubishi format seems to have pulses of +/-2 V whereas P1/P2 has +/-5 V pulse levels).

**Is it safe?**

Use is entirely at your own risk. No guarantees. It is advised to connect/disconnect devices to the P1/P2 bus only if the power of all connected devices is switched off. Some Daikin manuals warn that device settings should not be changed too often, because of wear in the solid state memory in their devices - one of their manuals states a maximum of 7000 setting changes per year - without indicating the expected life time of their product. Don't change settings too often.

**How do I build a P1/P2 adapter circuit?**

The easiest way to make an adapter for this bus is to use the MM1192 HBS-Compatible Driver and Receiver (http://pdf.datasheetcatalog.com/datasheet_pdf/mitsumi-electric/MM1192XD.pdf) and an Arduino Uno (or Teensy or other device). 

The preferred circuit schematics (version 2) has galvanic isolation and an isolated DC-DC converter to power the MM1192. An older circuit without galvanic isolation (version 1) and an older circuit with bus-powering and bus-powered capability (version 3) are no longer documented here, but can be found in the github historic versions.

The MM1192 data sheet does not provide information how to build a working circuit, but the data sheet for the MM1007 (https://www.digchip.com/datasheets/parts/datasheet/304/MM1007.php) and the XL1192 (http://www.xlsemi.com/datasheet/XL1192%20datasheet-English.pdf) show how to build it. Unfortunately, these schematics did not work for me as the MM1192 detected a lot of spurious edges in the noisy P1/P2 signal. This was due to the relatively high amplitude of signal and noise, and to the common-mode distortion of the signal in the initial version without galvanic isolation (likely due to capacitive leakage in the USB power supply for the Arduino). I had to make two modifications to resolve that: (1) both resistors between the MM1192 and the P1/P2 lines were changed from 33kOhm to 150kOhm; and (2) one 1.5 kOhm resistor was added between ground and P1, and one 1.5 kOhm resistor was added between ground and P2. A further modification, the addition of a 470pF capacitor between pin 15 and pin 16 of the MM1192, reduces the detection of spurious edges further. Line termination (a 10uF capacitor and a 100-200Ohm resistor in series between P1 and P2) may also improve the quality of the incoming signal. 

XLSemi produces the XL1192 which looks like a direct clone of the MM1192. The XL1192 data sheet suggests that the specifications are the same, but they are not. The XL1192 has a higher voltage threshold on its input and a slightly lower (20% lower) voltage swing on its output channel. Using 75kOhm resistors instead of 150kOhm resistors on the input side of the XL1192 works perfect in my situation.

If you want to power the bus (which is only needed if you want to communicate to a stand-alone controller which is disconnected from and thus not powered from the heat pump) you can do so by providing 15V to the bus over a 20mH inductor with a reasonably low internal resistance (for example 2 RLB1314-103KL 100mH/12ohm/100mA inductors in series).

**What does the data on the P1/P2 bus look like, at the physical level?**

The P1/P2 bus uses the Home Bus System protocol, which is a serial protocol. It is a variation of bipolar encoding (alternate mark inversion, https://en.wikipedia.org/wiki/Bipolar_encoding), but the pulses only take half of the bit time. So every falling edge in the signal represents a 0, and every "missing" falling edge represents a 1. Every byte is coded as a start bit (0), 8 data bits (LSB first), 1 parity bit (even), and 1 stop bit (1). The Daikin devices have no pause between the stop bit and the next start bit, but Zennio's KLIC-DA has a short pause in between the individual bytes of a packet. Oscilloscope pictures are available on http://www.grix.it/forum/forum_thread.php?ftpage=2&id_forum=1&id_thread=519140. As it is a two-wire interface, devices should not write on the bus if any other device is writing. The HBS standard has an advanced collision detection and priority mechanism. In a 2-device set up of a Daikin heat pump and a thermostat, the devices seem to simply alternate turns in writing to the bus, and Daikin does not seem to support bus usage detection and collission avoidance, so take care when writing to the bus to avoid bus collisions.

**What does the data on the P1/P2 bus look like, at the logical level?**

The Daikin protocol does not follow the HBS packet format or HBS timing specification. Instead, the data is transmitted in short packets of 4-24 bytes each. Each cycle has a number of request packets sent by the thermostat and answered by the heat pump; additionally, a number of request packets are sent by the thermostat and answered by an external auxiliary controller (if available); some systems support only one auxiliary controller, others support two. The Daikin hybrid system uses a protocol where a set of 13, sometimes more, packets are communicated in sequence. We will call this set of packets a package. The thermostat sends the first packet and the last packet in each package. Further exceptions apply. The first few bytes of each packet may indicate source, desination, and/or packet type number. After that the payload follows, followed by a CRC byte. The CRC algorithm for the Daikin hybrid is a simple 8-bit LFSR with a feed of 0x00 and a generator value of 0xD9. It may be different for other products.

**What does the payload look like?**

The payload contains various data items, like temperatures, data flow, software serial number, operating mode, number of operating hours, energy consumed, etcetera. Temperatures and flows can be coded as value\*10, so with an accuracy of one digit after the comma: a value of 0x01B6 (hex value for 438, means 43.8). Other values may be encoded as two bytes, one byte for the value before the comma, the other behind (for example a room temperature is encoded as 0x14 0x18, or 20 24, meaning 20 24/256 or 20.1 degrees).

**How to avoid bus collissions when writing?**

The simple answer is: write when others don't. In practice communication seems to be initiated by the thermostat(s), and parties alternate turns. The heat pump seems to answer each request after a fixed time (25ms in my situation). P1P2Serial implements a setDelay(t) function which ensures a write does not happen until t milliseconds after the last start bit falling edge detected. Calling setDelay(2) before starting a (block)transmission is a safe way to start any block of communication. If imitating a heat pump, calling setDelay(25) would make the adapter answer 25ms after the last byte received. Use of setDelay(1) is discouraged: a byte transmission takes slightly longer than 1 ms. A setDelay(1) will set transmission to 1.146ms, the time it takes to transmit 1 byte (11 bits at 9600 baud), but this still creates a bus collision if the other party transmitting has not finished. This may be detected by a parity error or read-back verification error, but bus collision avoidance is not implemented.

**Which files are included?**

- LICENSE: GPL-v2.0 license for P1P2Serial library and the P1P2\* example programs,
- ReleaseNotes.md,
- SerialProtocol.md, describes serial protocol used by P1P2Monitor and P1P2-bridge-esp8266
- MqttProtocol.md, describes mqtt channels used by P1P2-bridge-esp8266
- P1P2Serial.cpp and P1P2Serial.h: (GPL-licensed) P1P2Serial library, based on AltSoftSerial (uses AltSoftSerial configuration files),
- examples/P1P2Monitor: (GPL-licensed) monitor program on Arduino, uses P1P2Serial library. Shows data on P1/P2 bus on serial output, and enables writing data from serial input. Reading/writing packets of data and CRC-byte generation/verification is supported. This program has no LCD support as of version 0.9.4, json/udp support (output via serial or udp) has been added as of version 0.9.6,
- examples/P1P2-bridge-esp8266: (GPL-licensed) program to convert P1P2Monitor's serial output to mqtt and json format, and preliminary support to write packets to the P1P2 bus from a mqtt channel,
- examples/P1P2Convert: (GPL-licensed) host-based (Linux Ubuntu / Raspberry Pi) program to (post-)process P1P2Monitor's serial output (directly from USB or from recorded logs), outputs parameters to stdout, making reverse-engineering protocol data easier,
- examples/P1P2HardwareTest: (GPL-licensed) program to stand P1P2Serial adapter shield in stand-alone mode,
- examples/P1P2LCD: (GPL-licensed) program to show P1P2 data on a an additional SPI 128x64 display connected directly to the Arduino Uno pins. This works for the Daikin hybrid model, it may work (partially) for other models. This program requires that the u8g2 library is installed,
- Daikin specific but product independent header file P1P2_Daikin_json.h supporting conversion to json format (this is included by the product-dependent header file(s)),
- Daikin specific but product dependent header file P1P2_Daikin_json_EHYHB.h supporting conversion to json format, this file is included by the P1P2json example program,
- doc/README.md and doc/Daikin-protocol\*: generic and product-dependent observations of protocol data for various heat pumps (work-in-progress, contributions welcome),
- circuits/\*: P1/P2 adapter schematic and pictures of adapter board,
- examples/P1P2Monitor/usb2console.py: simple python program to copy USB serial output from Arduino to stdout on a Raspberry Pi or other host (for logging and/or processing by P1P2-bridge-ubuntu),
- config/AltSoftSerial_Boards.h and AltSoftSerial_Timers.h: (MIT-licensed) AltSoftSerial configuration files, copied without modification from the AltSoftserial project, and
- README.md: this file.

**Where can I buy a MM1192 or XL1192?**

Only a few sellers on ebay and aliexpress are selling the MM1192 (both in DIP and SOIC package) and MM1007. There is one seller of the XL1192S (SOIC package) on aliexpress. The XL1192S can also be purchased from https://lcsc.com. The read circuit could also be re-built using a few opamps as demonstrated by https://github.com/LenShustek/M-NET-Sniffer.

**Do you offer pre-soldered or DIY adapter circuit kits?**

Yes, please contact me if you are interested, my e-mail is in the source code header. The adapter circuit is a 0.5" x 2" PCB for the Arduino Uno, and connects to GND, 5V, and to digital pins 8 (RX) and 9 (TX). The schematic is circuits/Daikin_P1P2_Uno_version2.pdf. It is a single-sided PCB with SMD components (0603/0805 components, MM1192/XL1192 SOP-16, SI8621 SOIC-8, and a Murata SMD-mounted 5V/5V converter). Images of the board can be found in the circuits directory.

**Acknowledgements**

Many thanks to the following persons:
- jarosolanic for adding parameter support for EHVX08S26CA9W and for adding MQTT topic output in P1P2Monitor
- a user called "donato35" for discovering that the P1/P2 bus is using HBS adapters (http://www.grix.it/forum/forum_thread.php?ftpage=2&id_forum=1&id_thread=519140?id_forum=1),
- a user called "Krakra" published a link to a description of the HBS protocol in the Echonet standard (https://community.openenergymonitor.org/t/hack-my-heat-pump-and-publish-data-onto-emoncms/2551/43) and shared thoughts and code on product-dependent header files,
- Luis Lamich Arocas for providing and analyzing his log files; thanks to his work v0.9.6 version we could add support for switching DHW on/off and cooling/heating on/off (currently only confirmed to work on EHVX08S23D6V and EHYHBX08AAV3),
- Bart Ellast for sharing his P1/P2 protocol data and analysis,
- designer2k2 for sharing his EWYQ005ADVP protocol data and analysis, and
- Paul Stoffregen for publishing the AltSoftSerial library, on which the P1P2Serial library is heavily based.
