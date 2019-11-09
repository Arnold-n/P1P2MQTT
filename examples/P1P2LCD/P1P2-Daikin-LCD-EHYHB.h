/* P1P2LCD header file to process packets for display on OLED display.
 *         This file supports EHYHB* products (at least some of them)
 *
 * Copyright (c) 2019 Arnold Niessen, arnold.niessen -at- gmail-dot-com  - licensed under GPL v2.0 (see LICENSE)
 *
 * Version history
 * 20191109 v0.9.10 solved misalignment in data interpretation
 * 20190915 v0.9.4 separated from P1P2Monitor
 *
 */

#include <SPI.h>
#include <U8x8lib.h>
U8X8_SSD1306_128X64_NONAME_4W_SW_SPI u8x8(/* clock=*/ 2, /* data=*/ 3, /* cs=*/ 6, /* dc=*/ 5, /* reset=*/ 4);

static int packetcntmod12=0; // packetcntmod12 increments upon each package of 13 packets;
static int packetcntmod13=0; // packetcntmod13 as shown on LCD remains constant except (thus signals) additional packets and/or communication errors

#define LCD_char(i)  { u8x8.drawGlyph(col,row,i); }
#define LCD_int1(i)  { LCD_char('0'+(i)%10); }
#define LCD_int2(i)  { LCD_char('0'+((i)%100)/10); col++; LCD_char('0'+(i)%10); }
#define LCD_hex1(i)  { if ((i)<10) LCD_char(('0'+(i))) else LCD_char('A'+((i)-10)); }
#define LCD_hex2(i)  { LCD_hex1(i>>4); col++; LCD_hex1(i&0x0f); }

void init_LCD(void) {
  u8x8.begin();
  u8x8.setFont(u8x8_font_chroma48medium8_r);
  uint8_t row,col;
  row=0; col=0;  LCD_int2(20);  // Date/time
  row=0; col=8;  LCD_char(' ');
  row=0; col=11; LCD_char(':');
  row=1; col=0;  u8x8.drawString(col,row,"RWT  MWT  LWT");
  row=2; col=2;  LCD_char('.');
  row=2; col=7;  LCD_char('.');
  row=2; col=12; LCD_char('.');
  row=3; col=0;  u8x8.drawString(col,row,"Flow Trm  Tout");
  row=4; col=2;  LCD_char('.'); // flow
  row=4; col=7;  LCD_char('.'); // Troom
  row=4; col=12; LCD_char('.'); // Toutside
  row=5; col=0;  u8x8.drawString(col,row,"Setp Ref1 Ref2");
  row=6; col=2;  LCD_char('.'); // setp
  row=6; col=7;  LCD_char('.'); // Refr1
  row=6; col=12; LCD_char('.'); // Refr2
}

