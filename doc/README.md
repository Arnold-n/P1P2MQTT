# Daikin protocol data format

The information is this directory is based on reserse engineering and assumptions, so there may be mistakes and misunderstandings.

Below is a generic description, more detail per product can be found in the individual files here.

Communication consists of requests and responses between master device and slave devices. Requests are only issued by master, responses by slaves. Master waits for response (or timeout) before issuing new request. 

*Master*

There can only be one master on the bus: the main controller (user interface). For example, on Daikin Altherma the user interface is called EKRUCBL (https://www.daikin.eu/en_us/products/EKRUCBL.html)

*Slaves*

There is always at least one slave on bus: the heat pump itself (slave address 00). External controllers can be connected as additional slaves (slave address Fx, starting with F0), maximum number of external controllers is product-dependent. Various Daikin or 3rd party devices can be connected as external controllers (slaves), for example Daikin LAN adapters (BRP069A61, BRP069A62), Zennio KNX adapters (KLIC-DA), Realtime Modbus adapters (RTD-LT/CA) and other.
The P1/P2 adapter developed here sniffs communication between the main controller and other slaves (mainly heat pump - slave 00). We will also try to control the heat pump by acting as just another external controller (slave address Fx).

As of v0.9.6 the documentation format has been changed: byte nr starts now at 0 to match the C++ code.

**High-level packet description**

Each packet consists of header (3 bytes), payload data (between 0 and 20 bytes) and CRC checksum (1 byte):


| Byte position       |             | Hex value observed         |
|---------------|:------------------------------|:---------------|
|  0            | direction                | 00 = request from main controller to a slave device<br>  40 = response from slave device<br> 80 = other communication (during boot)
|  1            | slave address                 | 00 = heat pump<br>Fx = external controller 
|  2            | packet type                   | 1x = regular communication with heat pump (slave address 00)  <br> 3x = communication with external controller (slave address Fx)  <br> 2x,6x,7x,8x,9x,Ax,Bx = parameter/status communication and setting (also during boot sequence)
|  3..(N-2)     | payload data                           | see detailed info in the product-dependent files
|  (N-1)        | CRC checksum                           | 


**Cycle description**

Each regular data cycle consists of a package, where
-in a first phase the main controller and heat pump take turn as transmitter in a /master/slave mode request-response pattern, with packet types typically 10 and higher. 
-in a second phase the main controller and external controllers take turn as master/slave performing a request/response procedure

**Payload data format**

The following data-types were observed in the payload data:

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

This may vary , but seen is
- approximately 25ms between request from main controller and answer from heat pump
- approximately 20ms between request from main controller and answer from external controller
- approximately 30-50ms (42/37/47/31/29/36) between answer and next request
- approximately 100..200 ms timeout after a request for an external controller if there is no reply

This timing corresponds partly to the description found in a design guide from Daikin which specifies that a response must follow a request after 25ms, and a new request must not follow a response before another 25ms silence.

**Reboot process**

The reboot process consists of several phases, including lots of data communication on parameter settings.
