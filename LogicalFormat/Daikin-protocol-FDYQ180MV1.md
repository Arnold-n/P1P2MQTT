# FDYQ180MV1 protocol data format

Protocol data format for a FDYQ180MV1 Daikin ducted model, which seems to be specific for the Australian market. The accompanying outdoor unit is RZYQ7PY1. 

Please read the common README.md first for the general format description.

All values in this document are hex, except for the byte numbering in the payload and time (ms) references.

### Credits

Many thanks for John De Boni for reverse engineering this protocol and for providing the core of the controlling code for this system.

### Packet type

Each packet has in the header a packet type (and in certain cases a packet subtype as first payload byte) to identify the payload function. Packet type numbers observed so far are:
- 10, 11, 1F for regular data packets between main controller and heat pump (peripheral address 0x00)
- 30, 37, 3B, 3C communication between the main controller and an auxiliary controller (peripheral address 0xF0)
- 20 perhaps for polling additional peripherals/sensors

Packet 10 and 3B enabling monitoring main system settings, packet 11 the temperature(s).

Packet 3B is relevant for control of the main system settings.

In addition, a few packet types originating from 0x80 are observed, and remain unanswered. Their function has not been clarified yet:
- 800018 with 7-byte all-zero payload, no reply until time-out
- 808000 with empty payload, no reply until time-out
- 80F034 with 4-byte payload 10174401 or 10174501, no reply until time-out

### Timing between packets

- approximately 25ms or 50ms between request of main controller and reply of heat pump
- approximately 25ms up to 155ms between reply communication of heat pump and next request of main controller
- approximately 140 or 160 ms between last packet of a package and the first packet of a new package

## Packet type 00 - unknown

### Packet type 00: request

Observed at irregular moments.

Header: 808000 (note source = 0x80, target = 0x80)

Payload length: 0

### Packet type 00: reply

No reply observed. Perhaps 808000 is not even a request?

## Packet type 10 - operating status

### Packet type 10: request

Header: 000010

Payload length: 20

| Byte(:bit) | Hex value observed | Description              | Data type
|:-----------|:-------------------|:-------------------------|:-
| 0          | 00                 | Power off/on             | u8 
| 1          | 61                 | Target operating mode    | u8 
| 2          | 01                 | Actual operating mode    | u8
| 3          | 18                 | Target temperature       | u8
| 5          | 10                 | ?                        | u8
| 7          | 02                 | Fan speed?               | u8
| 13         | 80                 | ?                        | u8
| 14         | 40                 | ?                        | u8
| 15         | 01                 | ?                        | u8
| others     | 00                 | ?                        |

Operating mode

| Value     | Description
|:----------|:-
| 60        | fan/off?
| 61        | heat
| 62        | cool
| 63        | auto
| 64        | ?
| 65        | ?
| 66        | ?
| 67        | dry

Actual operating mode

| Value     | Description
|:----------|:-
| 00        | off or fan ?
| 01        | heat
| 02        | cool

Fan speed

| Value      | Description
|:-----------|:-
| 0          | low
| 1          | medium
| 2          | high

### Packet type 10: reply

Header: 400010

Payload length: 20

| Byte(:bit) | Hex value observed | Description              | Data type
|:-----------|:-------------------|:-------------------------|:-
| 1?         | 00                 | Power off/on             | u8 
| 2          | 61                 | Target operatin  mode    | u8 
| 3          | 01                 | Actual operating mode    | u8
| 4          | 18                 | Target temperature       | u8
| 6          | 10                 | ?                        | u8
| 8          | 3A                 | ?                        | u8
| 14         | 80                 | ?                        | u8
| 17         | 01                 | ?                        | u8
| 19         | 07                 | ?                        | u8
| others     | 00                 | ?                        |

## Packet type 11 - temperatures

### Packet type 11: request

Header: 000011

Payload length: 10

| Byte(:bit) | Hex value observed | Description              | Data type
|:-----------|:-------------------|:-------------------------|:-
| 2          | 01                 | ?                        | u8
| 3          | 01                 | ?                        | u8
| 5-6        | 16 64/7C/94/A8     | Temperature              | f8.8
| others     | 00                 | ?                        |

### Packet type 11: reply

Header: 400011

Payload length: 8

| Byte(:bit) | Hex value observed | Description              | Data type
|:-----------|:-------------------|:-------------------------|:-
| 5-6        | 16 64/7C/94/A8     | Temperature              | f8.8
| others     | 00                 | ?                        |

## Packet type 18 - unknown

### Packet type 18: request

Frequently observed, at irregular moments.

Header: 800018 (note source = 0x80)

Payload length: 7

| Byte(:bit) | Hex value observed | Description              | Data type
|:-----------|:-------------------|:-------------------------|:-
| 0-6        | 00                 | ?                        |

### Packet type 18: reply

