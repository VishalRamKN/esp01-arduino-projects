// 03 - WiFi scanner
// Open the ESP's IP on your phone to see every WiFi network nearby,
// sorted by signal strength, with signal bars. Results also print to Serial.
//
// Wiring: ESP TX -> pin 2, pin 3 -> 1k/2.2k divider -> ESP RX, shared GND
// ESP must already be set to 9600 baud (AT+UART_DEF=9600,8,1,0,0)

#include <SoftwareSerial.h>

SoftwareSerial esp(2, 3); // RX, TX

// ---------- CHANGE THESE ----------
const char* WIFI_SSID = "YourWiFi";
const char* WIFI_PASS = "YourPassword";
// ----------------------------------

// ---------- Scan results ----------
struct Net {
  char ssid[33];
  int  rssi;   // signal in dBm: -30 great, -90 very weak
  byte ch;     // channel 1-13
  byte enc;    // 0 = open, anything else = password protected
};
const byte MAX_NETS = 12;
Net nets[MAX_NETS];
byte netCount = 0;

// ---------- Helpers ----------

// Wait until 'target' appears in ESP output. Gives up after 'timeout' ms of silence.
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

int readChar(unsigned long timeout = 1000) {
  unsigned long t = millis();
  while (!esp.available()) {
    if (millis() - t > timeout) return -1;
  }
  return esp.read();
}

// Read one line from the ESP. Returns false on timeout.
bool readLine(char* buf, size_t size, unsigned long timeout) {
  size_t n = 0;
  int c;
  while ((c = readChar(timeout)) >= 0) {
    if (c == '\n') { buf[n] = 0; return true; }
    if (c != '\r' && n < size - 1) buf[n++] = c;
  }
  buf[n] = 0;
  return false;
}

// Parse: +CWLAP:(3,"MyWiFi",-52,"aa:bb:cc:dd:ee:ff",6,...)
bool parseNet(const char* line, Net& net) {
  const char* p = strchr(line, '(');
  if (!p) return false;
  net.enc = atoi(p + 1);

  const char* q1 = strchr(p, '"');                 // SSID start
  if (!q1) return false;
  const char* q2 = strchr(q1 + 1, '"');            // SSID end
  if (!q2) return false;
  size_t len = min((size_t)(q2 - q1 - 1), sizeof(net.ssid) - 1);
  memcpy(net.ssid, q1 + 1, len);
  net.ssid[len] = 0;

  net.rssi = atoi(q2 + 2);                         // after '",'

  const char* m1 = strchr(q2 + 1, '"');            // MAC start
  const char* m2 = m1 ? strchr(m1 + 1, '"') : NULL;
  net.ch = m2 ? atoi(m2 + 2) : 0;
  return true;
}

void scan() {
  netCount = 0;
  esp.println(F("AT+CWLAP"));
  char line[96];
  while (readLine(line, sizeof(line), 8000)) {
    if (strncmp(line, "+CWLAP:", 7) == 0 && netCount < MAX_NETS) {
      if (parseNet(line, nets[netCount])) netCount++;
    }
    if (strcmp(line, "OK") == 0 || strcmp(line, "ERROR") == 0) break;
  }

  // Sort strongest first (simple bubble sort - good to discuss in class!)
  for (byte i = 0; i < netCount; i++)
    for (byte j = 0; j + 1 < netCount - i; j++)
      if (nets[j].rssi < nets[j + 1].rssi) { Net t = nets[j]; nets[j] = nets[j + 1]; nets[j + 1] = t; }

  Serial.print(F("# Found "));
  Serial.print(netCount);
  Serial.println(F(" networks:"));
  for (byte i = 0; i < netCount; i++) {
    Serial.print(nets[i].rssi);
    Serial.print(F(" dBm  ch"));
    Serial.print(nets[i].ch);
    Serial.print(nets[i].enc ? F("  locked  ") : F("  OPEN    "));
    Serial.println(nets[i].ssid[0] ? nets[i].ssid : "(hidden)");
  }
}

