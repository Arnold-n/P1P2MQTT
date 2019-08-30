/* PP1P2_Daikin_ParameterConversion.h product-ndependent code
 *                     
 * Copyright (c) 2019 Arnold Niessen, arnold.niessen -at- gmail-dot-com  - licensed under GPL v2.0 (see LICENSE)
 *
 */

#ifndef P1P2_Daikin_json
#define P1P2_Daikin_json

// these conversion functions look at the current and often also one or more past byte(s) in the byte array
bool FN_flag8(uint8_t b, uint8_t n)  { return (b >> n) & 0x01; }
uint32_t FN_u24(uint8_t *b)          { return ((b[-2] << 16) | ((b[-1]) << 8) | (b[0]));}
uint16_t FN_u16(uint8_t *b)          { return (                ((b[-1]) << 8) | (b[0]));}
int8_t FN_u8delta(uint8_t *b)        { int c=b[0]; if (c & 0x10) return -(c & 0x0F); else return (c & 0x0F); }
float FN_u8div10(uint8_t *b)         { return b[0] * 0.1;}
float FN_f8_8(uint8_t *b)            { return (((int8_t) b[-1]) + (b[0] * 1.0 / 256)); }


float FN_f8s8(uint8_t *b)            { return ((float)(int8_t) b[-1]) + ((float) b[0]) /  10;}

#ifdef __AVR__
#define KEY(K) strlcpy_PF(key, F(K), KEYLEN-1)
#else /* not __AVR__ */
#ifdef ESP8266
#include <pgmspace.h>
#define KEY(K) strlcpy(key, K, KEYLEN-1)
#else /* ! ESP8266 */
// assume RPi
#define RPI
#define KEY(K) { strncpy(key, K, KEYLEN-1); key[KEYLEN-1] = '0'; }
#endif /* ESP8266 */
#endif /* __AVR__ */

#ifdef SAVEHISTORY
// save-history pointers
static byte rbhistory[RBHLEN];           // history storage
static uint16_t savehistoryend=0;        // #bytes saved so far
static uint16_t savehistoryp[RBHNP]={};  // history pointers
static  uint8_t savehistorylen[RBHNP]={}; // length of history for a certain packet

static byte paramhistory[PARAMLEN * 4];      // history storage for params in 00F035/00F135/40F035/40F13

int8_t savehistoryindex(byte *rb); // defined in product-dependent code
uint8_t savehistoryignore(byte *rb);

void savehistory(byte *rb, int n) {
// also modifies savehistoryp, savehistorylen, savehistoryend
  if (n > 3) {  
    int8_t shi = savehistoryindex(rb);
    if (shi >= 0) {
      uint8_t shign = savehistoryignore(rb);
      if (!savehistorylen[shi]) {
        if (savehistoryend + (n - shign) <= RBHLEN) {
          savehistoryp[shi] = savehistoryend;
  #ifdef RPI
          printf("* Savehistory allocating %i - \n",savehistoryend);
  #else /* RPI */
          //Serial.print(F("* Savehistory allocating "));
          //Serial.print(savehistoryend);
          //Serial.print("-");
  #endif /* RPI */
          savehistorylen[shi] = n - shign;
          savehistoryend += (n - shign);
  #ifdef RPI
          printf("%i for 0x%x%x-0x%x%x\n",savehistoryend, rb[0] >> 4, rb[0] & 0x0F, rb[2] >> 4, rb[2] & 0x0F);
  #else /* RPI */
          //Serial.print(savehistoryend);
          //Serial.print(F(" for "));
          //if (rb[0] < 0x10) Serial.print("0");
          //Serial.print(rb[0],HEX);
          //Serial.print(" ");
          //if (rb[2] < 0x10) Serial.print("0");
          //Serial.println(rb[2],HEX);
  #endif /* RPI */
        } else if (savehistoryend < RBHLEN) {
          // not enough space available, store what we can
          savehistoryp[shi] = savehistoryend;
          savehistorylen[shi] = (RBHLEN - savehistoryend);
          savehistoryend += RBHLEN;
  #ifdef RPI
          printf("* Not enough memory, shortage %i for %x%x-%x%x\n", (n-shign) + savehistoryend - RBHLEN, rb[0] >> 4, rb[0] & 0x0F, rb[2] >> 4, rb[2] & 0x0F);
  #else /* RPI */
          //Serial.print(F("* Not enough memory, shortage "));
          //Serial.print((n - shign) + savehistoryend - RBHLEN);
          //Serial.print(F(" for "));
          //if (rb[0] < 0x10) Serial.print("0");
          //Serial.print(rb[0],HEX);
          //Serial.print(" ");
          //if (rb[2] < 0x10) Serial.print("0");
          //Serial.println(rb[2],HEX);
  #endif /* RPI */
        } else {
          // no space left
  #ifdef RPI
          printf("* Warning: memory shortage %i for %x%x-%x%x\n", (n-shign), rb[0] >> 4, rb[0] & 0x0F, rb[2] >> 4, rb[2] & 0x0F);
  #else /* RPI */
          //Serial.print("* Warning: memory shortage ");
          //Serial.print((n - shign));
          //Serial.print(F(" for "));
          //if (rb[0] < 0x10) Serial.print("0");
          //Serial.print(rb[0],HEX);
          //Serial.print(" ");
          //if (rb[2] < 0x10) Serial.print("0");
          //Serial.println(rb[2],HEX);
  #endif /* RPI */
        }
      } else {
        // Serial.print("* savehistorylen[shi] "); Serial.println(savehistorylen[shi]);
      }
      if (savehistorylen[shi]) {
        for (byte i = shign;((i < n) && (i < shign + savehistorylen[shi])); i++) {
          // copying history
          rbhistory[savehistoryp[shi] + (i - shign)] = rb[i];
        }
      }
    } else if (shi == -0x35) {
      // save params in Fx35 packets
      for (byte i = 5; i < n; i += 3) {
        int paramnr = (((uint16_t) rb[i-1]) << 8) | rb[i-2];
        byte paramcat = ((rb[0] & 0x40) >> 5) + (rb[1] & 0x01);
        byte paramval = rb[i];
        if ((paramnr >= PARAMSTART) && (paramnr < PARAMSTART + PARAMLEN)) paramhistory[paramnr - PARAMSTART + PARAMLEN * paramcat] = paramval;
      }
    }
  }
}

