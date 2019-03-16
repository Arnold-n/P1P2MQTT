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
- 86..87 for a.o. status requests
- B8 (+subtype) for request of consumption usage and count of operating hours
- A1/20 for motors and operation mode
- 21 for tbd

# Main package

#### 1. Packet "000010..."

| Byte nr       | Value observed  |   		|
|---------------|:-------------------------------|:---------------|
|     1    	| 00       		| source: 00 = thermostat |
|     2    	| 00         		| 00 = no end-of-package |
|     3    	| 10         		| packet type 10 |
|     4    	| 01        		| ?? |
|     5    	| 81         		| ?? |
|     6    	| 01        		| ?? |
|     7    	| 00        		| ? |
|     8    	| 00        		| ? |
|     9    	| 00        		| ? |
|    10    	| 00        		| ? |
|    11    	| 14        		| room temperature setting? |
|    12    	| 00        		| ? |
|    13    	| 00        		| ? |
|    14    	| 00        		| ? |
|    15    	| 00        		| ? |
|    16    	| 08        		| ? |
|    17    	| 00        		| ? |
|    18    	| 00        		| ? |
|    19    	| 0F        		| ?? |
|    20    	| 00        		| ? |
|    21    	| 00        		| ? |
|    22    	| 3C        		| DHW target temperature |
|    23    	| 00        		| ? |
|    24    	| XX           		| CRC checksum |

#### 2. Packet "400010..."

| Byte nr       | Value observed  |   		|
|---------------|:-------------------------------|:---------------|
|     1    	| 40       			| source: 40 = heat pump |
|     2    	| 00         			| 00 = no end-of-package |
|     3    	| 10         			| packet type 10 |
|     4    	| 01               		| ?? |
|     5    	| 80                		| ?? |
|     6    	| 21               		| ?? |
|     7    	| 01               		| ? |
|     8    	| 3C               		| DHW target temperature |
|     9    	| 00               		| ? |
|    10    	| 0F               		| ? |
|    11    	| 00               		| ? |
|    12    	| 14               		| room temperature setting? |
|    13    	| 00               		| ? |
|    14    	| 1A               		| ? |
|    15-21 	| 00               		| ? |
|    22    	| 00/02/08/09      		| operating/defrost mode? |
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
|   6-7    	| XX YY            		| ?? (varies slightly) |
|   8-9    	| XX YY            		| outside temperature (outside unit) (XX.YY in 0.5 degree resolution) |
|  10-11   	| XX YY            		| AWT (XX.YY in 0.1 degree resolution) |
|  12-11   	| XX YY            		| MWT (XX.YY mid-way temperature heat pump - gas boiler in 0.1 degree resolution) | |
|  14-15   	| XX YY            		| regrigerant temperature? (XX.YY in 0.1 degree resolution) |
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
|     4    	| 00/02            		| 00=normal, 02=first packet of the day |
|     5    	| 00-??             		| #full days in operation |
|     6   	| 00-17            		| time - hours |
|     7    	| 00-3B            		| time - minutes |
|     8    	| 13 (example)     		| date - year (0x13 = 2019) |
|     9    	| 01-0C            		| date - month |
|    10    	| 01-1F            		| date - day of month |
|    11-15    	| 00               		| ? |
|    16    	| 41               		| operating mode on/off?? |
|    17    	| 04               		| operating mode on/off?? |
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
|    15    	| 7F				| ?? |
|    16    	| 01/40/41/C1      		| operating mode (d7: DHW; D6: gas?; D0: heat pump?) |
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
|     6    	| D0               		| ?? |
|     7    	| XX               		| CRC checksum |

#### 8. Packet "400013.."

