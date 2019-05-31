# EHYHB(H/X)-AV3 and EHV(H/X)-CB protocol data format

Protocol data format for Daikin Altherma hybrid heat pump EHYHB(H/X)-AV3 and Daikin Altherma LT heat pump EHV(H/X)-CB

The following data packets were observed on these particular units:
EHYHBX08AAV3: air-to-water heat pump with gas condensing technology, functioning in weather-dependent LWT mode
EHVX08S26CB9W: air-to-water heat pump (with cooling), functioning in weather-dependent LWT mode (so far only "main package" packets were tested)


| Byte nr       | Hex value observed            |                |
|---------------|:------------------------------|:---------------|
|  1            | 00 or 40                      | source address: <br> 00 = request from thermostat<br>  40 = response from heat pump<br> 80 = thermostat in boot mode?
|  2            | 00 or F0                      | 00 = not the last packet in package<br> F0 = last packet in package
|  3            |  XX                           | packet type
|  4..(N-1)     |  XX                           | data
|  N            |  XX                           | CRC checksum


**Source**

Each regular data cycle consists of a package, where the thermostat and heat pump take turn as transmitter in a request-response pattern. Every approximately 770ms one package of 13 (alternating) packets is communicated.

If the user of the thermostat requests certain information in the menu items, such as energy consumed, additional packet types are inserted before such a package.

**Packet type**

Each packet has a packet type (and in certain cases a subtype) to identify the payload function. Packet type numbers observed so far are:
- 10..15 for regular data packets
- 30 for the last packet in a package
- 60..8F for field setting read/write operations (during operation and upon restart)
- 90..9F,A1,B1 for data communication upon restart; packets 90, 92-98, 9A-9C are empty/not used; packet 91, 99, 9D, 9E, A1 are TBD; packet 9F contains information similar to packet 20 (tbd), packet B1 contains the ASCII name of the heat pump ("EHYHBX08AAV3\0\0\0\0\0")
- B8 (+subtype) for incidental requests of consumption usage and count of operating hours
- A1/20 for incidental requests on status of motors and operation mode
- 21 for incidental requests on tbd

**Data**

The following data-types were observed:

| Data type     | Definition                     |
|---------------|:-------------------------------|
| flag8         | byte composed of 8 single-bit flags
| u8            | unsigned 8-bit integer 0 .. 255
| u16           | unsigned 16-bit integer 0..65535
| u24           | unsigned 24-bit integer 0..16777215
| f8.8          | signed fixed point value : 1 sign bit, 7 integer bit, 8 fractional bits (two’s compliment, see explanation below)
| f8/8          | Daikin-style fixed point value: 1st byte is value before comma, and 2nd byte is first digit after comma (see explanation below)
| s-abs4        | Daikin-style temperature deviation: bit 4 is sign bit, bits 0-3 is absolute value of deviation

Explanation of f8.8 format: a temperature of 21.5°C in f8.8 format is represented by the 2-byte value in 1/256th of a unit as 1580 hex (1580hex = 5504dec, dividing by 256 gives 21.5). A temperature of -5.25°C in f8.8 format is represented by the 2-byte value FAC0 hex (FAC0hex = - (10000hex-FACOhex) = - 0540hex = - 1344dec, dividing by 256 gives -5.25).

Explanation of f8/8 format: a temperature of 21.5°C in f8/8 format is represented by the byte value of 21 (0x15) followed by the byte value of 5 (0x05). So far this format was only detected for the setpoint temperature, which is usually not negative, so we don't know how negative numbers are stored in this format.

The following data-types have not yet been observed with certainty:

| Data type     | Definition                    |
|---------------|:------------------------------|
| s8            | signed 8-bit integer -128 .. 127 (two’s compliment)
| s16           | signed 16-bit integer -32768..32767


**CRC Checksum**

The CRC checksum is calculated using an 8-bit LFSR with a feed of 0x00 and a generator value of 0xD9.

**Timing between packets**

