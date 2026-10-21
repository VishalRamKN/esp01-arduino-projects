// 05 - Classroom quiz buzzer
// Every student opens the ESP's IP on their phone, types a name and hits the
// big button. The Arduino records who buzzed first (and how far behind the
// others were), lights the LED and beeps.
//
// Reset for the next question:
//   - type 'r' in the Serial Monitor, or
//   - press a button between pin 4 and GND, or
//   - open http://<ESP-IP>/r?k=<RESET_KEY> on the teacher's phone
//
// Wiring: ESP TX -> pin 2, pin 3 -> 1k/2.2k divider -> ESP RX, shared GND
//         Optional: piezo buzzer pin 8 -> GND, push button pin 4 -> GND
// ESP must already be set to 9600 baud (AT+UART_DEF=9600,8,1,0,0)

#include <SoftwareSerial.h>

SoftwareSerial esp(2, 3); // RX, TX

// ---------- CHANGE THESE ----------
const char* WIFI_SSID = "YourWiFi";
const char* WIFI_PASS = "YourPassword";
const char* RESET_KEY = "teach";       // for http://<IP>/r?k=teach
// ----------------------------------

const int LED_PIN = 13;
const int PIEZO_PIN = 8;
const int RESET_BTN = 4;

// ---------- Buzz order ----------
struct Buzz {
  char name[17];
  unsigned long dt;    // ms after the first buzz
};
const byte MAX_BUZZ = 6;
Buzz order[MAX_BUZZ];
byte buzzCount = 0;
unsigned long firstTime = 0;

// ---------- Web page (stored in flash) ----------
const char HTML_H[] PROGMEM =
  "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n";
const char TXT_H[] PROGMEM =
  "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n";

const char PAGE[] PROGMEM = R"rawliteral(<html><head><meta name=viewport content='width=device-width'>
<title>Buzzer</title><style>body{font-family:sans-serif;text-align:center;padding:10px}
#z{width:200px;height:200px;border-radius:50%;border:0;background:#d22;color:#fff;font-size:32px;margin:20px}
#z:active{background:#a00}#z:disabled{background:#999}#o{white-space:pre-wrap;font-size:20px}
input{font-size:18px;padding:6px}</style></head><body><h2>Quiz Buzzer</h2>
<input id=n placeholder='Your name' maxlength=16><br><button id=z onclick=bz()>BUZZ</button>
<div id=o></div><br><button onclick=st()>Show results</button>
<script>try{n.value=localStorage.n||''}catch(e){}
function get(u){return fetch(u,{cache:'no-store'}).then(r=>r.text()).then(t=>o.textContent=t).catch(e=>o.textContent='No reply, try again')}
function bz(){if(!n.value){o.textContent='Enter your name first';return}
try{localStorage.n=n.value}catch(e){}z.disabled=1;o.textContent='...';
get('/b?n='+encodeURIComponent(n.value)).finally(()=>setTimeout(()=>z.disabled=0,3000))}
function st(){get('/s')}
</script></body></html>)rawliteral";

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

int readChar(unsigned long timeout = 1000) {
  unsigned long t = millis();
  while (!esp.available()) {
    if (millis() - t > timeout) return -1;
  }
  return esp.read();
}

void espPrintP(const char* p) {
  char c;
  while ((c = pgm_read_byte(p++))) esp.write(c);
}

void urlDecode(char* s) {
  char* o = s;
  while (*s) {
    if (*s == '+') { *o++ = ' '; s++; }
    else if (*s == '%' && s[1] && s[2]) {
      char h[3] = { s[1], s[2], 0 };
      *o++ = (char)strtol(h, NULL, 16);
      s += 3;
    }
    else *o++ = *s++;
  }
  *o = 0;
}

void getParam(const char* line, const char* name, char* out, size_t outSize) {
  out[0] = 0;
  const char* p = strchr(line, '?');
  if (!p) return;
  size_t n = strlen(name);
  while (*p && *p != ' ') {
    p++;
    if (strncmp(p, name, n) == 0 && p[n] == '=') {
      p += n + 1;
      size_t i = 0;
      while (*p && *p != '&' && *p != ' ' && i < outSize - 1) out[i++] = *p++;
      out[i] = 0;
      urlDecode(out);
      return;
    }
    while (*p && *p != '&' && *p != ' ') p++;
  }
}

void closeConn(int id) {
  esp.print(F("AT+CIPCLOSE="));
  esp.println(id);
  waitFor("OK", 1000);
}

// body == NULL sends the HTML page, otherwise a short text reply
void sendResponse(int id, const char* hdr, const char* body) {
  int len = strlen_P(hdr) + (body ? strlen(body) : strlen_P(PAGE));
  esp.print(F("AT+CIPSEND="));
  esp.print(id);
  esp.print(',');
  esp.println(len);
  if (!waitFor(">", 2000)) { closeConn(id); return; }
  espPrintP(hdr);
  if (body) esp.print(body); else espPrintP(PAGE);
  waitFor("SEND OK", 5000);
  closeConn(id);
}