No reply observed. Perhaps 800018 is not even a request?

## Packet type 1F - unknown

### Packet type 1F: request

Frequently observed.

Header: 00001F

Empty payload.

### Packet type FF: reply

Header: 4000FF

A reply is observed with packet type FF (instead of 1F), and an empty payload.

# Packet type 20 - sensor polling ?

### Packet type 20: request

Header: 00xx20 (with x varying from 00 to 10)

Payload length: 1

| Byte(:bit) | Hex value observed | Description              | Data type
|:-----------|:-------------------|:-------------------------|:-
| 0          | 04                 | ?                        |

### Packet type 20: reply

Only 000020 is replied to in this system. The other 00xx20 requests remain unanswered (time-out is approximately 150ms)

Header: 400020

Payload length: 20

| Byte(:bit) | Hex value observed | Description              | Data type
|:-----------|:-------------------|:-------------------------|:-
| 0          | 02                 | ?                        |
| 1          | 23                 | ?                        |
| 2          | A0                 | ?                        |
| 3          | 00                 | ?                        |
| 4          | 20                 | ?                        |
| 5          | 10                 | ?                        |
| 6          | 20                 | ?                        |
| 7          | 10                 | ?                        |
| 8          | 20                 | ?                        |
| 9          | 08                 | ?                        |
| 10         | 2D                 | ?                        |
| 11         | 40                 | ?                        |
| 12         | 7E                 | ?                        |
| 13         | 10                 | ?                        |
| 14         | C8                 | ?                        |
| 15         | 0D                 | ?                        |
| 16         | 00                 | ?                        |
| 17         | 52                 | ?                        |
| 18         | 8C                 | ?                        |
| 19         | 08                 | ?                        |


# Packet types 30-3F form communication between main and auxiliary controller(s)

Only a few of these packet types have been observed: 30, 37, 3B, 3C.

## Packet type 30 - polling auxiliary controller

### Packet type 30: request

Header: 00F030

Payload length: 20

| Byte(:bit) | Hex value observed | Description                      | Data type
|:-----------|:-------------------|:---------------------------------|:-
| 0          | 00                 | ?                                |
| 1          | 23                 | same as 400020-reply-byte-1      |
| 2          | A0                 | same as 400020-reply-byte-2      |
| 3          | 00                 | same as 400020-reply-byte-3      |
| 4          | 20                 | same as 400020-reply-byte-4      |
| 5          | 10                 | same as 400020-reply-byte-5      |
| 6          | 20                 | same as 400020-reply-byte-6      |
| 7          | 10                 | same as 400020-reply-byte-7      |
| 8          | 08                 | same as 400020-reply-byte-9      |
| 9          | 2D                 | same as 400020-reply-byte-10     |
| 10         | 00                 | ?                                |
| 11         | 7E                 | same as 400020-reply-byte-12     |
| 12         | 10                 | same as 400020-reply-byte-13     |
| 13         | C8                 | same as 400020-reply-byte-14     |
| 14         | 0D                 | same as 400020-reply-byte-15     |
| 15         | 00                 | same as 400020-reply-byte-16     |
| 16         | 52                 | same as 400020-reply-byte-17     |
| 17         | 8C                 | same as 400020-reply-byte-18     |
| 18         | 08                 | same as 400020-reply-byte-19     |
| 19         | 02                 | same as 400020-reply-byte-0 ?    |

### Packet type 30: reply

Header: 40F030

Payload length: an empty reply results in the main controller issues a second 00F030 communication after which the main controller continues the main/aux controller communication (i.e., accepts the auxiliary controller). No further 00F030 communications were observed for some time afterwards.

## Packet type 37 - zone names and schedule data (?)

### Packet type 37 subtype 00-06: request with schedule data (?)

Because there are 7 equal packet subtypes, we guess these are daily schedules for 7 week days.

Header: 00F037

Payload length: 16

| Byte(:bit) | Hex value observed | Description              | Data type
|:-----------|:-------------------|:-------------------------|:-
| 0          | 00-06              | subtype                  | u8
| 1          | C0                 | ?                        |
| 2          | 00                 | ?                        |
| 3          | 00                 | ?                        |
| 4          | 00                 | ?                        |
| 5          | 00                 | ?                        |
| 6          | 32                 | ?                        |
| 7          | 01                 | ?                        |
| 8          | 00                 | ?                        |
| 9          | 00                 | ?                        |
| 10         | 00                 | ?                        |
| 11         | 00                 | ?                        |
| 12         | 00                 | ?                        |
| 13         | 32                 | ?                        |
| 14         | 01                 | ?                        |
| 15         | 00                 | ?                        |

### Packet type 37 subtype 07-13: zone names

Header: 00F037

Payload length: 14

| Byte(:bit) | Hex value observed | Description              | Data type
|:-----------|:-------------------|:-------------------------|:-
| 0          | 07-13              | packet subtype           | u8
| 1          | 80                 | ?                        |
| 2-13       | C0                 | zone name                | c8

