// The following options can be defined, but not all of them on Arduino Uno or the result will be too large or unstable.
// Suggestion to either uncomment what you do not need.
// It will fit in an Arduino Mega.
                         // prog-size data-size   function
                         //    kB        kB
                         //     4       0.6       (no options selected, data-size requirement mainly determined by RX_BUFFER_SIZE (value 50, *5, usage 250 bytes)
                         //                                          and TX_BUFFER_SIZE (value 33, *3, usage 99 bytes) in P1P2Serial.h)
#define MONITOR          //     5       0.2        outputs data (and errors, if any) on Serial output
#define MONITORCONTROL   //     1       0          controls DHW on/off and cooling/heating on/off (requires MONITOR to be defined)
#define MONITORSERIAL    //     0       0          outputs raw hex data packets on serial (requires MONITOR to be defined)
#define JSON             //    14       0.2        generate JSON messages for serial or UDP
#define MQTTTOPICS       //    15       0.1        generate MQTT topics for serial or UDP
//#define OUTPUTUDP        //     6       0.2        transmit JSON and/or MQTT messages over UDP (requires JSON and/or MQTTTOPICS to be defined, tested on Mega+W5500 ethernet shield)
#define OUTPUTSERIAL     //     0       0          outputs JSON and/or MQTT messages on serial (requires JSON and/or MQTTTOPICS to be defined)
#define SAVEHISTORY      //     2       0.2-2.7    (data-size depends on product-dependent parameter choices) saves packet history enabling the use of "UnknownOnly" to output changed values only (no effect if JSON and MQTTTOPICS are not defined)
                         // --------------------
                         //    38       2          total (for all functions enabled, sketch too big)

// Parameter in packet type 35 for switching cooling/heating on/off
//      0x31 works on EHVX08S23D6V	byte setParamDHW = PARAM_DHW_ONOFF; 
//      0x2F works on EHYHBX08AAV3	
//      0x2D works on EHVX08S26CA9W	
#define PARAM_HC_ONOFF 0x31

// Parameter in packet type 35 for switching DHW on/off
//      0x40 works on EHVX08S23D6V 	
//      0x3E works on EHVX08S26CA9W
#define PARAM_DHW_ONOFF 0x40 
                         
#define CONTROL_ID_NONE 0xF0
#define CONTROL_ID_0 0xF0     // first external controller
#define CONTROL_ID_1 0xF1     // second external controller
#define CONTROL_ID_DEFAULT   CONTROL_ID_NONE

