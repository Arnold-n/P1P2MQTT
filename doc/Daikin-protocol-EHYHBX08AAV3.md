# EHYHB(H/X)-AV3 and EHV(H/X)-CB protocol data format

Protocol data format for Daikin Altherma hybrid heat pump for the EHYHBX08AAV3 (perhaps all EHYHB(H/X)-AV3 models) and (at least partially for the) Daikin Altherma LT heat pump EHV(H/X)-CB

Please read the README.md first.

The following data packets were observed on these particular units:
EHYHBX08AAV3: air-to-water heat pump with gas condensing technology, functioning in weather-dependent LWT mode
EHVX08S26CB9W: air-to-water heat pump (with cooling), functioning in weather-dependent LWT mode (so far only "main package" packets were tested)

**Cycle**

Each regular data cycle consists of a package, where the thermostat and heat pump take turn as transmitter in a request-response pattern. Every approximately 770ms one package of 12 (alternating) packets is communicated this way, followed by a 13th packet which requests data from an external controller - which is not present in this sytem so there is no reply, and the cycle restarts after a 140/160ms time-out period. If there is a reply to the 13th packet, additional request/response pairs are being communicated.

If the user of the thermostat requests certain information in the menu items, such as energy consumed, additional packet types are inserted before such a package.

**Packet type**

Each packet has a packet type (and in certain cases a subtype) to identify the payload function. Packet type numbers observed so far are:
- 10..15 for regular data packets between main controller and heat pump
- 30..39 for communication between the main controller and external controllers (not documented yet, but used in v0.9.6 to switch DHW and cooling/heating on/off)
- 60..8F for field setting read/write operations (during operation and upon restart)
- 90..9F,A1,B1 for data communication upon restart; packets 90, 92-98, 9A-9C are empty/not used; packet 91, 99, 9D, 9E, A1 are TBD; packet 9F contains information similar to packet 20 (tbd), packet B1 contains the ASCII name of the heat pump ("EHYHBX08AAV3\0\0\0\0\0")
- B8 (+subtype) for incidental requests of consumption usage and count of operating hours
- A1/20 for incidental requests on status of motors and operation mode
- 21 for incidental requests on tbd

**Timing between packets**

- approximately 25ms between communication thermostat and communication heat pump
- approximately 30-50ms (42/37/47/31/29/36) between communication heat pump and communication thermostat
- approximately 140 or 160 ms between last packet of a package and the first packet of a new package

This timing corresponds to the description found in a design guide from Daikin which specifies that a response must follow a request after 25ms, and a new request must not follow a response before another 25ms silence.

# Main package

#### 1. Packet "000010..."

| Byte nr       | Hex value observed            | Description           | Data type     | Bit: description |
|---------------|:------------------------------|:----------------------|:--------------|:-----------------|
|     0         | 00                            | Request               | u8
|     1         | 00                            | slave address: heat pump | u8
|     2         | 10                            | packet type 10        | u8
|     3         | 00/01                         | Heating               | flag8 | 0: power (off/on)
|     4         | 01/81                         | Operating mode?       | flag8 | 7: gas?
|     5         | 00/01                         | DHW tank              | flag8 | 0: power (off/on)
|     6         | 00                            | ?                     |
|     7         | 00                            | ?                     |
|     8         | 00                            | ?                     |
|     9         | 00                            | ?                     |
|    10         | 14                            |Target room temperature| u8 / f8.8?
|    11         | 00                            | ?                     |
|    12         | 00/20/40/60                   | ?                     | flag8 | 5: ? <br> 6: ?
|    13         | 00/04                         | Quiet mode            | flag8 | 2: quiet mode (off/on)
|    14         | 00                            | ?                     |
|    15         | 08                            | ?                     |flag8 ?| 3: ?
|    16         | 00                            | ?                     |
|    17         | 00                            | ?                     |
|    18         | 0F                            | ?                     |flag8 ?| 0: ? <br> 1: ? <br> 2: ? <br> 3: ?
|    19         | 00                            | ?                     |
|    20         | 00/40/42                      | DHW tank mode         | flag8 | 1: booster (off/on) <br> 6: operation (off/on)
|    21         | 3C                            |DHW target temperature | u8 / f8.8?
|    22         | 00                            | fractional part?      |
|    23         | XX                            | CRC checksum          | u8

#### 2. Packet "400010..."

