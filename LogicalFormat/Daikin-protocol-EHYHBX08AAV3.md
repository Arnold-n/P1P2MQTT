# EHYHB(H/X)-AV3 and EHV(H/X)-C(A/B) protocol data format

Protocol data format for Daikin Altherma hybrid heat pump for the EHYHBX08AAV3 (perhaps all EHYHB(H/X)-AV3 models) and (at least partially for the) Daikin Altherma LT heat pump EHV(H/X)-CA/CB. A lot of data is also valid for EHB\* models.

If you have any information, please contribute to one of the data format files or create a new one so we can support more models.

Please read the common README.md first for the general format description.

The following data packets were observed on these particular units:
EHYHBX08AAV3: air-to-water heat pump in cascade with gas condensing technology, functioning in weather-dependent LWT mode
EHVX08S26CB9W: air-to-water heat pump (with cooling), functioning in weather-dependent LWT mode (so far only "main package" packets were tested for this model)

All values in this document are hex, except for the byte numbering in the payload and time (ms) references.

**Cycle**

Each regular data cycle consists of a package, where the thermostat and heat pump take turn as transmitter in a request-response pattern. Every approximately 770ms one package of 6 pair of request/response packets are communicated this way, followed by a 13th packet. The 13th packet invites an auxiliary (or external/2nd) controller to identify itself.  If there is no auxiliary controller answering within 140/160ms the cycle restarts. If there is a reply to the 13th packet, additional request/response pairs are being communicated between main and auxiliary controller.

If the user of the thermostat requests certain information in the menu items, such as energy consumed, additional packet types are inserted before such a package.

**Packet type**

Each packet has in the header a packet type (and in certain cases a packet subtype as first payload byte) to identify the payload function. Packet type numbers observed so far are:
- 10..16 (i.e. 0x10-0x16) for regular data packets between main controller and heat pump (peripheral address 0x00)
- 30..3E for communication between the main controller and external controllers (peripheral address 0xF0 or higher)
- 60..8F for field setting read/write operations (during operation and upon restart)
- 90..9F,A1,B1 for data communication upon restart; packets 90, 92-98, 9A-9C are empty/not used; packet 91, 99, 9D, 9E, A1 are TBD; packet 9F contains information similar to packet 20 (tbd), packet B1 contains the ASCII name of the heat pump ("EHYHBX08AAV3\0\0\0\0\0")
- B8 (+subtype) for incidental requests of consumption usage and count of operating hours
- A1/20 for incidental requests on status of motors and operation mode
- 21 for incidental requests on tbd

**Timing between packets**

- approximately 25ms between communication main controller and communication heat pump
- approximately 30-50ms (42/37/47/31/29/36) between communication heat pump and communication thermostat
- approximately 140 or 160 ms between last packet of a package and the first packet of a new package

This timing corresponds to the description found in a design guide from Daikin which specifies that a response must follow a request after 25ms, and a new request must not follow a response before another 25ms silence.

# Packet types 00-05 are used during restart process

A description of the restart process follows at the end of this document.

# Packet types 10-1F form communication package between main controller and heat pump

Packet types 10-16 are part of the regular communication pattern between main controller and heat pump.

## Packet type 10 - operating status

### Packet type 10: request

Header: 000010

|Byte(:bit)| Hex value observed | Description              | Data type
|:---------|:-------------------|:-------------------------|:-
|0:0       | 0/1                | Heat pump (off/on)       | bit
|0:other   | 0                  | ?                        | bit
|1:7       | 0/1                | Heat pump (off/on)       | bit
|1:0       | 0/1                | 1=Heating mode           | bit
|1:1       | 0/1                | 1=Cooling mode           | bit
|1:0       | 1                  | Operating mode gas?      | bit
|1:other   | 0                  | Operating mode?          | bit
|2:1       | 0/1                | DHW tank power (off/on)  | bit
|2:0       | 0/1                | DHW (off/on)             | bit
|2:other   | 0                  | ?                        | bit
|     3    | 00                 | ?                        |
|     4    | 00                 | ?                        |
|     5    | 00                 | ?                        |
|     6    | 00                 | ?                        |
|   7-8    | 13 05              | Target room temperature  | f8.8
| 9:6      | 0/1                | ?                        | bit
| 9:5      | 0/1                | Heating/Cooling automatic mode | bit
| 9:others | 0                  | ?                        | bit
|10:2      | 0/1                | Quiet mode (off/on)      | bit
|10:others | 0                  | ?                        | bit
|    11    | 00                 | ?                        |
|12:3      | 1                  | ?                        | bit
|12:others | 0                  | ?                        | bit
|    13    | 00                 | ?                        |
|    14    | 00                 | ?                        |
|    15    | 0F                 | ?                        | flag8/bits?
|    16    | 00                 | ?                        |
|17:6      | 0/1                | operation (off/on)       | bit
|17:1      | 0/1                | booster (off/on)         | bit
|17:others | 0                  | ?                        | bit
|    18    | 3C                 | DHW target temperature   | u8 / f8.8?
|    19    | 00                 | fractional part byte 18? |

### Packet type 10: response

Header: 400010

|Byte(:bit)| Hex value observed | Description              | Data type
|:---------|:-------------------|:-------------------------|:-
| 0:0      | 0/1                | Heating power (off/on)   | bit
| 0:other  | 0                  | ?                        | bit
| 1:7      | 0/1                | Operating mode gas?      | bit
| 1:0      | 0                  | Operating mode?          | bit
| 1:other  | 0                  | Operating mode?          | bit
| 2:7      | 0/1                | DHW tank power (off/on)  | bit
| 2:6      | 0/1                | Additional zone (off/on) | bit
| 2:5      | 0/1                | Main zone (off/on)       | bit
| 2:4      | 0/1                | ?                        | bit
| 2:3      | 0                  | ?                        | bit
| 2:2      | 0                  | ?                        | bit
| 2:1      | 0/1                | cooling (off/on)         | bit
| 2:0      | 0/1                | heating (off/on)         | bit
| 3:4      | 0/1                | status 3-way valve (SHC/tank) | bit
| 3:0      | 0/1                | status 3-way valve (off/on)   | bit
| 3:others | 0                  | ?                        | bit
| 4        | 3C                 | DHW target temperature| u8 / f8.8?
| 5        | 00                 | +fractional part?        |
| 6        | 0F                 | ?                        | u8 / flag8?
| 7        | 00                 | ?
| 8        | 14                 | Target room temperature  | u8 / f8.8?
| 9        | 00                 | ?
|10        | 1A                 | ?
|11:2      | 0/1                | Quiet mode (off/on)      | bit
|11        | 0                  | ?                        | bit
|12        | 00                 | Error code part 1        | u8
|13        | 00                 | Error code part 2        | u8
|14        | 00                 | Error subcode            | u8
|15-17     | 00                 | ?
|17:1      | 0/1                | Defrost operation        | bit
|18:3      | 0/1                | Circ.pump (off/on)       | bit
|18:1      | 0/1                | ?                        | bit
|18:0      | 0/1                | Compressor (off/on)      | bit
|18:other  | 0                  | ?                        | bit
|19:2      | 0/1                | DHW mode                 | bit
|19:1      | 0/1                | gasboiler active1 (off/on) | bit
|19:other  | 0                  | ?                        | bit

