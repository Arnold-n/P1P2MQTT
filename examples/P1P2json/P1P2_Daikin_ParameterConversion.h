#ifndef P1P2_Daikin_json
#define P1P2_Daikin_json
// P1P2-Daikin-json product-independent code

// these conversion functions look at the current and often also one or more past byte(s) in the byte array 
bool FN_flag8(uint8_t b, uint8_t n)  { return (b >> n) & 0x01; }
uint32_t FN_u24(uint8_t *b)          { return ((b[-2]<<16) | ((b[-1])<<8) | (b[0]));}
uint16_t FN_u16(uint8_t *b)          { return (              ((b[-1])<<8) | (b[0]));}
int8_t FN_u8delta(uint8_t *b)        { int c=b[0]; if (c&0x10) return -(c&0x0f); else return (c&0x0f); }
float FN_u8div10(uint8_t *b)         { return b[0]/10.0;}
float FN_f8_8(uint8_t *b)            { return ((float)(int8_t) b[-1]) + ((float)b[0]) /256;}
float FN_f8s8(uint8_t *b)            { return ((float)(int8_t) b[-1]) + ((float)b[0]) / 10;}

#ifdef __AVR__
#define KEY(K) strlcpy_PF(key,F(K),KEYLEN-1)
#else /* not __AVR__ */
//#include <pgmspace.h>
#define KEY(K) strlcpy(key,K,KEYLEN-1)
#endif /* __AVR__ */

// save-history pointers 
static byte rbhistory[RBHLEN];           // history storage
static uint16_t savehistoryend=0;        // #bytes saved so far
static uint16_t savehistoryp[RBHNP]={};  // history pointers
static  uint8_t savehistorylen[RBHNP]={}; // length of history for a certain packet

int8_t savehistoryindex(byte *rb); // defined in product-dependent code
uint8_t savehistoryignore(byte *rb);

void savehistory(byte *rb, int n) {
// also modifies savehistoryp, savehistorylen, savehistoryend
  int8_t shi = savehistoryindex(rb);
  if (shi >= 0) {
    uint8_t shign = savehistoryignore(rb);
    if (!savehistorylen[shi]) {
      if (savehistoryend + (n - shign) <= RBHLEN) {
        savehistoryp[shi] = savehistoryend;
        savehistorylen[shi] = n - shign;
        savehistoryend += (n - shign);
      } else if (savehistoryend < RBHLEN) {
        // not enough space available, store what we can
        savehistoryp[shi] = savehistoryend;
        savehistorylen[shi] = (RBHLEN - savehistoryend);
        savehistoryend += RBHLEN;
        Serial.print("* Not enough memory, shortage ");
        Serial.println((n-shign) + savehistoryend - RBHLEN);
      } else {
        // no space left
        Serial.print("* Warning: memory shortage ");
        Serial.println((n-shign));
      }
    }
    if (savehistorylen[shi]) {
      for (byte i=shign;((i < n) && (i < shign + savehistorylen[shi])); i++) {
        // copying history
        rbhistory[savehistoryp[shi] + (i - shign)] = rb[i];
      }
    }
  }
}

bool newbytesval(byte *rb, byte i, byte k) { 
// returns whether any of bytes (i-k+1) .. byte i in rb has a new value (also returns true if byte has not been saved)
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
  } else {
if ((i==6) && (k==3) && (rb[2]=0xB8) && (rb[3]==0x05)){
  Serial.println("*nohist\n");
}
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
      if ((((rbhistory[i - shign + savehistoryp[shi]]) ^ rb[i])>>p) & 0x01) {
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

byte unknownbyte(char* key, char* value, byte* rb, uint8_t i) { 
  snprintf(key,KEYLEN,"Byte-0x%X%X-0x%X%X-%i",rb[0]>>4,rb[0]&0x0f,rb[2]>>4,rb[2]&0x0f,i);     
  snprintf(value,KEYLEN,"%i",rb[i]);             
  return (outputunknown && newbytesval(rb,i,1)); 
}

byte unknownbit(char* key, char* value, byte* rb, uint8_t i, uint8_t j) { 
  snprintf(key,KEYLEN,"Bit-0x%X%X-0x%X%X-%i-%i",rb[0]>>4,rb[0]&0x0f,rb[2]>>4,rb[2]&0x0f,i,j); 
  snprintf(value,KEYLEN,"%i",FN_flag8(rb[i],j)); 
  return (outputunknown && newbitval(rb,i,j)); 
}

// macros for use in device-dependent code
// note that snprintf(.. , "%f", ..) is not supported on Arduino/Atmega so use dtostrf instead
#define BITBASIS         {                                         return newbytesval(rb,i,1)<<3; } // returns 8 if at least one bit of a byte changed, otherwise 0
#define VALUE_u8         { snprintf(value,KEYLEN,"%i",rb[i]);              return newbytesval(rb,i,1); }
#define VALUE_u24        { snprintf(value,KEYLEN,"%i",FN_u24(&rb[i]));     return newbytesval(rb,i,3); }
#define VALUE_u8_add2k   { snprintf(value,KEYLEN,"%i",rb[i]+2000);         return newbytesval(rb,i,1); }
#define VALUE_u8delta    { snprintf(value,KEYLEN,"%i",FN_u8delta(&rb[i])); return newbytesval(rb,i,1); }
#define VALUE_flag8      { snprintf(value,KEYLEN,"%i",FN_flag8(rb[i],j));  return newbitval(rb,i,j); }
#define VALUE_f8_8       { dtostrf(FN_f8_8(&rb[i]),1,1,value);     return newbytesval(rb,i,2); }
#define VALUE_f8s8       { dtostrf(FN_f8s8(&rb[i]),1,1,value);     return newbytesval(rb,i,2); }
#define VALUE_u8div10    { dtostrf(FN_u8div10(&rb[i]),1,1,value);  return newbytesval(rb,i,1); }
#define VALUE_F(v,ch)    { dtostrf(v,1,1,value);                   return (changeonly ? ch : 1); }
#define VALUE_H4         { snprintf(value,KEYLEN,"%X%X%X%X%X%X%X%X",rb[i-3]>>4,rb[i-3]&0x0F,rb[i-2]>>4,rb[i-2]&0x0F,rb[i-1]>>4,rb[i-1]&0x0F,rb[i]>>4,rb[i]&0x0F); return (newbytesval(rb,i,4)); }
#define UNKNOWNBYTE      return unknownbyte(key, value, rb, i);
#define UNKNOWNBIT       return unknownbit(key, value, rb, i, j);
#define BITBASIS_UNKNOWN { switch (j) { case -1 : BITBASIS; default : UNKNOWNBIT; } }
#define TERMINATEJSON    { return 9; }

#endif /* P1P2_Daikin_json */