Subtypes 07-0E are names of 8 zones.

Subtypes 0F-11 are names of 3 sensors.

Subtypes 12-13 are names of 2 remote controllers.

### Packet type 37: reply

Header: 40F037

Empty payload

## Packet type 3B - operation control

### Packet type 3B: request

Header: 00F03B

Payload length: 20

| Byte(:bit) | Hex value observed | Description              | Data type
|:-----------|:-------------------|:-------------------------|:-
| 0          | 00/01              | Power off/on             | u8?
| 1          | 00                 | ?                        |
| 2          |    61/62/63/67/F1  | Target operating mode    | u4
| 3          | 01                 | Actual operating mode    |
| 4          | 20                 | Target temperature 1     | u8?
| 5          | 00                 | ?                        |
| 6:7-5      | 000-010            | Fan speed cooling/fan    | u3
| 6:4        | 1                  | ?                        | bit
| 6:others   | 0000               | ?                        | u4?
| 7          | 00                 | ?                        |
| 8          | 18                 | Target temperature heating| u8?
| 9          | 00                 | ?                        |
| 10:7-5     | 000-010            | Fan speed heating        | u3 
| 10:4       | 1                  | ?                        | bit
| 10:others  | 0000               | ?                        | u4?
| 11         | 00                 | ?                        |
| 12         | 00                 | ?                        |
| 13         | 2A                 | ?                        |
| 14         | 00                 | ?                        |
| 15         | C0                 | ?                        |
| 16         | 00                 | ?                        |
| 17         | F7                 | Zone status, 1 bit per zone | flag8
| 18:1-0     | 01                 | Fan speed heating        | u2
| 18:7-2     | 001000             | ?                        | u6?
| 19         | 08                 | ?                        |

#### Values observed and useful for setting status via reply:

Power

| Value      | Description
|:-----------|:-
| 00         | off
| 01         | on

Target operating mode

| Value     | Description
|:----------|:-
| 00        | off ?
| 60        | off or fan ?
| 61        | heat
| 62        | cool
| 63        | auto
| 64        | ?
| 65        | ?
| 66        | ?
| 67        | dry
| Fx        | ?

Actual operating mode

| Value     | Description
|:----------|:-
| 00        | off or fan ?
| 01        | heat
| 02        | cool

Fan speed

| Value      | Description
|:-----------|:-
| 0          | low
| 1          | medium
| 2          | high

To set fan speed, it seems to function to write both fan speed fields in the reply (bytes 4 and 8) to (fan speed (in bits 7-5) | 0x10).

Fan mode

| Value      | Description
|:-----------|:-
| 0          | manual ?
| 1          | manual ?
| 2          | ?
| 3          | auto

### Packet type 3B: reply

Header: 40F03B

Payload length: 19

| Byte(:bit) | Hex value observed | Description              | Data type
|:-----------|:-------------------|:-------------------------|:-
| 0          | 00/01              | Power off/on             | u8?
| 1          | 00/61              | Operating mode           | u8?
| 2          | 20                 | Target temperature cooling | u8?
| 3          | 00                 | ?                        |
| 4:7-5      | 000-010            | Fan speed cooling/fan    | u3
| 4:4        | 1                  | ?                        | bit
| 4:others   | 0000               | ?                        | u4?
| 5          | 00                 | ?                        |
| 6          | 18                 | Target temperature heating | u8?
| 7          | 00                 | ?                        |
| 8:7-5      | 000-010            | Fan speed heating        | u3
| 8:4        | 1                  | ?                        | bit
| 8:others   | 0000               | ?                        | u4?
| 9-15       | 00                 | ?                        |
| 16         | F7                 | Zone status (1 bit per zone) | flag8
| 17         | 01                 | Fan mode                 | u8?
| 18         | 00                 | ?                        |

To set target temperature, it seems to function to write both payload bytes 2 and 6.

## Packet type 3C - filter counter/alarm

### Packet type 3C: request

Header: 00F03C

Payload length: 12

| Byte(:bit) | Hex value observed | Description              | Data type
|:-----------|:-------------------|:-------------------------|:-
| 0          | 00/02              | 02 = filter alarm ?      |
| 1-5        | 00                 | ?                        |
| 6          | 00/02              | 02 = filter alarm ?      |
| 7          | 00                 | ?                        |
| 8          | 01                 | ?                        |
| 9          | 01                 | ?                        |
| 10-11      | 00                 | ?                        |

### Packet type 3C: reply

Header: 40F03C

Payload length: 2

| Byte(:bit) | Hex value observed | Description              | Data type
|:-----------|:-------------------|:-------------------------|:-
| 0          | 00                 | FF to reset filter alarm? |
| 1          | 00                 | ?                        |
