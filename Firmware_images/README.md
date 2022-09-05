# Firmware images for P1P2-ESP-Interface

## P1P2Monitor firmware for ATmega328 on P1P2-ESP-Interface

(does not work on Arduino Uno)

P1P2Monitor auxiliary control is currently only usable for Daikin E-series. Monitoring is also available for certain F-series models.

[P1P2Monitor v0.9.18](P1P2Monitor-0.9.18.ino.hex)

To install (Linux CLI):

```
avrdude -c avrisp -p atmega328p  -P net:<IPv4>:328 -e -Uflash:w:P1P2Monitor-0.9.18.ino.hex:i
```

## P1P2-bridge-esp8266 firmware for 4MB ESP8266 on P1P2-ESP-Interface

[P1P2-bridge-esp8266 0.9.20 (4MB version) for Daikin E-series](P1P2-bridge-esp8266-0.9.20-Eseries-4MB.ino.bin)

[P1P2-bridge-esp8266 0.9.20 (4MB version) for Daikin F-series](P1P2-bridge-esp8266-0.9.20-Fseries-4MB.ino.bin)

To install OTA (Linux CLI):

```
~/.arduino15/packages/esp8266/tools/python3/3.7.2-post1/python3 -I ~/.arduino15/packages/esp8266/hardware/esp8266/3.0.2/tools/espota.py -i <IPv4> -p 8266 --auth=P1P2MQTT -f P1P2-bridge-esp8266-0.9.20-Eseries-4MB.ino.bin
```
or
```
~/.arduino15/packages/esp8266/tools/python3/3.7.2-post1/python3 -I ~/.arduino15/packages/esp8266/hardware/esp8266/3.0.2/tools/espota.py -i <IPv4> -p 8266 --auth=P1P2MQTT -f P1P2-bridge-esp8266-0.9.20-Fseries-4MB.ino.bin
```

To install over USB with ESP01 programmer (Linux CLI):

```
~/.arduino15/packages/esp8266/tools/python3/3.7.2-post1/python3 -I ~/.arduino15/packages/esp8266/hardware/esp8266/3.0.2/tools/upload.py --chip esp8266 --port /dev/ttyUSB0 --baud 115200 --before default_reset --after hard_reset write_flash 0x0 P1P2-bridge-esp8266-0.9.20-Eseries-4MB.ino.bin
```
or
```
~/.arduino15/packages/esp8266/tools/python3/3.7.2-post1/python3 -I ~/.arduino15/packages/esp8266/hardware/esp8266/3.0.2/tools/upload.py --chip esp8266 --port /dev/ttyUSB0 --baud 115200 --before default_reset --after hard_reset write_flash 0x0 P1P2-bridge-esp8266-0.9.20-Eseries-4MB.ino.bin
```
## Firmware image for separate ESP01s

[P1P2-bridge-esp8266 0.9.20 (1MB version, 250kBaud) for Daikin E-series](P1P2-bridge-esp8266-Eseries-0.9.20-Eseries-1MB.ino.bin)

[P1P2-bridge-esp8266 0.9.20 (1MB version, 250kBaud) for Daikin F-series](P1P2-bridge-esp8266-Fseries-0.9.20-Eseries-1MB.ino.bin)

To install over USB with ESP01 programmer (Linux CLI):

```
~/.arduino15/packages/esp8266/tools/python3/3.7.2-post1/python3 -I ~/.arduino15/packages/esp8266/hardware/esp8266/3.0.2/tools/upload.py --chip esp8266 --port /dev/ttyUSB0 --baud 115200 --before default_reset --after hard_reset write_flash 0x0 P1P2-bridge-esp8266-0.9.20-Eseries-1MB.ino.bin
```
or
```
~/.arduino15/packages/esp8266/tools/python3/3.7.2-post1/python3 -I ~/.arduino15/packages/esp8266/hardware/esp8266/3.0.2/tools/upload.py --chip esp8266 --port /dev/ttyUSB0 --baud 115200 --before default_reset --after hard_reset write_flash 0x0 P1P2-bridge-esp8266-0.9.20-Fseries-1MB.ino.bin
```

## FOSS notice

These images are built with Arduino BSPs and libraries, for which source code and license information is made available [here](../OSS-dependencies/README.md).
