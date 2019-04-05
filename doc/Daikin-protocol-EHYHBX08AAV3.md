# EHYHBX08AAV3 protocol data format

For the hybrid Daikin heat pump EHYHBX08AAV3, the following for data packet format was observed. This particular unit is functioning in weather-dependent LWT mode.

| Byte nr       | Hex value observed            |   		|
|---------------|:-------------------------------|:---------------|
|  1    	| 00 or 40 			| source address, 00 = thermostat,  40 = heat pump   |
|  2    	| 00 or F0 			| 00 = not the last packet in package, F0 = last packet in package   |
|  3    	|  XX				| packet type         |
|  4..(N-1)   	|  XX 				| 00 = not the last packet in package, 30 = last packet |
|  N   		|  XX   			|  CRC checksum   				|

Hex values shown are typical values observed. The CRC checksum is calculated using an 8-bit LFSR with a feed of 0x00 and a generator value of 0xD9.

Each regular data cycle consists of a package, where the thermostat and heat pump take turn as transmitter. Every approximately 770ms one package of 13 packets is communicated.

If the user of the thermostat requests certain information in the menu items, such as energy consumed, additional packet types are inserted before such a package.

Each packet has a packet type (and in certain cases a subtype) to identify the payload function. Packet type numers observed so far are:
- 10..15 for regular data packets
- 30 for the last packet in a package
- 60..8F for field setting read/write operations (during operation and upon restart)
- 90..9F,A1,B1 for data communication upon restart; packets 90, 92-98, 9A-9C are empty/not used; packet 91, 99, 9D, 9E, A1 are TBD; packet 9F contains information similar to packet 20 (tbd), packet B1 contains the ASCII name of the heat pump ("EHYHBX08AAV3\0\0\0\0\0")
- B8 (+subtype) for incidental requests of consumption usage and count of operating hours
- A1/20 for incidental requests on status of motors and operation mode
- 21 for incidental requests on tbd

Timing between packets:
- approximately 25ms between communication thermostat and communication heat pump
- approximately 30-50ms (42/37/47/31/29/36) between communication heat pump and communication thermostat
- approximately 140 or 160 ms between last and first packet of package

# Main package

#### 1. Packet "000010..."

| Byte nr       | Value observed  |   		|
|---------------|:-------------------------------|:---------------|
|     1    	| 00       		| source: 00 = thermostat |
|     2    	| 00         		| 00 = no end-of-package |
|     3    	| 10         		| packet type 10 |
|     4    	| 00/01     		| ?? |
|     5    	| 01/81        		| Als Pref dan 81; anders 01/81. 81=hybrid?? |
|     6    	| 00/01        		| DHW tank (00: off; 01: on) |
|     7    	| 00        		| ? |
|     8    	| 00        		| ? |
|     9    	| 00        		| ? |
|    10    	| 00        		| ? |
|    11    	| 14        		| target room temperature |
|    12    	| 00        		| ? |
|    13    	| 00        		| ? |
|    14    	| 00/04        		| quiet mode (00: off; 04: on) |
|    15    	| 00        		| ? |
|    16    	| 08        		| ? |
|    17    	| 00        		| ? |
|    18    	| 00        		| ? |
|    19    	| 0F        		| ?? |
|    20    	| 00        		| ? |
|    21    	| 00/40/42        		| DHW tank mode (00: off; 40: normal; 42: booster) |
|    22    	| 3C        		| DHW target temperature |
|    23    	| 00        		| ? |
|    24    	| XX           		| CRC checksum |

#### 2. Packet "400010..."

