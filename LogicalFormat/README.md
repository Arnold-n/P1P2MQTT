# Daikin P1/P2 protocol introduction

### Warning

The information is this directory is based on reverse engineering and assumptions, so there may be mistakes and misunderstandings. If you find any errors or additions, please feel free to add or react.

### System introduction

This is only a generic description, more detail per product can be found in the individual files here.

The HBS standard has an advanced collision detection and priority mechanism. Daikin does not use this. Daikin does not seem to support this and does not seem to detect bus collisions. So take care when writing to the bus to avoid bus collisions. P1P2Serial attempts to detect bus collisions and stop writing upon detection.

In a Daikin system, devices simply alternate turns in writing to the bus. Communication on the P1/P2 bus consists of requests/response packets between a main controller device (previously called master device) and one or more peripheral devices (previously called slave devices).

#### Main controller

On Daikin Altherma the user interface may be called EKRUCBL (https://www.daikin.eu/en_us/products/EKRUCBL.html). Sometimes the main controller is located inside the heat pump system itself, especially if the system has a screen menu (example: EHVX08S23D6V). The main controller performs a request-response protocol with a main peripheral (the heat pump system) and optionally additional auxiliary controllers (such as a LAN adapter, or this project's P1P2Monitor running on the P1P2-ESP-interface). The main controller waits for a response (or timeout) before issuing a new request. 

#### Peripherals

There is always at least one peripheral on the P1/P2 bus: the heat pump itself (address 00). Depending on the main controller, one or more external controllers are allowed as additional peripherals (address 0xF0 and higher). An additional peripheral (such as P1P2Monitor itself, a Daikin LAN adapter BRP069A61 or BRP069A62, a Zennio KLIC-DA KNX adapter, or Realtime Modbus RTD-LT/CA adapter) acts as an auxiliary (or external) controller. 

#### How to avoid bus collissions when writing?

The simple answer is: write when others don't. In practice communication seems to be initiated by the thermostat(s), and parties alternate turns. The heat pump seems to answer each request after a fixed time (usually 25ms in my situation). P1P2Serial implements a setDelay(t) function which ensures a write does not happen until exactly t milliseconds after the last start bit falling edge detected - or if this time has passed, after a longer timout). Calling setDelay(2) before starting a (block)transmission is a safe way to start any block of communication. If imitating a heat pump, calling setDelay(25) would make the adapter answer 25ms after the last byte received. Use of setDelay(1) is discouraged: a byte transmission takes slightly longer than 1 ms. A setDelay(1) will set transmission to 1.146ms, the time it takes to transmit 1 byte (11 bits at 9600 baud), but this still creates a bus collision if the other party transmitting has not finished. This may be detected by a parity error or read-back verification error.ter avoided.

### P1P2Monitor interaction

P1P2Monitor not only acts as an auxiliary controller (like some commercial products), it also monitors all communication between main controller and heat pump, and reports detected parameters and/or raw data, which results in faster data reporting over MQTT. All communication is forwarded over MQTT/WiFi enabling integration in Home Assistant and other domotica solutions. Additional so-called pseudo-packets are added which do not originate from the P1/P2 bus as described below.

Because we have no access to Daikin's documentation, we do not know which systems are supported. Various users have reported success in basic control functions. In my system I can control a lot of parameters. It is logical to assume that devices that are supported by other auxiliary controllers (Zennio KLIC-DA KNX interface, Daikin LAN adapter, are or could be supported.

### Package cycle description

Each regular data cycle consists of a package, where
- in a first phase the main controller and heat pump take turn as transmitter in a request-response pattern, with packet types typically 0x10-0x16.
- in a second phase the main controller and external controller(s) take turn in a similar request/response procedure

The heat pump ignores any communication between main controller and external controller. Any parameter change request from the auxiliary controller (done in a response-packet) is handled by the main controller, after which the main controller requests a parameter change in the heat pump system. The same applies in the other direction: any information originating from the heat pump is forwarded by the main controller to the auxiliary controller.

#### Timing between packets

This may vary, but observed is
- approximately 25ms between request from main controller and answer from heat pump
- approximately 20ms between request from main controller and answer from external controller
- approximately 30-50ms (42/37/47/31/29/36) between answer and next request
- approximately 100..200 ms timeout after a request for an external controller if there is no reply

This timing corresponds partly to the description found in a design guide from Daikin which specifies that a response must follow a request after 25ms, and a new request must not follow a response before another 25ms silence.

### Packet types

The following packet types have been observed on the P1/P2 bus.

- 0x00-0x05 during system restart, a.o. peripheral identification
- 0x10-0x18 communication between main controller and heat pump
- 0x20-0x21 status information?
- 0x30-0x3F communication between main and auxiliary controller
- 0x60-0x8F field settings
- 0x90-0x9F during system restart, function unknown
- 0xA1, 0xB1 product identifier in ASCII
- 0xB8 kWh, hour and start counters

#### Pseudo-packets

Packet types 08-0F have not been observed on the P1/P2 bus. P1P2Monitor adds these packets (which we call pseudo-packets) to its serial output to communicate internal state information. P1P2-bridge-esp8266/P1P2MQTT adds further pseudo-packets to communicate more internal state information. This generates additional MQTT output data (in P1P2/A/# topic). The payload of these pseudo-packets is documented [here](Pseudo-packets.md).

### High-level packet description

The Daikin protocol does not follow the HBS packet format specification. Instead, the data is transmitted in a cyclic protocol of short packets of 4-24 bytes each. Each cycle has a number of request packets sent by the thermostat and answered by the heat pump; additionally, a number of request packets are sent by the thermostat and answered by an external auxiliary controller (if available); some systems support only one auxiliary controller, others support two. The Daikin hybrid system uses a protocol where approximately each second a set of 6 request/responses are exchanged with the heat pump, followed by one request to an auxiliary controller. If answered by the auxiliary controller, additional request/response pairs are exchanged. The first few bytes of each packet may indicate source, desination, and/or packet type number. After that the payload follows, followed by a CRC byte. The CRC algorithm for the Daikin hybrid is a simple 8-bit LFSR with a feed of 0x00 and a generator value of 0xD9. It may be different for other products.

Each packet consists of a header (3 bytes), payload data (minimum 0, maximum 20 bytes) and a CRC checksum (1 byte):

| Byte position  |                     | Hex value observed         |
|----------------|:--------------------|:----------------------------------------------------------------------------------------------------------|
|  Header byte 0 | direction           | 00 = request from main controller to peripheral<br> 40 = response from peripheral <br> 80 = observed a.o. during boot
|  Header byte 1 | peripheral address  | 00 = heat pump<br>Fx = external controller 
|  Header byte 2 | packet type         | 1X = packets in basic communication with heat pump  <br> 3X = packets from/to auxiliary controller(s) <br> 0X, 2X, 60-BF = various parameter/status communication and settings
|  0 - 19        | payload data        | see detailed info in the product-dependent files
|  CRC byte      | CRC checksum        | 

#### Payload contents

The payload contains various data items, like temperatures, data flow, software serial number, operating mode, number of operating hours, energy consumed, etcetera. Temperatures and flows can be coded as value\*10, so with an accuracy of one digit after the comma: a value of 0x01B6 (hex value for 438, means 43.8). Other values may be encoded as two bytes, one byte for the value before the comma, the other behind (for example a room temperature is encoded as 0x14 0x18, or 20 24, meaning 20 24/256 or 20.1 degrees).

#### Payload data format

There are two types of payload data:
 - direct information: location determines function. Most or all of the information with the heat pump is in this format. This information is useful for monitoring purposes.
 - enumerated information: in this case a payload segment consists of a <parameter-number, parameter-value> tuple. Parameter-numbers are not unique over the various packet types. Most of the information between main and auxiliary controller is in this format. It cannot only be used for monitoring, but also for controlling the Daikin system.
 - data segments: packet type 0x3E 'announces' a sequence of bytes to be written to a specific memory location; this is used to communicate weekly schedule information between main and auxiliary controller. It is likely that this allows to rewrite the weekly schedules using the P1/P2 adapter.

### Payload data types

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

#### CRC Checksum

The CRC checksum is calculated using an 8-bit LFSR with a feed of 0x00 and a generator value of 0xD9.

### Restart process

The restart process consists of several phases, including lots of data communication on parameter settings and field settings.
