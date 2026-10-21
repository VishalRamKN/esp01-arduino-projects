# 02 · Phone ↔ Serial Monitor Chat

Chat between a phone and the Arduino IDE. The phone uses a web page; on the laptop you simply type in the **Serial Monitor**. The Arduino keeps the last 8 messages, and the phone page checks for new ones every 3 seconds.

```
Serial Monitor                                      Phone
# Chat ready - type a message here and press Enter
Phone: hello from the back bench                    Phone: hello from the back bench
You: hi! can you see this?                          Laptop: hi! can you see this?
```

## Run it

1. Set `WIFI_SSID`, `WIFI_PASS` and `WEB_KEY` in the sketch and upload.
2. Open the Serial Monitor: **9600 baud**, line ending **Newline** (or Both NL & CR). Note the IP.
3. On the phone, open `http://<ESP-IP>`, **type the password first**, then chat.
4. Reply by typing in the Serial Monitor's input box and pressing Enter.

`led on` / `led off` from the phone also switch the LED on pin 13.

## How it works

The page is loaded once. After that, JavaScript calls two small endpoints:

| URL | Returns |
|---|---|
| `/m?k=PASSWORD` | The chat history as plain text |
| `/s?k=PASSWORD&c=MESSAGE` | Adds the message, returns the history |

This is **polling**: the phone keeps asking "anything new?". Lines typed in the Serial Monitor arrive over USB and are added to the same history.

## What you learn

- Separating a web page (loaded once) from an API (called many times)
- `fetch()` and updating a page without reloading it
- Reading whole lines from `Serial` character by character
- Fixed-size message buffers when you only have 2 KB of RAM
- Why the JavaScript never sends two requests at once (the Arduino can only handle one)

## Challenges

1. Let the phone choose a nickname instead of "Phone".
2. Show a timestamp (seconds since the Arduino started) with each message.
3. Beep a piezo on pin 8 when a phone message arrives, so you notice it.
4. Polling every 3 s wastes requests. Add a counter so the phone only downloads the history when something changed.
5. Two phones can chat with each other through this. Try it. What breaks when five people join?
