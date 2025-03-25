# Firmware images for P1P2MQTT bridge

The P1P2MQTT bridge (previously P1P2-ESP-interface) has 2 CPUs:
 - ATmega328P running P1P2Monitor
 - ESP8266 running P1P2MQTT-bridge (previously P1P2-bridge-esp8266)

A full upgrade involves flashing both CPUs.

## P1P2Monitor firmware for ATmega328

These images are for the P1P2MQTT bridge (previously called P1P2-ESP-interface). These images do not work on the Arduino Uno.

Minimal recommended upgrade is v0.9.58 for P1P2Monitor.

P1P2Monitor v0.9.53 does not yet add buffered commands, but a future version will.

P1P2Monitor auxiliary control is currently only usable for Daikin E-series, and FDY/FDYQ/FXMQ F-series. Monitoring is also available for certain F-series models (using the generic F-series firmware).

[P1P2Monitor v0.9.58 for Daikin E-series](P1P2Monitor-v0.9.58-Daikin-E.ino.hex)

[P1P2Monitor v0.9.58 for Daikin F-series (model selectable with M command or via HA)](P1P2Monitor-v0.9.58rc4-Daikin-F.ino.hex)

[P1P2Monitor v0.9.58 (experimental for Hitachi models)](P1P2Monitor-v0.9.58-Hitachi.ino.hex)

[P1P2Monitor v0.9.58 (experimental for Mitsubishi Heavy Industries (MHI) models)](P1P2Monitor-v0.9.58-MHI.ino.hex)

[P1P2Monitor v0.9.58 (experimental for Toshiba models)](P1P2Monitor-v0.9.58-Toshiba.ino.hex)

For monitoring F1/F2 traffic, use [P1P2Monitor v0.9.58 for Daikin F1/F2 monitoring)](P1P2Monitor-v0.9.58-Daikin-F1F2.ino.hex).

To flash this image OTA (Linux CLI):

```
avrdude -c avrisp -p atmega328p  -P net:<IPv4>:328 -e -Uflash:w:<hex-file>:i
```

If you are running P1P2-bridge-esp8266 v0.9.42 or later and are on a local network segment, you may use P1P2MQTT\_bridge0.local instead of &lt;IPv4>.

To flash this image OTA (Windows CLI, using avrdude 7.0 for Windows):

```
avrdude.exe -c avrisp -p m328p  -P net:<IPv4>:328 -e -Uflash:w:<hex-file>:i
```

If you have trouble installing avrdude, you may prefer to wait for a later P1P2-bridge-esp8266 version which will support P1P2Monitor updates via a webserver running on the ESP.


## P1P2MQTT-bridge firmware for ESP8266

Minimal recommended upgrade is v0.9.57 for P1P2MQTT-bridge (or v0.9.58rc5 for F-series).

[P1P2MQTT-bridge v0.9.57 for Daikin E-series](P1P2MQTT-bridge-v0.9.57-Daikin-E.ino.bin)

[experimental version P1P2MQTT-bridge v0.9.58rc5 for Daikin E-series](P1P2MQTT-bridge-v0.9.58rc5-Daikin-E.ino.bin)

[P1P2MQTT-bridge v0.9.58rc5 for Daikin F-series](P1P2MQTT-bridge-v0.9.58rc15-Daikin-F.ino.bin)

[P1P2MQTT-bridge v0.9.57 for Hitachi models (experimental)](P1P2MQTT-bridge-v0.9.57-Hitachi.ino.bin)

[P1P2MQTT-bridge v0.9.57 for Mitsubishi Heavy Industries (MHI) models (experimental)](P1P2MQTT-bridge-v0.9.57-MHI.ino.bin)

(for Toshiba, use Hitachi code for now).

Stable version is currently v0.9.55.

For monitoring Daikin F1/F2 traffic, use [P1P2MQTT-bridge v0.9.57 for Daikin F1/F2 monitoring](P1P2MQTT-bridge-v0.9.57-Daikin-F1F2.ino.bin).

Note to users of the original v1.0 version: please do not erase the flash memory of the ESP8266 when flashing these images; if you do, you will need to reset the hardware identifier back to 0 using the 'P31 0' command (or you will not be able to flash the ATmega328P).

To flash this image OTA (browser):

As of v0.9.41: browse to the webserver on the ESP: http://&lt;IPv4>/update (note: https not supported) and upload and install the new firmware image.

As of v0.9.42: browse to the webserver on the ESP: http://P1P2MQTT_bridge0.local/update (note: https not supported) and upload and install the new firmware image.

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

## P1P2MQTT-HomeWizard

To provide electricity meter data from an HomeWizards WiFi-enabled MID meter, via MQTT, to the P1P2MQTT bridge, a separate ESP8266 is required. Any ESP8266 with 1MB memory will do, like an ESP01S (using the USB programmer for power), or a WeMos module.

[P1P2MQTT-HomeWizard-kWh-bridge-v0.9.55.ino.bin](P1P2MQTT-HomeWizard-kWh-bridge-v0.9.55.ino.bin)

## FOSS notice

These images are built with Arduino BSPs and libraries, for which source code and license information is made available [here](../OSS-dependencies/README.md).
