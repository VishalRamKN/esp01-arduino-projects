// 00 - AT command bridge
// Type AT commands in the Serial Monitor and see the ESP-01's replies.
//
// Wiring: ESP TX -> pin 2, pin 3 -> 1k/2.2k divider -> ESP RX, shared GND
// Serial Monitor: 9600 baud, "Both NL & CR"
//
// First time only: the ESP's AT firmware starts at 115200.
//   1. Set ESP_BAUD to 115200, upload, send:  AT+UART_DEF=9600,8,1,0,0
//   2. Set ESP_BAUD back to 9600, upload again, power-cycle the ESP.

#include <SoftwareSerial.h>

const long ESP_BAUD = 9600;   // 115200 for the very first run

SoftwareSerial esp(2, 3); // RX, TX

void setup() {
  Serial.begin(9600);
  esp.begin(ESP_BAUD);
  Serial.println(F("Ready. Type AT commands:"));
}

void loop() {
  if (esp.available())    Serial.write(esp.read());
  if (Serial.available()) esp.write(Serial.read());
}