| Byte nr       | Value observed  |   		|
|---------------|:-------------------------------|:---------------|
|     1    	| 40       			| source: 40 = heat pump |
|     2    	| 00         			| 00 = no end-of-package |
|     3    	| 10         			| packet type 10 |
|     4    	| 00/01               		| ?? |
|     5    	| 00/80                		| ?? |
|     6    	| 01/21/22/31/41/42/81/A1               		| ?? valves  (x1: heating; x2: cooling; 2x: main zone; 4x: additional zone; 31: ???; 81: tank; A1: tank+main) |
|     7    	| 01               		| ? |
|     8    	| 3C               		| DHW target temperature |
|     9    	| 00               		| ? |
|    10    	| 0F               		| ? |
|    11    	| 00               		| ? |
|    12    	| 14               		| room temperature setting? |
|    13    	| 00               		| ? |
|    14    	| 1A               		| ? |
|    15   	| 00/04               		| quiet mode (00: off; 04: on) |
|    16-21 	| 00               		| ? |
|    22    	| 00/02/08/09      		| pump and compressor (00: off; 02: defrost??; 08: pump only; 09: pump + comp) |
|    23    	| 00/02            		| operating/defrost mode? |
|    24    	| XX               		| CRC checksum |

#### 3. Packet "000011..."

| Byte nr       | Value observed                |   		|
|---------------|:-------------------------------|:---------------|
|     1    	| 00       			| source: 00 = thermostat |
|     2    	| 00         			| 00 = no end-of-package |
|     3    	| 11         			| packet type 11 |
|     4    	| 13/14				| room temperature (measured) in degrees |
|     5    	| CC/E6/00/18/32/others		| and in 0.1 degrees (0x14+0x32/256=20.2 degrees) |
|     6    	| 00               		| ? |
|     7    	| 00               		| ? |
|     8    	| 00               		| ? |
|     9    	| 00               		| ? |
|    10    	| 00               		| ? |
|    11    	| 00               		| ? |
|    12    	| XX               		| CRC checksum |

#### 4. Packet "400011..."

| Byte nr       | Value observed                |   		|
|---------------|:-------------------------------|:---------------|
|     1    	| 40       			| source: 40 = heat pump |
|     2    	| 00         			| 00 = no end-of-package |
|     3    	| 11         			| packet type 11 |
|   4-5    	| XX YY            		| LWT (XX.YY degrees in hex format in 0.1 degree resolution) |
|    6     	| XX             		| ?? (varies?? slightly) |
|    7     	| 34/42/52       		| ??  |
|   8-9    	| XX YY            		| outside temperature (outside unit) (XX.YY in 0.5 degree resolution) |
|  10-11   	| XX YY            		| AWT (XX.YY in 0.1 degree resolution) |
|  12-11   	| XX YY            		| MWT (XX.YY mid-way temperature heat pump - gas boiler in 0.1 degree resolution) | |
|  14-15   	| XX YY            		| refrigerant temperature? (XX.YY in 0.1 degree resolution) |
|  16-17   	| XX YY            		| room temperature (XX.YY in 0.1 degree resolution) |
|  18-19   	| XX YY            		| outside temperature in higher resolution? |
|  20-23   	| 00               		| ? |
|    24    	| XX               		| CRC checksum |

#### 5. Packet "000012.."

| Byte nr       | Value observed                |   		|
|---------------|:-------------------------------|:---------------|
|     1    	| 00       			| source: 00 = thermostat |
|     2    	| 00         			| 00 = no end-of-package |
|     3    	| 12         			| packet type 12 |
|     4    	| 00/02            		| 00=normal, 02=(once) indication of new hour, but also 02 (once) in 2nd package after restart |
|     5    	| 00-06             		| day of week (0=Monday, 6=Sunday) |
|     6   	| 00-17            		| time - hours |
|     7    	| 00-3B            		| time - minutes |
|     8    	| 13 (example)     		| date - year (0x13 = 2019) |
|     9    	| 01-0C            		| date - month |
|    10    	| 01-1F            		| date - day of month |
|    11-15    	| 00               		| ? |
|    16    	| 00/01/41/61               	| upon restart: 1x 00; 1x 01; then 41; a single value of 61 triggers an immediate restart|
|    17    	| 00/04               		| once 00, then 04 |
|    18    	| 00               		| ? |
|    19    	| XX               		| CRC checksum |