- approximately 25ms between communication thermostat and communication heat pump
- approximately 30-50ms (42/37/47/31/29/36) between communication heat pump and communication thermostat
- approximately 140 or 160 ms between last and first packet of package

This timing corresponds to the description found in a design guide from Daikin which specifies that a response must follow a request after 25ms, and a new request must not follow a response before another 25ms silence.

# Main package

#### 1. Packet "000010..."

| Byte nr       | Hex value observed            | Description           | Data type     | Bit: description |
|---------------|:------------------------------|:----------------------|:--------------|:-----------------|
|     1         | 00                            | Request               | u8
|     2         | 00                            | no end-of-package     | u8
|     3         | 10                            | packet type 10        | u8
|     4         | 00/01                         | Heating               | flag8 | 0: power (off/on)
|     5         | 01/81                         | Operating mode?       | flag8 | 7: gas?
|     6         | 00/01                         | DHW tank              | flag8 | 0: power (off/on)
|     7         | 00                            | ?                     |
|     8         | 00                            | ?                     |
|     9         | 00                            | ?                     |
|    10         | 00                            | ?                     |
|    11         | 14                            |Target room temperature| u8 / f8.8?
|    12         | 00                            | ?                     |
|    13         | 00/20/40/60                   | ?                     | flag8 | 5: ? <br> 6: ?
|    14         | 00/04                         | Quiet mode            | flag8 | 2: quiet mode (off/on)
|    15         | 00                            | ?                     |
|    16         | 08                            | ?                     |flag8 ?| 3: ?
|    17         | 00                            | ?                     |
|    18         | 00                            | ?                     |
|    19         | 0F                            | ?                     |flag8 ?| 0: ? <br> 1: ? <br> 2: ? <br> 3: ?
|    20         | 00                            | ?                     |
|    21         | 00/40/42                      | DHW tank mode         | flag8 | 1: booster (off/on) <br> 6: operation (off/on)
|    22         | 3C                            |DHW target temperature | u8 / f8.8?
|    23         | 00                            | fractional part?      |
|    24         | XX                            | CRC checksum          | u8

#### 2. Packet "400010..."

| Byte nr       | Hex value observed            | Description           | Data type     | Bit: description |
|---------------|:------------------------------|:----------------------|:--------------|:-----------------|
|     1         | 40                            | Response              | u8
|     2         | 00                            | no end-of-package     | u8
|     3         | 10                            | packet type 10        | u8
|     4         | 00/01                         | Heating               | flag8         | 0: power (off/on) |
|     5         | 00/80                         | ??                    | flag8         | 7: ? |
|     6         | 01/21/22/31/41/42/81/A1       | Valves                | flag8         | 0: heating (off/on) <br> 1: cooling (off/on) <br> 4: ? <br> 5: main zone (off/on) <br> 6: additional zone (off/on) <br> 7: DHW tank (off/on) |
|     7         | 00/01/11                      | 3-way valve           | flag8         | 0: status (off/on) <br> 4: status (SHC/tank) |
|     8         | 3C                            | DHW target temperature| u8 / f8.8?
|     9         | 00                            | fractional part?      |
|    10         | 0F                            | ??                    | flag8         | 0: ? <br> 1: ? <br> 2: ? <br> 3: ? |
|    11         | 00                            | ?
|    12         | 14                            |Target room temperature| u8 / f8.8?
|    13         | 00                            | ?
|    14         | 1A                            | ?
|    15         | 00/04                         | Quiet mode            | flag8         | 2: quiet mode (off/on) |
|    16-21      | 00                            | ?
|    22         | 00/02/08/09                   | Pump and compressor   | flag8         | 0: compressor (off/on) <br> 3: pump (off/on) |
|    23         | 00/02                         | DHW mode              | flag8         | 1: DHW mode (off/on)
|    24         | XX                            | CRC checksum          | u8

#### 3. Packet "000011..."

