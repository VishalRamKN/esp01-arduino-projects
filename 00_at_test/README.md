# 00 · AT Command Test

A serial bridge: whatever you type in the Serial Monitor goes to the ESP-01, and its replies come back. Use it to check your wiring, change the baud rate, and explore the AT command set by hand.

## Run it

1. Wire the ESP as in the [main README](../README.md#wiring-same-for-every-project).
2. **First time with a new ESP:** set `ESP_BAUD = 115200`, upload, then send `AT+UART_DEF=9600,8,1,0,0`. Set `ESP_BAUD = 9600`, upload again and power-cycle the ESP.
3. Serial Monitor: **9600 baud**, **Both NL & CR**.

## Commands to try

| Command | What it does |
|---|---|
| `AT` | Should reply `OK` |
| `AT+GMR` | Firmware version |
| `AT+CWMODE=1` | Station (client) mode |
| `AT+CWLAP` | List nearby WiFi networks |
| `AT+CWJAP="YourWiFi","password"` | Join a network (wait for `WIFI GOT IP`) |
| `AT+CIFSR` | Show the ESP's IP address |
| `AT+PING="google.com"` | Ping a website (newer firmware) |
| `AT+CWQAP` | Disconnect from WiFi |
| `AT+RST` | Restart the ESP |

## What you learn

- Serial communication and why both ends must agree on the baud rate
- Why SoftwareSerial struggles at high speeds
- That the "WiFi chip" is really a small computer you talk to with text commands

## Challenges

1. Make the ESP a WiFi hotspot: `AT+CWMODE=2`, then `AT+CWSAP="MyESP","12345678",5,3`. Can you see it on your phone?
2. Find out what `AT+CWMODE=3` does.
3. Use `AT+CIPSTART`, `AT+CIPSEND` and type an HTTP request by hand to `example.com` on port 80. (Project 06 automates this.)
