# 03 · WiFi Scanner

Open the ESP's IP on your phone and see every WiFi network around you, sorted strongest first, with coloured signal bars, channel number and whether it's open. Refresh to scan again. Type `s` in the Serial Monitor to scan without a phone.

## Run it

1. Set `WIFI_SSID` and `WIFI_PASS`, upload, note the IP.
2. Open `http://<ESP-IP>` on a phone on the same WiFi. Each scan takes 3–5 seconds.

## How it works

`AT+CWLAP` makes the ESP scan and reply with one line per network:

```
+CWLAP:(3,"Home WiFi",-52,"aa:bb:cc:dd:ee:ff",6,...)
         │   │          │   │                   └ channel
         │   │          │   └ MAC address of the router
         │   │          └ signal strength in dBm
         │   └ network name (SSID)
         └ security type (0 = open)
```

The sketch parses these lines, sorts them with a bubble sort, and builds an HTML table. Because `AT+CIPSEND` needs the exact length up front, the page is rendered **twice**: once into a byte counter, once to the ESP.

## What you learn

- dBm: a logarithmic scale. -50 is excellent, -70 is fine, -85 is barely usable. Every -3 dB is half the power.
- WiFi channels and why neighbours on the same channel slow each other down
- Parsing structured text with `strchr`, `atoi`
- A simple sorting algorithm on real data

## Challenges

1. Walk around the building with a power bank and map the signal of your college WiFi. Where are the dead zones?
2. Show how many networks use each channel (1–13). Which channel would you pick for a new router?
3. Count open networks and show a warning about them.
4. Replace bubble sort with insertion sort. Which is better for nearly-sorted data?
5. Light the LED when a specific network (your phone's hotspot) comes into range: a "presence detector".