| Byte nr       | Hex value observed            | Description           | Data type     | Bit: description |
|---------------|:------------------------------|:----------------------|:--------------|:-----------------|
|     0         | 40                            | Response              | u8
|     1         | 00                            | slave address: heat pump | u8
|     2         | 10                            | packet type 10        | u8
|     3         | 00/01                         | Heating               | flag8         | 0: power (off/on) |
|     4         | 00/80                         | ??                    | flag8         | 7: ? |
|     5         | 01/21/22/31/41/42/81/A1       | Valves                | flag8         | 0: heating (off/on) <br> 1: cooling (off/on) <br> 4: ? <br> 5: main zone (off/on) <br> 6: additional zone (off/on) <br> 7: DHW tank (off/on) |
|     6         | 00/01/11                      | 3-way valve           | flag8         | 0: status (off/on) <br> 4: status (SHC/tank) |
|     7         | 3C                            | DHW target temperature| u8 / f8.8?
|     8         | 00                            | fractional part?      |
|     9         | 0F                            | ??                    | flag8         | 0: ? <br> 1: ? <br> 2: ? <br> 3: ? |
|    10         | 00                            | ?
|    11         | 14                            |Target room temperature| u8 / f8.8?
|    12         | 00                            | ?
|    13         | 1A                            | ?
|    14         | 00/04                         | Quiet mode            | flag8         | 2: quiet mode (off/on) |
|    15-20      | 00                            | ?
|    21         | 00/02/08/09                   | Pump and compressor   | flag8         | 0: compressor (off/on) <br> 3: Circ.pump (off/on) |
|    22         | 00/02                         | DHW active            | flag8         | 1: DHW active1 (off/on) <br> 0: mode?? |
|    23         | XX                            | CRC checksum          | u8

#### 3. Packet "000011..."

| Byte nr       | Hex value observed            | Description           | Data type     | Bit: description |
|---------------|:------------------------------|:----------------------|:--------------|:-----------------|
|     0         | 00                            | Request               | u8
|     1         | 00                            | slave address: heat pump | u8
|     2         | 11                            | packet type 11        | u8
|     3-4       | XX YY                         |Actual room temperature| f8.8
|     5         | 00                            | ?
|     6         | 00                            | ?
|     7         | 00                            | ?
|     8         | 00                            | ?
|     9         | 00                            | ?
|    10         | 00                            | ?
|    11         | XX                            | CRC checksum          | u8

#### 4. Packet "400011..."

| Byte nr       | Hex value observed            | Description           | Data type     | Bit: description |
|---------------|:------------------------------|:----------------------|:--------------|:-----------------|
|     0         | 40                            | Response              | u8
|     1         | 00                            | slave address: heat pump | u8
|     2         | 11                            | packet type 11        | u8
|   3-4         | XX YY                         | LWT temperature       | f8.8
|   5-6         | XX YY                         | DHW temperature (if connected) | f8.8
|   7-8         | XX YY                         | Outside temperature 1 (raw; in 0.5 degree resolution) | f8.8 |
|   9-10        | XX YY                         | RWT                   | f8.8 |
|  11-12        | XX YY                         | Mid-way temperature heat pump - gas boiler | f8.8 |
|  13-14        | XX YY                         | Refrigerant temperature | f8.8 |
|  15-16        | XX YY                         | Actual room temperature | f8.8 |
|  17-18        | XX YY                         | External temperature sensor (if connected); otherwise Outside temperature 2 derirved from external unit sensor, but stabilized; it does not change during defrosts; it also adds extra variations not in raw outside temperature, perhaps due to averaging over samples | f8.8 |
|  19-22        | 00                            | ? |
|    23         | XX                            | CRC checksum          | u8

#### 5. Packet "000012.."

| Byte nr       | Hex value observed            | Description           | Data type     | Bit: description |
|---------------|:------------------------------|:----------------------|:--------------|:-----------------|
|     0         | 00                            | Request               | u8
|     1         | 00                            | slave address: heat pump | u8
|     2         | 12                            | packet type 12        | u8
|     3         | 00/02                         | new hour value        | flag8 | 1: new-hour or restart indicator
|     4         | 00-06                         | day of week (0=Monday, 6=Sunday) | u8
|     5         | 00-17                         | time - hours          | u8
|     6         | 00-3B                         | time - minutes        | u8
|     7         | 13 (example)                  | date - year (0x13 = 2019) | u8
|     8         | 01-0C                         | date - month          | u8
|     9         | 01-1F                         | date - day of month   | u8
|    10-14      | 00                            | ? | u8
|    15         | 00/01/41/61                   | upon restart: 1x 00; 1x 01; then 41; a single value of 61 triggers an immediate restart| flag8 | 1: ?<br>5: reboot<br> 6: ?
|    16         | 00/04                         | once 00, then 04      | flag 8 | 2: ?
|    17         | 00                            | ?
|    18         | XX                            | CRC checksum          | u8

#### 6. Packet "400012.."

