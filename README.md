# ESP-01 + Arduino: WiFi Projects for the Classroom

Seven hands-on projects that put an Arduino Uno on WiFi using a cheap **ESP-01 (ESP8266) running AT firmware**. Students control hardware from their phones, chat with the Serial Monitor, scan networks, graph sensors live, run a quiz buzzer, and fetch data from the internet, all by sending text commands to the ESP over serial.

No WiFi libraries are used. Every project talks to the ESP with plain AT commands, so students see exactly how a web server and a web client work underneath. Everything runs in the **Arduino IDE**: no extra software on the laptop.

| # | Project | What students build | Key ideas |
|---|---|---|---|
| 00 | [AT test](00_at_test) | Type AT commands by hand | Serial, baud rate, AT command set |
| 01 | [Web remote](01_web_remote) | Phone buttons control an LED, buzzer and servo | HTTP GET, query strings, URL decoding, passwords |
| 02 | [Chat](02_chat) | Two-way chat between a phone and the Serial Monitor | Polling, JavaScript `fetch`, message buffers |
| 03 | [WiFi scanner](03_wifi_scanner) | Live list of nearby networks with signal bars | RSSI / dBm, parsing text, sorting |
| 04 | [Sensor dashboard](04_sensor_dashboard) | Live graph of a sensor on the phone | ADC, APIs that return data, canvas graphics |
| 05 | [Quiz buzzer](05_quiz_buzzer) | Whole class buzzes in from phones, first one wins | Multiple clients, timing, game state |
| 06 | [Internet info](06_internet_info) | Arduino fetches public IP, city and internet time | ESP as a **client**, raw HTTP requests, headers |

Do 00 first. After that, every project stands on its own.

**No AT firmware on your module, or it's misbehaving?** See [FLASHING.md](FLASHING.md) to install it with an Arduino Uno or a USB-serial adapter.

---

## Hardware

- Arduino Uno (or Nano)
- ESP-01 or ESP-01S module with **AT firmware** ([how to flash it](FLASHING.md))
- 3.3 V supply that can give **at least 300 mA**: an LM2596 / AMS1117 regulator module from a 5–12 V battery or adapter
- Resistors: **1 kΩ and 2.2 kΩ** (voltage divider)
- Optional: 10 kΩ resistor, 100–470 µF capacitor, potentiometer or LDR (project 04), piezo buzzer and push button (project 05)

> **Do not power the ESP-01 from the Uno's 3.3 V pin.** It only supplies about 50 mA. The ESP draws 250–300 mA spikes when transmitting, which causes random resets and garbage output.

## Wiring (same for every project)

ESP-01 pinout, antenna up, pins facing you:

```
 TX    | GND
 EN    | GPIO2
 RST   | GPIO0
 VCC   | RX
```

| ESP-01 pin | Connect to |
|---|---|
| VCC | 3.3 V from regulator |
| GND | Regulator GND **and** Arduino GND (shared ground is essential) |
| EN (CH_PD) | 3.3 V (**required**, the ESP is off without it) |
| RST | 3.3 V, ideally through 10 kΩ (stops noise resets) |
| GPIO0, GPIO2 | Leave unconnected (internal pull-ups = normal boot) |
| TX | Arduino **pin 2** (direct) |
| RX | Arduino **pin 3 through the voltage divider** |

Voltage divider (5 V Arduino signal → about 3.4 V for the ESP):

```
Arduino pin 3 ──[1 kΩ]──┬── ESP RX
                        │
                     [2.2 kΩ]
                        │
                       GND
```

If you use an LM2596 buck converter, **set it to 3.3 V with a multimeter before connecting the ESP**. They often ship set much higher, and anything above about 3.6 V can destroy the ESP-01.

A 100–470 µF capacitor across the ESP's VCC and GND helps a lot with resets during WiFi activity.

## One-time setup: switch the ESP to 9600 baud

AT firmware starts at 115200 baud, which is too fast for the Arduino's SoftwareSerial. Do this once per ESP module:

1. Open [`00_at_test/at_test/at_test.ino`](00_at_test/at_test/at_test.ino), set `ESP_BAUD = 115200`, upload.
2. Serial Monitor: **9600 baud**, line ending **Both NL & CR**.
3. Send `AT+UART_DEF=9600,8,1,0,0` (on old firmware: `AT+CIOBAUD=9600`).
4. Set `ESP_BAUD = 9600`, upload again, power-cycle the ESP, send `AT` and you should get a clean `OK`.

