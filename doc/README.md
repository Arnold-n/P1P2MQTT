# Daikin P1/P2 protocol introduction

### Warning

The information is this directory is based on reverse engineering and assumptions, so there may be mistakes and misunderstandings. If you find any errors or additions, please feel free to add or react.

### Introduction

This is only a generic description, more detail per product can be found in the individual files here.

Communication on the P1/P2 bus consists of requests/response packets between a main controller device (previously called master device) and one or more peripheral devices (previously called slave devices).

Requests are only issued by the main controller. Each request addresses a single peripheral. Responses by one of the (addressed) peripherals (heat pump or auxiliary controller). The main controller waits for a response (or timeout) before issuing a new request. 

#### Main controller

There can only be one main controller, this is usually the wall-mounted user interface or main room thermostat. For example, on Daikin Altherma the user interface is called EKRUCBL (https://www.daikin.eu/en_us/products/EKRUCBL.html). Sometimes the main controller is located inside the heat pump system itself, especially if the system has a screen menu (example: EHVX08S23D6V).

#### Peripherals

There is always at least one peripheral on the P1/P2 bus: the heat pump itself (address 00). Depending on the main controller, one or more external controllers are allowed as additional peripherals (address 0xF0 and higher). An additional peripheral (such as a Daikin LAN adapter BRP069A61 or BRP069A62, a Zennio KLIC-DA KNX adapter, or Realtime Modbus RTD-LT/CA adapter) acts as an auxiliary (or external) controller. 

#### P1P2Monitor interaction

P1P2Monitor not only acts as an auxiliary controller (like some commercial products), it also monitors all communication between main controller and heat pump, and reports detected parameters and/or raw data. This reduces delay from measurement to observation of the data on MQTT. All communication is forwarded over MQTT/WiFi enabling integration in Home Assistant and other domotica solutions.

Because we have no access to Daikin's documentation, we do not know which systems are supported. Various users have reported success in basic control functions. In my system I can control a lot of parameters. It is logical to assume that devices that are supported by other auxiliary controllers (Zennio KLIC-DA KNX interface, Daikin LAN adapter, are or could be supported.

**High-level packet description**

Each packet consists of a header (3 bytes), payload data (between 0 and 20 bytes) and a CRC checksum (1 byte):


| Byte position  |                     | Hex value observed         |
|----------------|:--------------------|:----------------------------------------------------------------------------------------------------------|
|  Header byte 0 | direction           | 00 = request from main controller to peripheral<br> 40 = response from peripheral <br> 80 = observed during boot
|  Header byte 1 | peripheral address  | 00 = heat pump<br>Fx = external controller 
|  Header byte 2 | packet type         | 1X = packets in basic communication with heat pump  <br> 3X = packets from/to auxiliary controller(s) <br> 0X, 2X, 60-BF = various parameter/status communication and settings
|  0 ..          | payload data        | see detailed info in the product-dependent files
|  CRC byte      | CRC checksum        | 

**Package cycle description**

Each regular data cycle consists of a package, where
- in a first phase the main controller and heat pump take turn as transmitter in a request-response pattern, with packet types typically 0x10-0x16.
- in a second phase the main controller and external controller(s) take turn in a similar request/response procedure

The heat pump ignores any communication between main controller and external controller. Any parameter change request from the auxiliary controller (done in a response-packet) is handled by the main controller, after which the main controller requests a parameter change in the heat pump system. The same applies in the other direction: any information originating from the heat pump is forwarded by the main controller to the auxiliary controller.

**Payload data format**

There are two types of payload data:
 - direct information: location determines function. Most or all of the information with the heat pump is in this format. This information is useful for monitoring purposes.
 - enumerated information: in this case a payload segment consists of a <parameter-number, parameter-value> tuple. Parameter-numbers are not unique over the various packet types. Most of the information between main and auxiliary controller is in this format. It cannot only be used for monitoring, but also for controlling the Daikin system.
 - data segments: packet type 0x3E 'announces' a sequence of bytes to be written to a specific memory location; this is used to communicate weekly schedule information between main and auxiliary controller. It is likely that this allows to rewrite the weekly schedules using the P1/P2 adapter.

**Payload data types**

The following data-types were observed or suspected in the payload data:

| Data type     | Definition                           |
|---------------|:-------------------------------------|
| flag8         | byte composed of 8 single-bit flags  |
| c8            | ASCII character byte                 |
| s8            | signed 8-bit integer -128 .. 127     |
| u8            | unsigned 8-bit integer 0 .. 255      |
| u6            | unsigned 6-bit integer 0 .. 63       |
| u2            | unsigned 2-bit integer 0 .. 3 or 00 .. 11 |
| s16           | signed 16-bit integer -32768..32767  |
| u16           | unsigned 16-bit integer 0 .. 65535   |
| u24           | unsigned 24-bit integer              |
| u32           | unsigned 32-bit integer              |
| u16hex        | unsigned 16-bit integer output as hex 0x0000-0xFFFF    |
| u24hex        | unsigned 24-bit integer          |
| u32hex        | unsigned 32-bit integer          |      
| f8.8 (code: f8_8)  | signed fixed point value : 1 sign bit, 7 integer bit, 8 fractional bits (two’s compliment, see explanation below) |
| f8/8 (code: f8s8)  | Daikin-style fixed point value: 1st byte is value before comma, and 2nd byte is first digit after comma (see explanation below) |
| s-abs4        | Daikin-style temperature deviation: bit 4 is sign bit, bits 0-3 is absolute value of deviation |
| sfp7          | signed floating point value: 1 sign bit, 4 mantissa bits, 2 exponent bits (used for field settings) |
| u8div10       | unsigned 8-bit integer 0 .. 255, to be divided by 10    |
| u16div10      | unsigned 16-bit integer 0 .. 65535, to be divided by 10 |
| t8            | schedule moment in 10 minute increments from midnight (0=0:00, 143=23:50) |
| d24           | day in format YY MM DD                 |

Explanation of f8.8 format: a temperature of 21.5°C in f8.8 format is represented by the 2-byte value in 1/256th of a unit as 1580 hex (1580hex = 5504dec, dividing by 256 gives 21.5). A temperature of -5.25°C in f8.8 format is represented by the 2-byte value FAC0 hex (FAC0hex = - (10000hex-FACOhex) = - 0540hex = - 1344dec, dividing by 256 gives -5.25).

Explanation of f8/8 format: a temperature of 21.5°C in f8/8 format is represented by the byte value of 21 (0x15) followed by the byte value of 5 (0x05). So far this format was only detected for the setpoint temperature, which is usually not negative, so we don't know how negative numbers are stored in this format.

**CRC Checksum**

The CRC checksum is calculated using an 8-bit LFSR with a feed of 0x00 and a generator value of 0xD9.

**Timing between packets**

This may vary, but observed is
- approximately 25ms between request from main controller and answer from heat pump
- approximately 20ms between request from main controller and answer from external controller
- approximately 30-50ms (42/37/47/31/29/36) between answer and next request
- approximately 100..200 ms timeout after a request for an external controller if there is no reply

This timing corresponds partly to the description found in a design guide from Daikin which specifies that a response must follow a request after 25ms, and a new request must not follow a response before another 25ms silence.

**Restart process**

The restart process consists of several phases, including lots of data communication on parameter settings and field settings.
