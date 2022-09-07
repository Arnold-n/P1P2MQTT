# FDY125LV1 protocol data format

Protocol data format for a FDY125LV1 Daikin ducted model, which seems to be specific for the Australian market.

Please read the common README.md first for the general format description.

All values in this document are hex, except for the byte numbering in the payload and time (ms) references.

### Credits

Many thanks for Chris Huitema for reverse engineering this protocol and contributing to better code for this model.

### Packet type

Each packet has in the header a packet type (and in certain cases a packet subtype as first payload byte) to identify the payload function. Packet type numbers observed so far are:
- 10, 11, 1F for regular data packets between main controller and heat pump (peripheral address 0x00)
- (30,?) 38, 39 communication between the main controller and an auxiliary controller (peripheral address 0xF0)
- 20 perhaps for polling additional peripherals/sensors

Packet 10 and 38 enabling monitoring main system settings, packet 11 the temperature(s).

In addition, a few packet types originating from 0x80 are observed, and remain unanswered. Their function has not been clarified yet:
- 800018 header with 7-byte payload 00000000000000 or 01000000000000, no reply until time-out
- 808000 header with empty payload, no reply until time-out
- 80F034 header with 4-byte payload 10174401 or 10174501, no reply until time-out

### Timing between packets

- approximately 25ms or 50ms between request of main controller and reply of heat pump
- approximately 25ms up to 155ms between reply communication of heat pump and next request of main controller
- approimxately 120-140ms time-out in case of a request without reply

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
| 0          | 00/01              | Power off/on             | u8 
| 1          | 60/61/62/63/67     | Target operating mode    | u8 
| 2          | 00/01/02           | Actual operating mode    | u8
| 3          | 12-1C              | Target temperature       | u8
| 5:6-5      | 00,10              | Fan speed (lo/high)      | u8
| 5:others   | 0..10001           | ?                        | u8
| 7          | 02                 | ?                        | u8
| 13         | 05/07/27/2F/80/A2  | 05=standby<br>07=after change cool to heat<br>27=compressor/fan on<br>2F=compressor+fan on<br>A2=unload<br>80=off | u8
| 14         | 40                 | ?                        | u8
| 15         | 01                 | ?                        | u8
| others     | 00                 | ?                        |

Operating mode

| Value     | Description
|:----------|:-
| 60        | fan
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
| 00        | fan only
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
| 0          | 00/01              |                          | u8 
| 2          | 60/61/62/63/67     | Mode                     | u8 
| 3          | 00/01/02           | Mode                     | u8
| 4          | 12-1C              | Target temperature       | u8
| 6:6-5      | 00,10              | Fan speed (lo/high)      | u8
| 6:others   | 0..10001           | ?                        | u8
| 7          | 02                 | ?                        | u8
| 8          | 3A                 | ?                        | u8
| 14         | 05/07/27/2F/80/A2  | 05=standby<br>07=after change cool to heat<br>27=compressor/fan on<br>2F=compressor+fan on<br>A2=unload<br>80=off | u8
| 17         | 01                 | ?                        | u8
| 19         | 07                 | ?                        | u8
| others     | 00                 | ?                        |

## Packet type 11 - temperatures

### Packet type 11: request

Header: 000011

Payload length: 10

| Byte(:bit) | Hex value observed | Description              | Data type
|:-----------|:-------------------|:-------------------------|:-
| 2          | 03                 | ?                        | u8
| 3          | 0F                 | ?                        | u8
| 5-6        | 11C8/11E0/11F8/1210 | Temperature main controller | f8.8
| others     | 00                 | ?                        |

### Packet type 11: reply

Header: 400011

Payload length: 8

| Byte(:bit) | Hex value observed | Description              | Data type
|:-----------|:-------------------|:-------------------------|:-
| 5-6        | 1307/1360/13B9/1413/... | Inside air intake temperature              | f8.8
| others     | 00                 | ?                        |

## Packet type 18 - unknown

### Packet type 18: request

Frequently observed, at irregular moments.

Header: 800018 (note source = 80, not 00)

Payload length: 7

| Byte(:bit) | Hex value observed | Description              | Data type
|:-----------|:-------------------|:-------------------------|:-
| 0          | 00/01              | Heatpum off/on           | u8
| 1-6        | 00                 | ?                        |

