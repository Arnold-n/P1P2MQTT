byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
unsigned int listenPort = 10003;                                  // listening port
unsigned int sendPort = 10000;                                    // sending port
unsigned int remPort = 10002;                                     // remote port
IPAddress ip(192, 168, 1, 33);                                       // IP address
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress sendIpAddress(255, 255, 255, 255);                      // remote IP or 255, 255, 255, 255 for broadcast (faster)