Error codes: tbd, HJ-11 is coded as 024D2C, 89-2 is coded as 08B908, 89-3 is coded as 08B90C.

## Packet type 11 - temperatures

### Packet type 11: request

Header: 000011

| Byte nr | Hex value observed | Description             | Data type
|:--------|:-------------------|:------------------------|:-
|     0-1 | XX YY              | Actual room temperature | f8.8
|     2   | 00                 | ?
|     3   | 00                 | ?
|     4   | 00                 | ?
|     5   | 00                 | ?
|     6   | 00                 | ?
|     7   | 00                 | ?

### Packet type 11: response

Header: 400011

| Byte nr | Hex value observed | Description                                           | Data type
|:--------|:-------------------|:------------------------------------------------------|:-
|   0-1   | XX YY              | LWT temperature                                       | f8.8
|   2-3   | XX YY              | DHW temperature tank (if present)                     | f8.8
|   4-5   | XX YY              | Outside temperature (raw; in 0.5 degree resolution)   | f8.8
|   6-7   | XX YY              | RWT                                                   | f8.8
|   8-9   | XX YY              | Mid-way temperature heat pump - gas boiler            | f8.8
|  10-11  | XX YY              | Refrigerant temperature                               | f8.8
|  12-13  | XX YY              | Actual room temperature                               | f8.8
|  14-15  | XX YY              | External outside temperature sensor (if connected); otherwise Outside temperature 2 derived from external unit sensor, but stabilized; does not change during defrosts; perhaps averaged over time | f8.8
|  16-19  | 00                 | ?

## Packet type 12 - Time, date and status flags

### Packet type 12: request

Header: 000012

| Byte(:bit) | Hex value observed            | Description                      | Data type
|:-----------|:------------------------------|:---------------------------------|:-
|     0:1    | 0/1                           | pulse at start each new hour     | bit
|     0:other| 0                             | ?                                | bit
|     1      | 00-06                         | day of week (0=Monday, 6=Sunday) | u8
|     2      | 00-17                         | time - hours                     | u8
|     3      | 00-3B                         | time - minutes                   | u8
|     4      | 13-16                         | date - year (16 = 2022)          | u8
|     5      | 01-0C                         | date - month                     | u8
|     6      | 01-1F                         | date - day of month              | u8
|     7-11   | 00                            | ?                                |
|    12:6    | 0/1                           | restart process indicator ?      | bit
|    12:5    | 0/1                           | restart process indicator ?      | bit
|    12:0    | 0/1                           | restart process indicator ?      | bit
|    12:other| 0                             | ?                                | bit
|    13:2    | 0/1                           | once 0, then 1                   | bit
|    13:other| 0                             | ?                                | bit
|    14      | 00                            | ?                                |

Byte 12 has following pattern upon restart: 1x 00; 1x 01; then 41. A single value of 61 triggers an immediate heat pump restart.

### Packet type 12: response

Header: 400012

| Byte(:bit) | Hex value observed | Description           | Data type
|:-----------|:-------------------|:----------------------|:-
|     0      | 40                 | ?
|     1      | 40                 | ?
|    2-9     | 00                 | ?
|    10:4    | 0/1                | kWh preference input  | bit
|    10:other| 0                  |                       | bit
|    11      | 00/7F              | once 00, then 7F      | u8
|    12      |                    | operating mode        | flag8
|    12:7    | 0/1                | DHW active2           | bit
|    12:6    | 0/1                | gas? (depends on DHW on/off and heating on/off) | bit
|    12:0    | 0/1                | heat pump?            | bit
|    12:other| 0                  | ?                     | bit
|    13:2    | 1                  | ?                     | bit
|    13:other| 0                  | ?                     | bit
|    14-19   | 00                 | ?

## Packet type 13 - software version, DHW target temperature and flow

### Packet type 13: request

Header: 000013

| Byte(:bit) | Hex value observed | Description                        | Data type
|:-----------|:-------------------|:-----------------------------------|:-
|    0-1     | 00                 | ?
|   2:4      | 0/1                | ?                                  | bit
|   2:5,3-0  | 0                  | ?                                  | bit
|   2:7-6    | 00/01/10/11        | modus ABS / WD / ABS+prog / WD+dev | bit2

### Packet type 13: response

Header: 400013

| Byte(:bit) | Hex value observed | Description                 | Data type
|:-----------|:-------------------|:----------------------------|:-
|     0      | 3C                 | DHW target temperature (one packet delayed/from/via boiler?/ 0 in first packet after restart) | u8 / f8.8 ?
|     1      | 00                 |  +fractional part?
|     2      | 01                 | ?
|     3      | 40/D0              | ?
|   3:5-0    | 0                  | ?                                  | bit
|   3:7-6    | 00/01/10/11        | modus ABS / WD / ABS+prog / WD+dev | bit2
|   4-7      | 00                 | ?
|    8-9     | 0000-010E          | flow (in 0.1 l/min)         | u16div10
|    10-11   | xxxx               | software version inner unit | u16
|    12-13   | xxxx               | software version outer unit | u16
|EHV only: 14| 00                 | ?                           | u8
|EHV only: 15| 00                 | ?                           | u8

The DHW target temperature is one packet delayed - perhaps this is due to communication with the Intergas gas boiler and confirms the actual gas boiler setting?

## Packet type 14 - LWT target temperatures, temperature deviation, ..

### Packet type 14: request

Header: 000014

| Byte(:bit) | Hex value observed | Description                      | Data type
|:-----------|:-------------------|:---------------------------------|:-
|   0-1      | 27 00              | Target temperature Heating Main Zone  | f8.8 or f8/8?
|   2-3      | 27 00              | Cooling, perhaps target temperature main zone | f8.8 or f8/8?
|   4-5      | 12 00              | Target temperature Heating, Additional Zone??  | f8.8 or f8/8?
|   6-7      | 12 00              | Cooling, perhaps target temperature additional zone?? | f8.8 or f8/8?
|     8      | 00-0A,10-1A        | Target deviation/delta main zone           | s-abs4
|     9      | 00?/02/05          | ? changes based on schedules     | flag8
|   10       | 00                 | Target deviation/delta additional zone     | s-abs4
|   11       | 00                 | ?
|   12       | 00/37              | first package 37 instead of 00   | u8
|   13-14    | 00                 | ?

### Packet type 14: response

Header: 400014

