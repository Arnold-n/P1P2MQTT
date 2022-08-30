# Source code packages for dependencies

To comply with the license conditions of the BSPs and library dependencies and to ensure that you can rebuild the firmware images using the same versions, this folder contains the BSPs and the libraries used for buidling the firmware images.

## BSPs

- for P1P2-bridge-esp8266: [ESP8266 BSP v3.0.2](Arduino_BSP/esp8266-3.0.2.zip) (LGPLv2.1 and various other licenses, [github repo](https://github.com/esp8266/Arduino))
- for P1P2Monitor: [MiniCore BSP v2.1.3 for ATmega328P](Arduino_BSP/MiniCore-2.1.3.tar.bz2) (LGPLv2.1, [github repo](https://github.com/MCUdude/MiniCore))

## Libraries

- for P1P2Monitor: none (the EEPROM library is part of the BSP)
- for P1P2-bridge-esp8266:
  - [WiFiManager 0.16.0 by tablatronix](libraries/WiFiManager-0.16.0.zip), installable via Arduino IDE (MIT license, [github repo with newer beta releases](https://github.com/tzapu/WiFiManager))
  - [ESP Telnet 1.3.1 by Lennart Hennigs version 1.3.1](libraries/ESPTelnet-1.3.1-07cc852ec610183db5f7aa1c94bda2ec7fe4d9c3.zip), installable via Arduino IDE (MIT license, [github repo newer versions]( https://github.com/LennartHennigs/ESPTelnet)
  - [async-mqtt-client 0.9.0 by Marvin Roger](libraries/async-mqtt-client-develop.zip) (MIT license, [github repo](https://github.com/marvinroger/async-mqtt-client))
  - [ESPAsyncTCP 2.0.1 by Phil Bowles](libraries/ESPAsyncTCP-master.zip) (LGPLv3, [github repo](https://github.com/philbowles/ESPAsyncTCP))