#### 6. Packet "400012.."

| Byte nr       | Value observed                |   		|
|---------------|:-------------------------------|:---------------|
|     1    	| 40       			| source: 40 = heat pump |
|     2    	| 00         			| 00 = no end-of-package |
|     3    	| 12         			| packet type 12 |
|     4    	| 40               		| ?? |
|     5    	| 40                		| ?? |
|    6-13  	| 00               		| ? |
|    14    	| 00/10            		| 00=normal, 10=preference kWh |
|    15    	| 7F				| first packet after restart 00; else 7F |
|    16    	| 00/01/40/41/80/81/C1   	| operating mode (bit 7: DHW; bit 6: gas?; bit 0: heat pump?) |
|    17    	| 04               		| ? |
|    18-23 	| 00               		| ? |
|    24    	| XX               		| CRC checksum |

#### 7. Packet "000013.."

| Byte nr       | Value observed                |   		|
|---------------|:-------------------------------|:---------------|
|     1    	| 00       			| source: 00 = thermostat |
|     2    	| 00         			| 00 = no end-of-package |
|     3    	| 13         			| packet type 13 |
|    4-5   	| 00               		| ? |
|     6    	| 00/D0               		| first package 0x00 instead of 0xD0 |
|     7    	| XX               		| CRC checksum |

#### 8. Packet "400013.."

| Byte nr       | Value observed                |   		|
|---------------|:-------------------------------|:---------------|
|     1    	| 40       			| source: 40 = heat pump |
|     2    	| 00         			| 00 = no end-of-package |
|     3    	| 13         			| packet type 13 |
|     4    	| 3C               		| DHW target temperature (one packet delayed/from/via boiler?/ 0 in first packet after restart)|
|     5   	| 00               		| ? |
|     6    	| 01               		| ?? |
|     7    	| D0               		| ?? |
|   8-12  	| 00               		| ? |
|    13    	| 00-E0            		| flow (in 0.1 l/min) |
|    14-15      | xxxx        			| software version inner unit |
|    16-17      | xxxx        			| software version outer unit |
|     18   	| XX               		| CRC checksum |

#### 9. Packet "000014.."

| Byte nr       | Value observed                |   		|
|---------------|:-------------------------------|:---------------|
|     1    	| 00       			| source: 00 = thermostat |
|     2    	| 00         			| 00 = no end-of-package |
|     3    	| 14         			| packet type 14 |
|     4    	| 2B         			| ?? |
|     5    	| 00         			| ? |
|     6    	| 13         			| ?? |
|     7    	| 00         			| ? |
|     8    	| 2D         			| ?? first package 0x37 instead of 0x2D (55 instead of 45) |
|     9    	| 00         			| ? |
|    10    	| 07         			| ?? first package 0x37 instead of 0x07 |
|    11    	| 00         			| ? |
|    12    	| 00/01/02/11  			| delta-T |
|    13    	| 00?/02/05   			| ?? |
|   14-15  	| 00         			| ? |
|   16     	| 00/37      			| first package 0x37 instead of 0x00 |
|   17-18  	| 00         			| ? |
|     19   	| XX               		| CRC checksum |

#### 10. Packet "400014.."

| Byte nr       | Value observed                |   		|
|---------------|:-------------------------------|:---------------|
|     1    	| 40       			| source: 40 = heat pump |
|     2    	| 00         			| 00 = no end-of-package |
|     3    	| 14         			| packet type 14 |
|     4    	| 2B         			| ?? |
|     5    	| 00         			| ? |
|     6    	| 13         			| ?? |
|     7    	| 00				| ? |
|     8    	| 2D         			| ?? |
|     9    	| 00				| ? |
|    10    	| 07         			| ?? |
|    11    	| 00         			| ? |
|    12    	| 03/01/02/11  			| delta-T |
|    13    	| 00/02/05     			| ?? |
|   14-18  	| 00         			| ? |
|   19     	| 1C-24                   	| Target temp LWT main zone (based on outside temp in 0.5 degree resolution)|
|   20     	| 00-09                   	| (+0.1 degree) |
|   21     	| 1C-24                    	| Target temp LWT add zone  |
|   22     	| 00-09                   	| (+0.1 degree)  |
|     23   	| XX               		| CRC checksum |