| Byte(:bit) | Hex value observed | Description               | Data type
|:-----------|:-------------------|:--------------------------|:-
|     0-14   | XX                 | echo of 000014-{00-14}    |
|   15-16    | 1C-24 00-09        | Target temperature LWT main zone in 0.1 degree (based on outside temperature in 0.5 degree resolution)| f8/8?
|   17-18    | 1C-24 00-09        | Target temperature LWT add zone  in 0.1 degree (based on outside temperature in 0.5 degree resolution)| f8/8?

## Packet type 15 - temperatures, operating mode

### Packet type 15: request

Header: 000015

| Byte(:bit) | Hex value observed    | Description                     | Data type
|:-----------|:----------------------|:--------------------------------|:-
|     0      | 00                    | ?
|     1-2    | 01/09/0A/0B 54/F4/C4/D6/F0 | schedule-induced operating mode? | flag8,flag8?
|     3      | 00                    | ?
|     4      | 03                    | ?
|     5      | 20/52                 | ?

### Packet type 15: response

Header: 400015

| Byte(:bit) | Hex value observed    | Description                       | Data type
|:-----------|:----------------------|:----------------------------------|:-
|    0-1     | 00                    | Refrigerant temperature?          | f8.8?
|    2-3     | FD-FF,00-08 00/80     | Refrigerant temperature (in 0.5C) | f8.8
|    4-5     | 00                    | Refrigerant temperature?          | f8.8?
|EHV only: 6 | 00-19                 | parameter number                  | u8/u16
|EHV only: 7 | 00                    | (part of parameter or value?)     |
|EHV only: 8 | XX                    | ?                                 | s16div10_LE?

EHV is the only model for which we have seen use of the <parameter,value> mechanism in the basic packet types.
The following parameters have been observed in this packet type on EHV model only:

| Parameter number | Parameter Value               | Description
|:-----------------|:------------------------------|:-
|EHV: 05           | 96                            | 15.0 degree?
|EHV: 08           | 73,78,7D                      | 11.5, 12.0, 12.5 ?
|EHV: 0A           | C3,C8                         | 19.5, 20.0
|EHV: 0C           | 6E,73,78                      | 11.0, 11.5, 12.0
|EHV: 0E           | B9,BE                         | 18.5, 19.0
|EHV: 0F           | 68,6B                         | 10.4, 10.7
|EHV: 10           | 05                            |  0.5
|EHV: 19           | 01                            |  0.1
|EHV: others       | 00                            | ?

These parameters are likely s16div10.

## Packet type 16 - temperatures

Only observed on EHV\* heat pumps.

### Packet type 16: request

Header: 000016

| Byte(:bit) | Hex value observed | Description       | Data type
|:-----------|:-------------------|:------------------|:-
|    0-1     | 00                 | ?
|    2-3     | 32 14              | room temperature? | f8.8?
|    4-15    | 00                 | ?

### Packet type 16: response

Header: 400016

| Byte(:bit) | Hex value observed | Description | Data type
|:-----------|:-------------------|:------------|:-
|    0-7     | 00                 | ?
|     8      | E6                 | ?

# Packet types 20-2F - ?

Packet types 20-2F are not part of the basic package in the regular communication pattern. Their function is not clear yet.

##Packet type 20

Packet type 20 is communicated during restart procedure. Perhaps also later?

### Packet type 20: request

Header: 000020

| Byte nr       | Hex value observed            | Description           | Data type
|:--------------|:------------------------------|:----------------------|:-
|     0         | 00                            | ?

### Packet type 20: response

Header: 400020

| Byte nr       | Hex value observed            | Description           | Data type
|:--------------|:------------------------------|:----------------------|:-
|    0-19       | XX                            | various values?, tbd  |

## Packet type 21

Packet type 21 is communicated at the end of the restart procedure. Perhaps also later?

### Packet type 21: request

Header: 000021

Payload is empty.

### Packet type 21: response

Header: 400021

| Byte nr       | Hex value observed            | Description           | Data type
|:--------------|:------------------------------|:----------------------|:-
|  0-12         | XX                            | Tbd

# Packet types 30-3F form communication between main and auxiliary controller(s)

Packet types 30-3F are used for communication parameter settings between main and auxiliary controller(s), allowing an auxiliary controller to request the main controller to change various settings.

## Packet type 30 - Auxiliary controller initial communication

The main controller regularly polls the bus for external controllers (address Fx) using packet type 30. Packet 00F030 requests a response from the first external controller (identity F0). Some Daikin products allow two (or perhaps more) multiple external controllers - if controller F0 is in use, the main controller will poll for another external controller with packet 00F130. We have not observed more than two controllers. Payload bytes 0-13 indicate that the main controller would like to send additional requests (for packets type 31....3E) to the external controller. This happens later if the auxiliary controller is available, but not necessarily immediately.

If an external controller is available, it will respond with a packet "40Fx30". The response packet may indicate that the external controller would like to receive a certain request packet - this mechanism is used when a setting in the external controller is changed and should be communicated to the main controller and then to the heat pump. The additional packets 00F03x - and the reply 40F03x following it - are used to communicate status changes between main controller and external controller.

### Packet type 30: request

Header: 00F030 or 00F130 (where F0 or F1 refers to first or second auxiliary controller)

| Byte nr | Hex value observed | Description           | Data type
|:--------|:-------------------|:----------------------|:-
|  0-13   | 00-03              | indicates whether additional packets "00Fx3y" will be sent; byte 0 for 00F031 .. byte 13 for 00F03E  | u8

### Packet type 30: response

Header: 40F030 or 40F130

| Byte nr | Hex value observed | Description           | Data type
|:--------|:-------------------|:----------------------|:-
| 0-13    | 00/01              | indicates whether additional packets requests "00F03x" are needed by auxiliary controller to transmit setting changes; byte 0 for 00F031 .. byte 13 for 00F03E  | u8

## Packet type 31 - auxiliary controller ID, date, time

Header: 00F031 or 40F031

| Byte nr | Hex value observed | Description               | Data type
|:--------|:-------------------|:--------------------------|:-
|     0-2 | 00                 | ?                         | u8
|     3:0 | 1                  | operation realted?        | u8
|     3:7 | 0/1                | operation related?        | u8
| 3:other | 0                  | operation related?        | u8
|     4   | 74, B4             | auxiliary controller ID?  | u8
|     4:5 | 0/1                | operation related?        | u8
|     5   | 11,15,51,55        | auxiliary controller ID?  | u8
|     6   | 13 (example)       | date - year (0x13 = 2019) | u8
|     7   | 01-0C              | date - month              | u8
|     8   | 01-1F              | date - day of month       | u8
|     9   | 00-17              | time - hours              | u8
|     10  | 00-3B              | time - minutes            | u8
|     11  | 00-3B              | time - seconds            | u8

# Packet type 35 and 3A - 8 bit parameters exchange

Header: 00F035 or 40F035 or 00F135 or 40F135 or 00F03A or 40F03A or 00F13A or 40F13A