// ---------- Game logic ----------

void resetRound() {
  buzzCount = 0;
  digitalWrite(LED_PIN, LOW);
  Serial.println(F("\n===== READY: next question ====="));
}

// Writes the results list into 'out'
void results(char* out, size_t size) {
  if (buzzCount == 0) { snprintf(out, size, "Nobody has buzzed yet"); return; }
  out[0] = 0;
  for (byte i = 0; i < buzzCount; i++) {
    size_t used = strlen(out);
    if (i == 0)
      snprintf(out + used, size - used, "1. %s\n", order[i].name);
    else
      snprintf(out + used, size - used, "%d. %s +%lu.%03lus\n", i + 1, order[i].name,
               order[i].dt / 1000, order[i].dt % 1000);
  }
}

// Returns this player's position (1 = first), or 0 if the list is full
byte buzz(const char* name) {
  for (byte i = 0; i < buzzCount; i++)                 // already buzzed? same answer
    if (strcmp(order[i].name, name) == 0) return i + 1;
  if (buzzCount == MAX_BUZZ) return 0;

  unsigned long now = millis();
  if (buzzCount == 0) {
    firstTime = now;
    digitalWrite(LED_PIN, HIGH);
    tone(PIEZO_PIN, 1200, 400);
  }
  Buzz& b = order[buzzCount++];
  strncpy(b.name, name, sizeof(b.name) - 1);
  b.name[sizeof(b.name) - 1] = 0;
  b.dt = now - firstTime;

  Serial.print('#');
  Serial.print(buzzCount);
  Serial.print(' ');
  Serial.print(b.name);
  Serial.print(F("  +"));
  Serial.print(b.dt);
  Serial.println(F(" ms"));
  return buzzCount;
}

void handleRequest() {
  int id = 0, c;
  while ((c = readChar()) >= 0 && c != ',') if (isDigit(c)) id = id * 10 + (c - '0');
  while ((c = readChar()) >= 0 && c != ':') ;          // skip length

  char line[96];
  size_t n = 0;
  while ((c = readChar()) >= 0 && c != '\r' && c != '\n') {
    if (n < sizeof(line) - 1) line[n++] = c;
  }
  line[n] = 0;
  waitFor("\r\n\r\n", 300);                            // skip rest of headers

  char reply[200];

  if (strncmp(line, "GET / ", 6) == 0) {
    sendResponse(id, HTML_H, NULL);

  } else if (strncmp(line, "GET /b?", 7) == 0) {
    char name[17];
    getParam(line, "n", name, sizeof(name));
    if (!name[0]) strcpy(name, "Anonymous");
    byte pos = buzz(name);
    if (pos == 1)      snprintf(reply, sizeof(reply), "YOU WERE FIRST!");
    else if (pos > 1)  snprintf(reply, sizeof(reply), "You are #%d (+%lu.%03lus)\nFirst: %s",
                                pos, order[pos - 1].dt / 1000, order[pos - 1].dt % 1000, order[0].name);
    else               snprintf(reply, sizeof(reply), "Too late! First: %s", order[0].name);
    sendResponse(id, TXT_H, reply);

  } else if (strncmp(line, "GET /s ", 7) == 0) {
    results(reply, sizeof(reply));
    sendResponse(id, TXT_H, reply);

  } else if (strncmp(line, "GET /r?", 7) == 0) {
    char key[16];
    getParam(line, "k", key, sizeof(key));
    if (strcmp(key, RESET_KEY) == 0) { resetRound(); sendResponse(id, TXT_H, "Reset done. Next question!"); }
    else sendResponse(id, TXT_H, "Wrong key");

  } else {
    closeConn(id);                                     // favicon etc.
  }
}

// ---------- Setup / loop ----------

void setup() {
  Serial.begin(9600);
  esp.begin(9600);
  pinMode(LED_PIN, OUTPUT);
  pinMode(RESET_BTN, INPUT_PULLUP);

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
  at(F("AT+CIPSTO=5"));

  Serial.println(F("# IP address:"));
  esp.println(F("AT+CIFSR"));
  waitFor("OK", 2000, true);
  Serial.println(F("\n# Buzzer ready. Type 'r' to reset."));
  resetRound();
}

void loop() {
  if (Serial.available() && Serial.read() == 'r') resetRound();
  if (digitalRead(RESET_BTN) == LOW) {          // wait for release, then reset
    delay(30);
    while (digitalRead(RESET_BTN) == LOW) ;
    resetRound();
  }
  if (esp.available() && waitFor("+IPD,", 100)) handleRequest();
}