| Byte nr       | Hex value observed            | Description           | Data type     | Bit: description |
|---------------|:------------------------------|:----------------------|:--------------|:-----------------|
|     0         | 40                            | Response              | u8
|     1         | 00                            | slave address: heat pump | u8
|     2         | 12                            | packet type 12        | u8
|     3         | 40                            | ?? |
|     4         | 40                            | ?? |
|    5-12       | 00                            | ? |
|    13         | 00/10                         | kWh preference input(s) | flag8 | 4: preference kWh input
|    14         | 7F                            | first packet after restart 00; else 7F | u8
|    15         | 00/01/40/41/80/81/C1          | operating mode        | flag8 | 0: heat pump?<br>6: gas?<br> 7: DHW active2 ?
|    16         | 04                            | ?
|    17-22      | 00                            | ?
|    23         | XX                            | CRC checksum          | u8

#### 7. Packet "000013.."

| Byte nr       | Hex value observed            | Description           | Data type     | Bit: description |
|---------------|:------------------------------|:----------------------|:--------------|:-----------------|
|     0         | 00                            | Request               | u8
|     1         | 00                            | slave address: heat pump | u8
|     2         | 13                            | packet type 13        | u8
|    3-4        | 00                            | ? |
|     5         | 00/D0                         | first package 0x00 instead of 0xD0 | flag8 ?
|     6         | XX                            | CRC checksum          | u8

#### 8. Packet "400013.."

| Byte nr       | Hex value observed            | Description           | Data type     | Bit: description |
|---------------|:------------------------------|:----------------------|:--------------|:-----------------|
|     0         | 40                            | Response              | u8
|     1         | 00                            | slave address: heat pump | u8
|     2         | 13                            | packet type 13        | u8
|     3         | 3C                            | DHW target temperature (one packet delayed/from/via boiler?/ 0 in first packet after restart)| u8 / f8.8 ?
|     4         | 00                            | ? |
|     5         | 01                            | ?? |
|     6         | D0                            | ?? |
|   7-11        | 00                            | ? |
|    12         | 00-E0                         | flow (in 0.1 l/min) | u8
|    13-14      | xxxx                          | software version inner unit | u16
|    15-16      | xxxx                          | software version outer unit | u16
| EHV only:    17        | 00                            | ??                    | u8
| EHV only:    18        | 00                            | ??                    | u8
| EHY:    17        | XX                            | CRC checksum          | u8
| EHY:    19        | XX                            | CRC checksum          | u8

#### 9. Packet "000014.."

| Byte nr       | Hex value observed            | Description           | Data type     | Bit: description |
|---------------|:------------------------------|:----------------------|:--------------|:-----------------|
|     0         | 00                            | Request               | u8
|     1         | 00                            | slave address: heat pump | u8
|     2         | 14                            | packet type 14        | u8
|     3         | 2B                            | ??
|     4         | 00                            | ?
|     5         | 13                            | ??
|     6         | 00                            | ?
|     7         | 2D                            | ?? first package 0x37 instead of 0x2D (55 instead of 45) | u8
|     8         | 00                            | ?
|     9         | 07                            | ?? first package 0x37 instead of 0x07 | u8
|    10         | 00                            | ? |
|    11         | 00/01/02/11                   | delta-T | s-abs4
|    12         | 00?/02/05                     | ?? night/eco related mode ?  7:00: 02 9:00: 05 |
|   13-14       | 00                            | ? |
|   15          | 00/37                         | first package 0x37 instead of 0x00 |
|   16-17       | 00                            | ? |
|     18        | XX                            | CRC checksum          | u8

#### 10. Packet "400014.."

| Byte nr       | Hex value observed            | Description           | Data type     | Bit: description |
|---------------|:------------------------------|:----------------------|:--------------|:-----------------|
|     0         | 40                            | Response              | u8
|     1         | 00                            | slave address: heat pump | u8
|     2         | 14                            | packet type 14        | u8
|     3         | 2B                            | echo of 000014-03 ? |
|     4         | 00                            | echo of 000014-04 ? |
|     5         | 13                            | echo of 000014-05 ? |
|     6         | 00                            | echo of 000014-06 ? |
|     7         | 2D                            | echo of 000014-07 ? |
|     8         | 00                            | echo of 000014-08 ? |
|     9         | 07                            | echo of 000014-09 ? |
|    10         | 00                            | echo of 000014-10 ? |
|    11         | 03/01/02/11                   | echo of 000014-11 delta-T |
|    12         | 00/02/05                      | echo of 000014-12 |
|   13-17       | 00                            | echo of 000014-{13-17} ? |
|   18-19       | 1C-24 00-09                   | Target temperature LWT main zone in 0.1 degree (based on outside temperature in 0.5 degree resolution)| f8/8
|   20-21       | 1C-24 00-09                   | Target temperature LWT add zone  in 0.1 degree (based on outside temperature in 0.5 degree resolution)| f8/8
|     22        | XX                            | CRC checksum          | u8

#### 11. Packet "000015.."