void process_for_LCD(byte *rb, uint8_t n) {
  uint8_t row,col;
  packetcntmod12++; if (packetcntmod12 == 12) packetcntmod12 = 0;
  packetcntmod13++; if (packetcntmod13 == 13) packetcntmod13 = 0;
  for (byte i = 3; i < n; i++) {
    byte linesrc = rb[0];
    byte linetype = rb[2];
    byte c = rb[i];
    switch (linetype) {
      case 0x10 :  switch (linesrc) {
        case 0x00 : row=6; col=15; LCD_hex1(packetcntmod12); 
                    row=4; col=15; LCD_hex1(packetcntmod13); switch (i) {
          case  3 : row=7; col=8;  LCD_hex2(c);                   break; // heating on/off?
          case  4 : row=7; col=10; LCD_hex2(c);                   break; // 0x01/0x81?
          case 10 : /* row=X; col=X; LCD_int2(c); */              break; // target room temp
          case 12 : row=7; col=12; LCD_hex2(c);                   break; // flags 5/6?
          case 13 : row=7; col=14; LCD_hex2(c);                   break; // quiet mode
        } break;
        case 0x40 : switch (i) {
          case  3 : /* row=X; col=X; LCD_hex2(c); */              break; // heating on/off copy ?
          case  6 : row=7; col=4;  LCD_hex2(c);                   break; // 0:DHW on/off 4: SHC/tank 3-way valve?
          case 11 : row=7; col=0;  LCD_int2(c);                   break; // Troom target
          case 21 : row=7; col=2;  LCD_hex2(c);                   break; // 0:compressor 3:pump
        } break;
      } break;
      case 0x11 : switch (linesrc) {
        case 0x00 : switch (i) {
          case  3 : row=4; col=5;  LCD_int2(c);                   break; // Troom
          case  4 : row=4; col=8;  LCD_char('0'+(c*5)/128);       break; // Troom
        } break;
        case 0x40 : switch (i) {
          case  3 : row=2; col=10; LCD_int2(c);                   break; // LWT
          case  4 : row=2; col=13; LCD_char('0'+(c*5)/128);       break; // LWT
          case  7 : row=4; col=10; LCD_int2(c);                   break; // Toutside
          case  8 : row=4; col=13; LCD_char('0'+(c*5)/128);       break; // Toutside
          case  9 : row=2; col=0;  LCD_int2(c);                   break; // RWT
          case 10 : row=2; col=3;  LCD_char('0'+(c*5)/128);       break; // RWT
          case 11 : row=2; col=5;  LCD_int2(c);                   break; // MWT
          case 12 : row=2; col=8;  LCD_char('0'+(c*5)/128);       break; // MWT
          case 13 : row=6; col=5;  LCD_int2(c);                   break; // Trefr1
          case 14 : row=6; col=8;  LCD_char('0'+(c*5)/128);       break; // Trefr1
        } break;
      } break;
      case 0x12 : switch (linesrc) {
        case 0x00 : switch (i) {
          case  4 : row=0; col=15; LCD_int1(c);                   break; // DayOfWeek
          case  5 : row=0; col=9;  LCD_int2(c);                   break; // Hours
          case  6 : row=0; col=12; LCD_int2(c);                   break; // Minutes
          case  7 : row=0; col=2;  LCD_int2(c);                   break; // year
          case  8 : row=0; col=4;  LCD_int2(c);                   break; // month
          case  9 : row=0; col=6;  LCD_int2(c);                   break; // day
        } break;
        case 0x40 : switch (i) {
          case 15 : row=7; col=6; LCD_hex2(c);                    break; // 0:heat pump 6:? 7:DHW
        } break;
      } break;
      case 0x13 : switch (linesrc) {
        case 0x00 : switch (i) {
          default : break;
        } break;
        case 0x40 : switch (i) {
          case  3 : /* row=X; col=X; LCD_int2(c); */              break; // DHW
          case 12 : row=4; col=0; LCD_int2(c/10);
                    col=3; LCD_int1(c%10);                        break; // Flow
        } break;
      } break;
      case 0x14 : switch (linesrc) {
        case 0x00 : switch (i) {
          case 11 : /* row=X; col=X; LCD_int2(c); */              break; // Tdelta
          case 12 : /* row=X; col=X; LCD_hex2(c); */              break; // Op?
        } break;
        case 0x40 : switch (i) {
          case 12 : /* row=X; col=X; LCD_hex2(c); */              break; // Op?
          case 18 : row=6; col=0; LCD_int2(c);                    break; // T setpoint1
          case 19 : row=6; col=3; LCD_char('0'+(c*5)/128);        break; // T setpoint1
          case 20 : /* row=X; col=X; LCD_int2(c); */              break; // T setpoint2
          case 21 : /* row=X; col=X; LCD_char('0'+(c*5)/128); */  break; // T setpoint2
        } break;
      } break;
      case 0x15 : switch (linesrc) {
        case 0x40 : switch (i) {
          case  5 : row=6; col=10; LCD_int2(c);                   break; // Refr2
          case  6 : row=6; col=13; LCD_char('0'+(c*5)/128);       break; // Refr2
        } break;
      } break;
      default : break;
    }
  }
}