// Prints HTML-safe text (no < > & " ' that could break the page)
void printSafe(Print& out, const char* s) {
  for (; *s; s++) {
    char c = *s;
    out.write((c == '<' || c == '>' || c == '&' || c == '"' || c == '\'') ? '?' : c);
  }
}

// The whole page is written to a Print object. We call this twice:
// once with a counter (to learn the length for AT+CIPSEND), once to the ESP.
void renderPage(Print& out) {
  out.print(F("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n"
              "<html><head><meta name=viewport content='width=device-width'>"
              "<title>WiFi Scanner</title><style>body{font-family:sans-serif;padding:8px}"
              "td{padding:4px}.b{height:12px;border-radius:3px}</style></head><body>"
              "<h3>WiFi networks nearby: "));
  out.print(netCount);
  out.print(F("</h3><table>"));
  for (byte i = 0; i < netCount; i++) {
    int pct = constrain(2 * (nets[i].rssi + 100), 2, 100);      // -100 dBm = 0%, -50 = 100%
    const char* col = pct > 60 ? "#3a3" : pct > 30 ? "#da2" : "#d33";
    out.print(F("<tr><td>"));
    if (nets[i].ssid[0]) printSafe(out, nets[i].ssid); else out.print(F("<i>hidden</i>"));
    out.print(nets[i].enc ? F("") : F(" (open)"));
    out.print(F("</td><td style='width:90px'><div class=b style='width:"));
    out.print(pct);
    out.print(F("%;background:"));
    out.print(col);
    out.print(F("'></div></td><td>"));
    out.print(nets[i].rssi);
    out.print(F(" dBm</td><td>ch "));
    out.print(nets[i].ch);
    out.print(F("</td></tr>"));
  }
  out.print(F("</table><p><a href='/'>Scan again</a></p></body></html>"));
}

class CountPrint : public Print {
public:
  int n = 0;
  size_t write(uint8_t) override { n++; return 1; }
};

void closeConn(int id) {
  esp.print(F("AT+CIPCLOSE="));
  esp.println(id);
  waitFor("OK", 1000);
}

void sendPage(int id) {
  CountPrint counter;
  renderPage(counter);
  esp.print(F("AT+CIPSEND="));
  esp.print(id);
  esp.print(',');
  esp.println(counter.n);
  if (!waitFor(">", 2000)) { closeConn(id); return; }
  renderPage(esp);
  waitFor("SEND OK", 5000);
  closeConn(id);
}

void handleRequest() {
  int id = 0, c;
  while ((c = readChar()) >= 0 && c != ',') if (isDigit(c)) id = id * 10 + (c - '0');
  while ((c = readChar()) >= 0 && c != ':') ;          // skip length

  char line[64];
  size_t n = 0;
  while ((c = readChar()) >= 0 && c != '\r' && c != '\n') {
    if (n < sizeof(line) - 1) line[n++] = c;
  }
  line[n] = 0;
  waitFor("\r\n\r\n", 300);                            // skip rest of headers

  if (strncmp(line, "GET / ", 6) != 0) { closeConn(id); return; }   // favicon etc.

  Serial.println(F("# Scanning..."));
  scan();
  sendPage(id);
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

  Serial.println(F("# Connecting to WiFi..."));
  esp.print(F("AT+CWJAP=\""));
  esp.print(WIFI_SSID);
  esp.print(F("\",\""));
  esp.print(WIFI_PASS);
  esp.println('"');
  Serial.println(waitFor("OK", 15000) ? F("# WiFi connected") : F("# WiFi FAILED"));

  at(F("AT+CIPMUX=1"));
  at(F("AT+CIPSERVER=1,80"));
  at(F("AT+CIPSTO=10"));          // scans take a few seconds

  Serial.println(F("# IP address:"));
  esp.println(F("AT+CIFSR"));
  waitFor("OK", 2000, true);
  Serial.println(F("\n# Open the IP on your phone. Type 's' here to scan from Serial."));
}

void loop() {
  if (Serial.available() && Serial.read() == 's') scan();
  if (esp.available() && waitFor("+IPD,", 100)) handleRequest();
}
