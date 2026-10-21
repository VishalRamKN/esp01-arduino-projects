# 06 · Internet Info (ESP as a client)

All the other projects make the ESP a **server** that phones connect to. Here it's a **client**: the Arduino connects to a real website, writes the HTTP request itself, and prints what comes back:

```
------------------------------
Country  : India
City     : Chennai
Timezone : Asia/Kolkata
Your IP  : 49.xxx.xxx.xxx
Time GMT : Wed, 21 Oct 2026 08:43:00 GMT
------------------------------
```

No phone needed. Everything appears in the Serial Monitor. Type `g` to fetch again.

## Run it

1. Set `WIFI_SSID` and `WIFI_PASS` and upload.
2. Open the Serial Monitor at 9600 and wait for the result.

The data comes from the free [ip-api.com](https://ip-api.com) service over plain HTTP. The time comes from the `Date:` header that every web server sends.

## What the Arduino does, step by step

```
AT+CIPSTART="TCP","ip-api.com",80        open a connection
AT+CIPSEND=95                            "I'm about to send 95 bytes"
GET /line/?fields=country,city,timezone,query HTTP/1.1
Host: ip-api.com
Connection: close
                                         (blank line = end of request)
```

The server replies with headers (`HTTP/1.1 200 OK`, `Date: ...`, ...), a blank line, then the body. The ESP wraps it as `+IPD,<length>:<data>` and says `CLOSED` when the server hangs up.

## What you learn

- Client vs. server: who connects to whom
- The structure of an HTTP request and response, written and read by hand
- What a public IP is, and why the ESP's own IP (`192.168.x.x`) is different (NAT)
- Using a web API from a microcontroller

## Challenges

1. Convert the GMT time to local time (IST is GMT + 5:30) and print just `HH:MM`.
2. Turn this into a clock: fetch once, then count with `millis()`, re-sync every 10 minutes.
3. Fetch other plain-HTTP data, for example `http://wttr.in/Chennai?format=3` for the weather.
4. Combine with project 04: serve the time on a web page.
5. Why can't the ESP-01 easily open `https://` sites? What does TLS need that this tiny chip lacks?
