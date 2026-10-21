// 02 - Chat: phone browser <-> Arduino Serial Monitor
// The phone chats through a web page. On the laptop, just use the Arduino
// IDE's Serial Monitor: type a message and press Enter to reply.
// Serial Monitor settings: 9600 baud, "Newline" or "Both NL & CR".
//
// Wiring: ESP TX -> pin 2, pin 3 -> 1k/2.2k divider -> ESP RX, shared GND
// ESP must already be set to 9600 baud (AT+UART_DEF=9600,8,1,0,0)

#include <SoftwareSerial.h>

SoftwareSerial esp(2, 3); // RX, TX

// ---------- CHANGE THESE ----------
const char* WIFI_SSID = "YourWiFi";
const char* WIFI_PASS = "YourPassword";
const char* WEB_KEY   = "1234";        // chat password
// ----------------------------------

const int LED_PIN = 13;

// ---------- Chat history (last 8 messages) ----------
const byte LOG_N = 8;
const byte LOG_W = 48;
char logBuf[LOG_N][LOG_W];
byte logCount = 0;
const char EMPTY_LOG[] = "No messages yet";

void addLog(const char* who, const char* msg) {
  if (logCount == LOG_N) {                       // full: drop oldest
    memmove(logBuf[0], logBuf[1], (LOG_N - 1) * LOG_W);
    logCount--;
  }
  snprintf(logBuf[logCount++], LOG_W, "%s%s", who, msg);
}

int logLength() {
  if (logCount == 0) return strlen(EMPTY_LOG);
  int n = 0;
  for (byte i = 0; i < logCount; i++) n += strlen(logBuf[i]) + 1;
  return n;
}

void printLog() {
  if (logCount == 0) { esp.print(EMPTY_LOG); return; }
  for (byte i = 0; i < logCount; i++) { esp.print(logBuf[i]); esp.print('\n'); }
}

// ---------- Web page (stored in flash) ----------
const char HTML_H[] PROGMEM =
  "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n";
const char TXT_H[] PROGMEM =
  "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n";

const char PAGE[] PROGMEM = R"rawliteral(<html><head><meta name=viewport content='width=device-width'>
<title>ESP Chat</title><style>body{font-family:sans-serif;padding:10px}
#l{white-space:pre-wrap;border:1px solid #888;height:55vh;overflow:auto;padding:6px}
#c{width:70%}</style></head><body><h3>ESP Chat</h3>
<input id=k type=password placeholder=Password><br><br><div id=l></div><br>
<input id=c placeholder=Message maxlength=39><button onclick=s()>Send</button>
<script>var b=0,q=[];
function g(u,m){b=1;fetch(u+'?k='+encodeURIComponent(k.value)+'&c='+encodeURIComponent(m||''),{cache:'no-store'})
.then(r=>r.text()).then(t=>{l.textContent=t;l.scrollTop=1e6}).catch(e=>0)
.finally(()=>{b=0;if(q.length)g('/s',q.shift())})}
function s(){if(c.value){b?q.push(c.value):g('/s',c.value);c.value=''}}
c.onkeydown=e=>{if(e.key=='Enter')s()};
setInterval(()=>{if(k.value&&!b)g('/m')},3000);
</script></body></html>)rawliteral";

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

