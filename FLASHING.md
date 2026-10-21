# Flashing AT Firmware onto the ESP-01

Every project in this repo needs the ESP-01 running Espressif's **AT firmware**. Many modules ship with it already: try [00_at_test](00_at_test) first, and if `AT` replies `OK`, you can skip this guide.

Flash (or re-flash) when:
- the module replies with garbage at every baud rate, or nothing at all
- it was used with Arduino IDE / MicroPython / NodeMCU code before
- `AT+GMR` shows a very old version and some commands give `ERROR`

---

## 1. What you need

- ESP-01 / ESP-01S
- A way to talk to it over USB, either:
  - an **Arduino Uno** (used only as a USB-to-serial bridge), or
  - a **3.3 V USB-to-serial adapter** (CP2102, CH340, FT232 set to 3.3 V)
- The same 3.3 V regulator and 1 kΩ / 2.2 kΩ divider used in the projects
- A computer with Python 3

> Cheap "ESP-01 USB adapter" boards often have **no flash-mode switch**. They work for AT commands but can't flash unless you add a wire from GPIO0 to GND.

## 2. Check your module's flash size

| Module | Board colour | Usual flash |
|---|---|---|
| ESP-01S | Black | **1 MB** |
| ESP-01 (newer) | Black | **1 MB** |
| ESP-01 (old) | Blue | 512 KB |

You'll confirm the exact size with `esptool` in step 5. This guide covers **1 MB** modules, which is almost every ESP-01 sold today. Old 512 KB boards are not supported by current AT firmware; replacing the module with an ESP-01S is the easiest fix.

## 3. Download the tools and firmware

