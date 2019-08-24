#ifndef NetworkParams
#define NetworkParams

// Fill in your default configuration parameters
// Intention is that these can be modified by wifimanager
// But storage of thoses parameters is not added yet

char mqttServer[50]   = "192.168.Your.Server";     // Configuration of mqtt parameters  (set-up via via WiFiManager tbd)
char mqttPort[10]     = "1234";
int  mqttPortNr       = 1234;                      // int fallback in case mqttPort string cannot be parsed
char mqttUser[20]     = "MqttUser";
char mqttPassword[30] = "MqttPassword";

//emoncms upload via http-post is not supported yet:
//const char[50] emonURL="URL";                  // Fill in YOUR emoncms URL
//const char[30] emonAPI="APIkey";               // Fill in YOUR emoncms API key

#endif // NetworkParams
