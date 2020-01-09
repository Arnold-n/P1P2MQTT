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
//#define JSON             //    14       0.2        generate JSON messages for serial or UDP
//#define MQTTTOPICS       //    15       0.1        generate MQTT topics for serial or UDP
//#define OUTPUTUDP        //     6       0.2        transmit JSON and/or MQTT messages over UDP (requires JSON and/or MQTTTOPICS to be defined, tested on Mega+W5500 ethernet shield)
#define OUTPUTSERIAL     //     0       0          outputs JSON and/or MQTT messages on serial (requires JSON and/or MQTTTOPICS to be defined)
#define SAVEHISTORY      //     2       0.2-2.7    (data-size depends on product-dependent parameter choices) saves packet history enabling the use of "UnknownOnly" to output changed values only (no effect if JSON and MQTTTOPICS are not defined)
                         // --------------------
                         //    38       2          total (for all functions enabled, sketch too big)

#define OUTPUTUNKNOWN 0 // whether mqtt/json parameters include parameters even if functionality is unknown (can be changed run-time using 'U'/'u' command)
#define CHANGEONLY 1    // whether mqtt/json parameters are included only if changed (can be changed run-time using 'S'/'s' command)
                        //   (both work only for parameters for which savehistory() is active)
                        //
#define COUNTERREPEATINGREQUEST 0 // Change this to 1 to trigger a counter request cycle at the start of each minute
                                  //   By default this works only as, and only if there is no other, first auxiliary controller (F0)
				  //   The counter request is done after the (unanswered) 00F030*, unless KLICDA is defined
				  //   Requesting counters can also be done manually using the 'C' command
//#define KLICDA                    // If KLICDA is defined, 
                                  //   the counter request is not done in the F0 pause but is done after the 400012* response
                                  //   This works for systems were the usual pause between 400012* and 000013* is around 47ms (+/- 20ms timer resolution)
				  //   So this may only work without bus collission on these systems, use with care!
#define KLICDA_delay 9            // If KLICDA is defined, the counter request is inserted at KLICDA_delay ms after the 400012* response
                                  //   This value needs to be selected carefully to avoid bus collission between the 4000B8* response and the next 000013* request
				  //   A value around 9ms works for my system; use with care, and check your logs

// CRC settings
//
#define CRC_GEN 0xD9    // Default generator/Feed for CRC check; these values work at least for the Daikin hybrid
#define CRC_FEED 0x00   // Define CRC_GEN to 0x00 means no CRC is checked when reading or added when writing
                        // These values can be changed via serial port

// External controller settings
//
// Parameter in packet type 35 for switching cooling/heating on/off
//      0x31 works on EHVX08S23D6V
//      0x2F works on EHYHBX08AAV3	
//      0x2D works on EHVX08S26CA9W	
#define PARAM_HC_ONOFF 0x31

// Parameter in packet type 35 for switching DHW on/off
//      0x40 works on EHVX08S23D6V 	
//      0x3E works on EHVX08S26CA9W
#define PARAM_DHW_ONOFF 0x40

// Parameter in packet type 35 for switching DHWbooster on/off
//      0x48 works on EHVX08S26CB9W
//      (no code specifically supporting this function, but parameter can be set using 'P48' 'Y1'/'Y0' commands)

#define CONTROL_ID_NONE 0x00  
#define CONTROL_ID_0 0xF0     // first external controller
#define CONTROL_ID_1 0xF1     // second external controller
#define CONTROL_ID_DEFAULT CONTROL_ID_NONE   // will only work if no external controller is detected

#define F030DELAY 100   // Time delay for external controller simulation, should be larger than any response of other external controllers (which is typically 25-80 ms)
#define F03XDELAY  30   // Time delay for external controller simulation, should preferably be a bit larger than any regular response from external controllers (which is typically 25 ms)
#define F0THRESHOLD 5   // Number of 00Fx30 messages to be unanswered before we feel safe to act as this controller

// Program schedule settings (planned)

/* 
#define T_HOUR_ON 7
#define T_MIN_ON 30
#define T_HOUR_OFF 23
#define T_MIN_OFF 30
*/