| Byte nr       | Hex value observed            | Description           | Data type     | Bit: description |
|---------------|:------------------------------|:----------------------|:--------------|:-----------------|
|     1         | 00                            | Request               | u8
|     2         | 00                            | no end-of-package     | u8
|     3         | 11                            | packet type 11        | u8
|     4-5       | XX YY                         |Actual room temperature| f8.8
|     6         | 00                            | ?
|     7         | 00                            | ?
|     8         | 00                            | ?
|     9         | 00                            | ?
|    10         | 00                            | ?
|    11         | 00                            | ?
|    12         | XX                            | CRC checksum          | u8

#### 4. Packet "400011..."

| Byte nr       | Hex value observed            | Description           | Data type     | Bit: description |
|---------------|:------------------------------|:----------------------|:--------------|:-----------------|
|     1         | 40                            | Response              | u8
|     2         | 00                            | no end-of-package     | u8
|     3         | 11                            | packet type 11        | u8
|   4-5         | XX YY                         | LWT temperature       | f8.8
|   6-7         | XX YY                         | DHW temperature       | f8.8
|   8-9         | XX YY                         | Outside temperature (in 0.5 degree resolution) | f8.8 |
|  10-11        | XX YY                         | RWT                   | f8.8 |
|  12-11        | XX YY                         | Mid-way temperature heat pump - gas boiler | f8.8 |
|  14-15        | XX YY                         | Refrigerant temperature | f8.8 |
|  16-17        | XX YY                         | Actual room temperature | f8.8 |
|  18-19        | XX YY                         | Outside temperature | f8.8 |
|  20-23        | 00                            | ? |
|    24         | XX                            | CRC checksum          | u8

#### 5. Packet "000012.."

| Byte nr       | Hex value observed            | Description           | Data type     | Bit: description |
|---------------|:------------------------------|:----------------------|:--------------|:-----------------|
|     1         | 00                            | Request               | u8
|     2         | 00                            | no end-of-package     | u8
|     3         | 12                            | packet type 12        | u8
|     4         | 00/02                         | new hour value        | flag8 | 1: new-hour or restart indicator
|     5         | 00-06                         | day of week (0=Monday, 6=Sunday) | u8
|     6         | 00-17                         | time - hours          | u8
|     7         | 00-3B                         | time - minutes        | u8
|     8         | 13 (example)                  | date - year (0x13 = 2019) | u8
|     9         | 01-0C                         | date - month          | u8
|    10         | 01-1F                         | date - day of month   | u8
|    11-15      | 00                            | ? | u8
|    16         | 00/01/41/61                   | upon restart: 1x 00; 1x 01; then 41; a single value of 61 triggers an immediate restart| flag8 | 1: ?<br>5: reboot<br> 6: ?
|    17         | 00/04                         | once 00, then 04      | flag 8 | 2: ?
|    18         | 00                            | ?
|    19         | XX                            | CRC checksum          | u8

#### 6. Packet "400012.."

| Byte nr       | Hex value observed            | Description           | Data type     | Bit: description |
|---------------|:------------------------------|:----------------------|:--------------|:-----------------|
|     1         | 40                            | Response              | u8
|     2         | 00                            | no end-of-package     | u8
|     3         | 12                            | packet type 12        | u8
|     4         | 40                            | ?? |
|     5         | 40                            | ?? |
|    6-13       | 00                            | ? |
|    14         | 00/10                         | Preference input(s) | flag8 | 4: preference kWh input
|    15         | 7F                            | first packet after restart 00; else 7F | u8
|    16         | 00/01/40/41/80/81/C1          | operating mode        | flag8 | 0: heat pump?<br>6: gas?<br> 7: DHW?
|    17         | 04                            | ?
|    18-23      | 00                            | ?
|    24         | XX                            | CRC checksum          | u8

#### 7. Packet "000013.."

| Byte nr       | Hex value observed            | Description           | Data type     | Bit: description |
|---------------|:------------------------------|:----------------------|:--------------|:-----------------|
|     1         | 00                            | Request               | u8
|     2         | 00                            | no end-of-package     | u8
|     3         | 13                            | packet type 13        | u8
|    4-5        | 00                            | ? |
|     6         | 00/D0                         | first package 0x00 instead of 0xD0 | flag8 ?
|     7         | XX                            | CRC checksum          | u8