### Packet type 18: reply

No reply observed. Perhaps 800018 is not even a request?

## Packet type 1F - unknown

### Packet type 1F: request

Frequently observed.

Header: 00001F

Empty payload.

### Packet type FF: reply

Header: 4000FF

A reply is observed with packet type FF (instead of 1F), and empty payload.

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
| 0          | 01                 | ?                        |
| 1          | 23                 | ?                        |
| 2          | A0                 | ?                        |
| 3          | 00                 | ?                        |
| 4          | 20                 | ?                        |
| 5          | 10                 | ?                        |
| 6          | 20                 | ?                        |
| 7          | 10                 | ?                        |
| 8          | 20                 | ?                        |
| 9-10       | 00 00/01 91        | Warning                  |
| 11         | 40                 | ?                        |
| 12         | 2C                 | ?                        |
| 13         | 02                 | ?                        |
| 14         | 00                 | ?                        |
| 15         | 0D                 | ?                        |
| 16         | 00                 | ?                        |
| 17         | 5B                 | ?                        |
| 18         | 03                 | ?                        |
| 19         | 13                 | ?                        |


# Packet types 30-3F form communication between main and auxiliary controller(s)

Only a few of these packet types have been observed: 30, 37, 3B, 3C.

## Packet type 30 - polling auxiliary controller

This packet has *not* been observed, perhaps because it is a communication that occurs only once. The description below is a guess based on the observation of the FDYQ180MV1.

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
| 8          | 01?                | same as 400020-reply-byte-9      |
| 9          | 91?                | same as 400020-reply-byte-10     |
| 10         | ?                  | ?                                |
| 11         | 2C?                | same as 400020-reply-byte-12     |
| 12         | 02?                | same as 400020-reply-byte-13     |
| 13         | 00?                | same as 400020-reply-byte-14     |
| 14         | 0D                 | same as 400020-reply-byte-15     |
| 15         | 00                 | same as 400020-reply-byte-16     |
| 16         | 5B?                | same as 400020-reply-byte-17     |
| 17         | 03?                | same as 400020-reply-byte-18     |
| 18         | 13?                | same as 400020-reply-byte-19     |
| 19         | 01?                | same as 400020-reply-byte-0 ?    |

### Packet type 30: reply

Header: 40F030

Payload length: empty?? Not observed.

## Packet type 38 - operation control

### Packet type 38: request

Header: 00F038

Payload length: 16

| Byte(:bit) | Hex value observed | Description                | Data type
|:-----------|:-------------------|:---------------------------|:-
| 0          | 00/01/02/03        | Power off/on               | u8?
| 1          | 00                 | ?                          |
| 2          | 60/61/62/63/67     | Target operating mode      | u4
| 3          | 00/01/02           | Actual operating mode?     |
| 4          | 10-20              | Target temperature cooling | u8?
| 5          | 00/80              | 80 upon change of (temp/mode?) |
| 6:6-5      | 00,10              | Fan speed cooling/fan      | u3 
| 6:7        | 0,1                | 1 upon change fan speed    | bit
| 6:4-0      | 10001              | ?                          | u5
| 7          | 00                 | ?                          |
| 8          | 12-1C              | Target temperature heating | u8?
| 9          | 00/80              | 80 upon change of (temp/mode?) |
| 10:6-5     | 00,10              | Fan speed heating          | u3 
| 10:7       | 0,1                | 1 upon change fan speed    | bit
| 10:4-0     | 10001              | ?                          | u5
| 11         | 01                 | ?                          |
| 12         | 00                 | ?                          |
| 13         | 2A                 | ?                          |
| 14         | 00/01              | 01 twice to confirm clearing error code |
| 15         | C0                 | ?                          |

#### Values observed and useful for setting status via reply:

Power

| Value      | Description
|:-----------|:-
| 00         | off
| 01         | on
| 02         | turn off (once, then 00)
| 03         | turn on (once, then 01)

Target operating mode

| Value     | Description
|:----------|:-
| 60        | fan
| 61        | heat
| 62        | cool
| 63        | auto
| 64        | ?
| 65        | ?
| 66        | ?
| 67        | dry

