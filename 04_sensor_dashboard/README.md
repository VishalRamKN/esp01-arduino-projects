# 04 · Live Sensor Dashboard

A phone page that shows a sensor reading as a big number and a live graph of the last 60 seconds, with min/max and a button to toggle the LED.

## Sensor wiring (pick one)

**Potentiometer** (easiest): outer legs to 5 V and GND, middle leg to **A0**.

**LDR (light sensor):**
```
5V ──[LDR]──┬── A0
            │
         [10 kΩ]
            │
           GND
```
Cover it with your hand and watch the graph drop.

**LM35 temperature sensor:** left pin 5 V, middle pin A0, right pin GND. Temperature in °C = reading × 0.488 (see challenge 1).

## Run it

1. Set `WIFI_SSID` and `WIFI_PASS`, upload, note the IP.
2. Open `http://<ESP-IP>` on the phone. The graph updates every second.

## How it works

| URL | Returns |
|---|---|
| `/` | The dashboard page (sent once) |
| `/d` | Just the data: `512,0` (reading, LED state) |
| `/led` | Toggles the LED, returns the data |

The page does all the drawing on an HTML `<canvas>`. The Arduino only ever sends a few bytes, which is how real IoT devices work: small data, smart display.

## What you learn

- The Arduino ADC: 0–1023 for 0–5 V (about 4.9 mV per step)
- Voltage dividers as sensors (LDR + resistor)
- Designing a tiny data API instead of sending whole pages
- Drawing graphs in the browser

## Challenges

1. Convert the reading to real units (volts, °C, % light) on the Arduino, or in the JavaScript.
2. Add a second sensor on A1 and draw it as a second line.
3. Add an alarm: turn the page red and the LED on when the value crosses a threshold set from the phone.
4. Also print each reading with `Serial.println()` and watch it in the Arduino IDE's **Serial Plotter** (Tools → Serial Plotter) next to the phone graph.
5. Why does the reading jitter even when nothing changes? Try averaging 10 readings.