#### 8. Packet "400013.."

| Byte nr       | Hex value observed            | Description           | Data type     | Bit: description |
|---------------|:------------------------------|:----------------------|:--------------|:-----------------|
|     1         | 40                            | Response              | u8
|     2         | 00                            | no end-of-package     | u8
|     3         | 13                            | packet type 13        | u8
|     4         | 3C                            | DHW target temperature (one packet delayed/from/via boiler?/ 0 in first packet after restart)| u8 / f8.8 ?
|     5         | 00                            | ? |
|     6         | 01                            | ?? |
|     7         | D0                            | ?? |
|   8-12        | 00                            | ? |
|    13         | 00-E0                         | flow (in 0.1 l/min) | u8
|    14-15      | xxxx                          | software version inner unit | u16
|    16-17      | xxxx                          | software version outer unit | u16
|     18        | XX                            | CRC checksum          | u8

#### 9. Packet "000014.."

| Byte nr       | Hex value observed            | Description           | Data type     | Bit: description |
|---------------|:------------------------------|:----------------------|:--------------|:-----------------|
|     1         | 00                            | Request               | u8
|     2         | 00                            | no end-of-package     | u8
|     3         | 14                            | packet type 14        | u8
|     4         | 2B                            | ??
|     5         | 00                            | ?
|     6         | 13                            | ??
|     7         | 00                            | ?
|     8         | 2D                            | ?? first package 0x37 instead of 0x2D (55 instead of 45) | u8
|     9         | 00                            | ?
|    10         | 07                            | ?? first package 0x37 instead of 0x07 | u8
|    11         | 00                            | ? |
|    12         | 00/01/02/11                   | delta-T | s-abs4
|    13         | 00?/02/05                     | ?? |
|   14-15       | 00                            | ? |
|   16          | 00/37                         | first package 0x37 instead of 0x00 |
|   17-18       | 00                            | ? |
|     19        | XX                            | CRC checksum          | u8

#### 10. Packet "400014.."

| Byte nr       | Hex value observed            | Description           | Data type     | Bit: description |
|---------------|:------------------------------|:----------------------|:--------------|:-----------------|
|     1         | 40                            | Response              | u8
|     2         | 00                            | no end-of-package     | u8
|     3         | 14                            | packet type 14        | u8
|     4         | 2B                            | ?? |
|     5         | 00                            | ? |
|     6         | 13                            | ?? |
|     7         | 00                            | ? |
|     8         | 2D                            | ?? |
|     9         | 00                            | ? |
|    10         | 07                            | ?? |
|    11         | 00                            | ? |
|    12         | 03/01/02/11                   | delta-T |
|    13         | 00/02/05                      | ?? |
|   14-18       | 00                            | ? |
|   19-20       | 1C-24 00-09                   | Target temperature LWT main zone in 0.1 degree (based on outside temperature in 0.5 degree resolution)| f8/8
|   21-22       | 1C-24 00-09                   | Target temperature LWT add zone  in 0.1 degree (based on outside temperature in 0.5 degree resolution)| f8/8
|     23        | XX                            | CRC checksum          | u8

#### 11. Packet "000015.."

| Byte nr       | Hex value observed            | Description           | Data type     | Bit: description |
|---------------|:------------------------------|:----------------------|:--------------|:-----------------|
|     1         | 00                            | Request               | u8
|     2         | 00                            | no end-of-package     | u8
|     3         | 15                            | packet type 15        | u8
|     4         | 00                            | ? |
|     5         | 01/09/0A                      | operating mode?
|     6         | F4/C4/D6/F0                   | operating mode?
|     7         | 00                            | ?
|     8         | 03                            | ??
|     9         | 20                            | ??
|     10        | XX                            | CRC checksum          | u8

#### 12. Packet "400015.."

