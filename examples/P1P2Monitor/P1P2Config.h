// The following options can be defined,
// but not all of them at the same time on an Arduino Uno
// or the result will be too large or unstable,
// so please uncomment what you do not need.
// If it does not fit on an Arduino Uno, an Arduino Mega2560 can be used.
//
// Examples of valid combinations are: 
// - MONITORSERIAL+DATASERIAL+DEBUG on an Arduino Uno combined with an ESP running P1P2-bridge-esp8266
// - MONITORSERIAL+DATASERIAL+DEBUG on an Minicore/ATmega328 combined with an ESP running P1P2-bridge-esp8266
// - MONITORSERIAL+MQTTTOPICS+SAVEHISTORY+OUTPUTSERIAL on an Arduino Uno (to be combined with RPi or ESP8266)
// - MONITORSERIAL+JSON+SAVEHISTORY+OUTPUTSERIAL on an Arduino Uno (to be combined with RPi or ESP8266)
// - MONITORSERIAL+MONITORCONTROL+DATASERIAL+JSON+SAVEHISTORY+OUTPUTUDP on an Arduino Mega2560 and a W5500

                         // prog-size data-size   function
                         //      kB        kB
                         //    10.7       0.8       (no options selected)
#define DEBUG            //     0.5       0         (extra serial info on boot)
#define MONITORCONTROL   //     2.2       0          Enables P1P2 bus writing (as external interface and/or requesting counters)
#define DATASERIAL       //     0.5       0          outputs raw hex data packets on serial
//#define JSON             //    16.2       0.2      generate JSON messages for serial and/or UDP
//#define MQTTTOPICS       //    16.2       0.2        generate MQTT topics for serial and/or UDP
//#define SAVEHISTORY      //     3.4       0.3-3.4    (data-size depends on product-dependent parameter choices) saves packet history
                         //                          enabling the use of "UnknownOnly" to output changed values only (no effect if JSON and MQTTTOPICS are not defined)
// Output method selection:
//#define OUTPUTUDP        //     8.3       0.2        transmit JSON and/or MQTT messages over UDP (requires JSON and/or MQTTTOPICS to be defined, tested on Mega+W5500 ethernet shield)
//#define OUTPUTSERIAL     //     0.1       0          outputs JSON and/or MQTT messages on serial (requires JSON and/or MQTTTOPICS to be defined)
                         // --------------------
                         //    41.2       1.9        total on ATmega328P (for all functions enabled, sketch too big for 32kB memory)
                         //    41.7       4.6        total on ATmega2560 (for all functions enabled)
#define SERIALSPEED 115200
#define OUTPUTUNKNOWN 0 // whether mqtt/json parameters include parameters even if functionality is unknown (can be changed run-time using 'U'/'u' command)
#define CHANGEONLY 1    // whether mqtt/json parameters are included only if changed (can be changed run-time using 'S'/'s' command)
                        //   (both work only for parameters for which savehistory() is active)

#define COUNTERREPEATINGREQUEST 0 // Change this to 1 to trigger a counter request cycle at the start of each minute
                                  //   By default this works only as, and only if there is no other, first auxiliary controller (F0)
				  //   The counter request is done after the (unanswered) 00F030*, unless KLICDA is defined
				  //   Requesting counters can also be done manually using the 'C' command
//#define KLICDA                    // If KLICDA is defined, 
                                  //   the counter request is not done in the F0 pause but is done after the 400012* response
                                  //   This works for systems were the usual pause between 400012* and 000013* is around 47ms (+/- 20ms timer resolution)
				  //   So this may only work without bus collission on these systems, use with care!
#define KLICDA_DELAY 9            // If KLICDA is defined, the counter request is inserted at KLICDA_delay ms after the 400012* response
                                  //   This value needs to be selected carefully to avoid bus collission between the 4000B8* response and the next 000013* request
				  //   A value around 9ms works for my system; use with care, and check your logs

// CRC settings
//
#define CRC_GEN 0xD9    // Default generator/Feed for CRC check; these values work at least for the Daikin hybrid
#define CRC_FEED 0x00   // Define CRC_GEN to 0x00 means no CRC is checked when reading or added when writing
                        // These values can be changed via serial port

// External controller settings (see more writable parameters documented in Writable_Parameters.md and/or
// https://github.com/Arnold-n/Daikin-P1P2---UDP-Gateway/blob/main/Payload-data-write.md)
//
// Packet type 35 has 16-bit parameter address and 8-bit values
// Parameter in packet type 35 for switching cooling/heating on/off
//      0x31 works on EHVX08S23D6V
//      0x2F works on EHYHBX08AAV3	
//      0x2D works on EHVX08S26CA9W	
#define PARAM_HC_ONOFF 0x0031

// Parameter in packet type 35 for switching DHW on/of
//      0x40 works on EHVX08S23D6V 	
//      0x40 works on EHYHBX08AAV3	
//      0x3E works on EHVX08S26CA9W
#define PARAM_DHW_ONOFF 0x0040

// Other parameters in packet type 35:
//      0x48 works on EHVX08S26CB9W
//      (no code specifically supporting this function, but parameter can be set using 'P48' 'Y1'/'Y0' commands)

// Packet type 36 has 16-bit parameter address and 16-bit values
// Parameter in packet type 36 for temperature settings
//      0x0003 works on EHYHBX08AAV3 for DHW temperature setting
//      (no code specifically supporting this function, but parameter can be set using 'P48' 'Y1'/'Y0' commands)
#define PARAM_TEMP 0x0003

// Packet type 3A has 16-bit parameter address and 8-bit values
// Parameters in packet type 3A are used for various system settings
// such as holiday, schedule, cooling/heating
#define PARAM_SYS 0x005B

#define CONTROL_ID_NONE 0x00  
#define CONTROL_ID_0    0xF0     // first external controller address
#define CONTROL_ID_1    0xF1     // second external controller address
#define CONTROL_ID_DEFAULT CONTROL_ID_NONE   // will only work if no external controller is detected

#define F030DELAY 100   // Time delay for in ms external controller simulation, should be larger than any response of other external controllers (which is typically 25-80 ms)
#define F03XDELAY  30   // Time delay for in ms external controller simulation, should preferably be a bit larger than any regular response from external controllers (which is typically 25 ms)
#define F0THRESHOLD 5   // Number of 00Fx30 messages to remain unanswered before we feel safe to act as this controller