| Byte nr       | Hex value observed            | Description           | Data type     | Bit: description |
|---------------|:------------------------------|:----------------------|:--------------|:-----------------|
|     0         | 00                            | Request               | u8
|     1         | 00                            | slave address: heat pump | u8
|     2         | 15                            | packet type 15        | u8
|     3         | 00                            | ? |
|     4-5       | 01/09/0A F4/C4/D6/F0          | operating mode? 7:30: 01D6
|     6         | 00                            | ?
|     7         | 03                            | ??
|     8         | 20                            | ??
|     9         | XX                            | CRC checksum          | u8

#### 12. Packet "400015.."

| Byte nr       | Hex value observed            | Description           | Data type     | Bit: description |
|---------------|:------------------------------|:----------------------|:--------------|:-----------------|
|     0         | 40                            | Response              | u8
|     1         | 00                            | slave address: heat pump | u8
|     2         | 15                            | packet type 15        | u8
|    3-4        | 00                            | ?
|    5-6        | FD-FF,00-08 00/80             | Refrigerant temperature in 0.5 degree resolution | f8.8
|    7-8        | 00                            | ?
|EHY:  9        | XX                            | CRC checksum          | u8
|EHV:  9        | 00-19                         | parameter number      | u8
|EHV: 10        | 00                            | (part of parameter or parameter value?)                    | u8
|EHV: 11        | XX                            | ??                    | u8
|EHV: 12        | XX                            | CRC checksum          | u8


| Parameter number  | Parameter Value           | Description           | Data type     | Bit: description |
|---------------|:------------------------------|:----------------------|:--------------|:-----------------|
|EHV: 5         | 96                            | 15.0 degree  ?        | u8div10
|EHV: 8         | 73,78,7D                      | 11.5, 12.0, 12.5 ?    | u8div10
|EHV: A         | C3,C8                         | 19.5, 20.0            | u8div10
|EHV: C         | 6E,73,78                      | 11.0, 11.5, 12.0      | u8div10
|EHV: E         | B9,BE                         | 18.5, 19.0            | u8div10
|EHV: F         | 68,6B                         | 10.4, 10.7            | u8div10
|EHV: 10        | 05                            |  0.5                  | u8div10
|EHV: 19        | 01                            |  0.1                  | u8div10
|EHV: others    | 00                            | ??                    | u8

#### 13. Packet "000016.." (EHV only)

| Byte nr       | Hex value observed            | Description           | Data type     | Bit: description |
|---------------|:------------------------------|:----------------------|:--------------|:-----------------|
|     0         | 00                            | Request               | u8
|     1         | 00                            | slave address: heat pump | u8

|     5-6       | 32 14                         | room temperature?

|     2         | 16                            | packet type 16        | u8
|    3-10       | 00                            | ?
|     11        | E6                            | ??                    | u8
|     12        | XX                            | CRC checksum          | u8

#### 14. Packet "400016.." (EHV only)

| Byte nr       | Hex value observed            | Description           | Data type     | Bit: description |
|---------------|:------------------------------|:----------------------|:--------------|:-----------------|
|     0         | 40                            | Response              | u8
|     1         | 00                            | slave address: heat pump | u8
|     2         | 16                            | packet type 16        | u8
|    3-10       | 00                            | ?
|     11        | E6                            | ??                    | u8
|     12        | XX                            | CRC checksum          | u8

#### 15. Packet "00F030.."

| Byte nr       | Hex value observed            | Description           | Data type     | Bit: description |
|---------------|:------------------------------|:----------------------|:--------------|:-----------------|
|     0         | 00                            | Request               | u8
|     1         | F0                            | Slave address: external controller 0       | u8 
|     2         | 30                            | packet type 30        | u8
|     3-16      | 00-03                         | indicate whether additional packets "00F03x" will be sent | u8
|     17        | XX                            | CRC checksum          | u8

Packet 00F030 requests a response from an external controller (slave with identity F0). Bytes 4-17 indicate whether additional packets (requests) are present for the external controller. If the external controller is available, it will respond with a packet "40F030". The response packet may indicate that the external controller would like to receive a certain request packet - this mechanism is used when a setting in the external controller is changed and should be communicated to the main controller and then to the heat pump. The additional packets 00F03x - and the reply 40F03x following it - are used to communicate status changes between main controller and external controller.

F1 has been observed as 2nd external controller in some devices.

#### 14. Packet "40F030.." by external controller

| Byte nr       | Hex value observed            | Description           | Data type     | Bit: description |
|---------------|:------------------------------|:----------------------|:--------------|:-----------------|
|     0         | 40                            | Response              | u8
|     1         | F0                            | Slave address: external controller 0       | u8 
|     2         | 30                            | packet type 30        | u8
|     3-16      | 00-01                         | indicate whether additional packets requests "00F03x" are needed; byte 3 for 00F031 .. byte 16 for 00F03E  | u8
|     17        | XX                            | CRC checksum          | u8


