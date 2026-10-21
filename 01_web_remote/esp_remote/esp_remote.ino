// 01 - Web remote
// Control the Arduino from your phone: open the ESP's IP, log in with a
// password, then tap a button or type a command. Every command is also
// shown in the Arduino Serial Monitor.
//
// Commands:
//   led on / led off      built-in LED (pin 13)
//   blink 5               blink the LED 5 times (1-20)
//   beep                  short beep on a piezo buzzer (pin 8)
//   servo 90              move a servo to 0-180 degrees (pin 9)
//   anything else         shown as a message in the Serial Monitor
//
// Wiring: ESP TX -> pin 2, pin 3 -> 1k/2.2k divider -> ESP RX, shared GND
//         Optional: piezo pin 8 -> GND; servo signal pin 9 (power it from 5V/GND)
// ESP must already be set to 9600 baud (AT+UART_DEF=9600,8,1,0,0)

#include <SoftwareSerial.h>
#include <Servo.h>

SoftwareSerial esp(2, 3); // RX, TX
Servo servo;

// ---------- CHANGE THESE ----------
const char* WIFI_SSID = "YourWiFi";
const char* WIFI_PASS = "YourPassword";
const char* WEB_KEY   = "1234";        // password for the web page
// ----------------------------------

const int LED_PIN = 13;
const int PIEZO_PIN = 8;
const int SERVO_PIN = 9;

// ---------- Web page (stored in flash to save RAM) ----------
const char H[] PROGMEM =
  "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n";
const char P1[] PROGMEM =
  "<html><head><meta name=viewport content='width=device-width'>"
  "<title>Web Remote</title><style>body{font-family:sans-serif;padding:12px}"
  "button{font-size:17px;padding:8px 12px;margin:3px}input{font-size:17px;padding:6px}</style></head>"
  "<body><h3>Arduino Web Remote</h3><form>"
  "<input name=k type=password placeholder=Password value='";
const char P2[] PROGMEM =
  "'><br><br>"
  // Text box + Send come first: pressing Enter uses the FIRST button in a form
  "<input name=t placeholder='Command or message'><button>Send</button><br><br>"
  "<button name=c value='led on'>LED on</button><button name=c value='led off'>LED off</button>"
  "<button name=c value='blink 5'>Blink</button><button name=c value=beep>Beep</button><br>"
  "<button name=c value='servo 0'>Servo 0</button><button name=c value='servo 90'>90</button>"
  "<button name=c value='servo 180'>180</button></form><p>";
const char P3[] PROGMEM = "</p></body></html>";

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

// Get ?name=value from "GET /?k=1234&c=led+on HTTP/1.1"
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

void sendPage(int id, const char* key, const char* status) {
  int len = strlen_P(H) + strlen_P(P1) + strlen(key) + strlen_P(P2) + strlen(status) + strlen_P(P3);
  esp.print(F("AT+CIPSEND="));
  esp.print(id);
  esp.print(',');
  esp.println(len);
  if (!waitFor(">", 2000)) { closeConn(id); return; }
  espPrintP(H);
  espPrintP(P1);
  esp.print(key);
  espPrintP(P2);
  esp.print(status);
  espPrintP(P3);
  waitFor("SEND OK", 5000);
  closeConn(id);
}

// Carry out a command from the phone. Writes what happened into 'status'.
void runCommand(const char* cmd, char* status, size_t size) {
  Serial.print(F("Phone: "));
  Serial.println(cmd);

  if (strcmp(cmd, "led on") == 0) {
    digitalWrite(LED_PIN, HIGH);
    snprintf(status, size, "LED is on");

  } else if (strcmp(cmd, "led off") == 0) {
    digitalWrite(LED_PIN, LOW);
    snprintf(status, size, "LED is off");

  } else if (strncmp(cmd, "blink", 5) == 0) {
    int times = constrain(atoi(cmd + 5), 1, 20);      // "blink" alone = 1
    for (int i = 0; i < times; i++) {
      digitalWrite(LED_PIN, HIGH); delay(150);
      digitalWrite(LED_PIN, LOW);  delay(150);
    }
    snprintf(status, size, "Blinked %d times", times);

  } else if (strcmp(cmd, "beep") == 0) {
    tone(PIEZO_PIN, 1000, 300);
    snprintf(status, size, "Beep!");

  } else if (strncmp(cmd, "servo ", 6) == 0) {
    int angle = constrain(atoi(cmd + 6), 0, 180);
    servo.write(angle);
    snprintf(status, size, "Servo at %d degrees", angle);

  } else {
    snprintf(status, size, "Message shown on Serial Monitor");
  }
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

  // Only answer the main page (ignore favicon etc.)
  if (strncmp(line, "GET / ", 6) != 0 && strncmp(line, "GET /?", 6) != 0) {
    closeConn(id);
    return;
  }

  char key[24], cmd[48], status[64];
  getParam(line, "k", key, sizeof(key));
  getParam(line, "c", cmd, sizeof(cmd));               // from a button
  if (!cmd[0]) getParam(line, "t", cmd, sizeof(cmd));  // from the text box
  bool ok = strcmp(key, WEB_KEY) == 0;

  if (!key[0])     strcpy(status, "Enter password");
  else if (!ok)    strcpy(status, "Wrong password");
  else if (cmd[0]) runCommand(cmd, status, sizeof(status));
  else             strcpy(status, "Logged in");

  sendPage(id, ok ? key : "", status);
}

// ---------- Setup / loop ----------

void setup() {
  Serial.begin(9600);
  esp.begin(9600);
  pinMode(LED_PIN, OUTPUT);
  servo.attach(SERVO_PIN);
  servo.write(90);

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
  Serial.println(F("\n# Remote ready - open the IP on your phone"));
}

void loop() {
  if (esp.available() && waitFor("+IPD,", 100)) handleRequest();
}
