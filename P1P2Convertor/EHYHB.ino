
void convertPacket() {
  long dontdeleteme = packetTypeInt();                            // I do not know why but this is needed for switch case to work correctly. 
  byte index;

  switch (packetTypeInt()) {
    case 0x400010:                                                             // packet type
      storeCompare = true;                                                     // set to TRUE if you want to store packet and print only in case data bytes change. Only <numberofStoredPackets> packets can be stored.
      if (storeCompare) index = findPacket();
      for (byte i = 0; i < dataCnt; i++) {
        if ((currentPacket.data[i] != storedPackets[index].data[i]) || !storeCompare) {
          switch (i + 4) {
            case  4 :                                                          // byte number (I start with 4 to match our existing tables)
              for (byte b = 0; b < 8; b++) {
                switch (b) {
                  case 0 :                                                     // bit number (for flag8 data types)
                    json[F("heating power status")] = flag8(i, b);
                    break;
                  default :
                    unknownBit(i, b);
                    break;
                }
              }
              break;
            case  6 :
              for (byte b = 0; b < 8; b++) {
                switch (b) {
                  case 0 :
                    json[F("heating valve")] = flag8(i, b);          // true = valve open
                    break;
                  case 1 :
                    json[F("cooling valve")] = flag8(i, b);
                    break;
                  case 5 :
                    json[F("main zone valve")] = flag8(i, b);
                    break;
                  case 6 :
                    json[F("additional zone valve")] = flag8(i, b);
                    break;
                  case 7 :
                    json[F("DHW tank valve")] = flag8(i, b);
                    break;
                  default :
                    unknownBit(i, b);
                    break;
                }
              }
            case  7 :
              for (byte b = 0; b < 8; b++) {
                switch (b) {
                  default :
                    unknownBit(i, b);
                    break;
                }
              }
              break;
            default :
              unknownByte(i);
              break;
          }
        }
      }
      if (storeCompare) storePacket();
      break;        //  case 0x400010
    case 0x400011:
      storeCompare = true;
      if (storeCompare) index = findPacket();
      for (byte i = 0; i < dataCnt; i++) {
        if (currentPacket.data[i] != storedPackets[index].data[i]) {
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
              unknownByte(i);
              break;
          }
        }
      }
      if (storeCompare) storePacket();
      break;        //  case 0x400011
  }
}
