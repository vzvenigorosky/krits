/*
  SPY BUG DETECTOR - the hidden "bug" (beacon)
  Board: plain ESP8266 (ESP8266MOD / ESP-12 dev board), no screen needed.

  It just broadcasts its own WiFi network. Nobody connects to it: the
  detector only measures how strong its signal is.
*/

#include <ESP8266WiFi.h>

// Must match the detector.
const char *BUG_SSID = "SPYBUG-1";
const char *BUG_PASS = "topsecret123";  // stops anyone joining; detector doesn't need it
const int BUG_CHANNEL = 6;

// Transmit power in dBm (0 to 20.5). Lower = signal fades faster with
// distance, which makes the "hot/cold" difference clearer in a small house.
const float TX_POWER_DBM = 20.5;

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);

  WiFi.persistent(false);
  WiFi.mode(WIFI_AP);
  WiFi.setOutputPower(TX_POWER_DBM);
  bool ok = WiFi.softAP(BUG_SSID, BUG_PASS, BUG_CHANNEL, false);

  Serial.printf("\nBug %s on channel %d: %s\n", BUG_SSID, BUG_CHANNEL, ok ? "broadcasting" : "FAILED");
}

void loop() {
  // Short blink every 2 s so you know the bug is alive (LED is active-low).
  digitalWrite(LED_BUILTIN, LOW);
  delay(30);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(1970);
}
