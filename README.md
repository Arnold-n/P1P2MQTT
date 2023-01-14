# Monitor data on your Mitsubishi X-Y Line protocol

## Warning

This software comes without warranty. Any use is entirely at your own risk (CC BY-NC-ND 4.0 Section 5 applies, for earlier versions GPL sections 11 and 12 apply).

**New design: P1P2-ESP-interface v1.1**

![P1P2-ESP-interface.png](circuits/P1P2-ESP-interface.png)

Shown is the new P1P2-ESP-interface v1.1 (more pictures [here](circuits/README.md)), a complete single-PCB bus-powered wireless P1P2-MQTT bridge. It is based on an ESP8266 (running P1P2-bridge-esp8266), an ATmega328P (running P1P2Monitor), and the MAX22088 HBS adapter (mounted on the lower side of the PCB). No Arduino or power supply is needed any more. Power is supplied by a DC/DC converter to minimize bus load (30-40mA at 15V). Connect it to P1/P2 near your main controller or near your Daikin system, enter your wifi and mqtt credentials via its built-in AP, and the system runs. Functionality:
- monitor and (for some models) control the Daikin heat pump via the P1/P2 bus from Home Assistant (automatic configuration, using MQTT discovery),
- communicate via MQTT over WiFi,
- accessible via telnet,
- OTA upgradable (both ESP and ATmega) (and if that would fail, using an ESP01-programmer via an ESP01-compatible connector),
- powered entirely by the P1/P2 bus, no external power supply is needed, low power consumption (only 30mA at 15V or 0.5W from the P1/P2 bus) thanks to a DC/DC converter,
- 4 LEDS for power (white), reading (green), writing (blue), or to signal an error (red),
- monitors P1/P2 DC bus voltage,
- screw terminals for P1 and P2 wires,
- SPI interface option for W5500 ethernet adapter (using an enclosure extension, power consumption with adapter 60mA at 15V or 0.9W from P1/P2 bus), and
- fits nicely in a small semi-transparant enclosure (50mm x 35mm x 20mm without ethernet, or 82mm x 35mm x 29mm with ethernet adapter).