The ESP remembers this setting permanently. All other projects assume 9600.

## Running any project

1. Open the `.ino` file in the Arduino IDE (each sketch is in a folder with the same name, as the IDE requires).
2. Set `WIFI_SSID` and `WIFI_PASS` at the top. Change any password/key too.
3. Upload and open the Serial Monitor at 9600.
4. Wait for the `+CIFSR:STAIP,"192.168.x.x"` line. That's the ESP's address.
5. On a phone **connected to the same WiFi**, open `http://192.168.x.x`.

Keep the Serial Monitor open while a project runs: it shows what the Arduino is doing, and in project 02 it's where you type your chat replies.

## How the server projects work

```
Phone browser ──WiFi──> ESP-01 ──serial "+IPD,0,350:GET /?c=hi HTTP/1.1..."──> Arduino
                                                                                  │ parse request,
                                                                                  │ do something
Phone browser <──WiFi── ESP-01 <──"AT+CIPSEND=0,512" then the HTML page────────── ┘
```

1. `AT+CIPMUX=1` + `AT+CIPSERVER=1,80` turn the ESP into a web server.
2. When a browser connects, the ESP forwards the request to the Arduino as `+IPD,<connection id>,<length>:<data>`.
3. The Arduino reads the first line (`GET /path?query HTTP/1.1`), decides what to do and builds a reply.
4. `AT+CIPSEND=<id>,<length>` announces the reply, the ESP answers `>`, the Arduino sends exactly that many bytes, then `AT+CIPCLOSE=<id>`.

Project 06 flips this around: `AT+CIPSTART` connects *out* to a website and the Arduino writes the HTTP request itself.

## Troubleshooting

| Symptom | Likely cause and fix |
|---|---|
| No response to `AT` | Swap TX/RX; check EN is at 3.3 V; Serial Monitor line ending must be **Both NL & CR** |
| Only garbage characters | Baud mismatch: try 115200, 9600, 74880 in `esp.begin()` |
| Works, then resets during WiFi commands | Weak power supply: use a proper regulator, add a capacitor, check the battery |
| A few garbled characters now and then | Normal with SoftwareSerial; shorten wires and make sure the ground is solid |
| `ERROR` replies when Serial is set to 115200 | Keep **both** serials at 9600 (SoftwareSerial drops characters otherwise) |
| `# WiFi FAILED` | Wrong SSID/password, or a 5 GHz-only network (ESP8266 is 2.4 GHz only) |
| Page never loads on the phone | Phone must be on the same WiFi; some college/guest networks block device-to-device traffic. Use a phone hotspot instead |
| Upload fails with "port busy" | Close any other program using the port (another Serial Monitor window, a terminal) |

## Limitations worth discussing with students

- **Memory:** the Uno has 2 KB of RAM. Pages live in flash (`PROGMEM`, `F()`), and buffers are kept small.
- **Speed:** 9600 baud is about 1 KB per second, so a page takes about a second to load. Good for seeing how bandwidth limits design.
- **One thing at a time:** the Arduino handles one request at a time. The JavaScript in projects 02 and 04 avoids overlapping requests for this reason.
- **Security:** everything is plain HTTP. Passwords travel unencrypted and appear in URLs. Fine on a lab network, never on the internet. A good discussion starter about why HTTPS exists.

## Suggested lab sequence

| Session | Activity |
|---|---|
| 1 | Wiring, power, 00 AT test, baud change, scan and join WiFi by hand |
| 2 | 03 WiFi scanner: what is dBm, why does signal drop through walls? Walk around and measure |
| 3 | 04 Sensor dashboard: ADC, voltage dividers, live data |
| 4 | 01 Web remote: HTTP requests, query strings, why passwords in URLs are a bad idea |
| 5 | 02 Chat: client polling vs. push, buffers |
| 6 | 06 Internet info: client vs. server, reading HTTP headers by hand |
| 7 | 05 Quiz buzzer: play it, then discuss fairness (is "first to the Arduino" really first to press?) |

Every project README ends with **challenges** for students to extend it.

## License

MIT, see [LICENSE](LICENSE).