A few hundred 8-bit parameters can be exchanged via packet type 35 and 3A. Some are fixed (a range of parameters is used to communicate the device ID), others are related to operating settings.

| Byte nr   | Hex value observed | Description                             | Data type
|:----------|:-------------------|:----------------------------------------|:-
|     0-2   | XX YY ZZ           | communicates value ZZ of parameter YYXX | u16 + u8
|     3-5   | XX YY ZZ           | communicates value ZZ of parameter YYXX | u16 + u8
|     6-8   | XX YY ZZ           | communicates value ZZ of parameter YYXX | u16 + u8
|     9-11  | XX YY ZZ           | communicates value ZZ of parameter YYXX | u16 + u8
|    12-14  | XX YY ZZ           | communicates value ZZ of parameter YYXX | u16 + u8
|    15-17  | XX YY ZZ           | communicates value ZZ of parameter YYXX | u16 + u8

### Parameters for packet type 35

The parameter range in this system is 0x0000-0x0149, can be different on other models.

Frequently we see 2 parameters related to the same function: one is used by the auxiliary controller to request a change to the main controller, and the other one is used by the main controller to the auxiliary controller to confirm the request, and/or is the result of the operation once the heat pump has taken over the new setting.

These 2 parameters frequently differ by 1 or by 4 positions in parameter number.

| Param nr      | Hex value observed            | Description           | Data type
|:--------------|:------------------------------|:----------------------|:-
|  0008
|  000E         | 00/01                         | Circ.pump off/on
|  000A         | 00/01                         |
|  000E         | 00/01                         |
|  0011         | 00/01                         | DHW related ?
|  0012         | 00/01                         | DHW active or flow sensor
|  0013         | 01                            | ?
|  0019         | 00/01                         | kWh preference input off/on
|  0021         | 01                            | ?
|  0022         | 01                            | ?
|  0027         | 01                            | ?
|  002D         | 00/01                         | heating/cooling off/on  (on some systems: setting this parameter in a 40Fx35 response to 0x00/0x01 switches heating/cooling off/on)
|  002E         | 00/01                         | heating/cooling off/on
|  002F         | 00/01                         | heating/cooling off/on  (on some systems: setting this parameter in a 40Fx35 response to 0x00/0x01 switches heating/cooling off/on)
|  0030         | 00/01                         | heating/cooling off/on
|  0031         | 00/01                         | heating/cooling off/on  (on some systems: setting this parameter in a 40Fx35 response to 0x00/0x01 switches heating/cooling off/on)
|  0032         | 00/01                         | heating/cooling off/on
|  0037         | 00/01/04/05                   | Status gas boiler ? related to heating/cooling on/off, DHW on/off, and DHW water flow
|  0039         | 01/02                         | ?
|  003A         | 01/02                         | ?
|  003C         | 00/01
|  003E         | 00/01                         | DHW off/on (setting this parameter in a 40Fx35 response to 0x00/0x01 switches DHW off/on on some systems)
|  003F         | 00/01                         | DHW off/on
|  0040         | 00/01                         | DHW off/on (setting this parameter in a 40Fx35 response to 0x00/0x01 switches DHW off/on on some systems)
|  0041         | 00/01                         | DHW related ?
|  0042         | 00/01                         | DHW related ?
|  0047         | 00/01                         | DHW booster on/off (on some systems: writable)
|  0048         | 00/01                         | DHW booster on/off (on some systems: writable)
|  004E         | 00/01/02                      | 00=Heating/01=cooling/02=automatic mode
|  004F         | 01                            | ?
|  0050         | 00/01                         | DHW active or flow sensor
|  0055         | 00/01/02/03                   | modus ABS / WD / ABS+prog / WD+dev
|  0056         | 00/01/02/03                   | modus ABS / WD / ABS+prog / WD+dev (writable, 02/03 only in LWT modus)
|  005C         | 7F                            | ?
|  0088         | 01                            | ?
|  008D         | 02                            | ?
|  0093         | 02                            | ?
|  0098         | 01                            | indication manual setting?
|  009A         | 4B                            | ?
|  009B         | 00/01                         | 00 = room thermostat, 01 = LWT modus
|  009D         | XX                            | counter, schedule related
|  00A2         | XX                            | sequence counter/timer?
|  00B4         | 01                            | ?
|  00B6         | 01                            | ?
|  00B7         | 01                            | ?
|  00C2         | 01                            | ?
|  00C3         | 07                            | ?
|  00C5         | 05                            | ?
|  00C6         | 09                            | ?
|  00C7         | 08                            | ?
|  00C8         | 01                            | ?
|  00C9         | 01                            | ?
|  00CA         | 02                            | ?
|  00CC         | 04                            | ?
|  010C         | 01                            | ?
|  011B         | 0E                            | ?
|  011C         | 03                            | ?
|  011F         | 04                            | ?
|  0121         | 07                            | ?
|  0122         | 04                            | ?
|  013A..0145   | 45 48 59 48 42 58 30 38 41 41 56 33 | "EHYHBX08AAV3" name inside unit | c8
|  0146..0149   | 00                            | "\0" ?                | c8?
|  FFFF         | FF                            | Padding pattern | u8

### Parameters for packet type 3A

The parameter range in this system is 0x0000-0x006B, can be different on other models.

| Param nr | Description                    | Data type
|:---------|:-------------------------------|:-
| 0000     | 24h format                     | u8
| 0031     | Holiday enable                 | u8
| 003B     | Dec delimiter                  | u8
| 003D     | Flow-unit                      | u8
| 003F     | Temp unit                      | u8
| 0040     | Energy unit                    | u8
| 004B     | DST                            | u8
| 004C     | Silent mode                    | u8
| 004D     | Silent level                   | u8
| 004E     | Operation heating/cooling mode (writable on some systems, 00=heating 01=cooling 02=auto) | u8
| 005B     | Holiday                        | u8
| 005E     | Heating schedule               | u8
| 005F     | Cooling schedule               | u8
| 0064     | DHW schedule                   | u8
| FFFF     | Padding pattern                | u8

# Packet type 36 and 3B - 16 bit parameters exchange

Header: [04]00F[01]03[6B]

A few hundred 16-bit parameters can be exchanged via packet type 36 and 3A.

| Byte nr       | Hex value observed | Description                               | Data type
|:--------------|:-------------------|:------------------------------------------|:-
|     0-3       | XX YY ZZ ZZ        | communicates value ZZZZ of parameter YYXX | u16 + u16
|     4-7       | XX YY ZZ ZZ        | communicates value ZZZZ of parameter YYXX | u16 + u16
|     8-11      | XX YY ZZ ZZ        | communicates value ZZZZ of parameter YYXX | u16 + u16
|    12-15      | XX YY ZZ ZZ        | communicates value ZZZZ of parameter YYXX | u16 + u16
|    16-19      | XX YY ZZ ZZ        | communicates value ZZZZ of parameter YYXX | u16 + u16