// Get ?name=value from "GET /s?k=1234&c=hello HTTP/1.1"
void getParam(const char* line, const char* name, char* out, size_t outSize) {
  out[0] = 0;
  const char* p = strchr(line, '?');
  if (!p) return;
  size_t n = strlen(name);
  while (*p && *p != ' ') {
    p++; // skip '?' or '&'
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

// kind: 0 = chat page, 1 = chat log, 2 = plain text
void sendResponse(int id, byte kind, const char* text = "") {
  const char* hdr = (kind == 0) ? HTML_H : TXT_H;
  int len = strlen_P(hdr);
  if (kind == 0)      len += strlen_P(PAGE);
  else if (kind == 1) len += logLength();
  else                len += strlen(text);

  esp.print(F("AT+CIPSEND="));
  esp.print(id);
  esp.print(',');
  esp.println(len);
  if (!waitFor(">", 2000)) { closeConn(id); return; }

  espPrintP(hdr);
  if (kind == 0)      espPrintP(PAGE);
  else if (kind == 1) printLog();
  else                esp.print(text);

  waitFor("SEND OK", 5000);
  closeConn(id);
}

void handleRequest() {
  int id = 0, c;
  while ((c = readChar()) >= 0 && c != ',') if (isDigit(c)) id = id * 10 + (c - '0');
  while ((c = readChar()) >= 0 && c != ':') ;          // skip length

  char line[128];
  size_t n = 0;
  while ((c = readChar()) >= 0 && c != '\r' && c != '\n') {
    if (n < sizeof(line) - 1) line[n++] = c;
  }
  line[n] = 0;
  waitFor("\r\n\r\n", 300);                            // skip rest of headers

  // line looks like "GET /s?k=...&c=... HTTP/1.1"; path starts at line[4]
  if (strncmp(line + 4, "/ ", 2) == 0) { sendResponse(id, 0); return; }

  char p = line[5];
  bool isMsg  = (line[4] == '/' && p == 'm' && (line[6] == '?' || line[6] == ' '));
  bool isSend = (line[4] == '/' && p == 's' && (line[6] == '?' || line[6] == ' '));
  if (!isMsg && !isSend) { closeConn(id); return; }   // favicon etc.

  char key[24], msg[40];
  getParam(line, "k", key, sizeof(key));
  if (strcmp(key, WEB_KEY) != 0) { sendResponse(id, 2, "Wrong password"); return; }

  if (isSend) {
    getParam(line, "c", msg, sizeof(msg));
    if (msg[0]) {
      addLog("Phone: ", msg);
      Serial.print(F("Phone: "));                      // show in Serial Monitor
      Serial.println(msg);
      if (strcmp(msg, "led on") == 0)  digitalWrite(LED_PIN, HIGH);
      if (strcmp(msg, "led off") == 0) digitalWrite(LED_PIN, LOW);
    }
  }
  sendResponse(id, 1);
}

// Messages typed on the laptop arrive over USB, one per line
char inBuf[40];
byte inLen = 0;

void readLaptop() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (inLen) {
        inBuf[inLen] = 0;
        addLog("Laptop: ", inBuf);
        Serial.print(F("You: "));                      // echo, so the Monitor reads like a chat
        Serial.println(inBuf);
        inLen = 0;
      }
    } else if (inLen < sizeof(inBuf) - 1) {
      inBuf[inLen++] = c;
    }
  }
}

// ---------- Setup / loop ----------

void setup() {
  Serial.begin(9600);
  esp.begin(9600);
  pinMode(LED_PIN, OUTPUT);

  Serial.println(F("# Resetting ESP..."));
  esp.println(F("AT+RST"));
  waitFor("ready", 5000);
  delay(500);

  at(F("ATE0"));                  // turn off echo
  at(F("AT+CWMODE=1"));           // station mode

  Serial.println(F("# Connecting to WiFi..."));
  esp.print(F("AT+CWJAP=\""));
  esp.print(WIFI_SSID);
  esp.print(F("\",\""));
  esp.print(WIFI_PASS);
  esp.println('"');
  Serial.println(waitFor("OK", 15000) ? F("# WiFi connected") : F("# WiFi FAILED"));

  at(F("AT+CIPMUX=1"));           // multiple connections (needed for server)
  at(F("AT+CIPSERVER=1,80"));     // web server on port 80
  at(F("AT+CIPSTO=5"));           // drop idle connections after 5 s

  Serial.println(F("# IP address:"));
  esp.println(F("AT+CIFSR"));
  waitFor("OK", 2000, true);
  Serial.println(F("\n# Chat ready - type a message here and press Enter"));
}

void loop() {
  readLaptop();
  if (esp.available() && waitFor("+IPD,", 100)) handleRequest();
}
