#ifndef _WEBFUNCTIONS
#define _WEBFUNCTIONS

#include "LittleFS.h"
#include <avr/pgmspace.h> // progmem
// webserver
#include <ESP8266WebServer.h>
#include "./rtc/RtcDS1307.h"

#include "urlDecode.h"

#ifdef _global_web
ESP8266WebServer server(80);
#else
extern ESP8266WebServer server;
#endif

void handleGetBirthday();
void handleSetBirthday();
void setupWebserver();
void handleNotFound();
void handleGetConfig();
String getContentType(String filename);
bool handleFileRead(String path);
void handleFileUpload();
void handleSetTime();
void handleGoToSleep();

#endif