// 06 - Internet info (the ESP as a CLIENT)
// Projects 01-05 make the ESP a web *server*. Here it is a web *client*:
// it connects to a real website, sends an HTTP request by hand, and prints
// your public IP, city, country and the current internet time (GMT).
//
// Type 'g' in the Serial Monitor (9600 baud) to fetch again.
//
// Wiring: ESP TX -> pin 2, pin 3 -> 1k/2.2k divider -> ESP RX, shared GND
// ESP must already be set to 9600 baud (AT+UART_DEF=9600,8,1,0,0)

#include <SoftwareSerial.h>

SoftwareSerial esp(2, 3); // RX, TX

// ---------- CHANGE THESE ----------
const char* WIFI_SSID = "YourWiFi";
const char* WIFI_PASS = "YourPassword";
// ----------------------------------

// Free service, plain HTTP (the ESP-01 AT firmware can't easily do HTTPS)
const char* HOST = "ip-api.com";
const char* PATH = "/line/?fields=country,city,timezone,query";

// ---------- Helpers ----------

bool waitFor(const char* target, unsigned long timeout, bool echo = false) {
  size_t idx = 0, n = strlen(target);
  unsigned long t = millis();
  while (millis() - t < timeout) {
    if (esp.available()) {
      char c = esp.read();
      t = millis();
      if (echo) Serial.write(c);
      if (c == target[idx]) {
        if (++idx == n) return true;
      } else {
        idx = (c == target[0]) ? 1 : 0;
      }
    }
  }
  return false;
}

bool at(const __FlashStringHelper* cmd, const char* expect = "OK", unsigned long timeout = 2000) {
  esp.println(cmd);
  return waitFor(expect, timeout);
}

int readChar(unsigned long timeout) {
  unsigned long t = millis();
  while (!esp.available()) {
    if (millis() - t > timeout) return -1;
  }
  return esp.read();
}

bool readLine(char* buf, size_t size, unsigned long timeout) {
  size_t n = 0;
  int c;
  while ((c = readChar(timeout)) >= 0) {
    if (c == '\n') { buf[n] = 0; return true; }
    if (c != '\r' && n < size - 1) buf[n++] = c;
  }
  buf[n] = 0;
  return n > 0;
}

// ---------- The HTTP request ----------

void fetchInfo() {
  Serial.print(F("\n# Connecting to "));
  Serial.println(HOST);

  // 1. Open a TCP connection to port 80
  esp.print(F("AT+CIPSTART=\"TCP\",\""));
  esp.print(HOST);
  esp.println(F("\",80"));
  if (!waitFor("OK", 5000)) { Serial.println(F("# Could not connect")); return; }

  // 2. Build the request text and tell the ESP how many bytes are coming
  char req[128];
  snprintf(req, sizeof(req), "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", PATH, HOST);
  esp.print(F("AT+CIPSEND="));
  esp.println(strlen(req));
  if (!waitFor(">", 2000)) { Serial.println(F("# CIPSEND failed")); return; }

  // 3. Send it
  esp.print(req);
  if (!waitFor("SEND OK", 5000)) { Serial.println(F("# Send failed")); return; }

  // 4. Read the reply. It arrives as: +IPD,<len>:HTTP/1.1 200 OK ... headers ... blank line ... body
  //    We only keep what we need and print at the end, so the slow Serial
  //    printing doesn't make us miss incoming bytes.
  char line[64];
  char date[40] = "";
  char body[4][40] = { "", "", "", "" };
  byte bodyLines = 0;
  bool inBody = false;

  while (readLine(line, sizeof(line), 3000)) {
    char* p = line;
    char* ipd = strstr(p, "+IPD,");
    if (ipd) {                                    // strip the "+IPD,123:" marker
      char* colon = strchr(ipd, ':');
      if (colon) p = colon + 1;
    }
    char* closed = strstr(p, "CLOSED");           // the ESP says CLOSED when the server hangs up
    if (closed) {
      if (closed == p) break;
      *closed = 0;                                // "CLOSED" glued to the last line: cut it off
    }

    if (!inBody) {
      if (strncmp(p, "Date: ", 6) == 0) strncpy(date, p + 6, sizeof(date) - 1);
      if (p[0] == 0) inBody = true;               // blank line = end of headers
    } else if (p[0] && bodyLines < 4) {
      strncpy(body[bodyLines++], p, sizeof(body[0]) - 1);
    }
  }

  // 5. Show the results
  const char* labels[4] = { "Country ", "City    ", "Timezone", "Your IP " };
  Serial.println(F("------------------------------"));
  for (byte i = 0; i < bodyLines; i++) {
    Serial.print(labels[i]);
    Serial.print(F(" : "));
    Serial.println(body[i]);
  }
  if (date[0]) {
    Serial.print(F("Time GMT : "));
    Serial.println(date);
  }
  if (!bodyLines) Serial.println(F("No data received - try 'g' again"));
  Serial.println(F("------------------------------"));
}

// ---------- Setup / loop ----------

void setup() {
  Serial.begin(9600);
  esp.begin(9600);

  Serial.println(F("# Resetting ESP..."));
  esp.println(F("AT+RST"));
  waitFor("ready", 5000);
  delay(500);

  at(F("ATE0"));
  at(F("AT+CWMODE=1"));
  at(F("AT+CIPMUX=0"));           // single connection: simplest for a client

  Serial.println(F("# Connecting to WiFi..."));
  esp.print(F("AT+CWJAP=\""));
  esp.print(WIFI_SSID);
  esp.print(F("\",\""));
  esp.print(WIFI_PASS);
  esp.println('"');
  Serial.println(waitFor("OK", 15000) ? F("# WiFi connected") : F("# WiFi FAILED"));

  fetchInfo();
  Serial.println(F("# Type 'g' to fetch again"));
}

void loop() {
  if (Serial.available() && Serial.read() == 'g') fetchInfo();
}