# Other packets

#### Packet "00F031.." (and "40F031")

| Byte nr       | Hex value observed            | Description           | Data type     | Bit: description |
|---------------|:------------------------------|:----------------------|:--------------|:-----------------|
|     0         | 00                            | Request               | u8
|     1         | F0                            | Slave address: external controller 0       | u8 
|     2         | 31                            | packet type 31        | u8
|     3-5       | 00                            | ?                     | u8
|     6         | 01,81                         | ?                     | u8
|     7         | B4                            | ?                     | u8
|     8         | 11,15,51,55                   | ?                     | u8
|     9         | 13 (example)                  | date - year (0x13 = 2019) | u8
|     10        | 01-0C                         | date - month          | u8
|     11        | 01-1F                         | date - day of month   | u8
|     12        | 00-17                         | time - hours          | u8
|     13        | 00-3B                         | time - minutes        | u8
|     14        | 00-3B                         | time - seconds        | u8
|     15        | XX                            | CRC checksum          | u8





#### Packet "00F035.." and "40F035.."

A few hundred parameters can be exchanged via packet type 35. Some are fixed (a range of parameters is used to communicate the device ID), others are relating to operating settings. Unfortunately temperating settings are not visible here.

| Byte nr       | Hex value observed            | Description           | Data type     | Bit: description |
|---------------|:------------------------------|:----------------------|:--------------|:-----------------|
|     0         | 00 or 40                      | Request or response               | u8
|     1         | F0                            | Slave address: external controller 0       | u8 
|     2         | 35                            | packet type 35        | u8
|     3-20      | XX YY ZZ                      | communicates value ZZ of parameter YYXX; padding form is FFFFFF | u16 + u8
|     21        | XX                            | CRC checksum          | u8

The parameter range for the is 0x0000-0x144.

| Param nr      | Hex value observed            | Description           | Data type     | Bit: description |
|---------------|:------------------------------|:----------------------|:--------------|:-----------------|
|    0E         | 00/01                         | Circ.pump off/on      |               |
|    11         | 00/01                         | DHW active            |               |
|    13         | 01                            | ?                     |               |
|    19         | 00/01                         | kWh preference input off/on|               |
|    21         | 01                            | ?                     |               |
|    22         | 01                            | ?                     |               |
|    27         | 01                            | ?                     |               |
|    2E         | 00/01                         | Related to heating/cooling on/off |   |
|    2F         | 00/01                         | Related to heating/cooling on/off  (setting this parameter in a 40Fx35 response to 0x00/0x01 switches heating/cooling off/on) |   |
|    37         | 00/01/04                      | Related to heating/cooling and DHW on/off |   |
|    39         | 01                            | ?                     |               |
|    3A         | 01                            | ?                     |               |
|    3F         | 00/01                         | Related to DHW on/off |               |
|    40         | 00/01                         | Related to DHW on/off (setting this parameter in a 40Fx35 response to 0x00/0x01 switches DHW off/on) |               |
|    48         | 00/01                         | Related to DHW booster on/off |               |
|    4E         | 01                            | ?                     |               |
|    4F         | 01                            | ?                     |               |
|    50         | 00/01                         | DHW active            |               |
|    55         | 03                            | ?                     |               |
|    56         | 03                            | ?                     |               |
|    5C         | 7F                            | ?                     |               |
|    88         | 01                            | ?                     |               |
|    8D         | 02                            | ?                     |               |
|    93         | 02                            | ?                     |               |
|    98         | 01                            | indication manual setting? |          |
|    9A         | 4B                            | ?                     |               |
|    9D         | XX                            | counter, # solid state writes |       |
|    A2         | XX                            | counter, ?            |               |
|    B4         | 01                            | ?                     |               |
|    B6         | 01                            | ?                     |               |
|    B7         | 01                            | ?                     |               |
|    C2         | 01                            | ?                     |               |
|    C3         | 07                            | ?                     |               |
|    C5         | 05                            | ?                     |               |
|    C6         | 09                            | ?                     |               |
|    C7         | 08                            | ?                     |               |
|    C8         | 01                            | ?                     |               |
|    C9         | 01                            | ?                     |               |
|    CA         | 02                            | ?                     |               |
|    CC         | 04                            | ?                     |               |
|   10C         | 01                            | ?                     |               |
|   11B         | 0E                            | ?                     |               |
|   11C         | 03                            | ?                     |               |
|   11F         | 04                            | ?                     |               |
|   121         | 07                            | ?                     |               |
|   122         | 04                            | ?                     |               |
|   13A..145    | 45 48 59 48 42 58 30 38 41 41 56 33 | "EHYHBX08AAV3"  | ASCII         |
|   others      | 00                            | ?                     |               |

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
|     0         | 00                            | Request                 | u8
|     1         | 00                            | slave address: heat pump | u8
|     2         | XX                            | packet type (60..8F) | u8
|  3-22         | 00                            | All fields empty
|    23         | XX                            | CRC checksum          | u8

