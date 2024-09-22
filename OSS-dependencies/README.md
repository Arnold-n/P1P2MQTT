# Source code packages for dependencies

To comply with the license conditions of the BSPs and library dependencies and to ensure that you can rebuild the firmware images using the same BSP and library versions, this folder contains the BSPs and the libraries used for building the provided firmware images.

## BSPs

- for P1P2Monitor: unmodified [MiniCore BSP v2.1.3 for ATmega328P](Arduino_BSP/MiniCore-2.1.3.tar.bz2) (LGPLv2.1, [github repo](https://github.com/MCUdude/MiniCore))
- for P1P2-bridge-esp8266: modified version of [ESP8266 BSP v3.0.2](Arduino_BSP/esp8266-3.0.2-modified.zip) (LGPLv2.1 and various other licenses, [github repo](https://github.com/esp8266/Arduino))

#### Modifications explained

The modified BSP adds bit-banging SPI support for non-HSPI pins to the ESP8266AVRISP library. This is required for updating the ATmega328P firmware on the P1P2MQTT bridge v1.1 and later.

A local modified copy of the ESP8266HTTPUpdateServer library is used to provide an update mechanism for both the ESP8266 (finished) and for the ATmega328P (planned).

The w5500 ethernet driver hangs and (WDT-)crashes the ESP8266 if the w5500 hardware is not present, the modified BSP implements [this solution](https://github.com/esp8266/Arduino/issues/8498).

## Libraries

- for P1P2Monitor: none (the EEPROM library is part of the BSP)
- for P1P2-bridge-esp8266:
  - WiFiManager 0.16.0 by tablatronix, installable via Arduino IDE (MIT license, [github repo with newer beta releases](https://github.com/tzapu/WiFiManager))
  - ESP Telnet 2.0.0 by Lennart Hennigs, installable via Arduino IDE (MIT license, [github repo with newer versions](https://github.com/LennartHennigs/ESPTelnet))
  - [async-mqtt-client 0.9.0 by Marvin Roger](libraries/async-mqtt-client-develop.zip) (MIT license, download/unzip this in library folder, [github repo](https://github.com/marvinroger/async-mqtt-client))
  - [ESPAsyncTCP 2.0.1 by Phil Bowles with extra fix](libraries/ESPAsyncTCP-master-modified.zip) (LGPLv3, download/unzip this in library folder, [github repo](https://github.com/philbowles/ESPAsyncTCP))
- and for P1P2-bridge-esp8266 (W-SERIES only):
  - [ArduinoJson 6.11.3 by Benoit Blanchon](libraries/ArduinoJson-6.11.3.zip) (MIT license, download/unzip this in library folder, [github repo with newer version](https://github.com/bblanchon/ArduinoJson))

#### Modifications explained

The ESPAsyncTCP library has a memory leak (occurs only during mqtt dis-/reconnect), see [fix AsyncClient::connect memory leak #138](https://github.com/me-no-dev/ESPAsyncTCP/pull/138/files/6d98cc6eba40e3718e141e51139be8d95eb950d5)

You can use the unmodified ESPAsyncTCP library if you use ethernet or if you have a reliable WiFi signal.