**esptool** (Espressif's flashing tool):

```
pip install esptool
```

**AT firmware:** download the latest release zip of the **ESP8266 NONOS SDK** from
<https://github.com/espressif/ESP8266_NONOS_SDK/releases> (version 3.0.x) and unzip it.

You need these four files from the `bin` folder:

| File | Where in the zip |
|---|---|
| `boot_v1.x.bin` (newest, e.g. `boot_v1.7.bin`) | `bin/` |
| `user1.1024.new.2.bin` | `bin/at/512+512/` |
| `esp_init_data_default_vXX.bin` (e.g. `_v08`) | `bin/` |
| `blank.bin` | `bin/` |

Copy them into one folder so the command in step 6 is short. Use the exact file names from your download in the commands below.

## 4. Wiring for flashing

The difference from the project wiring: **GPIO0 goes to GND**, and the serial lines go to the USB bridge.

### Option A: Arduino Uno as a USB-to-serial bridge

Connect the Uno's **RESET pin to GND**. This switches off the Uno's own chip so its USB port talks straight to the ESP.

| ESP-01 | Connect to |
|---|---|
| VCC, EN, RST | 3.3 V from regulator |
| GND | Regulator GND **and** Uno GND |
| **GPIO0** | **GND** (flash mode) |
| GPIO2 | Unconnected |
| TX | Uno **pin 1 (TX)** |
| RX | Uno **pin 0 (RX)** through the 1 kΩ / 2.2 kΩ divider |

Yes, TX to TX and RX to RX: with RESET grounded you're wiring to the USB chip, not to the Arduino's processor.

```
Uno pin 0 (RX) ──[1 kΩ]──┬── ESP RX
                         │
                      [2.2 kΩ]
                         │
                        GND
```

### Option B: 3.3 V USB-to-serial adapter

| ESP-01 | Adapter |
|---|---|
| TX | RX |
| RX | TX (no divider needed if the adapter is 3.3 V) |
| GND | GND |
| GPIO0 | GND |

Power the ESP from the external 3.3 V regulator, with grounds shared. The adapter's own 3.3 V pin is usually too weak.

### Enter flash mode

The ESP only checks GPIO0 **at power-up**. With GPIO0 on GND, switch the ESP's power off and on (or touch RST to GND for a moment). The blue LED should blink once and then stay off.

## 5. Check the connection

Find your port: **Windows**: Device Manager → Ports (e.g. `COM3`). **Linux**: `/dev/ttyUSB0` or `/dev/ttyACM0`. **macOS**: `/dev/cu.usbserial-*` or `/dev/cu.usbmodem*`.

```
python -m esptool --port COM3 flash_id
```

You should see `Detected flash size: 1MB`. If you get `Failed to connect`, see [Troubleshooting](#8-troubleshooting).

## 6. Erase and flash

Power-cycle the ESP into flash mode before **each** command (the ESP leaves flash mode after every esptool run).

**Erase:**
```
python -m esptool --port COM3 erase_flash
```

**Flash** (one line; replace file names to match your download):
```
python -m esptool --port COM3 --baud 115200 write_flash --flash_mode dout --flash_size 1MB 0x00000 boot_v1.7.bin 0x01000 user1.1024.new.2.bin 0xfc000 esp_init_data_default_v08.bin 0x7e000 blank.bin 0xfe000 blank.bin
```

These are Espressif's official addresses for 1 MB (8 Mbit, 512 KB + 512 KB) flash:

| Address | File |
|---|---|
| `0x00000` | `boot_v1.x.bin` |
| `0x01000` | `user1.1024.new.2.bin` |
| `0xfc000` | `esp_init_data_default_vXX.bin` |
| `0x7e000` | `blank.bin` |
| `0xfe000` | `blank.bin` |

It takes about 30 seconds. Wait for `Hash of data verified.`

> Newer esptool versions also accept `write-flash`, `--flash-mode`, `erase-flash`, etc. If your version complains about the underscore names, use the hyphen ones.

## 7. Back to normal mode

1. **Disconnect GPIO0 from GND** (leave it unconnected).
2. Power-cycle the ESP.
3. Open any serial terminal at **115200 baud** with **Both NL & CR** line endings:
   - Uno bridge: the Arduino IDE Serial Monitor works directly (Uno RESET still on GND).
   - Or: `python -m serial.tools.miniterm COM3 115200 --eol CRLF`
4. Send `AT`, which should reply `OK`. Send `AT+GMR` to see the new version.

Then remove the RESET-to-GND wire from the Uno, rewire as in the [main README](README.md#wiring-same-for-every-project), and do the [one-time switch to 9600 baud](README.md#one-time-setup-switch-the-esp-to-9600-baud).

## 8. Troubleshooting

| Problem | Fix |
|---|---|
| `Failed to connect ... No serial data received` | GPIO0 wasn't on GND **when the ESP powered up**: power-cycle it. Also check TX/RX (Uno bridge: TX→TX, RX→RX; adapter: crossed) |
| Connects, then fails partway | Power: use the external regulator, add a 100–470 µF capacitor, shorten wires. Try `--baud 57600` |
| `Detected flash size: 512KB` | Old module; current AT firmware won't fit. Use an ESP-01S |
| After flashing, only garbage at 115200 | GPIO0 still on GND (the ESP is still in flash mode). Disconnect it and power-cycle. Also try 74880 baud to read the boot message |
| Boot message says `boot mode:(1,x)` | Flash mode: GPIO0 is low. `boot mode:(3,x)` is normal mode |
| `AT` gets no reply, but boot text shows `ready` | Line ending must be **Both NL & CR** |
| Uno bridge shows nothing at all | Uno RESET must be on GND; Uno GND must connect to ESP GND |

## Alternative: Espressif Flash Download Tool (Windows, no command line)

Espressif's [Flash Download Tool](https://www.espressif.com/en/support/download/other-tools) does the same thing with a GUI. Choose **ESP8266**, add the four files at the addresses in the table above, set **SPI MODE: DOUT** and **FLASH SIZE: 8Mbit**, select your COM port, then press **ERASE** and **START** (power-cycling into flash mode before each).

## References

- [ESP8266 NONOS SDK releases](https://github.com/espressif/ESP8266_NONOS_SDK/releases), including `bin/at/README.md` with the official flash addresses
- [esptool documentation](https://docs.espressif.com/projects/esptool/en/latest/esp8266/)