#### Packet "4000XX.." Field setting reply by heat pump during restart process

For supported field settings:

| Byte nr       | Hex value observed            | Description           | Data type     | Bit: description |
|---------------|:------------------------------|:----------------------|:--------------|:-----------------|
|     0         | 40                            | Response                | u8
|     1         | 00                            | slave address: heat pump | u8
|     2         | XX                            | packet type (60..62,64-65,68-6E,70-75,78-85,88-8F) | u8
|  3            | XX                            | 1st field value (usually 0xC0 is added to this value)  | u8
|  4            | XX                            | maximum field value  | u8
|  5            | XX                            | offset for field value  | u8
|  6            | 00/08/0A/28/2A/8A/92/AA       | field setting info      | flag8        | 3: in use<br>7: comma used<br>1,5: step size(00/02/20/22=step size 1/0.1/5/0.5)<br>4: read-only with comma?
|  7-10         | 00                            | 2nd field value, see bytes 4-7           | u8,u8,u8,flag8
| 11-14         | 00                            | 3rd field value, see bytes 4-7           | u8,u8,u8,flag8
| 15-18         | 00                            | 4th field value, see bytes 4-7           | u8,u8,u8,flag8
| 19-22         | 00                            | 5th field value, see bytes 4-7           | u8,u8,u8,flag8
|    23         | XX                            | CRC checksum          | u8

For non-supported field settings:

| Byte nr       | Hex value observed            | Description           | Data type     | Bit: description |
|---------------|:------------------------------|:----------------------|:--------------|:-----------------|
|     0         | 40                            | Response                | u8
|     1         | 00                            | slave address: heat pump | u8
|     2         | FF                            | FF as packet type answer if request is for packet type (63,66-67,6F,76-77,86,87) | u8
|     3         | XX                            | CRC checksum          | u8

#### Packet "0000XX.." Field setting change by thermostat during operation

| Byte nr       | Hex value observed            | Description           | Data type     | Bit: description |
|---------------|:------------------------------|:----------------------|:--------------|:-----------------|
|     0         | 00                            | Request                 | u8
|     1         | 00                            | slave address: heat pump | u8
|     2         | XX                            | packet type (60..8F) | u8
|  3            | XX                            | 1st field, old value (no 0xC0 added), or new value (0x40 added) | u8
|  4            | XX                            | maximum field value  | u8
|  5            | XX                            | offset for field value  | u8
|  6            | 00/08/0A/28/2A/8A/92/AA       | field setting info  | flag8 | see above
|  7-10         | 00                            | 2nd field value, see bytes 3-6           | u8,u8,u8,flag8
| 11-14         | 00                            | 3rd field value, see bytes 3-6           | u8,u8,u8,flag8
| 15-18         | 00                            | 4th field value, see bytes 3-6           | u8,u8,u8,flag8
| 19-22         | 00                            | 5th field value, see bytes 3-6           | u8,u8,u8,flag8
|    23         | XX                            | CRC checksum          | u8

#### Packet "4000XX.." Field setting reply by heat pump during restart process

Format is the same for supported and non-supported field settings

| Byte nr       | Hex value observed            | Description           | Data type     | Bit: description |
|---------------|:------------------------------|:----------------------|:--------------|:-----------------|
|     0         | 40                            | Response                | u8
|     1         | 00                            | slave address: heat pump | u8
|     2         | XX                            | packet type (60..8F) | u8
|  3            | XX                            | 1st field value (usually 0xC0 is added to this value)  | u8
|  4            | XX                            | maximum field value  | u8
|  5            | XX                            | offset for field value  | u8
|  6            | 00/08/0A/28/2A/8A/92/AA       | field setting info  | flag8 | see above
|  7-10         | 00                            | 2nd field value, see bytes 3-6           | u8,u8,u8,flag8
| 11-14         | 00                            | 3rd field value, see bytes 3-6           | u8,u8,u8,flag8
| 15-18         | 00                            | 4th field value, see bytes 3-6           | u8,u8,u8,flag8
| 19-22         | 00                            | 5th field value, see bytes 3-6           | u8,u8,u8,flag8
|    23         | XX                            | CRC checksum          | u8

## A1/20 for motors and operation mode

#### 001. Packet "0000A100.."

| Byte nr       | Hex value observed            | Description           | Data type     | Bit: description |
|---------------|:------------------------------|:----------------------|:--------------|:-----------------|
|     0         | 00                            | Request                 | u8
|     1         | 00                            | slave address: heat pump | u8
|     2         | A1                            | packet type A1 | u8
|     3         | 00                            | ?
|  4-17         | 30                            | ASCII '0'             | u8
| 18-20         | 00                            | ?
|    21         | XX                            | CRC checksum          | u8

