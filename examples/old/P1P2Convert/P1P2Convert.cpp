/* P1P2Convert: application for Linux/Ubuntu to post-process serial output (or recorded log file thereof) from P1P2Monitor,
 *                  aimed to support mqtt/json/wifi (work in progress)
 *                  for now P1P2Convert only supports stdout
 *                  For protocol description, see SerialProtocol.md and MqttProtocol.md
 *                     
 * Copyright (c) 2019 Arnold Niessen, arnold.niessen -at- gmail-dot-com  - licensed under GPL v2.0 (see LICENSE)
 *
 * Version history
 * 20190908 v0.9.8 work in progress
 *
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// define Arduino style types
#define byte unsigned char
#define uint8_t unsigned char
#define uint16_t unsigned int
#define uint32_t unsigned long int
#define int8_t signed char
#define int16_t signed int
#define int32_t signed long int

// Include Daikin product-dependent header file for parameter conversion
//
// Note that this program does not use the (avr only) P1P2Serial library
// so we need to include the json header files with a relative reference below
//
static int changeonly = 1;    // whether json parameters are included if unchanged
static int outputunknown = 1; // whether json parameters include parameters for which we don't know function
#define SAVEHISTORY           // Always to be defined for ESP8266 or Linux, as we have plenty of memory

#include "P1P2_Daikin_ParameterConversion_EHYHB.h"

void process_for_serial(byte* rb, int n) {
  char value[KEYLEN];    
  char key[KEYLEN + 9]; // 9 character prefix for topic
  byte cat;
  strcpy(key,"P1P2/P/U/");
  for (byte i = 3; i < n; i++) {
    int kvrbyte = bytes2keyvalue(rb, i, 8, key + 9, value, cat);
    // returns 0 if byte does not trigger any output
    // returns 1 if a new value should be output
    // returns 8 if byte should be treated per bit
    // returns 9 if json string should be terminated
    if (kvrbyte != 9) {
      for (byte j = 0; j < kvrbyte; j++) {
        int kvr= (kvrbyte==8) ? bytes2keyvalue(rb, i, j, key + 9, value, cat) : kvrbyte;
        if (kvr) {
          switch (cat) {
            case 1 : key[7] = 'P'; // parameter settings
                     break;
            case 2 : key[7] = 'M'; // measurements, time, date
                     break;
            case 3 : key[7] = 'F'; // F related params
                     break;
            case 4 : key[7] = 'T'; // Temp Flow Power measurements
                     break;
            case 0 : // fallthrough
            default: key[7] = 'U'; // unknown
          }
          printf("%s %s %s\n", (SelectTimeString2 ? TimeString2 : TimeString), key, value);
        }
      }
    }
  }
  savehistory(rb, n);
}

#define RB 200     // max size of readbuf (serial input from Arduino)
#define HB 32      // max size of hexbuf, same as P1P2Monitor
char readbuf[RB];
byte readhex[HB];
  
int main() {
  printf("* [Host] P1P2Convert v0.9.8\n");
  byte rb;
  while (fgets(readbuf, RB, stdin) != NULL) {
    // stops when either (RB-1) characters are read, the newline character is read, or the end-of-file is reached, whichever comes first.
    // for now we trust input and we assume we always receive a newline character (worst-case we delete last character)
    rb = strlen(readbuf);
    rb--;
    if ((rb > 0) && (readbuf[rb - 1] == '\r')) rb--;
    readbuf[rb] = '\0';
    //printf("%i:%s <->\n", strlen(readbuf), readbuf);
    if (readbuf[0] == 'R') {
      int rbp = 0;
      int n, rbtemp;
      if ((rb >= 10) && (readbuf[7] == ':')) rbp = 9;
      byte rh = 0;
      // check for errors is done in P1P2Monitor as of v0.9.7 (31 August 2019 version)
      // dirty trick: check for space in serial input to avoid that " CRC" in input is recognized as hex value 0x0C...
      while ((rh < HB) && (readbuf[rbp] != ' ') && (sscanf(readbuf + rbp, "%2x%n", &rbtemp, &n) == 1)) {
        readhex[rh++] = rbtemp;
        rbp += n;
      }
      // rh is packet length (not counting CRC byte)
      process_for_serial(readhex, rh);
    } else {
      printf("%s P1P2/S %s\n", (SelectTimeString2 ? TimeString2 : TimeString), readbuf);
    }
  }
}