This interface was designed for Daikin systems. On Mitsibushi Heavy Industries systems, it can monitor the traffic on the X-Y line. The software implements the conversion from 3-byte to 1-byte as described [here](https://community.openhab.org/t/mitsubishi-heavy-x-y-line-protocol/82898/8), many thanks to HamdiOlgun for reverse engineering the data encoding and checksum.

When the data in the packet is further reverse-engineered, the resulting information can easily be output over MQTT and integrated in Home Assistant.

In this stage, the interface only observes raw bus traffic. Writing packets to the bus is possible but use with care!

**How can you build or buy the required hardware?**

Buy new complete stand-alone P1P2-ESP-interface): I have a number of factory-assembled PCBs available (with ATmega328P and ESP12F, bus-powered, with enclosure, soldered, pre-programmed, optionally with ethernet, documented and tested, this is the 2nd version, shown above, v1.1, of October 2022). Please let me know if you are interested: my e-mail address can be found on line 3 of [P1P2Serial.cpp](https://github.com/Arnold-n/P1P2Serial/blob/main/P1P2Serial.cpp).

Buy P1P2-adapter (older design described below): I also sell the original MM1192/XL1192-based 0.5"x2" P1P2-adapter which is a HAT for the Arduino Uno.

Build P1P2-adapter yourself (MM1192/XL1192): schematics and pictures for the MM1192/XL1192-based P1P2-adapter (for use with the Arduino Uno) are [here](https://github.com/Arnold-n/P1P2Serial/tree/main/circuits). The MM1192 is available in traditional DIP format so you can build it on a breadboard. 

Build P1P2-adapter (MAX22088): Alternatively, you may build a circuit based on the newer MAX22088 IC from Maxim. Be warned that it is difficult to solder: it's only available as a 4x4mm 0.5mm pitch TQFN-24 package. The MAX22088 is powered directly from the P1/P2 bus (take care - we don't know how much power Daikin's P1/P2 bus may provide, perhaps max 60mA) and is able to power the Arduino Uno (max 70mA at Vcc=5V). PCB and schematic files for a [MAX22088-based design](https://github.com/rothn/P1P2Adapter) are made available by Nicholas Roth. His design does not provide galvanic isolation from the P1P2 bus, but that is OK if you connect only via WiFi or ethernet.


**Is it safe?**

There is always a risk when you connect to or write to the bus based on reverse engineering and assumptions that unexpected things happen or break. Reading without writing should be safe though. My system has been running continuously in controller mode (writing and reading) for 3 years now. Still, use is entirely at your own risk.

It is advised to connect/disconnect devices to the bus only if the power of all connected devices is switched off.

**Which other HBS interfaces are supported?**

This project was started for the Daikin P1/P2 bus. However the underlying electrical Japanese Home Bus System is used by many heat pump / air conditioning manufacturers. We have indications that, in addition to Daikin's P1/P2, the following buses are also based on the HBS format:
- Daikin F1/F2 (DIII-Net), 
- Mitsubishi M-NET (Len Shustek made an OPAMP-based reading circuit and documented his [protocol observations](https://github.com/LenShustek/M-NET-Sniffer)),
- Toshiba TCC-Link (used for indoor-outdoor, but not for the indoor-indoor and indoor-thermostaat AB-protocol (schematics and code [here](https://github.com/issalig/toshiba_air_cond) and [here](https://github.com/burrocargado/toshiba-aircon-mqtt-bridge) and [here](https://burro-hatenablog-com.translate.goog/entry/2022/07/18/230300?_x_tr_sl=auto&_x_tr_tl=en&_x_tr_hl=en&_x_tr_pto=wapp)),
- Hitachi H-link (code to read data from a Hitachi Yutaki S80 Combi heat pump is available [here](https://github.com/hankerspace/HLinkSniffer)).
- Panasonic/Sanyo SIII-Net, and
- perhaps also products from Haier and York.

At least raw data should be observable using the electronics and P1P2Monitor from this project, but the logical format will of course be different.

**Programs in this repo**

Other example programs translate the data packets to parameter format in json and/or mqtt format.

The P1P2Monitor program and the P1P2Serial library read and write raw data packets on Daikin P1/P2 two-wire interface, and similar (home bus system/Echonet) interfaces from other manufacturers, using an P1P2-ESP-interface or using an Arduino Uno and a P1P2Serial shield. Depending on your model there is support for parameter interpretation, via json and mqtt, and you can switch basic functions (DHW, heating/cooling) on/off on at least 3 specific models. WiFi/MQTT can be added using an additional ESP8266 running the P1P2MQTT program. The P1/P2 bus data is translated from/to a regular serial protocol.  Several P1P2Monitor operating parameters via the serial interface.

The P1P2MQTT program (running on an ESP8266) bridges between this serial interface and wireless MQTT or telnet.

If data (from MQTT or the serial line) is logged it can be analysed using P1P2Convert on any Linux system, enabling further reverse engineering of the data patterns if needed, in order to modify the P1P2MQTT code for models that are not (fully) supported yet.

**How is the software licensed?**

The software is licensed under the CC BY-NC-ND 4.0 with exceptions, see [LICENSE.md](https://www.github.com/Arnold-n/P1P2Serial/LICENSE.md). Please comply with the license conditions. If you improve or add, I would be interested to hear from you.

**Software requirements**

Version 0.9.17 uses Arduino IDE 1.8.19 and the following libraries:
- WiFiManager 0.16.0 by tzapu (installed using Arduino IDE)
- [AsyncMqttClient 0.9.0](https://github.com/marvinroger/async-mqtt-client) by Marvin Roger obtained with git clone or as ZIP file and extracted in Arduino's library directory
- [ESPAsyncTCP 2.0.1](https://github.com/philbowles/ESPAsyncTCP/) by Phil Bowles obtained with git clone or as ZIP file and extracted in Arduino's library directory
- ESP_Telnet 2.0.0 by Lennart Hennigs (installed using Arduino IDE)

For 8MHz ATmega328P support, install [MCUdude's MiniCore](https://github.com/MCUdude/MiniCore) by adding [board manager URL](https://mcudude.github.io/MiniCore/package_MCUdude_MiniCore_index.json) under File/Preferences/Additional board manager URLs.

For ESP8266 support, install the ESP8266 BSP by adding [board manager URL](http://arduino.esp8266.com/stable/package_esp8266com_index.json) under File/Preferences/Additional board manager URLs.

**Acknowledgements**

Many thanks to the following persons:
- Martin Dell for providing me with an RTD Modbus interface for further testing and for getting another system under control
- Robert Pot for diving into and solving the MQTT disconnect problem and library memory leak
- jarosolanic for adding parameter support for EHVX08S26CA9W and for adding initial MQTT topic output in P1P2Monitor (now moved to P1P2MQTT)
- a user called "donato35" for discovering that the [P1/P2 bus is using HBS adapters](http://www.grix.it/forum/forum_thread.php?ftpage=2&id_forum=1&id_thread=519140?id_forum=1),
- a user called "Krakra" published a link to a description of the HBS protocol in the [Echonet standard](https://community.openenergymonitor.org/t/hack-my-heat-pump-and-publish-data-onto-emoncms/2551/43) and shared thoughts and code on product-dependent header files,
- Luis Lamich Arocas for providing and analyzing his log files; thanks to his work we could add support for switching DHW on/off and cooling/heating on/off in version 0.9.6 (currently confirmed to work on several EH\* models),
- Bart Ellast for sharing his P1/P2 protocol data and analysis,
- designer2k2 for sharing his EWYQ005ADVP protocol data and analysis, and
- Nicholas Roth for testing and making available a MAX22088 schematic and PCB design, and providing his log data
- Paul Stoffregen for publishing the AltSoftSerial library, on which the initial P1P2Serial library is heavily based.
