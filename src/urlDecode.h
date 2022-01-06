#ifndef _URLDECODE
#define _URLDECODE

#include <avr/pgmspace.h> // progmem
// webserver
#include <ESP8266WebServer.h>

unsigned char h2int(char c);
String urldecode(String str);

#endif