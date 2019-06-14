/* P1P2_EHYHB: Library for conversion of EHYHB(H/X)-AV3 and EHV(H/X)-CB protocol data format to JSON
 *
 * Copyright (c) 2019 Arnold Niessen, arnold.niessen -at- gmail-dot-com  - licensed under GPL v2.0 (see LICENSE)
 *
 *  Version history
 *  20190614 v0.0.1 initial release; support for hex monitoring and conversion to JSON
 *
 *     Thanks to Krakra for providing the hints and references to the HBS and MM1192 documents on
 *     https://community.openenergymonitor.org/t/hack-my-heat-pump-and-publish-data-onto-emoncms
 *     to Bart Ellast for providing explanations and sample output from his heat pump, and
 *     to Paul Stoffregen for publishing the AltSoftSerial library.
 *
  *
 */
 

extern byte packetType[];
extern byte packet[];
extern bool flag8(int, int);
extern float f8_8(int);
extern String packetTypeToString(byte type[]);

#define packet400010Size 20
static byte packet400010[packet400010Size];
#define packet400011Size 20
static byte packet400011[packet400011Size];


void processPacket() {

  const size_t capacity = JSON_OBJECT_SIZE(30);               // what is the optimum capacity??
  DynamicJsonDocument json(capacity);


  switch (packetType[0]) {
    case 0x40 :
      switch (packetType[2]) {
        case 0x10 :
          for (int i = 0; i < packet400010Size; i++) {
            if (packet[i] != packet400010[i]) {
              switch (i + 4) {                                                                  // I use i + 4 to match our existing tables
                case  4 :
                  for (int b = 0; b < 8; b++) {
                    switch (b) {
                      case 0:
                        json[F("power")] = flag8(i, b);
                        break;
                      default :
                        break;
                    }
                  }
                  break;
                case  5 :
                  for (int b = 0; b < 8; b++) {
                    switch (b) {
                      case 0:
                        break;
                      default :
                        json[packetTypeToString(packetType) + "-" + String(i + 4) + "-" + String(b)] = flag8(i, b);
                        break;
                    }
                  }
                case  6 :
                  for (int b = 0; b < 8; b++) {
                    switch (b) {
                      default :
                        json[packetTypeToString(packetType) + "-" + String(i + 4) + "-" + String(b)] = flag8(i, b);
                        break;
                    }
                  }
                  break;
                default :
                  json[packetTypeToString(packetType) + "-" + String(i + 4)] = "0x" + String(packet[i], HEX);
                  break;
              }
            }
          }

          for (int i = 0; i < packet400010Size; i++) {
            packet400010[i] = packet[i];
          }
          break;
        case 0x11 :
          for (int i = 0; i < packet400011Size; i++) {
            if (packet[i] != packet400011[i]) {
              switch (i + 4) {
                case  4 :
                  json[F("leaving water temp")] = f8_8(i);
                  break;
                case  5 :
                  json[F("leaving water temp")] = f8_8(i - 1);
                  break;
                case  6 :
                  json[F("DHW temp")] = f8_8(i);
                  break;
                case  7 :
                  json[F("DHW temp")] = f8_8(i - 1);
                  break;
                case  8 :
                  json[F("outside air temp (low res)")] = f8_8(i);
                  break;
                case  9 :
                  json[F("outside air temp (low res)")] = f8_8(i - 1);
                  break;
                case  10 :
                  json[F("inlet water temp")] = f8_8(i);
                  break;
                case  11 :
                  json[F("inlet water temp")] = f8_8(i - 1);
                  break;
                case  12 :
                  json[F("leaving water temp PHE")] = f8_8(i);
                  break;
                case  13 :
                  json[F("leaving water temp PHE")] = f8_8(i - 1);
                  break;
                case  14 :
                  json[F("refrigerant temp")] = f8_8(i);
                  break;
                case  15 :
                  json[F("refrigerant temp")] = f8_8(i - 1);
                  break;
                case  16 :
                  json[F("measured room temp")] = f8_8(i);
                  break;
                case  17 :
                  json[F("measured room temp")] = f8_8(i - 1);
                  break;
                case  18 :
                  json[F("outside air temp")] = f8_8(i);
                  break;
                case  19 :
                  json[F("outside air temp")] = f8_8(i - 1);
                  break;
                default :
                  json[packetTypeToString(packetType) + "-" + String(i + 4)] = packet[i];
                  break;
              }
            }
          }

          for (int i = 0; i < packet400011Size; i++) {
            packet400011[i] = packet[i];
          }
          break;
      }
      break;
  }
  if (!json.isNull()) {
    serializeJson(json, Serial);
    Serial.println();
  }
}