#### 11. Packet "000015.."

| Byte nr       | Value observed                |   		|
|---------------|:-------------------------------|:---------------|
|     1    	| 00       			| source: 00 = thermostat |
|     2    	| 00         			| 00 = no end-of-package |
|     3    	| 15         			| packet type 15 |
|     4    	| 00         			| ? |
|     5    	| 01/09/0A     			| operating mode? |
|     6    	| F4/C4/D6/F0  			| operating mode? |
|     7    	| 00         			| ? |
|     8    	| 03         			| ?? |
|     9    	| 20         			| ?? |
|     10   	| XX               		| CRC checksum |

#### 12. Packet "400015.."

| Byte nr       | Value observed                |   		|
|---------------|:-------------------------------|:---------------|
|     1    	| 40       			| source: 40 = heat pump |
|     2    	| 00         			| 00 = no end-of-package |
|     3    	| 15         			| packet type 15 |
|    4-5   	| 00         			| ? |
|     6    	| FD-FF,00-08			| temperature? |
|     7    	| 00 or 80    			| could indicate additional 0.5 degree for previous field |
|    8-9   	| 00         			| ? |
|     10   	| XX               		| CRC checksum |

#### 13. Packet "00F030.."

| Byte nr       | Value observed                |   		|
|---------------|:-------------------------------|:---------------|
|     1    	| 00       			| source: 00 = thermostat |
|     2    	| F0         			| F0 = end-of-package |
|     3    	| 30         			| packet type 30 |
|     4-17    	| 00-03      			| bytes form a repeating pattern of a "01" moving along several byte fields, over 241 packages. This could be another CRC code, or identity or authentication communication. On top of this there is additional variation between different packets in some of these bytes (observed in bytes 8 (+01) ,9 (-01/+01/+02),11 (+01). |
|     18   	| XX               		| CRC checksum |


# Other packets

## Packet types 60..8F for communicating field settings

A packet of type 60..8F communicates 5 field settings (using a payload of 20 bytes, or 4 bytes per field setting). This occurs after a restart (all field settings are sent by heat pump upon requests by the thermostat), or when a field setting is manually changed at the thermostat.
For some reason, 4 000086/000087 packets will be inserted  even though these do not communicate any usable field settings.

The packet type corresponds to the field settings according to the following table:

| Packet type   | Field settings |
|---------------|:-------------------------------|
|     8C   	| 00-00 .. 00-04 |
|     7C   	| 00-05 .. 00-09 |
|     6C   	| 00-0A .. 00-0E |
|     8D   	| 01-00 .. 01-04 |
|     7D   	| 01-05 .. 01-09 |
|     6D   	| 01-0A .. 01-0E |
|     8E   	| 02-00 .. 02-04 |
|     7E   	| 02-05 .. 02-09 |
|     6E   	| 02-0A .. 02-0E |
|     8F   	| 03-00 .. 03-04 |
|     7F   	| 03-05 .. 03-09 |
|     6F   	| 03-0A .. 03-0E |
|     80   	| 04-00 .. 04-04 |
|     70   	| 04-05 .. 04-09 |
|     60   	| 04-0A .. 04-0E |
|     ..        | ..             |
|     8B   	| 0F-00 .. 0F-04 |
|     7B   	| 0F-05 .. 0F-09 |
|     6B   	| 0F-0A .. 0F-0E |


#### Packet "0000XX.." Field setting request by thermostat upon restart

| Byte nr       | Value observed                |   		|
|---------------|:-------------------------------|:---------------|
|     1    	| 00       			| source: 00 = thermostat |
|     2    	| 00         			| 00 = no end-of-package |
|     3    	| XX         			| packet type (60..8F) |
|  4-23 	| 00                		| All fields empty |
|    24    	| XX               		| CRC checksum |

#### Packet "4000XX.." Field setting reply by heat pump during restart process

For supported field settings:

| Byte nr       | Value observed                |   		|
|---------------|:-------------------------------|:---------------|
|     1    	| 00       			| source: 00 = thermostat |
|     2    	| 00         			| 00 = no end-of-package |
|     3    	| XX         			| packet type (60..62,64-65,68-6E,70-75,78-85,88-8F) |
|  4    	| XX                		| 1st field value (usually 0xC0 is added to this value)  |
|  5    	| XX                		| maximum field value  |
|  6    	| XX                		| offset for field value  |
|  7    	| 00/08/0A/28/2A/8A/92/AA       | bit 3: 00/08=not in use/in use; bit 7: 00/80=no comma/comma; bit 1 and 5: 02/20/22=step size 0.1/5/0.5; bit 4 seems set for R/O comma values|
|  8-11   	| 00                		| 2nd field value, see bytes 4-7           |
| 12-15   	| 00                		| 3rd field value, see bytes 4-7           |
| 16-19   	| 00                		| 4th field value, see bytes 4-7           |
| 20-23   	| 00                		| 5th field value, see bytes 4-7           |
|    24    	| XX               		| CRC checksum |

For non-supported field settings

| Byte nr       | Value observed                |   		|
|---------------|:-------------------------------|:---------------|
|     1    	| 00       			| source: 00 = thermostat |
|     2    	| 00         			| 00 = no end-of-package |
|     3    	| FF         			| FF as packet type answer if request is for packet type (63,66-67,6F,76-77,86,87) |
|     4    	| XX               		| CRC checksum |

#### Packet "0000XX.." Field setting change by thermostat during operation

| Byte nr       | Value observed                |   		|
|---------------|:-------------------------------|:---------------|
|     1    	| 00       			| source: 00 = thermostat |
|     2    	| 00         			| 00 = no end-of-package |
|     3    	| XX         			| packet type (60..8F) |
|  4    	| XX                		| 1st field, old value (no 0xC0 added), or new value (0x40 added) |
|  5    	| XX                		| maximum field value  |
|  6    	| XX                		| offset for field value  |
|  7    	| 00/08/0A/28/2A/8A/92/AA       | see above |
|  8-11   	| 00                		| 2nd field value, see bytes 4-7           |
| 12-15   	| 00                		| 3rd field value, see bytes 4-7           |
| 16-19   	| 00                		| 4th field value, see bytes 4-7           |
| 20-23   	| 00                		| 5th field value, see bytes 4-7           |
|  4-23 	| 00                		| All fields empty |
|    24    	| XX               		| CRC checksum |

#### Packet "4000XX.." Field setting reply by heat pump during restart process

Format is the same for supported and non-supported field settings

| Byte nr       | Value observed                |   		|
|---------------|:-------------------------------|:---------------|
|     1    	| 00       			| source: 00 = thermostat |
|     2    	| 00         			| 00 = no end-of-package |
|     3    	| XX         			| packet type (60..8F) |
|  4    	| XX                		| 1st field value (usually 0xC0 is added to this value)  |
|  5    	| XX                		| maximum field value  |
|  6    	| XX                		| offset for field value  |
|  7    	| 00/08/0A/28/2A/8A/92/AA       | bit 3: 00/08=not in use/in use; bit 7: 00/80=no comma/comma; bit 1 and 5: 02/20/22=step size 0.1/5/0.5; bit 4 seems set for R/O comma values|
|  8-11   	| 00                		| 2nd field value, see bytes 4-7           |
| 12-15   	| 00                		| 3rd field value, see bytes 4-7           |
| 16-19   	| 00                		| 4th field value, see bytes 4-7           |
| 20-23   	| 00                		| 5th field value, see bytes 4-7           |
|    24    	| XX               		| CRC checksum |

## A1/20 for motors and operation mode

#### 001. Packet "0000A100.."

| Byte nr       | Value observed                |   		|
|---------------|:-------------------------------|:---------------|
|     1    	| 00       			| source: 00 = thermostat |
|     2    	| 00         			| 00 = no end-of-package |
|     3    	| A1         			| packet type A1 |
|  4-21    	| 00                		| ? |
|    22    	| XX               		| CRC checksum |

#### 002. Packet "4000A100.."

| Byte nr       | Value observed                |   		|
|---------------|:-------------------------------|:---------------|
|     1    	| 40       			| source: 40 = heat pump |
|     2    	| 00         			| 00 = no end-of-package |
|     3    	| A1         			| packet type A1 |
|  4-21    	| 00                		| ? |
|    22    	| XX               		| CRC checksum |
length 22

#### 003. Packet "00002000.."

| Byte nr       | Value observed                |   		|
|---------------|:-------------------------------|:---------------|
|     1    	| 00       			| source: 00 = thermostat |
|     2    	| 00         			| 00 = no end-of-package |
|     3    	| 20         			| packet type 20 |
|     4    	| 00                		| ? |
|     5    	| XX               		| CRC checksum |

#### 004. Packet "40002000.."

| Byte nr       | Value observed                |   		|
|---------------|:-------------------------------|:---------------|
|     1    	| 40       			| source: 40 = heat pump |
|     2    	| 00         			| 00 = no end-of-package |
|     3    	| 20         			| packet type 20 |
|    4-23  	| XX                		| various values??, tbd |
|    24    	| XX               		| CRC checksum |

## B8 for request of energy consumed and count of operating hours

#### 001. Packet "0000B800.."

| Byte nr       | Value observed                |   		|
|---------------|:-------------------------------|:---------------|
|     1    	| 00       			| source: 00 = thermostat |
|     2    	| 00         			| 00 = no end-of-package |
|     3    	| B8         			| packet type B8 |
|     4    	| 00                		| packet subtype 00 |
|     5    	| XX               		| CRC checksum |

#### 002. Packet "4000B800.."

| Byte nr       | Value observed                |   		|
|---------------|:-------------------------------|:---------------|
|     1    	| 40       			| source: 40 = heat pump  |
|     2    	| 00         			| 00 = no end-of-package |
|     3    	| B8         			| packet type B8 |
|     4    	| 00                		| packet subtype 00 |
|  5-10    	| 00                		| two 3-byte numbers? |
| 11-13    	| 00 XX XX             		| ??   |
| 14-19    	| 00                		| two 3-byte numbers? |
| 20-22    	| 00 XX XX             		| electric energy consumed |
|    23    	| XX               		| CRC checksum |


#### 003. Packet "0000B801.."

| Byte nr       | Value observed                |   		|
|---------------|:-------------------------------|:---------------|
|     1    	| 00       			| source: 00 = thermostat |
|     2    	| 00         			| 00 = no end-of-package |
|     3    	| B8         			| packet type B8 |
|     4    	| 01                		| packet subtype 00 |
|     5    	| XX               		| CRC checksum |

#### 004. Packet "4000B801.."

| Byte nr       | Value observed                |   		|
|---------------|:-------------------------------|:---------------|
|     1    	| 40       			| source: 40 = heat pump  |
|     2    	| 00         			| 00 = no end-of-package |
|     3    	| B8         			| packet type B8 |
|     4    	| 01                		| packet subtype 01 |
|  5-7     	| XX XX XX          		| heat produced |
|  8-13    	| 00                		| 4 3-byte numbers?   |
| 14-16    	| XX XX XX          		| heat produced (duplicate?)|
|    17    	| XX               		| CRC checksum |


#### 005. Packet "0000B802.."

| Byte nr       | Value observed                |   		|
|---------------|:-------------------------------|:---------------|
|     1    	| 00       			| source: 00 = thermostat |
|     2    	| 00         			| 00 = no end-of-package |
|     3    	| B8         			| packet type B8 |
|     4    	| 02                		| packet subtype 02 |
|     5    	| XX               		| CRC checksum |

#### 006. Packet "4000B802.."

| Byte nr       | Value observed                |   		|
|---------------|:-------------------------------|:---------------|
|     1    	| 40       			| source: 40 = heat pump  |
|     2    	| 00         			| 00 = no end-of-package |
|     3    	| B8         			| packet type B8 |
|     4    	| 02                		| packet subtype 02 |
|  5-7     	| XX XX XX          		| number of pump hours |
|  8-10    	| XX XX XX          		| number of compressor hours |
| 11-16    	| XX XX XX          		| 2 3-byte numbers? (cooling hours?) |
|    17    	| XX               		| CRC checksum |


#### 007. Packet "0000B803.."

| Byte nr       | Value observed                |   		|
|---------------|:-------------------------------|:---------------|
|     1    	| 00       			| source: 00 = thermostat |
|     2    	| 00         			| 00 = no end-of-package |
|     3    	| B8         			| packet type B8 |
|     4    	| 03                		| packet subtype 03 |
|     5    	| XX               		| CRC checksum |

#### 008. Packet "4000B803.."

| Byte nr       | Value observed                |   		|
|---------------|:-------------------------------|:---------------|
|     1    	| 40       			| source: 40 = heat pump  |
|     2    	| 00         			| 00 = no end-of-package |
|     3    	| B8         			| packet type B8 |
|     4    	| 03                		| packet subtype 03 |
|  5-22    	| 00                		| ? |
|    23    	| XX               		| CRC checksum |


#### 009. Packet "0000B804.."

| Byte nr       | Value observed                |   		|
|---------------|:-------------------------------|:---------------|
|     1    	| 00       			| source: 00 = thermostat |
|     2    	| 00         			| 00 = no end-of-package |
|     3    	| B8         			| packet type B8 |
|     4    	| 04                		| packet subtype 04 |
|     5    	| XX               		| CRC checksum |

#### 010. Packet "4000B804.."

| Byte nr       | Value observed                |   		|
|---------------|:-------------------------------|:---------------|
|     1    	| 40       			| source: 40 = heat pump  |
|     2    	| 00         			| 00 = no end-of-package |
|     3    	| B8         			| packet type B8 |
|     4    	| 04                		| packet subtype 04 |
|  5-13    	| 00                		| ? |
| 14-16    	| XX XX XX          		| number of start attempts |
|    17    	| XX               		| CRC checksum |


#### 011. Packet "0000B805.."

| Byte nr       | Value observed                |   		|
|---------------|:-------------------------------|:---------------|
|     1    	| 00       			| source: 00 = thermostat |
|     2    	| 00         			| 00 = no end-of-package |
|     3    	| B8         			| packet type B8 |
|     4    	| 05                		| packet subtype 05 |
|     5    	| XX               		| CRC checksum |

#### 012. Packet "4000B805.."

| Byte nr       | Value observed                |   		|
|---------------|:-------------------------------|:---------------|
|     1    	| 40       			| source: 40 = heat pump  |
|     2    	| 00         			| 00 = no end-of-package |
|     3    	| B8         			| packet type B8 |
|     4    	| 05                		| packet subtype 05 |
|  5-7     	| XX XX XX          		| number of boiler hours heating |
|  8-10    	| XX XX XX          		| number of boiler hours DHW |
| 11-16    	| 00 00 00          		| 2 3-byte numbers? |
| 17-19    	| XX XX XX          		| 1 3-byte number (tbd)? |
| 20-22    	| 00 00 00          		| 3-byte numbers? |
|    23    	| XX               		| CRC checksum |


## 21 for tbd

#### Tbd

## Reboot process

The reboot process is initiated by setting byte 16 of packet "000012.." to 0x61. The heat pump stops answering requests almost immediately. 

The reboot process consists of several steps, including lots of data communication, still to be described.
