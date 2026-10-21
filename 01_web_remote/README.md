# 01 · Web Remote

Open the ESP's IP on your phone, log in with a password, and control the Arduino: switch the LED, blink it, beep a buzzer, or turn a servo. Type any other text and it appears in the Arduino Serial Monitor as a message.

```
Phone ──WiFi──> ESP-01 ──> Arduino ──> LED / buzzer / servo
                                └────> Serial Monitor shows every command
```

## Extra wiring (optional)

- Piezo buzzer: pin 8 → buzzer → GND
- Servo (SG90): signal (orange) → pin 9, red → 5 V, brown → GND

The LED on pin 13 is built in, so the project works with no extra parts.

## Run it

1. Set `WIFI_SSID`, `WIFI_PASS` and `WEB_KEY` (the web password) in the sketch and upload.
2. Open the Serial Monitor at 9600 and note the IP (`+CIFSR:STAIP,"192.168.x.x"`).
3. On a phone on the same WiFi, open `http://<ESP-IP>`, enter the password and tap a button.

## Commands

| Command | What it does |
|---|---|
| `led on` / `led off` | Built-in LED |
| `blink 5` | Blinks the LED 5 times (1–20) |
| `beep` | Short beep on the piezo |
| `servo 90` | Moves the servo to 0–180° |
| anything else | Shown as `Phone: ...` in the Serial Monitor |

The buttons send these same commands; the text box lets you type your own.

## What you learn

- An HTTP request is just a line of text: `GET /?k=1234&c=servo+90 HTTP/1.1`
- HTML forms and buttons put their values in the URL (query string)
- URL encoding (`+` is a space, `%41` is `A`) and how to decode it
- Parsing commands with `strcmp`, `strncmp` and `atoi`

## Challenges

1. Add a command that plays a short tune on the piezo.
2. Add `rgb 255 0 128` to drive an RGB LED with PWM on pins 5, 6 and 10.
3. Control a relay module to switch a lamp (with a teacher checking the wiring!).
4. Wrong passwords can be retried forever. Add a lockout after 3 wrong attempts.
5. Log in, then look at the browser's history. What do you notice about the password? How would HTTPS or a POST form help?
6. The servo twitches slightly while WiFi data is moving. Find out why. (Hint: SoftwareSerial pauses interrupts while it sends.)