bool newbytesval(byte *rb, byte i, byte k) {
// returns whether any of bytes (i-k+1) .. byte i in rb has a new value (also returns true if byte has not been saved or if changeonly=0)
  int8_t shi = savehistoryindex(rb);
  if (shi >= 0) {
    // relevant packet has been saved (at least partially)
    byte shign = savehistoryignore(rb);
    if ((i - k + 1 >= shign) && (i < savehistorylen[shi] + shign)) {
      // enough history available, so compare k bytes
      for (byte j = i - k + 1; j <= i; j++) {
        if ((rbhistory[j - shign + savehistoryp[shi]]) != rb[j]) return 1;
      }
      // no diff found
      return !changeonly;
    } else {
      // no or not enough history in the partly saved packet, so assume value is new
      return 1;
    }
  } else if (shi == -0x35) {
    // check param in Fx35 packet
    // check whether param nr (i-2)/(i-1) in byte i has a new value (also returns true if byte has not been saved or if changeonly=0)
    int paramnr = (((uint16_t) rb[i-1]) << 8) | rb[i-2];
    byte paramcat = ((rb[0] & 0x40) >> 5) + (rb[1] & 0x01);
    byte paramval = rb[i];
    if ((paramnr >= PARAMSTART) && (paramnr < PARAMSTART + PARAMLEN)) {
      if (paramhistory[paramnr - PARAMSTART + PARAMLEN * paramcat] != paramval) return 1; else return !changeonly;
    } else {
      // paramval outside saved range
      return 1;
    }
  } else {
    // no history for this packet, assume value is new
    return 1;
  }
}

bool newbitval(byte *rb, byte i, byte p) {
// returns whether bit p of byte i in rb has a new value (also returns true if byte has not been saved)
  int8_t shi = savehistoryindex(rb);
  if (shi >= 0) {
    // relevant packet has been saved (at least partially)
    byte shign = savehistoryignore(rb);
    if ((i >= shign) && (i < savehistorylen[shi] + shign)) {
      // enough history available, so compare bits
      if ((((rbhistory[i - shign + savehistoryp[shi]]) ^ rb[i]) >> p) & 0x01) {
        // difference found
        return 1;
      } else {
        // no diff
        return !changeonly;
      }
    } else {
      // no history for this byte, so assume value is new
      return 1;
    }
  } else {
    // no history for this packet, assume value is new
    return 1;
  }
}
#else /* !SAVEHISTORY */
#define newbytesval(a, b, c) (1)
#define newbitval(a, b, c) (1)
#endif /* SAVEHISTORY */

byte handleparam(char* key, char* value, byte* rb, uint8_t i) {
  int paramnr = (((uint16_t) rb[i-1]) << 8) | rb[i-2];
  byte paramval = rb[i];
  //TODO if (!outputunknown) return 0;
  if (!newbytesval(rb, i, 0)) return 0;
  if (paramnr == 0xFFFF) return 0; // no param
  if (paramnr == 0xA2) return 0; // counter; useless?
  snprintf(key, KEYLEN, "P0x%X%X-0x%X%X-0x%X%X-0x%X%X", rb[0] >> 4, rb[0]&0x0F, rb[1] >> 4, rb[1] & 0x0F, rb[2] >> 4, rb[2] & 0x0F, paramnr >> 4, paramnr & 0x0F);
  snprintf(value, KEYLEN, "0x%2X", paramval);
  return 1;
}

