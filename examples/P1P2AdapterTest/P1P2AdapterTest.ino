/* P1P2AdapterTest: ping-pong test using data communication between two P1P2 adapters.
 *			Connect two adapters (on 2 Arduino's), and power one of them via the 15V connection.
 *                      After a reset of one of the two Arduinos, a first message is sent,
 *                      and each message is slightly modified and copied back to the other adapter.
 *                      both adapters output the incoming signal on the USB/serial output.
 *                      Optionally a third adapter running P1P2Monitor can be used to inspect bus communication.
 *
 * Copyright (c) 2019 Arnold Niessen, arnold.niessen -at- gmail-dot-com
 *                    P1P2Serial and P1P2AdapterTest are licensed under the GPL v2.0 (see LICENSE)
 *
 * Version history P1P2AdapterTest:
 * 20190428 0.9.2 initial version
 *
 */

#define CRC_GEN 0xD9    // Add and perform CRC check; these values work for the Daikin hybrid
#define CRC_FEED 0x00   //
#define DELAY 10        // Delay between read and write in ms; 
                        // Shorter than 10ms may result in instabablity, especially if both adapters have low-resistance line termination installed
                        // This instability may be caused by higher power consumption with dual line termination and high throughput
                        // in practice min delay is 25ms anyway

#include <P1P2Serial.h>

#define RWbuflen 10     // max read/write buffer (packet) length (excludes CRC byte)
uint8_t RB[RWbuflen];
uint8_t WB[RWbuflen];

P1P2Serial P1P2Serial;

void setup() {
	Serial.begin(115200);
	while (!Serial) ; // wait for Arduino Serial Monitor to open
	Serial.println("");
	Serial.println("P1P2AdapterTest v0.9.2");
	P1P2Serial.begin(9600);
	P1P2Serial.setEcho(0);
	for (int i=0;i<RWbuflen;i++) WB[i]=0x00;
	WB[1]=0x14;
	WB[3]=0xFF;
	WB[RWbuflen-1]=0x66;
	P1P2Serial.writepacket(WB, RWbuflen, DELAY, CRC_GEN, CRC_FEED);
}

void loop() {
	uint16_t n = P1P2Serial.readpacket(RB, RWbuflen, CRC_GEN, CRC_FEED);
	switch (n) {
		case DELTA_OVERRUN  : Serial.println("Delta overrun"); break;
		case DELTA_PE       : Serial.println("Delta PE"); break;
		case DELTA_PE_EOB   : Serial.println("Delta PE EOB"); break;
		case DELTA_COLLISION: Serial.println("Delta collission"); break;
		case DELTA_MAXLEN   : Serial.println("Delta maxlen"); break;
		case DELTA_CRCE     : Serial.println("Delta CRCE"); break;
		default             : Serial.print("Read    n="); Serial.print(n);Serial.print(" ");
		                      for (int i=0; i<n; i++) {if (RB[i] < 0x10) Serial.print('0'); Serial.print(RB[i],HEX);}
		                      Serial.println(); WB[0]=RB[0]+1; P1P2Serial.writepacket(WB, n, DELAY, CRC_GEN, CRC_FEED); break;
	}
} 
