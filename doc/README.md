# Daikin protocol data format

The information is this directory is based on reserse engineering and assumptions, so there may be mistakes and misunderstandings.

Below is a generic description, more detail per product can be found in the individual files here.

It seems the primary controller functions as a master, and the heat pump and (if available) the external controllers operate as a slave.

As of v0.9.6 the documentation format has been changed: byte nr starts now at 0 to match the C++ code.

**High-level packet description**

The following data packet format was observed on various units (detailed info in the product-dependent files):


| Byte nr       | Hex value observed            |                |
|---------------|:------------------------------|:---------------|
|  0            | 00 or 40 or 80                | 00 = request from main controller to a slave device<br>  40 = response from slave device<br> 80 = other communication (during boot)
|  1            | 00 or FX                      | Slave address:<br> 00 = heat pump<br>FX = external controller
|  2            |  XX                           | packet type, typically:<br>
|               |                               |  1x used for regular with heat pump
|               |                               |  3x used for communication with external controller
|               |                               |  2x,6x,7x,8x,9x,Ax,Bx used for parameter/status communication and setting (also during boot sequence)
|  3..(N-2)     |  XX                           | payload data
|  (N-1)        |  XX                           | CRC checksum


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