Actual operating mode

| Value    | Description
|:---------|:-
| 00       | fan only
| 01       | heat
| 02       | cool

Fan speed

| Value      | Description
|:-----------|:-
| 0          | low
| 1          | medium
| 2          | high

Fan mode 

| Value      | Description
|:-----------|:-
| 0          | manual
| 1          | ?
| 2          | ?
| 3          | auto

### Packet type 38: reply

Header: 40F038

Payload length: 15

| Byte(:bit) | Hex value observed | Description              | Data type
|:-----------|:-------------------|:-------------------------|:-
| 0          | 00/01              | Power off/on             | u8?
| 1          | 60/61/62/63/67     | Operating mode           | u4
| 2          | 12/1C              | Target temperature cooling | u8?
| 3          | 00                 | ?                        |
| 4:7        | 0                  | ?                        | bit
| 4:6-5      | 00,10              | Fan speed cooling/fan    | u2
| 4:4        | 1                  | ?                        | bit
| 4:3-0      | 0001               | ?                        | u4 
| 5          | 00                 | ?                        |
| 6          | 12-1C              | Target temperature heating | u8?
| 7          | 00                 | ?                        |
| 8:7        | 0                  | ?                        | bit
| 8:6-5      | 00,10              | Fan speed heating        | u2
| 8:4        | 1                  | ?                        | bit
| 8:3-0      | 0001               | ?                        | u4 
| 10         | 00                 | ?                        |
| 11         | 00                 | Clear error code         |
| 12         | 00                 | ?                        |
| 13         | C0                 | Fan mode?                | u8?
| 14         | 00                 | ?                        |

To set fan speed, it seems to function to write payload byte 4 or 8 with the new fan speed in bits 6-5 with bit 4 set to 1.

## Packet type 39 - filter counter/alarm

### Packet type 39: request

Header: 00F039

Payload length: 11

| Byte(:bit) | Hex value observed | Description              | Data type
|:-----------|:-------------------|:-------------------------|:-
| 0-7        | 00                 | ?                        |
| 8-9        | 03 0F              | filter?                  |
| 10         | 00                 | ?                        |

### Packet type 39: reply

Header: 40F039

Payload length: 4

| Byte(:bit) | Hex value observed | Description                        | Data type
|:-----------|:-------------------|:-----------------------------------|:-
| 0          | 00                 | in other system reset filter alarm |
| 1          | 00                 |                                    |
| 2-3        | 03 0F              | filter, copy of 00F030 bytes 8-9?|

## Packet type 80 - enter test mode

### Packet type 80: request

Header: 000080

Payload length: 10

| Byte(:bit) | Hex value observed | Description              | Data type
|:-----------|:-------------------|:-------------------------|:-
| 0-9        | 0F                 | ?                        |

### Packet type 80: reply

Header: 400080

Payload length: 10

| Byte(:bit) | Hex value observed | Description               | Data type
|:-----------|:-------------------|:--------------------------|:-
| 0          | 20                 | ?                         |
| 1          | 00                 | ?                         |
| 2          | 21                 | ?                         |
| 3          | 20                 | ?                         |
| 4-9        | 00                 | ?                         |

## Packet type C1 - service mode

### Packet type C1: request

Header: 0000C1

Payload length: 1

| Byte(:bit) | Hex value observed | Description              | Data type
|:-----------|:-------------------|:-------------------------|:-
| 0          | 00                 | subtype/value request    |

### Packet type C1: reply

Header: 4000FF (so FF, not C1)

Payload length: 0 or 4 (depends on subtype, unsupported subtype (a.o.: 00) results in empty payload)

| Byte(:bit) | Hex value observed | Description              | Data type
|:-----------|:-------------------|:-------------------------|:-
| 0          | 01                 | subtype 01               |
| 1          | 00                 | ?                        |
| 2-3        | 10 F1              | Inside air intake temperature (same as in 400011) | f8.8?

| Byte(:bit) | Hex value observed | Description               | Data type
|:-----------|:-------------------|:--------------------------|:-
| 0          | 02                 | subtype 02                |
| 1          | 00                 | ?                         |
| 2-3        | 11 FC              | heat exchanger temperature (not seen elsewhere) | f8.8?