### Parameters for packet type 36

The parameter range in this system is 0x0000-0x002C, can be different on other models.

| Param nr | Description                              | HA? | MQTT/HA name                  | Data type
|:---------|:-----------------------------------------|:----|:------------------------------|:-
| 0000     | Room temperature setpoint                | no  | Target_Temperature_Room       | u16div10
| 0001     | Temperature?                             | no  |                               | u16div10
| 0002     | Room temperature?                        | no  |                               | u16div10
| 0003     | DHW setpoint (writable on some systems)  | no  |                               | u16div10
| 0004     | DHW setpoint                             | no  |                               | u16div10
| 0006     | LWT target Main Zone (writable on some systems)                    | no  |                               | u16div10
| 0008     | LWT weather-dep mode deviation Main Zone (writable on some systems) | no  |                               | s16
| 0009
| 000B     | LWT target Add Zone                      | no  |                               | u16div10
| 000D     | LWT weather-dep mode deviation Add Zone (writable on some systems)  | no  |                               | s16
| 000F     | LWT weather-dep target Add Zone          | no  |                               | u16div10
| 0002     | TempRoom1                                | no  |                               | u16div10
| 0011     | Tempout1 (0.5C)                          | no  |                               | u16div10
| 0012     | Tempout2 (0.1C)                          | no  |                               | u16div10
| 0013     | TempRoom2                                | no  |                               | u16div10
| 0014     | TempRWT                                  | no  |                               | u16div10
| 0015     | TempMWT                                  | no  |                               | u16div10
| 0016     | TempLWT                                  | no  |                               | u16div10
| 0017     | TempRefr1                                | no  |                               | u16div10
| 0018     | TempRefr2                                | no  |                               | u16div10
| 0027     | Temp (55.0C)?                            | no  |                               | u16div10
| 002A     | Flow                                     | no  |                               | u16div10
| 002B     | software version outer unit              | no  |                               | u16hex_BE     
| 002C     | software version inner unit              | no  |                               | u16hex_BE
| FFFF     | Padding pattern                          | no  |                               | u16

### Parameters for packet type 3B

The parameter range in this system is 0x0000-0x00AE, can be different on other models.

| Param nr | Description                              | Data type
|:---------|:-----------------------------------------|:-
| FFFF     | Padding pattern                          | u16

# Packet type 37 and 3C - 24 bit parameters exchange

Header: [04]00F[01]3[7C]

A few hundred 24-bit parameters can be exchanged via packet type 37 and 3C.

| Byte nr       | Hex value observed | Description                                 | Data type
|:--------------|:-------------------|:--------------------------------------------|:-
|     0-4       | XX YY ZZ ZZ ZZ     | communicates value ZZZZZZ of parameter YYXX | u16 + u24
|     5-9       | XX YY ZZ ZZ ZZ     | communicates value ZZZZZZ of parameter YYXX | u16 + u24
|    10-14      | XX YY ZZ ZZ ZZ     | communicates value ZZZZZZ of parameter YYXX | u16 + u24
|    15-19      | XX YY ZZ ZZ ZZ     | communicates value ZZZZZZ of parameter YYXX | u16 + u24

### Parameters for packet type 37

The parameter range in this system is 0x0000-0x0000, can be different on other models.

| Param nr      | Description                        | Data type
|:--------------|:-----------------------------------|:-
| FFFF          | Padding pattern                    | u24

### Parameters for packet type 3C

The parameter range in this system is 0x0000-0x0001, can be different on other models.

| Param nr      | Description                        | Data type
|:--------------|:-----------------------------------|:-
| 0000          | Start of holiday                   | t24
| 0001          | End of holiday                     | t24
| FFFF          | Padding pattern                    | u24

# Packet type 38, 39, and 3D - 32 bit parameters exchange

Header: [04]00F[01]3[89D]

A few hundred 32-bit parameters can be exchanged via packet types 38, 39 and 3D.

| Byte nr       | Hex value observed | Description                                   | Data type
|:--------------|:-------------------|:----------------------------------------------|:-
|     0-5       | XX YY ZZ ZZ ZZ ZZ  | communicates value ZZZZZZZZ of parameter YYXX | u16 + u32
|     6-11      | XX YY ZZ ZZ ZZ ZZ  | communicates value ZZZZZZZZ of parameter YYXX | u16 + u32
|    12-17      | XX YY ZZ ZZ ZZ ZZ  | communicates value ZZZZZZZZ of parameter YYXX | u16 + u32

The parameter range in this system is 0x0000-0x001E for packet type 38, 0x0000-0x00EF for packet type 39, and 0x0000-0x001F for packet type 3D

### Parameters for packet type 38 - (counters, #hours, #starts)

The parameter range in this system is 0x0000-0x001E, it can be different on other models.

These parameters resemble the counters in packet type B8 but there are some differences and the order is different.

| Param nr | Description                                        | Data type
|:---------|:---------------------------------------------------|:-
| 0000     | Energy consumed (kWh) by backup heater for heating | u32
| 0001     | Energy consumed (kWh) by backup heater for DHW     | u32
| 0002     | Energy consumed (kWh) by compressor for heating    | u32
| 0003     | Energy consumed (kWh) by compressor for cooling    | u32
| 0004     | Energy consumed (kWh) by compressor for DHW        | u32
| 0005     | Energy consumed (kWh) total                        | u32
| 0006     | Energy produced (kWh) for heating                  | u32
| 0007     | Energy produced (kWh) for cooling                  | u32
| 0008     | Energy produced (kWh) for DHW                      | u32
| 0009     | Energy produced (kWh) total                        | u32
| 000A     | ?                                                  | u32
| 000B     | ?                                                  | u32
| 000C     | ?                                                  | u32
| 000D     | pump hours                                         | u32
| 000E     | compressor for heating                             | u32
| 000F     | compressor for cooling                             | u32
| 0010     | compressor for DHW                                 | u32
| 0011     | backup heater1 for heating                         | u32
| 0012     | backup heater1 for DHW                             | u32
| 0013     | backup heater2 for heating                         | u32
| 0014     | backup heater2 for DHW                             | u32
| 0015     | ?                                                  | u32
| 0016     | ?                                                  | u32
| 0017     | ?                                                  | u32
| 0018     | ?                                                  | u32
| 0019     | ?                                                  | u32
| 001A     | boiler operating hours for heating                 | u32
| 001B     | boiler operating hours for DHW                     | u32
| 001C     | number of compressor starts                        | u32
| 001D     | number of boiler starts                            | u32
| 001E     | ?                                                  | u32
| FFFF     | Padding pattern                                    | u32

### Parameters for packet type 39

Packet type 39 is used to communicate the field settings from the main to the auxiliary controller (and likely vice versa).

The parameter range in this system is 0x0000-0x00EF, this can be different on other models.