| Byte nr       | Hex value observed            | Description           | Data type     | Bit: description |
|---------------|:------------------------------|:----------------------|:--------------|:-----------------|
|     1         | 40                            | Response              | u8
|     2         | 00                            | no end-of-package     | u8
|     3         | 15                            | packet type 15        | u8
|    4-5        | 00                            | ?
|    6-7        | FD-FF,00-08 00/80             | Refrigerant temperature in 0.5 degree resolution | f8.8
|    8-9        | 00                            | ?
|     10        | XX                            | CRC checksum          | u8

#### 13. Packet "00F030.."

| Byte nr       | Hex value observed            | Description           | Data type     | Bit: description |
|---------------|:------------------------------|:----------------------|:--------------|:-----------------|
|     1         | 00                            | Request               | u8
|     2         | F0                            | F0 = end-of-package   | u8
|     3         | 30                            | packet type 30        | u8
|     4-17      | 00-03                         | bytes form a repeating pattern of a "01" moving along several byte fields, over a cyclus of 241 packages. This could be another CRC code, or identity or authentication communication. On top of this there is additional variation between different packets in some of these bytes (observed in bytes 8 (+01) ,9 (-01/+01/+02),11 (+01). |
|     18        | XX                            | CRC checksum          | u8


# Other packets

## Packet types 60..8F for communicating field settings

A packet of type 60..8F communicates 5 field settings (using a payload of 20 bytes, or 4 bytes per field setting). This occurs after a restart (all field settings are sent by heat pump upon requests by the thermostat), or when a field setting is manually changed at the thermostat.
For some reason, 4 000086/000087 packets will be inserted at certain moments even though these do not communicate any usable field settings.

The packet type corresponds to the field settings according to the following table:

| Packet type   | Field settings |
|---------------|:---------------|
|     8C        | 00-00 .. 00-04 |
|     7C        | 00-05 .. 00-09 |
|     6C        | 00-0A .. 00-0E |
|     8D        | 01-00 .. 01-04 |
|     7D        | 01-05 .. 01-09 |
|     6D        | 01-0A .. 01-0E |
|     8E        | 02-00 .. 02-04 |
|     7E        | 02-05 .. 02-09 |
|     6E        | 02-0A .. 02-0E |
|     8F        | 03-00 .. 03-04 |
|     7F        | 03-05 .. 03-09 |
|     6F        | 03-0A .. 03-0E |
|     80        | 04-00 .. 04-04 |
|     70        | 04-05 .. 04-09 |
|     60        | 04-0A .. 04-0E |
|     ..        | ..             |
|     8B        | 0F-00 .. 0F-04 |
|     7B        | 0F-05 .. 0F-09 |
|     6B        | 0F-0A .. 0F-0E |


#### Packet "0000XX.." Field setting information request by thermostat upon restart

| Byte nr       | Hex value observed            | Description           | Data type     | Bit: description |
|---------------|:------------------------------|:----------------------|:--------------|:-----------------|
|     1         | 00                            | Request                 | u8
|     2         | 00                            | no end-of-package | u8
|     3         | XX                            | packet type (60..8F) | u8
|  4-23         | 00                            | All fields empty
|    24         | XX                            | CRC checksum          | u8

#### Packet "4000XX.." Field setting reply by heat pump during restart process

For supported field settings:

| Byte nr       | Hex value observed            | Description           | Data type     | Bit: description |
|---------------|:------------------------------|:----------------------|:--------------|:-----------------|
|     1         | 40                            | Response                | u8
|     2         | 00                            | no end-of-package | u8
|     3         | XX                            | packet type (60..62,64-65,68-6E,70-75,78-85,88-8F) | u8
|  4            | XX                            | 1st field value (usually 0xC0 is added to this value)  | u8
|  5            | XX                            | maximum field value  | u8
|  6            | XX                            | offset for field value  | u8
|  7            | 00/08/0A/28/2A/8A/92/AA       | field setting info      | flag8        | 3: in use<br>7: comma used<br>1,5: step size(00/02/20/22=step size 1/0.1/5/0.5)<br>4: read-only with comma?
|  8-11         | 00                            | 2nd field value, see bytes 4-7           | u8,u8,u8,flag8
| 12-15         | 00                            | 3rd field value, see bytes 4-7           | u8,u8,u8,flag8
| 16-19         | 00                            | 4th field value, see bytes 4-7           | u8,u8,u8,flag8
| 20-23         | 00                            | 5th field value, see bytes 4-7           | u8,u8,u8,flag8
|    24         | XX                            | CRC checksum          | u8

For non-supported field settings:

| Byte nr       | Hex value observed            | Description           | Data type     | Bit: description |
|---------------|:------------------------------|:----------------------|:--------------|:-----------------|
|     1         | 40                            | Response                | u8
|     2         | 00                            | no end-of-package | u8
|     3         | FF                            | FF as packet type answer if request is for packet type (63,66-67,6F,76-77,86,87) | u8
|     4         | XX                            | CRC checksum          | u8

#### Packet "0000XX.." Field setting change by thermostat during operation

| Byte nr       | Hex value observed            | Description           | Data type     | Bit: description |
|---------------|:------------------------------|:----------------------|:--------------|:-----------------|
|     1         | 00                            | Request                 | u8
|     2         | 00                            | no end-of-package | u8
|     3         | XX                            | packet type (60..8F) | u8
|  4            | XX                            | 1st field, old value (no 0xC0 added), or new value (0x40 added) | u8
|  5            | XX                            | maximum field value  | u8
|  6            | XX                            | offset for field value  | u8
|  7            | 00/08/0A/28/2A/8A/92/AA       | field setting info  | flag8 | see above
|  8-11         | 00                            | 2nd field value, see bytes 4-7           | u8,u8,u8,flag8
| 12-15         | 00                            | 3rd field value, see bytes 4-7           | u8,u8,u8,flag8
| 16-19         | 00                            | 4th field value, see bytes 4-7           | u8,u8,u8,flag8
| 20-23         | 00                            | 5th field value, see bytes 4-7           | u8,u8,u8,flag8
|  4-23         | 00                            | All fields empty |
|    24         | XX                            | CRC checksum          | u8

#### Packet "4000XX.." Field setting reply by heat pump during restart process

Format is the same for supported and non-supported field settings

| Byte nr       | Hex value observed            | Description           | Data type     | Bit: description |
|---------------|:------------------------------|:----------------------|:--------------|:-----------------|
|     1         | 40                            | Response                | u8
|     2         | 00                            | no end-of-package | u8
|     3         | XX                            | packet type (60..8F) | u8
|  4            | XX                            | 1st field value (usually 0xC0 is added to this value)  | u8
|  5            | XX                            | maximum field value  | u8
|  6            | XX                            | offset for field value  | u8
|  7            | 00/08/0A/28/2A/8A/92/AA       | field setting info  | flag8 | see above
|  8-11         | 00                            | 2nd field value, see bytes 4-7           | u8,u8,u8,flag8
| 12-15         | 00                            | 3rd field value, see bytes 4-7           | u8,u8,u8,flag8
| 16-19         | 00                            | 4th field value, see bytes 4-7           | u8,u8,u8,flag8
| 20-23         | 00                            | 5th field value, see bytes 4-7           | u8,u8,u8,flag8
|    24         | XX                            | CRC checksum          | u8

## A1/20 for motors and operation mode

#### 001. Packet "0000A100.."

| Byte nr       | Hex value observed            | Description           | Data type     | Bit: description |
|---------------|:------------------------------|:----------------------|:--------------|:-----------------|
|     1         | 00                            | Request                 | u8
|     2         | 00                            | no end-of-package | u8
|     3         | A1                            | packet type A1 | u8
|  4-21         | 00                            | ?
|    22         | XX                            | CRC checksum          | u8

#### 002. Packet "4000A100.."

| Byte nr       | Hex value observed            | Description           | Data type     | Bit: description |
|---------------|:------------------------------|:----------------------|:--------------|:-----------------|
|     1         | 40                            | Response               |
|     2         | 00                            | no end-of-package |
|     3         | A1                            | packet type A1 |
|  4-21         | 00                            | ? |
|    22         | XX                            | CRC checksum          | u8
length 22

#### 003. Packet "00002000.."

| Byte nr       | Hex value observed            | Description           | Data type     | Bit: description |
|---------------|:------------------------------|:----------------------|:--------------|:-----------------|
|     1         | 00                            | Request                 | u8
|     2         | 00                            | no end-of-package | u8
|     3         | 20                            | packet type 20 | u8
|     4         | 00                            | ?
|     5         | XX                            | CRC checksum          | u8

#### 004. Packet "40002000.."

| Byte nr       | Hex value observed            | Description           | Data type     | Bit: description |
|---------------|:------------------------------|:----------------------|:--------------|:-----------------|
|     1         | 40                            | Response               | u8
|     2         | 00                            | no end-of-package | u8
|     3         | 20                            | packet type 20 | u8
|    4-23       | XX                            | various values??, tbd |
|    24         | XX                            | CRC checksum | u8

## B8 for request of energy consumed and count of operating hours

#### 001. Packet "0000B800.."

| Byte nr       | Hex value observed            | Description           | Data type     | Bit: description |
|---------------|:------------------------------|:----------------------|:--------------|:-----------------|
|     1         | 00                            | Request                 | u8
|     2         | 00                            | no end-of-package | u8
|     3         | B8                            | packet type B8 | u8
|     4         | 00                            | packet subtype 00 | u8
|     5         | XX                            | CRC checksum | u8

#### 002. Packet "4000B800.."

| Byte nr       | Hex value observed            | Description           | Data type     | Bit: description |
|---------------|:------------------------------|:----------------------|:--------------|:-----------------|
|     1         | 40                            | Response                | u8
|     2         | 00                            | no end-of-package | u8
|     3         | B8                            | packet type B8 | u8
|     4         | 00                            | packet subtype 00 | u8
|  5-10         | 00                            | two 3-byte numbers? | u24
| 11-13         | 00 XX XX                      | ??   | u24
| 14-19         | 00                            | two 3-byte numbers? | u24
| 20-22         | 00 XX XX                      | electric energy consumed | u24
|    23         | XX                            | CRC checksum | u8


#### 003. Packet "0000B801.."

| Byte nr       | Hex value observed            | Description           | Data type     | Bit: description |
|---------------|:------------------------------|:----------------------|:--------------|:-----------------|
|     1         | 00                            | Request                 | u8
|     2         | 00                            | no end-of-package | u8
|     3         | B8                            | packet type B8 | u8
|     4         | 01                            | packet subtype 00 | u8
|     5         | XX                            | CRC checksum | u8

#### 004. Packet "4000B801.."

| Byte nr       | Hex value observed            | Description           | Data type     | Bit: description |
|---------------|:------------------------------|:----------------------|:--------------|:-----------------|
|     1         | 40                            | Response                | u8
|     2         | 00                            | no end-of-package | u8
|     3         | B8                            | packet type B8 | u8
|     4         | 01                            | packet subtype 01 | u8
|  5-7          | XX XX XX                      | heat produced | u24
|  8-13         | 00                            | 4 3-byte numbers?   | u24,u24,u24,u24
| 14-16         | XX XX XX                      | heat produced (duplicate?)| u24
|    17         | XX                            | CRC checksum | u8


#### 005. Packet "0000B802.."

| Byte nr       | Hex value observed            | Description           | Data type     | Bit: description |
|---------------|:------------------------------|:----------------------|:--------------|:-----------------|
|     1         | 00                            | Request                 | u8
|     2         | 00                            | no end-of-package | u8
|     3         | B8                            | packet type B8 | u8
|     4         | 02                            | packet subtype 02 | u8
|     5         | XX                            | CRC checksum | u8

#### 006. Packet "4000B802.."

| Byte nr       | Hex value observed            | Description           | Data type     | Bit: description |
|---------------|:------------------------------|:----------------------|:--------------|:-----------------|
|     1         | 40                            | Response                | u8
|     2         | 00                            | no end-of-package | u8
|     3         | B8                            | packet type B8 | u8
|     4         | 02                            | packet subtype 02 | u8
|  5-7          | XX XX XX                      | number of pump hours | u24
|  8-10         | XX XX XX                      | number of compressor hours | u24
| 11-16         | XX XX XX                      | 2 3-byte numbers? (cooling hours?) | u24,u24
|    17         | XX                            | CRC checksum | u8


#### 007. Packet "0000B803.."

| Byte nr       | Hex value observed            | Description           | Data type     | Bit: description |
|---------------|:------------------------------|:----------------------|:--------------|:-----------------|
|     1         | 00                            | Request                 | u8
|     2         | 00                            | no end-of-package | u8
|     3         | B8                            | packet type B8 | u8
|     4         | 03                            | packet subtype 03 | u8
|     5         | XX                            | CRC checksum | u8

#### 008. Packet "4000B803.."

| Byte nr       | Hex value observed            | Description           | Data type     | Bit: description |
|---------------|:------------------------------|:----------------------|:--------------|:-----------------|
|     1         | 40                            | Response                | u8
|     2         | 00                            | no end-of-package | u8
|     3         | B8                            | packet type B8 | u8
|     4         | 03                            | packet subtype 03 | u8
|  5-22         | 00                            | ? | u8
|    23         | XX                            | CRC checksum | u8


#### 009. Packet "0000B804.."

| Byte nr       | Hex value observed            | Description           | Data type     | Bit: description |
|---------------|:------------------------------|:----------------------|:--------------|:-----------------|
|     1         | 00                            | Request                 | u8
|     2         | 00                            | no end-of-package | u8
|     3         | B8                            | packet type B8 | u8
|     4         | 04                            | packet subtype 04 | u8
|     5         | XX                            | CRC checksum | u8

#### 010. Packet "4000B804.."

| Byte nr       | Hex value observed            | Description           | Data type     | Bit: description |
|---------------|:------------------------------|:----------------------|:--------------|:-----------------|
|     1         | 40                            | Response                | u8
|     2         | 00                            | no end-of-package | u8
|     3         | B8                            | packet type B8 | u8
|     4         | 04                            | packet subtype 04 | u8
|  5-13         | 00                            | ? | u8
| 14-16         | XX XX XX                      | number of start attempts | u24
|    17         | XX                            | CRC checksum | u8


#### 011. Packet "0000B805.."

| Byte nr       | Hex value observed            | Description           | Data type     | Bit: description |
|---------------|:------------------------------|:----------------------|:--------------|:-----------------|
|     1         | 00                            | Request                 | u8
|     2         | 00                            | no end-of-package | u8
|     3         | B8                            | packet type B8 | u8
|     4         | 05                            | packet subtype 05 | u8
|     5         | XX                            | CRC checksum | u8

#### 012. Packet "4000B805.."

| Byte nr       | Hex value observed            | Description           | Data type     | Bit: description |
|---------------|:------------------------------|:----------------------|:--------------|:-----------------|
|     1         | 40                            | Response                | u8
|     2         | 00                            | no end-of-package | u8
|     3         | B8                            | packet type B8 | u8
|     4         | 05                            | packet subtype 05 | u8
|  5-7          | XX XX XX                      | number of boiler hours heating | u24
|  8-10         | XX XX XX                      | number of boiler hours DHW | u24
| 11-16         | 00 00 00                      | 2 3-byte numbers? | u24,u24
| 17-19         | XX XX XX                      | 1 3-byte number (tbd)? | u24
| 20-22         | 00 00 00                      | 1 3-byte number? | u24
|    23         | XX                            | CRC checksum | u8


## 21 for tbd

#### Tbd

## Reboot process

The reboot process is initiated by setting byte 16 of packet "000012.." to 0x61. The heat pump stops answering requests almost immediately.

The reboot process consists of several steps, including lots of data communication, still to be described.
