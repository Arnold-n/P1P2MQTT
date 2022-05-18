# EWYQ005ADVP protocol data format

For the Daikin air heat pump EWYQ005ADVP, the following packet types and payload data were observed.

# Packet type 10

Packet types 11 and 12 are part of the regular communication pattern between main controller and heat pump.

### Packet type 10:request

Header: 000010

| Byte nr       | Hex value observed            | Description           | Data type     | Bit: description |
|---------------|:------------------------------|:----------------------|:--------------|:-----------------|
|     0         | 00/01                         | Power                 | flag8 | 0: power (off/on)
|     1         | 01/02                         | Operating mode        | flag8 | 1: Heating 2: Cooling
|     2         | 00                            | ?                     | 
|     3         | 36                            | Target heating temp   | f8/8
|     4         | 05/00                         | Automatic mode        | u8 | 0: off 5: on
|     5         | 0F                            | Target cooling temp   | f8/8
|     6         | 00                            | ?                     |
|     7         | 30                            | ?                     |
|     8         | 00                            | ?                     |
|     9         | 00/01                         | Quiet mode            | flag8 | 1: on 0: off
|    10         | 00                            | ?                     | 
|    11         | 00                            | ?                     |
|    12         | 00                            | ?                     |
|    13         | 00/01                         | Test mode             | flag8 | 1: on 0: off
|    14         | 46                            |                       |
|    15         | 0A                            |                       |

### Packet type 10:response

Header: 400010

| Byte nr       | Hex value observed            | Description           | Data type     | Bit: description |
|---------------|:------------------------------|:----------------------|:--------------|:-----------------|
|     0         | 00/01                         | Power                 | flag8         | 0: power (off/on) |
|     1         | 00                            | ?                     | 
|     2         | 01/02                         | Operating mode        | flag8         | 1: Heating 2: Cooling
|     3         | 00                            | ?                     |
|     4         | 36                            | Target heating temp   | f8/8
|     5         | 05/00                         | Automatic mode        | u8 | 0: off 5: on
|     6         | 0F                            | Target cooling temp   | f8/8
|     7         | 00                            | ?                     | 
|     8         | 01                            | ?                     |
|     9         | 00/01                         | Quiet mode            | flag8 | 1: on 0: off
|    10         | 00                            | ?
|    11         | 00                            | ?
|    12         | 00                            | Quiet mode            | flag8         | 2: quiet mode (off/on) |
|    13         | 00                            | ?
|    14         | 00/80                         | ? 
|    15         | 07/08/09/0B                   |  mode                 | flag8         | 2: compressor (off/on) 
|    16         | 00                            | ?
|    17         | 00                            | ?
|    18         | 01                            | ?
|    19         | D1                            | ?

# Packet type 11

### Packet type 11:request

Header: 000011

Empty payload

### Packet type 10:response

Header: 400010

| Byte nr       | Hex value observed            | Description           | 
|---------------|:------------------------------|:----------------------|:-
|     0         | 07                            | ?                     | 
|     1         | 09                            | ?                     |
|     2         | E9                            | ?                     |
|     3         | DF                            | ?                     |
|     4         | B3                            | ?                     | 
|     5         | 32                            | ?                     |

# Packet type 20

Packet type 20 might be related to external peripherals.

### Packet type 20:request

Header: 000020 or 000120

Empty payload

### Packet type 20:response

In the example below, request 00*00*20 is responded to, this could perhaps be a response from a first peripheral.

Request 00*01*20 is not responded to, this could signal that there is no second peripheral active.

Header: 400020

| Byte nr       | Hex value observed            | Description           | Data type  
|---------------|:------------------------------|:----------------------|:-----------
|     0         | 03                            | ?                     | 
|     1         | 14                            | ?                     |
|     2         | 05                            | ?                     |
|     3         | 37                            | ?                     |
|     4         | 19                            | ?                     |
|     5         | 50                            | ?                     |
|     6         | 19                            | ?                     |
|     7         | 01                            | ?                     |
|     8         | 95                            | ?                     |
|     9         | 3E                            | ?                     |
|    10         | 40                            | ?                     |
|    11         | 32                            | ?                     |
|    12         | 3C                            | ?                     |
|    13         | 00                            | ?                     |
|    14         | 32                            | ?                     |
|    15         | 01                            | ?                     |
|    16         | 0F                            | ?                     |
|    17         | 00                            | ?                     |
|    18         | 01                            | ?                     |

# Packet type 00

Header 808000 with empty payload is seen below, but not responded to. Function not known.

## communication example

logfile from the bus, with some unknown packets:

21:07:04.087 -> 0.026: 000011 CRC=48\
21:07:04.087 -> 0.024: 4000110709E9DFB432 CRC=9D\
21:07:04.127 -> 0.026: 000020 CRC=83\
21:07:04.167 -> 0.024: 4000200314053719501901953E40323C0032010F0001 CRC=D1\
21:07:04.207 -> 0.026: 00001001010036000A0030000000000000460A CRC=B4\
21:07:04.247 -> 0.024: 4000100100010036000A000100000000008007000001D1 CRC=44\
21:07:04.327 -> 0.026: 000011 CRC=48\
21:07:04.327 -> 0.024: 4000110709E9DFB432 CRC=9D\
21:07:04.367 -> 0.026: 000120 CRC=77\
21:07:04.527 -> 0.130: 00001001010036000A0030000000000000460A CRC=B4\
21:07:04.567 -> 0.024: 4000100100010036000A000100000000008007000001D1 CRC=44\
21:07:04.607 -> 0.026: 000011 CRC=48\
21:07:04.647 -> 0.024: 4000110709E9DFB432 CRC=9D\
21:07:04.687 -> 0.026: 808000 CRC=12\
21:07:04.807 -> 0.131: 00001001010036000A0030000000000000460A CRC=B4\
21:07:04.847 -> 0.024: 4000100100010036000A000100000000008007000001D1 CRC=44\
21:07:04.927 -> 0.026: 000011 CRC=48\
21:07:04.927 -> 0.024: 4000110709E9DFB432 CRC=9D\
21:07:04.967 -> 0.026: 000020 CRC=83\
21:07:05.007 -> 0.024: 4000200314053719501901953E40323C0032010F0001 CRC=D1\
21:07:05.047 -> 0.026: 00001001010036000A0030000000000000460A CRC=B4\
21:07:05.087 -> 0.024: 4000100100010036000A000100000000008007000001D1 CRC=44\
21:07:05.167 -> 0.026: 000011 CRC=48\
21:07:05.167 -> 0.024: 4000110709E9DFB432 CRC=9D\
21:07:05.207 -> 0.026: 000120 CRC=77\
21:07:05.367 -> 0.127: 00001001010036000A0030000000000000460A CRC=B4\
21:07:05.407 -> 0.024: 4000100100010036000A000100000000008007000001D1 CRC=44\
21:07:05.447 -> 0.026: 000011 CRC=48\
21:07:05.487 -> 0.024: 4000110709E9DFB432 CRC=9D\
21:07:05.527 -> 0.026: 808000 CRC=12\
21:07:05.647 -> 0.131: 00001001010036000A0030000000000000460A CRC=B4\
21:07:05.687 -> 0.024: 4000100100010036000A000100000000008007000001D1 CRC=44
