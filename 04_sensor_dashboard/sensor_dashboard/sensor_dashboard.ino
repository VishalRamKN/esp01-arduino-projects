// 04 - Live sensor dashboard
// Reads a sensor on A0 (potentiometer, LDR, temperature sensor...) and shows
// a live updating graph on your phone. A button on the page toggles the LED.
//
// Wiring: ESP TX -> pin 2, pin 3 -> 1k/2.2k divider -> ESP RX, shared GND
//         Sensor on A0 (see README), LED on pin 13 (built in)
// ESP must already be set to 9600 baud (AT+UART_DEF=9600,8,1,0,0)

#include <SoftwareSerial.h>

SoftwareSerial esp(2, 3); // RX, TX

// ---------- CHANGE THESE ----------
const char* WIFI_SSID = "YourWiFi";
const char* WIFI_PASS = "YourPassword";
// ----------------------------------

const int SENSOR_PIN = A0;
const int LED_PIN = 13;
bool ledOn = false;

// ---------- Web page (stored in flash) ----------
const char HTML_H[] PROGMEM =
  "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n";
const char TXT_H[] PROGMEM =
  "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n";

const char PAGE[] PROGMEM = R"rawliteral(<html><head><meta name=viewport content='width=device-width'>
<title>Sensor</title><style>body{font-family:sans-serif;padding:10px;text-align:center}
#v{font-size:48px;margin:6px}canvas{width:100%;border:1px solid #888}
button{font-size:18px;padding:8px 20px}</style></head><body>
<h3>Live sensor (A0)</h3><div id=v>--</div><canvas id=g width=300 height=150></canvas>
<p>Min <b id=mn>-</b> &nbsp; Max <b id=mx>-</b></p><button onclick=t()>Toggle LED</button> <span id=L></span>
<script>var d=[],b=0,w=0,x=g.getContext('2d');
function draw(){x.clearRect(0,0,300,150);x.strokeStyle='#28a';x.lineWidth=2;x.beginPath();
d.forEach((y,i)=>{var px=i*5,py=148-y*146/1023;i?x.lineTo(px,py):x.moveTo(px,py)});x.stroke()}
function upd(t){var p=t.split(',');v.textContent=p[0];L.textContent=p[1]=='1'?'LED on':'LED off';
d.push(+p[0]);if(d.length>60)d.shift();mn.textContent=Math.min(...d);mx.textContent=Math.max(...d);draw()}
function get(u){if(b)return;b=1;fetch(u,{cache:'no-store'}).then(r=>r.text()).then(upd).catch(e=>0).finally(()=>b=0)}
function t(){w=1;L.textContent='...'}
setInterval(()=>{if(b)return;get(w?'/led':'/d');w=0},1000);
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

void closeConn(int id) {
  esp.print(F("AT+CIPCLOSE="));
  esp.println(id);
  waitFor("OK", 1000);
}

// Send a PROGMEM header followed by either the page (body == NULL) or a short text body
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

void sendReading(int id) {
  char body[16];
  int value = analogRead(SENSOR_PIN);
  snprintf(body, sizeof(body), "%d,%d", value, ledOn ? 1 : 0);   // e.g. "512,1"
  sendResponse(id, TXT_H, body);
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

  if (strncmp(line, "GET / ", 6) == 0) {
    sendResponse(id, HTML_H, NULL);
  } else if (strncmp(line, "GET /d ", 7) == 0) {
    sendReading(id);
  } else if (strncmp(line, "GET /led ", 9) == 0) {
    ledOn = !ledOn;
    digitalWrite(LED_PIN, ledOn);
    Serial.println(ledOn ? F("# LED on") : F("# LED off"));
    sendReading(id);
  } else {
    closeConn(id);                                     // favicon etc.
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
  Serial.println(F("\n# Dashboard ready"));
}

void loop() {
  if (esp.available() && waitFor("+IPD,", 100)) handleRequest();
}