byte unknownbyte(char* key, char* value, byte* rb, uint8_t i) {
  if (!outputunknown || !newbytesval(rb, i, 1)) return 0;
  snprintf(key, KEYLEN, "Byte-0x%X%X-0x%X%X-%i", rb[0] >> 4, rb[0] & 0x0F, rb[2] >> 4, rb[2] & 0x0F, i);
  snprintf(value, KEYLEN, "0x%x%x", rb[i] >> 4, rb[i] & 0x0F);
  return 1;
}

byte unknownbit(char* key, char* value, byte* rb, uint8_t i, uint8_t j) {
  if ((!outputunknown) || !newbitval(rb, i, j)) return 0;
  snprintf(key, KEYLEN, "Bit-0x%X%X-0x%X%X-%i-%i", rb[0] >> 4, rb[0] & 0x0F, rb[2] >> 4, rb[2] & 0x0F, i, j);
  snprintf(value, KEYLEN, "%i", FN_flag8(rb[i], j));
  return 1;
}

// macros for use in device-dependent code
#define UNKNOWN     {(cat = 0);}
#define SETTING     {(cat = 1);}
#define MEASUREMENT {(cat = 2);}
#define PARAM35     {(cat = 3);}
#define TEMPFLOWP   {(cat = 4);}

#define BITBASIS         { return newbytesval(rb, i, 1) << 3; } // returns 8 if at least one bit of a byte changed, otherwise 0
#define VALUE_u8         { if (!newbytesval(rb, i, 1)) return 0; snprintf(value, KEYLEN, "%i", rb[i]);                return 1; }
#define VALUE_u24        { if (!newbytesval(rb, i, 3)) return 0; snprintf(value, KEYLEN, "%lui", FN_u24(&rb[i]));     return 1; }
#define VALUE_u8_add2k   { if (!newbytesval(rb, i, 1)) return 0; snprintf(value, KEYLEN, "%i", rb[i]+2000);           return 1; }
#define VALUE_u8delta    { if (!newbytesval(rb, i, 1)) return 0; snprintf(value, KEYLEN, "%i", FN_u8delta(&rb[i]));   return 1; }
#define VALUE_flag8      { if (!newbitval(rb, i, j))   return 0; snprintf(value, KEYLEN, "%i", FN_flag8(rb[i], j));   return 1; }
#ifdef RPI
#define VALUE_f8_8       { if (!newbytesval(rb, i, 2)) return 0; snprintf(value, KEYLEN, "%11f", FN_f8_8(&rb[i]));    return 1; }
#define VALUE_f8s8       { if (!newbytesval(rb, i, 2)) return 0; snprintf(value, KEYLEN, "%11f", FN_f8s8(&rb[i]));    return 1; }
#define VALUE_u8div10    { if (!newbytesval(rb, i, 1)) return 0; snprintf(value, KEYLEN, "%11f", FN_u8div10(&rb[i])); return 1; }
#define VALUE_F(v, ch)   { if (changeonly && !ch)      return 0; snprintf(value, KEYLEN, "%11f", v);                  return 1; }
#else /* RPI */
// note that snprintf(.. , "%f", ..) is not supported on Arduino/Atmega so use dtostrf instead
#define VALUE_f8_8       { if (!newbytesval(rb, i, 2)) return 0; dtostrf(FN_f8_8(&rb[i]), 1, 2, value);               return 1; }
#define VALUE_f8s8       { if (!newbytesval(rb, i, 2)) return 0; dtostrf(FN_f8s8(&rb[i]), 1, 1, value);               return 1; }
#define VALUE_u8div10    { if (!newbytesval(rb, i, 1)) return 0; dtostrf(FN_u8div10(&rb[i]), 1, 1, value);            return 1; }
#define VALUE_F(v, ch)   { if (changeonly && !ch)      return 0; dtostrf(v, 1, 1, value);                             return 1; }
#endif /* RPI */
#define VALUE_H4         { if (!newbytesval(rb, i, 4)) return 0; snprintf(value, KEYLEN, "%X%X%X%X%X%X%X%X", rb[i-3] >> 4, rb[i-3] & 0x0F, rb[i-2] >> 4, rb[i-2] & 0x0F, rb[i-1] >> 4, rb[i-1] & 0x0F, rb[i] >> 4, rb[i] & 0x0F); return 1; }
#define HANDLEPARAM      { PARAM35; return handleparam(key, value, rb, i);}
#define UNKNOWNBYTE      { UNKNOWN; return unknownbyte(key, value, rb, i);}
#define UNKNOWNBIT       { UNKNOWN; return unknownbit(key, value, rb, i, j);}
#define BITBASIS_UNKNOWN { switch (j) { case 8 : BITBASIS; default : UNKNOWNBIT; } }
#define TERMINATEJSON    { return 9; }

#endif /* P1P2_Daikin_json */
