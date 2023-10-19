# Firmware images for P1P2-ESP-Interface

The P1P2MQTT bridge (previously P1P2-ESP-interface) has 2 CPUs:
 - ATmega328P running P1P2Monitor
 - ESP8266 running P1P2-bridge-esp8266 (software will later be renamed by P1P2MQTT)

A full upgrade involves flashing both CPUs.

## P1P2Monitor firmware for ATmega328 on P1P2-ESP-Interface

These images are for the P1P2-ESP-interface (P1P2MQTT bridge) v1.0, v1.1 and v1.2. They do not work on the Arduino Uno.

P1P2Monitor auxiliary control is currently only usable for Daikin E-series, and FDY/FDYQ/FXMQ F-series. Monitoring is also available for certain F-series models (using the generic F-series firmware).

[P1P2Monitor v0.9.42 for Daikin E-series](P1P2Monitor-v0.9.42-Eseries-P1P2MQTT-bridge.ino.hex)

[P1P2Monitor v0.9.42 for Daikin F-series (no control)](P1P2Monitor-v0.9.42-Fseries-P1P2MQTT-bridge.ino.hex)

[P1P2Monitor v0.9.42 (only for Daikin FDY)](P1P2Monitor-v0.9.42-FDY-P1P2MQTT-bridge.ino.hex)

[P1P2Monitor v0.9.42 (only for Daikin FDYQ)](P1P2Monitor-v0.9.42-FDYQ-P1P2MQTT-bridge.ino.hex)

[P1P2Monitor v0.9.42 (only for Daikin FXMQ and some other daikin F-series models)](P1P2Monitor-v0.9.42-FXMQ-P1P2MQTT-bridge.ino.hex)

[P1P2Monitor v0.9.42 (experimental for Hitachi models)](P1P2Monitor-v0.9.42-Hseries-P1P2MQTT-bridge.ino.hex)

[P1P2Monitor v0.9.42 (experimental for Toshiba models)](P1P2Monitor-v0.9.42-Tseries-P1P2MQTT-bridge.ino.hex)

To flash this image OTA (Linux CLI):

```
avrdude -c avrisp -p atmega328p  -P net:<IPv4>:328 -e -Uflash:w:<hex-file>:i
```

If you are running P1P2-bridge-esp8266 v0.9.42 or later and are on a local network segment, you may use P1P2.local instead of &lt;IPv4>.

To flash this image OTA (Windows CLI, using avrdude 7.0 for Windows):

```
avrdude.exe -c avrisp -p m328p  -P net:<IPv4>:328 -e -Uflash:w:<hex-file>:i
```

If you have trouble installing avrdude, you may prefer to wait for a later P1P2-bridge-esp8266 version which will support P1P2Monitor updates via a webserver running on the ESP.


## P1P2-bridge-esp8266 firmware for ESP8266 on P1P2-ESP-Interface

[P1P2-bridge-esp8266 v0.9.43 (P1P2-ESP-Interface version) for Daikin E-series](P1P2-bridge-esp8266-v0.9.43-Eseries-P1P2MQTT-bridge.ino.bin)

[P1P2-bridge-esp8266 v0.9.42 (P1P2-ESP-Interface version) for Daikin F-series](P1P2-bridge-esp8266-v0.9.42-Fseries-P1P2MQTT-bridge.ino.bin)

[P1P2-bridge-esp8266 v0.9.42 (P1P2-ESP-Interface version) for Hitachi models (experimental)](P1P2-bridge-esp8266-v0.9.42-Hseries-P1P2MQTT-bridge.ino.bin)

[P1P2-bridge-esp8266 v0.9.42 (P1P2-ESP-Interface version) for Toshiba models (experimental)](P1P2-bridge-esp8266-v0.9.42-Tseries-P1P2MQTT-bridge.ino.bin)

Note to users of the older v1.0 version of the P1P2-ESP-interface: please do not erase the flash area of the ESP8266 when flashing these images; if you do, you will need to reset the hardware identifier back to 0 using the 'B' command (or you will not be able to flash the ATmega328P).

To flash this image OTA (browser):

As of v0.9.41: browse to the webserver on the ESP: http://&lt;IPv4>/update (note: https not supported) and upload and install the new firmware image.

As of v0.9.42: browse to the webserver on the ESP: http://P1P2.local/update (note: https not supported) and upload and install the new firmware image.

To flash this image OTA (Linux CLI):

```
~/.arduino15/packages/esp8266/tools/python3/3.7.2-post1/python3 -I ~/.arduino15/packages/esp8266/hardware/esp8266/3.0.2/tools/espota.py -i <IPv4> -p 8266 --auth=P1P2MQTT -f <bin-file>
```

If you bricked the ESP, you can still flash a new image with the ESP01 USB programmer (Linux CLI):

```
~/.arduino15/packages/esp8266/tools/python3/3.7.2-post1/python3 -I ~/.arduino15/packages/esp8266/hardware/esp8266/3.0.2/tools/upload.py --chip esp8266 --port /dev/ttyUSB0 --baud 115200 --before default_reset --after hard_reset write_flash 0x0 <bin-file>
```

To flash OTA from Windows with espota.py from [here](https://github.com/esp8266/Arduino.git):

```
yourpath\Arduino-master\tools\espota.py -i <IPv4> -p 8266 --auth=P1P2MQTT -f <bin-file>
```

```
yourpath\Arduino-master\tools\espota.py -i <IPv4> -p 8266 --auth=P1P2MQTT -f <bin-file>
```

## FOSS notice

These images are built with Arduino BSPs and libraries, for which source code and license information is made available [here](../OSS-dependencies/README.md).