#### 002. Packet "4000A100.."

| Byte nr       | Hex value observed            | Description           | Data type     | Bit: description |
|---------------|:------------------------------|:----------------------|:--------------|:-----------------|
|     0         | 40                            | Response               |
|     1         | 00                            | slave address: heat pump |
|     2         | A1                            | packet type A1 |
|     3         | 00                            | ?
|  4-17         | 00                            | ASCII '\0'             | u8
| 18-20         | 00                            | ?
|    21         | XX                            | CRC checksum          | u8

#### 001. Packet "0000B100.."

| Byte nr       | Hex value observed            | Description           | Data type     | Bit: description |
|---------------|:------------------------------|:----------------------|:--------------|:-----------------|
|     0         | 00                            | Request                 | u8
|     1         | 00                            | slave address: heat pump | u8
|     2         | B1                            | packet type A1 | u8
|     3         | 00                            | ?
|  4-18         | 30                            | ASCII '0'             | u8
| 19-20         | 00                            | ?
|    21         | XX                            | CRC checksum          | u8

#### 002. Packet "4000B100.."

| Byte nr       | Hex value observed            | Description           | Data type     | Bit: description |
|---------------|:------------------------------|:----------------------|:--------------|:-----------------|
|     0         | 40                            | Response               |
|     1         | 00                            | slave address: heat pump |
|     2         | B1                            | packet type A1 |
|     3         | 00                            | ?
|  4-15         | XX                            | ASCII "EHYHBH08AAV3"   | u8
| 16-20         | 00                            | ?
|    21         | XX                            | CRC checksum          | u8

#### 003. Packet "00002000.."

| Byte nr       | Hex value observed            | Description           | Data type     | Bit: description |
|---------------|:------------------------------|:----------------------|:--------------|:-----------------|
|     0         | 00                            | Request                 | u8
|     1         | 00                            | slave address: heat pump | u8
|     2         | 20                            | packet type 20 | u8
|     3         | 00                            | ?
|     4         | XX                            | CRC checksum          | u8

#### 004. Packet "40002000.."

| Byte nr       | Hex value observed            | Description           | Data type     | Bit: description |
|---------------|:------------------------------|:----------------------|:--------------|:-----------------|
|     0         | 40                            | Response               | u8
|     1         | 00                            | slave address: heat pump | u8
|     2         | 20                            | packet type 20 | u8
|    3-22       | XX                            | various values??, tbd |
|    23         | XX                            | CRC checksum | u8

## B8 for request of energy consumed and count of operating hours

#### 001. Packet "0000B800.."

| Byte nr       | Hex value observed            | Description           | Data type     | Bit: description |
|---------------|:------------------------------|:----------------------|:--------------|:-----------------|
|     0         | 00                            | Request                 | u8
|     1         | 00                            | slave address: heat pump | u8
|     2         | B8                            | packet type B8 | u8
|     3         | 00                            | packet subtype 00 | u8
|     4         | XX                            | CRC checksum | u8

#### 002. Packet "4000B800.."

| Byte nr       | Hex value observed            | Description           | Data type     | Bit: description |
|---------------|:------------------------------|:----------------------|:--------------|:-----------------|
|     0         | 40                            | Response                | u8
|     1         | 00                            | slave address: heat pump | u8
|     2         | B8                            | packet type B8 | u8
|     3         | 00                            | packet subtype 00 | u8
|  4-9          | 00                            | two 3-byte numbers? | u24
| 10-12         | 00 XX XX                      | ??   | u24
| 13-18         | 00                            | two 3-byte numbers? | u24
| 19-21         | 00 XX XX                      | electric energy consumed | u24
|    22         | XX                            | CRC checksum | u8


#### 003. Packet "0000B801.."

| Byte nr       | Hex value observed            | Description           | Data type     | Bit: description |
|---------------|:------------------------------|:----------------------|:--------------|:-----------------|
|     0         | 00                            | Request                 | u8
|     1         | 00                            | slave address: heat pump | u8
|     2         | B8                            | packet type B8 | u8
|     3         | 01                            | packet subtype 00 | u8
|     4         | XX                            | CRC checksum | u8

#### 004. Packet "4000B801.."

| Byte nr       | Hex value observed            | Description           | Data type     | Bit: description |
|---------------|:------------------------------|:----------------------|:--------------|:-----------------|
|     0         | 40                            | Response                | u8
|     1         | 00                            | slave address: heat pump | u8
|     2         | B8                            | packet type B8 | u8
|     3         | 01                            | packet subtype 01 | u8
|  4-6          | XX XX XX                      | heat produced | u24
|  7-12         | 00                            | 4 3-byte numbers?   | u24,u24,u24,u24
| 13-15         | XX XX XX                      | heat produced (duplicate?)| u24
|    16         | XX                            | CRC checksum | u8


