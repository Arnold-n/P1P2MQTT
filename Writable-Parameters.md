** Writable parameters **

Parameters can be set in the so-called F035, F036, and F03A packet typess. There are many parameters, some are informational, others are writable. These parameters are different for different systems and/or different operation modes of the system. The best way to learn which parameter you need to set it so change a setting manually on the main controller while P1P2Monitor acts as auxiliary controller and data is logged via P1P2/R/#. The main controller will then inform the P1P2Monitor of the new settings which you can then try to use.

Example writable parameters:

Parameter in packet type 35 for switching cooling/heating on/off
- 0x31 works on EHVX08S23D6V
- 0x2F works on EHYHBX08AAV3 and EHBH16CAV3
- 0x2D works on EHVX08S26CA9W     

For example, to switch heating off and on on EHYHBX08AAV3:
- E 35 2F 0
- E 35 2F 1

Parameter in packet type 3A for switching between heating, cooling, and automatic mode
- 0x4E works on EHYHBX08AAV3 (value 0=heating 1=cooling 2=auto):
- E 3A 4E 0 = heating
- E 3A 4E 1 = cooling
- E 3A 4E 2 = auto

Parameter in packet type 35 for switching DHW on/off
- 0x40 works on EHVX08S23D6V and EHBH16CAV3
- 0x3E works on EHVX08S26CA9W

Parameter in packet type 35 for switching DHWbooster on/off
- 0x48 works on EHVX08S26CB9W

Parameter in packet type 36 for setting heating setpoint
- 0x06 works on EHBH16CAV3

Parameter in packet type 36 for setting main zone heating temperature deviation in WD mode
- 0x08 works on EHYHBX08AAV3

Parameter in packet type 36 for setting additional zone heating temperature deviation in WD mode
- 0x0D works on EHYHBX08AAV3

Parameter in packet type 36 for setting DHW setpoint
- 0x03 works on EHYHBX08AAV3 and EHBH16CAV3

More writable parameters can be found in Budulinek's [documentation](https://github.com/budulinek/Daikin-P1P2---UDP-Gateway/blob/main/Payload-data-write.md).
