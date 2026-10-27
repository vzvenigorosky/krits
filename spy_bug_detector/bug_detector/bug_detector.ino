/*
  SPY BUG DETECTOR - the detector (step 1: signal on screen)
  Board: ESP8266 with built-in 0.96" SSD1306 OLED.

  Scans the bug's WiFi channel a few times per second and shows how strong
  the bug's signal is. Use it to walk around and note the numbers you get
  far away / in the next room / right next to the bug: those become the
  COLD / WARM / HOT thresholds below.

  Libraries: "Adafruit SSD1306" and "Adafruit GFX Library".
*/

#include <ESP8266WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Must match the beacon.
const char *BUG_SSID = "SPYBUG-1";
const int BUG_CHANNEL = 6;

// Signal strength (RSSI, in dBm: closer to 0 = stronger). Tune after testing.
const int RSSI_MIN = -90;   // bar empty
const int RSSI_MAX = -30;   // bar full
const int HOT_RSSI = -50;   // stronger than this = HOT
const int WARM_RSSI = -65;  // stronger than this = WARM, else COLD

const float SMOOTHING = 0.3;  // 0..1, lower = steadier but slower to react
const int LOST_AFTER = 3;     // missed scans before showing NO SIGNAL

Adafruit_SSD1306 display(128, 64, &Wire, -1);

// (SDA, SCL) pairs tried at boot. Other pins are kept free for LEDs/buzzer.
const uint8_t PIN_PAIRS[][2] = { { 14, 12 }, { 12, 14 } };
const uint8_t ADDRESSES[] = { 0x3C, 0x3D };

float smoothRssi = 0;
int rawRssi = 0;
int missed = LOST_AFTER;

bool findDisplay() {
  for (auto &p : PIN_PAIRS) {
    Wire.begin(p[0], p[1]);
    for (uint8_t addr : ADDRESSES) {
      Wire.beginTransmission(addr);
      if (Wire.endTransmission() == 0) {
        Serial.printf("OLED found: SDA=%d SCL=%d addr=0x%02X\n", p[0], p[1], addr);
        Wire.setClock(400000);
        return display.begin(SSD1306_SWITCHCAPVCC, addr, true, false);
      }
    }
  }
  return false;
}

// Scans only the bug's channel for its SSID (~100 ms instead of ~2 s for a
// full scan). Returns the RSSI, or 0 if the bug wasn't heard.
int scanForBug() {
  int found = 0;
  int n = WiFi.scanNetworks(false, true, BUG_CHANNEL, (uint8 *)BUG_SSID);
  for (int i = 0; i < n; i++) {
    if (WiFi.SSID(i) == BUG_SSID) {
      found = WiFi.RSSI(i);
      break;
    }
  }
  WiFi.scanDelete();
  return found;
}

void drawScreen() {
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(F("BUG DETECTOR "));
  display.print(BUG_SSID);
  display.drawFastHLine(0, 10, 128, WHITE);

  if (missed >= LOST_AFTER) {
    display.setTextSize(2);
    display.setCursor(10, 30);
    display.print(F("NO SIGNAL"));
    display.display();
    return;
  }

  int level = round(smoothRssi);
  const char *zone = level > HOT_RSSI ? "HOT!" : level > WARM_RSSI ? "WARM" : "COLD";

  display.setTextSize(2);
  display.setCursor(0, 16);
  display.print(zone);
  display.setTextSize(1);
  display.setCursor(74, 16);
  display.printf("%d dBm", level);
  display.setCursor(74, 26);
  display.printf("raw %d", rawRssi);

  int w = map(constrain(level, RSSI_MIN, RSSI_MAX), RSSI_MIN, RSSI_MAX, 0, 124);
  display.drawRect(0, 44, 128, 14, WHITE);
  display.fillRect(2, 46, w, 10, WHITE);

  display.display();
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  if (!findDisplay()) {
    Serial.println("No OLED found.");
    while (true) {
      digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
      delay(100);
    }
  }
}

void loop() {
  int rssi = scanForBug();
  if (rssi != 0) {
    rawRssi = rssi;
    smoothRssi = missed >= LOST_AFTER ? rssi : smoothRssi + SMOOTHING * (rssi - smoothRssi);
    missed = 0;
  } else if (missed < LOST_AFTER) {
    missed++;
  }

  Serial.printf("raw=%d smooth=%.1f missed=%d\n", rssi, smoothRssi, missed);
  drawScreen();
}
