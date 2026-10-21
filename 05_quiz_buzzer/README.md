# 05 · Classroom Quiz Buzzer

Every student opens the ESP's page on their phone, types their name and gets a big red **BUZZ** button. The first to buzz lights the LED and beeps the piezo. Everyone sees their position and how far behind they were (`You are #3 (+0.412s)`). The Serial Monitor shows the full order for the teacher.

## Extra wiring (optional)

- Piezo buzzer: pin 8 → buzzer → GND
- Reset button: pin 4 → button → GND (uses the internal pull-up)

## Run it

1. Set `WIFI_SSID`, `WIFI_PASS` and `RESET_KEY`, upload, note the IP.
2. Write the IP on the board. Students open `http://<ESP-IP>` and enter their names.
3. Ask a question. Students buzz. Tap **Show results** to see the order on any phone.
4. Reset for the next question in any of three ways:
   - type `r` in the Serial Monitor
   - press the reset button on pin 4
   - open `http://<ESP-IP>/r?k=teach` on the teacher's phone

Up to 6 buzzes are recorded per question. Pressing twice doesn't change your position.

## Tips for a big class

- Use a phone hotspot or a lab router. Many college WiFi networks block phones from reaching each other.
- The ESP-01 handles up to about 5 connections at once. With 30 students, a few buzzes may take a second longer; the order is still recorded fairly as they arrive.
- Play in teams (one phone per team) for the smoothest game.

## What you learn

- Many clients talking to one server
- Shared state (who buzzed, in what order) and resetting it
- Measuring time intervals with `millis()`
- Why `%f` doesn't work in `snprintf` on an Arduino (and the integer trick used instead)

## Discussion: is it fair?

The Arduino records when each request **arrives**, not when the button was **pressed**. A phone with weaker signal, or a request that arrives while the Arduino is busy answering someone else, can lose by a few hundred milliseconds. How would you make it fairer? (Hint: what if the phone sent its own timestamp? Can you trust it?)

## Challenges

1. Add a "lock" mode: the teacher opens buzzing only after finishing the question; early buzzes are rejected.
2. Keep scores across questions; the teacher awards a point to the winner.
3. Add a scoreboard page that refreshes every few seconds for the projector.
4. Play different tones for 1st, 2nd and 3rd.
5. Stop someone buzzing under five different names.