Observations from field setting changes from auxiliary controller to main controller (to change settings) are welcome - also on how the protocol triggers a system restart which is required after changing any of these settings.

Surprisingly, some field settings ([A-00] .. [A-04] and [B-00] .. [B-04]) are sometimes transmitted as having a range of 0-7, perhaps when active on the main controller in installer's mode, and otherwise just as having a range of 0-0. These field settings A/B are unspecified anyway.

| Param nr      | Description                        | Data type
|:--------------|:-----------------------------------|:-
| 0000          | Field setting [0-00]               | 4 byte field setting
| 0001          | Field setting [0-01]               | 4 byte field setting
| ..            |                                    |                     
| 000E          | Field setting [0-0E]               | 4 byte field setting
| 000F          | Field setting [1-00]               | 4 byte field setting
| 0010          | Field setting [1-01]               | 4 byte field setting
| ..            |                                    |                     
| 001D          | Field setting [1-0E]               | 4 byte field setting
| 001E          | Field setting [2-00]               | 4 byte field setting
| 001F          | Field setting [2-01]               | 4 byte field setting
| ..            |                                    |                     
| 00EE          | Field setting [F-0D]               | 4 byte field setting
| FFFF          | FF FF FF FF                        | u32

| Byte nr       | Hex value observed            | Description                      | Data type
|:--------------|:------------------------------|:---------------------------------|:-
|  0:7-6        | bb                            | 00                               | u2
|  0:5-0        |   bb bbbb                     | field value                      | u6
|  1            | XX                            | field value maximum (in steps)   | u8
|  2            | XX                            | field value offset               | u8
|  3:7-1        | b0bb b0b                      | field value step size            | sfp7
|  3:0          | b                             | always 1 for comm to auxiliary controller | bit

Step value, 7-bit signed floating point sfp7, observed values:

| Bit 6-3 mantissa | Bit 7 exponent sign | Bit (2?-)1 exponent | field value step size
|:-----------------|:--------------------|:--------------------|:-
| 0001             |  1                  |  01                 | 0.1
| 0010             |  1                  |  01                 | 0.2
| 0101             |  1                  |  01                 | 0.5
| 0001             |  0                  |  00                 | 1.0
| 0101             |  0                  |  00                 | 5.0
| 0001             |  0                  |  01                 | 10 
| 0101             |  0                  |  01                 | 50 

### Parameters for packet type 3D

The parameter range in this system is 0x0000-0x001F, can be different on other models.

| Param nr      | Description                        | Data type
|:--------------|:-----------------------------------|:-
| FFFF          | Padding pattern                    | u32

## Packet type 3E - scheduler memory communication

Communicates memory data segments, mostly used for weekly schedule communication, but perhaps also for some other purposes.

The data below shows data communicated from main to auxiliary controller. We do not know exactly how the communication is done in the other direction, but it very likely will be similar.

### Packet type 3E: request

Header: 00003E

#### Packet type 3E, subtype 00, data segment header

If the first payload byte is 00, it announces a data segment communication

| Byte nr | Hex value observed | Description             | Data type
|:--------|:-------------------|:------------------------|:-
|     0   | 00                 | Data segment definition | f8.8
|   1-2   | XX XX              | Memory location         | u16
|   3-4   | XX 00              | Length of data segment  | u16 (could be u8, as we never saw segments larger than 84 bytes)

#### Packet type 3E, subtype 01, data segment contents

If the first payload byte is 01, it communicates memory data. The next byte countes the payloads used from 00.

| Byte nr | Hex value observed | Description             | Data type
|:--------|:-------------------|:------------------------|:-
|     0   | 01                 | Data follows            | f8.8
|     1   | 00-04              | Payload counter         | u16
|   2-19  | XX                 | Data segment            | u8 (18x)

The last data segment is padded with FFs.

### Packet type 3E: response

Header: 40003E

#### Packet type 3E, subtype 00, data segment header

| Byte nr | Hex value observed | Description             | Data type
|:--------|:-------------------|:------------------------|:-
|     0   | 00                 | Data segment definition | f8.8
|   1-4   | FF                 |

#### Packet type 3E, subtype 01, data segment contents

| Byte nr | Hex value observed | Description             | Data type
|:--------|:-------------------|:------------------------|:-
|     0   | 01                 | Data follows            | f8.8
|   1-19  | FF                 |

### Memory for weekly schedules

Writes to memory locations 0x0250-0x070D have been observed.

Writes to 0x0250-0x0693 occur in data segment runs of 28, 56, or 84 bytes, which are weekly schedules with 2, 4, or 6 schedule moments/week day. Weekly schedules relate a.o. to heating/cooling modes and silent mode operation. Some of the unknown weekly schedules could be schedules for the addtional zone.

Writes to 0x0694-0x070D occur in data segment runs of 12, 15, 35, or 45 bytes. Its function is not yet clarified.

| Address | Length    | Description
|:--------|:----------|:-
|  0250   | 001C (28) | Weekly schedule silent mode, 2 moments/day, starting on Monday
|  026C   | 001C (28) | Weekly schedule (?), 2 moments/day
|  0288   | 0038 (56) | Weekly schedule (?), 4 moments/day
|  02C0   | 0038 (56) | Weekly schedule (?), 4 moments/day
|  02F8   | 0054 (84) | Weekly schedule, 6 moments/day
|  034C   | 0054 (84) | Weekly schedule, 6 moments/day
|  03A0   | 0054 (84) | Weekly schedule, 6 moments/day
|  03F4   | 0054 (84) | Weekly schedule, 6 moments/day
|  0448   | 0054 (84) | Weekly schedule heating own program 1, 6 moments/day
|  049C   | 0054 (84) | Weekly schedule heating own program 2, 6 moments/day
|  04F0   | 0054 (84) | Weekly schedule heating own program 3, 6 moments/day
|  0544   | 0054 (84) | Weekly schedule cooling own program 1, 6 moments/day
|  0598   | 0054 (84) | Weekly schedule, 6 moments/day
|  05EC   | 0054 (84) | Weekly schedule, 6 moments/day
|  0640   | 0054 (84) | Weekly schedule, 6 moments/day
|  0694   | 000C (12) | ?
|  06A0   | 000F (15) | Phone number customer service UI
|  06AF   | 000F (15) | ?
|  06BE   | 0023 (35) | Error history (format: 5x E1 E2 YY MM DD HH mm)
|  06E1   | 002D (45) | Names of 3 heating schedules (15 characters each)

Error codes: tbd, HJ-11 is coded as 4D0B, 89-3 is coded as B903 (different coding mechanism than in packet type 10).

### Memory contents (weekly schedules)

Each 2-byte slot for a schedule moment consists of

| Byte nr | Hex value observed | Description                                         | Data type
|:--------|:-------------------|:----------------------------------------------------|:-
|     0   | 00-8F              | Time of new schedule (in 0:10 increments, 8F=23:50) | t8
|     1   | XX                 | new data value (format below or to be determined)   | u8