| Byte nr       | Value observed                |   		|
|---------------|:-------------------------------|:---------------|
|     1    	| 40       			| source: 40 = heat pump |
|     2    	| 00         			| 00 = no end-of-package |
|     3    	| 13         			| packet type 13 |
|     4    	| 3C               		| DHW target temperature |
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
|     8    	| 2D         			| ?? |
|     9    	| 00         			| ? |
|    10    	| 07         			| ?? |
|    11    	| 00         			| ? |
|    12    	| 03         			| delta-T |
|    13    	| 00/02      			| ?? |
|   14-18  	| 00         			| ? |
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
|     7    	| 00         			| ? |
|     8    	| 2D         			| ?? |
|     9    	| 00         			| ? |
|    10    	| 07         			| ?? |
|    11    	| 00         			| ? |
|    12    	| 03         			| delta-T |
|    13    	| 00/02      			| ?? |
|   14-18  	| 00         			| ? |
|   19-22  	| 00         			| ? |
|     23   	| XX               		| CRC checksum |

#### 11. Packet "000015.."

| Byte nr       | Value observed                |   		|
|---------------|:-------------------------------|:---------------|
|     1    	| 00       			| source: 00 = thermostat |
|     2    	| 00         			| 00 = no end-of-package |
|     3    	| 15         			| packet type 15 |
|     4    	| 00         			| ? |
|     5    	| 01/09        			| operating mode? |
|     6    	| F4/C4      			| operating mode? |
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
|     4-17    	| 00-03      			| bytes form a repeating pattern of a "01" moving along several byte fields, over 241 packages. This could be another CRC code, or identity or authentication communication. On top of this there is additional variation between different packets in some of these bytes. |
|     18   	| XX               		| CRC checksum |

# Additional (prefixed) packets

## 86/87 for operating status request

These 4 packets will be inserted before the start of a regular package:

#### 001. Packet "000086.."

| Byte nr       | Value observed                |   		|
|---------------|:-------------------------------|:---------------|
|     1    	| 00       			| source: 00 = thermostat |
|     2    	| 00         			| 00 = no end-of-package |
|     3    	| 86         			| packet type 86 |
|  4-23    	| 00                		| ? |
|    24    	| XX               		| CRC checksum |

#### 002. Packet "400086.."

| Byte nr       | Value observed                |   		|
|---------------|:-------------------------------|:---------------|
|     1    	| 40       			| source: 40 = heat pump |
|     2    	| 00         			| 00 = no end-of-package |
|     3    	| 86         			| packet type 86 |
|  4-7     	| C0 07 00 08                   | operation mode(s)? |
|  8-11    	| C0 07 00 08                   | operation mode(s) (duplicated?)? |
| 12-15    	| C0 07 00 08                   | operation mode(s) (duplicated?)? |
| 16-19    	| C0 07 00 08                   | operation mode(s) (duplicated?)? |
| 20-23    	| C0 07 00 08                   | operation mode(s) (duplicated?)? |
|    24    	| XX               		| CRC checksum |

#### 003. Packet "000087.."

| Byte nr       | Value observed                |   		|
|---------------|:-------------------------------|:---------------|
|     1    	| 00       			| source: 00 = thermostat |
|     2    	| 00         			| 00 = no end-of-package |
|     3    	| 87         			| packet type 86 |
|  4-23    	| 00                		| ? |
|    24    	| XX               		| CRC checksum |

#### 004. Packet "400087.."

| Byte nr       | Value observed                |   		|
|---------------|:-------------------------------|:---------------|
|     1    	| 40       			| source: 40 = heat pump |
|     2    	| 00         			| 00 = no end-of-package |
|     3    	| 87         			| packet type 87 |
|  4-7     	| C0 07 00 08                   | operation mode(s)? |
|  8-11    	| C0 07 00 08                   | operation mode(s) (duplicated?)? |
| 12-15    	| C0 07 00 08                   | operation mode(s) (duplicated?)? |
| 16-19    	| C0 07 00 08                   | operation mode(s) (duplicated?)? |
| 20-23    	| C0 07 00 08                   | operation mode(s) (duplicated?)? |
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

The reboot process consists of approximately 120 request-response pairs. Tbd.
