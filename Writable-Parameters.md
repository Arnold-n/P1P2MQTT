** Writable parameters **

Parameters can be set in the so-called F035, F036, and F03A communication blocks. There are many parameters but only some of them are writable with a known function, also, the parameters are different for different systems. The best way to learn which parameter you need to set it so change a setting manually on the main controller while P1P2Monitor acts as auxiliary controller and data is logged. The main controller will then inform the P1P2Monitor of the new setting which you can then try to use.

Example writable parameters:

Parameter in packet type 35 for switching cooling/heating on/off

- 0x31 works on EHVX08S23D6V
- 0x2F works on EHYHBX08AAV3      
- 0x2D works on EHVX08S26CA9W     

Parameter in packet type 35 for switching DHW on/off
- 0x40 works on EHVX08S23D6V      
- 0x3E works on EHVX08S26CA9W

Parameter in packet type 35 for switching DHWbooster on/off
- 0x48 works on EHVX08S26CB9W

More writable parameters can be found in Budulinek's [documentation](https://github.com/budulinek/Daikin-P1P2---UDP-Gateway/blob/main/Payload-data-write.md).