#### Data value for weekly schedule silent mode

| Value   | Description
|:--------|:-
|    00   | Silent mode off
|    01   | level 1
|    02   | level 2
|    03   | level 3

#### Data value for weekly schedule heating/cooling

| Value   | Description
|:--------|:-
|    00   | eco
|    01   | comfort 
| 0C..34  | -10 .. +10  (to be confirmed)

# Packet types 60..8F (and FF) communicate field settings

Packet types 60..8F communicate field settings between main controller and heat pump. This occurs after a restart, when all field settings are sent (in responses) by the heat pump, and during operation when a field setting is manually changed by the main controller.

For some reason, 4 000086/000087 packets will be inserted at certain moments even though these do not communicate any usable field settings.

Each packet type contains 5 field settings. The packet type corresponds to the field settings according to the following table:

| Packet type   | Field settings
|:--------------|:-
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

### Packet type 60..8F: request

Header: 000060..00008F

Field setting information request by main controller.

#### Request during restart:

| Byte nr       | Hex value observed            | Description           | Data type
|:--------------|:------------------------------|:----------------------|:-
|  0-19         | 00                            | no information        | 

#### Request during operation (after manual change of field setting on main controller):

| Byte nr       | Hex value observed            | Description                      | Data type
|:--------------|:------------------------------|:---------------------------------|:-
|  0:7-6        | bb                            | 01 for new value, 00 otherwise   | u2
|  0:5-0        |   bb bbbb                     | (new/old) field value            | u6
|  1            | XX                            | field value maximum (in steps)   | u8
|  2            | XX                            | field value offset               | u8
|  3:7-1        | b0bb b0b                      | field value step size            | sfp7
|  3:0          | b                             | 0 for communication to heat pump | bit
|  4-7          | XX XX XX XX                   | 2nd field setting, see bytes 0-3 | u8,u8,u8,sfp7+bit
|  8-11         | XX XX XX XX                   | 3rd field setting, see bytes 0-3 | u8,u8,u8,sfp7+bit
| 12-15         | XX XX XX XX                   | 4th field setting, see bytes 0-3 | u8,u8,u8,sfp7+bit
| 16-19         | XX XX XX XX                   | 5th field setting, see bytes 0-3 | u8,u8,u8,sfp7+bit

Step value, 7-bit signed floating point sfp7, observed values:

| Bit 6-3 mantissa | Bit 7 exponent sign | Bit (2?-)1 exponent | field value step size
|:-----------------|:--------------------|:--------------------|:-
| 0001             |  1                  |  01                 | 0.1
| 0010             |  1                  |  01                 | 0.2
| 0101             |  1                  |  01                 | 0.5
| 0001             |  0                  |  00                 | 1.0
| 0101             |  0                  |  00                 | 5.0
| 0001             |  0                  |  01                 | 10 
| 0101             |  0                  |  01                 | 50 

### Packet type 60..8F and FF: response

Header: 400060..40008F or 4000FF

#### Response:

If none of the field settings in a packet reply are supported, the response header is 4000FF and the payload is empty. Otherwise, if some of the field settings in a packet reply are not supported, 0000000000 is used as padding pattern.

Payload for supported field settings:

| Byte nr | Hex value observed | Description                    | Data type
|:--------|:-------------------|:-------------------------------|:-
|  0:7-6  | bb                 | (usually 11, but 10 or 00 for some unspecified/system-type field settings) | u2
|  0:5-0  |   bb bbbb          | field value                    | u6
|  1      | XX                 | field value maximum            | u8
|  2      | XX                 | field value offset             | u8
|  3:7-1  | b0bb b0b           | field value step value         | sfp7
|  3:0    | 0                  | 0                              | bit
|  4-7    | XX XX XX XX        | 2nd field value, see bytes 0-3 | u8,u8,u8,sfp7+bit
|  8-11   | XX XX XX XX        | 3rd field value, see bytes 0-3 | u8,u8,u8,sfp7+bit
| 12-15   | XX XX XX XX        | 4th field value, see bytes 0-3 | u8,u8,u8,sfp7+bit
| 16-19   | XX XX XX XX        | 5th field value, see bytes 0-3 | u8,u8,u8,sfp7+bit

# Packet types 90..9F (and FF) communicate unknown data

Packet types 90..9F communicate data from heat pump to main controller during restart procedure. Its purpose has not been clarified.

### Packet type 90..9F: request

Header: 000090..00009F

This request has only been observed in the restart sequence.

| Byte nr       | Hex value observed            | Description           | Data type
|:--------------|:------------------------------|:----------------------|:-
|  0-19         | 00                            | All fields empty

### Packet type 90..9F and FF: response

Header: 400090..40009F or 4000FF

| Byte nr       | Hex value observed            | Description           | Data type
|:--------------|:------------------------------|:----------------------|:-
|  0-19         | XX                            | Tbd

# Packet types A1 and B1 communicate text data

## Packet type A1

Used for name of product?

### Packet type A1: request

Header: 0000A1

| Byte nr       | Hex value observed            | Description           | Data type
|:--------------|:------------------------------|:----------------------|:-
|     0         | 00                            | ?
|  1-15         | 30                            | ASCII '0'             | c8
| 16-17         | 00                            | ASCII '\0'            | c8

### Packet type A1: response

Header: 4000A1

| Byte nr       | Hex value observed            | Description           | Data type
|:--------------|:------------------------------|:----------------------|:-
|     0         | 00                            | ?
|  1-15         | 00                            | ASCII '\0' (missing) name outside unit | c8
| 16-17         | 00                            | ASCII '\0'            | c8

## Packet type B1 - heat pump name

Product name.

### Packet type B1: request

Header: 0000B1

| Byte nr       | Hex value observed            | Description           | Data type
|:--------------|:------------------------------|:----------------------|:-
|     0         | 00                            | ?
|  1-15         | 30                            | ASCII '0'             | c8
| 16-17         | 00                            | ASCII '\0'            | c8

### Packet type B1: response

Header: 4000B1

| Byte nr       | Hex value observed            | Description           | Data type
|:--------------|:------------------------------|:----------------------|:-
|     0         | 00                            | ?
|  1-12         | XX                            | ASCII "EHYHBH08AAV3" name inside unit | c8
| 13-17         | 00                            | ASCII '\0'            | c8

# Packet type B8 - counters, #hours, #starts, electricity used, energy produced

Counters for energy consumed and operating hours. The main controller specifies which data type it would like to receive. The heat pump responds with the requested data type counters. A B8 package is only transmitted by the main controller after a manual menu request for these counters. P1P2Monitor can insert B8 requests to poll these counters, but this violates the rule that an auxiliary controller should not act as main controller. But if timed carefully, it works.

### Packet type B8: request

Header: 0000B8

