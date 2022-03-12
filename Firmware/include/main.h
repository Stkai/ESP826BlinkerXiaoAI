#include <Blinker.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <OneButton.h>
#include <Servo.h>
#include <EEPROM.h>
#include <WString.h>

void handleResetButtonPressStop();

void handleRestButtonDuringLongPress();

bool restoreConfig();

void dataRead(const String &data);

void ledBlink(int intervel, int number);

void saveServoDegrees(int address, int degrees);

void setupMode();

void startWebServer();

void switchCallback(const String &state);

String makePage(const String &title, const String &contents);
