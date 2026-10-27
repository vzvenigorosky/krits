# Spy Bug Detector

A hide-and-seek game for young spies. One spy hides a **bug** somewhere
in the house. The others hunt it down with a **detector** that gets
"hotter" as they get closer. It beeps faster and higher, and the lights
change from green to yellow to red.

## How it works

The bug is a small ESP8266 that broadcasts its own WiFi network
(`SPYBUG-1`). Nobody ever connects to it.

The detector scans for that network a few times per second and reads its
**signal strength** (RSSI, in dBm). The closer you are, the stronger the
signal:

| Signal (example) | Zone | Light  | Buzzer                      |
|------------------|------|--------|-----------------------------|
| weaker than −65  | COLD | green  | low tone, slow beeps        |
| −65 to −50       | WARM | yellow | middle tone, faster beeps   |
| stronger than −50| HOT! | red    | high tone, rapid beeps      |
| not heard        | —    | off    | silent, "NO SIGNAL" on screen |

The thresholds are examples. Every house is different, so step 1 is about
measuring yours.

Science bonus: walls, people and water (fish tanks, even your own body)
weaken WiFi. Turn slowly in a circle holding the detector against your
chest. The signal is weakest when your body is between you and the bug.
That gives you a direction.

## Hardware

**Bug (beacon)**: ESP8266MOD dev board (no screen), powered by a USB
power bank or a 3×AA pack. The on-board LED blinks every 2 s to show it's
alive.

**Detector**: ESP8266 with a built-in 0.96" OLED, plus:

| Part | Pin (GPIO / label) | Notes |
|------|--------------------|-------|
| OLED | 14 / 12 (built in) | found automatically at boot |
| Green LED | GPIO 5 (D1) | 220–330 Ω resistor to GND |
| Yellow LED | GPIO 4 (D2) | 220–330 Ω resistor to GND |
| Red LED | GPIO 13 (D7) | 220–330 Ω resistor to GND |
| Passive buzzer | GPIO 15 (D8) | 100 Ω series resistor; a transistor if it's a big magnetic one |
| "Found it!" button (later) | GPIO 16 (D0) | to 3.3 V, with a 10 kΩ pull-down |

The buzzer must be **passive** (no built-in oscillator) to play
different tones. An active buzzer only does one beep. If you're not sure,
put 3.3 V across it: an active one beeps, a passive one only clicks.

Avoid GPIO 0, 2 and 15 for buttons. They decide how the board boots.
GPIO 15 is fine for the buzzer.

## Build plan

1. **Signal on screen** ✅ *code ready*
   `bug_beacon` broadcasts, and `bug_detector` shows the signal strength,
   a bar and the zone name. Walk around and note the numbers right next
   to the bug, across the room, in the next room and at the far end of
   the house. Set `HOT_RSSI` / `WARM_RSSI` from those.
2. **Traffic lights**: green / yellow / red LEDs follow the zone.
3. **Geiger-counter buzzer**: three tones, and the beeps speed up as the
   signal gets stronger. Add a mute switch for quiet games.
4. **Game mode**:
   - several bugs (`SPYBUG-1`, `-2`, `-3`), each with its own beacon
     board; the detector tracks the closest one not yet found
   - press "Found it!" when the signal is very strong to capture a bug,
     then "BUG 2 CAPTURED"
   - mission timer and best time on the OLED
   - difficulty: lower `TX_POWER_DBM` on a bug to make it fainter
5. **Boxing**: detector in a handheld box with the screen window, three
   LEDs and the button on the lid, and the buzzer behind a few small
   holes. The bug goes in a tiny box with its battery so it's easy to
   hide.

### Ideas for later
- **Two-way bug**: the bug notices it's being scanned at close range and
  flashes or chirps back.
- **Signal graph** on the OLED: the last few seconds as a little
  oscilloscope line.
- **Directional antenna**: half a tin can or foil-lined cup behind the
  detector, so it acts like a "radar dish".
- **Disarm sequence**: when the bug is captured, it plays a sound or
  shows a code to type into your security card reader.

## Uploading

Arduino IDE, board **NodeMCU 1.0 (ESP-12E Module)** or **Generic ESP8266
Module**. The detector needs the **Adafruit SSD1306** and **Adafruit GFX
Library** libraries. Open each `.ino` from its own folder. Serial Monitor
at 115200 baud shows what's happening on both boards.

If you change `BUG_SSID` or `BUG_CHANNEL`, change it in **both**
sketches.