| Byte nr       | Hex value observed            | Description           | Data type
|:--------------|:------------------------------|:----------------------|:-
|     0         | XX                            | data type requested <br> 00: energy consumed <br> 01: energy produced <br> 02: pump and compressor hours <br> 03: backup heater hours <br> 04: compressor starts <br> 05: boiler hours and starts | u8

### Packet type B8: response

Header: 4000B8

#### Data type 00

| Byte nr       | Hex value observed            | Description                           | Data type
|:--------------|:------------------------------|:--------------------------------------|:-
|     0         | 00                            | data type 00 : energy consumed (kWh)  | u8
|   1-3         | XX XX XX                      | by backup heater for heating          | u24
|   4-6         | XX XX XX                      | by backup heater for DHW              | u24
|   7-9         | 00 XX XX                      | by compressor for heating             | u24
| 10-12         | XX XX XX                      | by compressor for cooling             | u24
| 13-15         | XX XX XX                      | by compressor for DHW                 | u24
| 16-18         | XX XX XX                      | total                                 | u24

#### Data type 01

| Byte nr       | Hex value observed            | Description                          | Data type
|:--------------|:------------------------------|:-------------------------------------|:-
|     0         | 01                            | data type 01 : energy produced (kWh) | u8
|   1-3         | XX XX XX                      | for heating                          | u24
|   4-6         | XX XX XX                      | for cooling                          | u24
|   7-9         | XX XX XX                      | for DHW                              | u24
| 10-12         | XX XX XX                      | total                                | u24

#### Data type 02

| Byte nr       | Hex value observed            | Description                    | Data type
|:--------------|:------------------------------|:-------------------------------|:-
|     0         | 02                            | data type 02 : operating hours | u8
|   1-3         | XX XX XX                      | pump hours                     | u24
|   4-6         | XX XX XX                      | compressor for heating         | u24
|   7-9         | XX XX XX                      | compressor for cooling         | u24
| 10-12         | XX XX XX                      | compressor for DHW             | u24

#### Data type 03

| Byte nr       | Hex value observed            | Description                    | Data type
|:--------------|:------------------------------|:-------------------------------|:-
|     0         | 03                            | data type 03 : operating hours | u8
|   1-3         | XX XX XX                      | backup heater1 for heating     | u24
|   4-6         | XX XX XX                      | backup heater1 for DHW         | u24
|   7-9         | XX XX XX                      | backup heater2 for heating     | u24
| 10-12         | XX XX XX                      | backup heater2 for DHW         | u24
| 13-15         | XX XX XX                      | ?                              | u24
| 17-18         | XX XX XX                      | ?                              | u24

#### Data type 04

| Byte nr       | Hex value observed            | Description                 | Data type
|:--------------|:------------------------------|:----------------------------|:-
|     0         | 04                            | data type 04                | u8
|   1-3         | XX XX XX                      | ?                           | u24
|   4-6         | XX XX XX                      | ?                           | u24
|   7-9         | XX XX XX                      | ?                           | u24
| 10-12         | XX XX XX                      | number of compressor starts | u24

#### Data type 05

| Byte nr       | Hex value observed            | Description                               | Data type
|:--------------|:------------------------------|:------------------------------------------|:-
|    0          | 05                            | data type 05 : gas boiler in hybrid model | u8
|  1-3          | XX XX XX                      | boiler operating hours for heating        | u24
|  4-6          | XX XX XX                      | boiler operating hours for DHW            | u24
|  7-9          | XX XX XX                      | gas usage for heating (unit tbd)          | u24
| 10-12         | XX XX XX                      | gas usage for heating (unit tbd)          | u24
| 13-15         | XX XX XX                      | number of boiler starts                   | u24
| 16-18         | XX XX XX                      | gas usage total (unit tbd)                | u24

Internal gas metering seems only supported on newer models, not on the AAV3.

# Restart process

The restart process is initiated by setting byte 16 of packet "000012.." to 0x61. The heat pump stops answering requests almost immediately. 

### Initial sequence

During the initial sequence and test patterns, the main controller (or another entity taking over this role) uses address 0x80 instead of 0x40

- Request header: 808000, payload empty
- Response header: 400000 payload empty
- Request header: 808000, payload empty
- Response header: 400000 payload empty
- Request header: 808000, payload empty
- Response header: 400000 payload empty
- Request header: 808001, payload empty
- No response
- Request header: 808001, payload empty
- No response
- Request header: 808001, payload empty
- No response
- Request header: 808003, payload: 00FFFFFF
- Response header: 408003 payload empty

### Test patterns

23 request/response pairs for XX increasing from 01 to 17:
- Request header: 080003, payload: XX YY YY YY where YY YY YY has bit pattern [1 (23-i)x1 (i-1)x0 1]
- No response

Followed by one time:
- Request header: 808004, payload: 00010000
- Response header: 400004, payload empty

And followed by 24 request/response pairs for XX increasing from 00 to 17:
- Request header: 080003, payload: XX YY YY YY where YY YY YY has bit pattern [1 (23-i)x1 (i-1)x0 1]
- No response

### Start of main controller using 0x00 as identification

- Request header: 000005, payload empty
- Response header: 400005, payload: 0100000100000100000100000100000100000100

Followed by 23 requests for XX increasing from 01 to 23:
- Request header: 00XX05, payload empty
- No response

### Packet type 20 communication

- Request header: 000020, payload: 00
- Response header: 400020, payload: <20 bytes, function tbd>

### Communication of field settings

The heat pump communicates all field settings (format and data value) to main controller, for packet types 60-8F as described above.

48 requests for XX increasing from 60 to 8F:
- Request header: 0000XX, payload: <20 bytes 00>
- Response either header: 4000XX, payload: <4 5-byte field settings> or header: 4000FF, payload empty

### Communication of unknown data

48 requests for XX increasing from 90 to 9F:
- Request header: 0000XX, payload: <12 bytes 00>
- Response either header: 4000XX, payload: <20 bytes, function ?> or header: 4000FF, payload empty

## Product names

- Request header: 0000A1, payload: <00 15x30 00 00>
- Response header: 4000A1, payload: <18 bytes 00>
- Request header: 0000B1, payload: <00 15x30 00 00>
- Response header: 4000B1, payload: <00 "EHYHBX08AAV3" 00 00 00 00 00>

## Packet type B8 - counters, #hours, #starts

- Request header: 0000B8, payload: XX
- Response header: 4000B8, payload: <XX and 4 or 6 3-byte counters>

## Packet type 21 communication

- Request header: 000020, payload: 00
- Response header: 400020, payload: <20 bytes, function tbd>

## Start of regular package communication

Restart procedure finished, start of regular pattern:
- Request header: 000010, regular payload 
- Response header: 400010, regular payload
- Request header: 000011, regular payload
- Response header: 400011, regular payload
- Request header: 000012, regular payload
- Response header: 400012, regular payload
...