#### 005. Packet "0000B802.."

| Byte nr       | Hex value observed            | Description           | Data type     | Bit: description |
|---------------|:------------------------------|:----------------------|:--------------|:-----------------|
|     0         | 00                            | Request                 | u8
|     1         | 00                            | slave address: heat pump | u8
|     2         | B8                            | packet type B8 | u8
|     3         | 02                            | packet subtype 02 | u8
|     4         | XX                            | CRC checksum | u8

#### 006. Packet "4000B802.."

| Byte nr       | Hex value observed            | Description           | Data type     | Bit: description |
|---------------|:------------------------------|:----------------------|:--------------|:-----------------|
|     0         | 40                            | Response                | u8
|     1         | 00                            | slave address: heat pump | u8
|     2         | B8                            | packet type B8 | u8
|     3         | 02                            | packet subtype 02 | u8
|  4-6          | XX XX XX                      | number of pump hours | u24
|  7-9          | XX XX XX                      | number of compressor hours | u24
| 10-15         | XX XX XX                      | 2 3-byte numbers? (cooling hours?) | u24,u24
|    16         | XX                            | CRC checksum | u8


#### 007. Packet "0000B803.."

| Byte nr       | Hex value observed            | Description           | Data type     | Bit: description |
|---------------|:------------------------------|:----------------------|:--------------|:-----------------|
|     0         | 00                            | Request                 | u8
|     1         | 00                            | slave address: heat pump | u8
|     2         | B8                            | packet type B8 | u8
|     3         | 03                            | packet subtype 03 | u8
|     4         | XX                            | CRC checksum | u8

#### 008. Packet "4000B803.."

| Byte nr       | Hex value observed            | Description           | Data type     | Bit: description |
|---------------|:------------------------------|:----------------------|:--------------|:-----------------|
|     0         | 40                            | Response                | u8
|     1         | 00                            | slave address: heat pump | u8
|     2         | B8                            | packet type B8 | u8
|     3         | 03                            | packet subtype 03 | u8
|  4-21         | 00                            | ? | u8
|    22         | XX                            | CRC checksum | u8


#### 009. Packet "0000B804.."

| Byte nr       | Hex value observed            | Description           | Data type     | Bit: description |
|---------------|:------------------------------|:----------------------|:--------------|:-----------------|
|     0         | 00                            | Request                 | u8
|     1         | 00                            | slave address: heat pump | u8
|     2         | B8                            | packet type B8 | u8
|     3         | 04                            | packet subtype 04 | u8
|     4         | XX                            | CRC checksum | u8

#### 010. Packet "4000B804.."

| Byte nr       | Hex value observed            | Description           | Data type     | Bit: description |
|---------------|:------------------------------|:----------------------|:--------------|:-----------------|
|     0         | 40                            | Response                | u8
|     1         | 00                            | slave address: heat pump | u8
|     2         | B8                            | packet type B8 | u8
|     3         | 04                            | packet subtype 04 | u8
|  4-12         | 00                            | ? | u8
| 13-15         | XX XX XX                      | number of start attempts | u24
|    16         | XX                            | CRC checksum | u8


#### 011. Packet "0000B805.."

| Byte nr       | Hex value observed            | Description           | Data type     | Bit: description |
|---------------|:------------------------------|:----------------------|:--------------|:-----------------|
|     0         | 00                            | Request                 | u8
|     1         | 00                            | slave address: heat pump | u8
|     2         | B8                            | packet type B8 | u8
|     3         | 05                            | packet subtype 05 | u8
|     4         | XX                            | CRC checksum | u8

#### 012. Packet "4000B805.."

| Byte nr       | Hex value observed            | Description           | Data type     | Bit: description |
|---------------|:------------------------------|:----------------------|:--------------|:-----------------|
|     0         | 40                            | Response                | u8
|     1         | 00                            | slave address: heat pump | u8
|     2         | B8                            | packet type B8 | u8
|     3         | 05                            | packet subtype 05 | u8
|  4-6          | XX XX XX                      | number of boiler hours heating | u24
|  7-9          | XX XX XX                      | number of boiler hours DHW | u24
| 10-15         | 00 00 00                      | 2 3-byte numbers? | u24,u24
| 16-18         | XX XX XX                      | 1 3-byte number (tbd)? | u24
| 19-21         | 00 00 00                      | 1 3-byte number? | u24
|    22         | XX                            | CRC checksum | u8


## 21 for tbd

#### Tbd

## Reboot process

The reboot process is initiated by setting byte 16 of packet "000012.." to 0x61. The heat pump stops answering requests almost immediately.

The reboot process consists of several steps, including lots of data communication, still to be described.
