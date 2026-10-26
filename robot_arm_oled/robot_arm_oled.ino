/*
  Robot arm animation test for ESP8266 boards with a 0.96" SSD1306 OLED (128x64).

  A little 2-joint robot arm picks up a box, swings it to the other side,
  drops it, and repeats forever.

  Libraries (Arduino IDE > Tools > Manage Libraries):
    - "Adafruit SSD1306"
    - "Adafruit GFX Library"
  Board: "NodeMCU 1.0 (ESP-12E Module)" or "Generic ESP8266 Module".
*/

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <math.h>

#define SCREEN_W 128
#define SCREEN_H 64

Adafruit_SSD1306 display(SCREEN_W, SCREEN_H, &Wire, -1);

// Pin pairs (SDA, SCL) tried at boot, in order. Most ESP8266 boards with a
// built-in OLED use the first one; the rest cover other common wirings.
const uint8_t PIN_PAIRS[][2] = { { 14, 12 }, { 12, 14 }, { 4, 5 }, { 5, 4 } };
const uint8_t ADDRESSES[] = { 0x3C, 0x3D };

// ---- Arm geometry --------------------------------------------------------
const float SHOULDER_X = 64, SHOULDER_Y = 46;
const float L1 = 24;          // upper arm length
const float L2 = 22;          // forearm length
const int GROUND_Y = 63;
const int BOX = 10;           // box size in pixels
const int LEFT_X = 30, RIGHT_X = 98;
const int UP_Y = 26, DOWN_Y = 42;  // wrist heights: travelling / grabbing

struct Keyframe {
  int8_t side;     // -1 = pick side, +1 = drop side
  uint8_t wristY;
  float grip;      // 1 = open, 0 = closed on the box
  bool holding;    // box travels with the gripper after this keyframe
  uint8_t frames;  // frames to move from the previous keyframe
};

const Keyframe SEQ[] = {
  { -1, UP_Y,   1, false, 20 },  // above the box
  { -1, DOWN_Y, 1, false, 16 },  // lower down
  { -1, DOWN_Y, 0, true,  10 },  // grab
  { -1, UP_Y,   0, true,  16 },  // lift
  { +1, UP_Y,   0, true,  34 },  // swing across
  { +1, DOWN_Y, 0, true,  16 },  // lower
  { +1, DOWN_Y, 1, false, 10 },  // release
  { +1, UP_Y,   1, false, 16 },  // back up
};
const int SEQ_LEN = sizeof(SEQ) / sizeof(SEQ[0]);

int boxSide = -1;     // which side the box is resting on (-1 left, +1 right)
int step = 0;         // keyframe we are moving towards
int frame = 0;        // frame within the current step
uint32_t moves = 0;   // how many times the box has been moved

// ---- Helpers ------------------------------------------------------------
float easeInOut(float t) { return t * t * (3 - 2 * t); }

// Keyframes are written for "pick on boxSide, drop on the other side".
int sideX(int side) {
  int s = side * -boxSide;  // pick side == boxSide
  return s < 0 ? LEFT_X : RIGHT_X;
}

// 2-link inverse kinematics. Elbow always bends upward, which makes the arm
// swing nicely over the top when interpolating between the two sides.
void solveIK(float wx, float wy, float &shoulder, float &elbow) {
  float x = wx - SHOULDER_X;
  float y = SHOULDER_Y - wy;  // screen y points down, math y points up
  float c = (x * x + y * y - L1 * L1 - L2 * L2) / (2 * L1 * L2);
  c = constrain(c, -1.0f, 1.0f);
  elbow = acosf(c);
  if (x >= 0) elbow = -elbow;
  shoulder = atan2f(y, x) - atan2f(L2 * sinf(elbow), L1 + L2 * cosf(elbow));
}

void thickLine(int x0, int y0, int x1, int y1) {
  display.drawLine(x0, y0, x1, y1, WHITE);
  display.drawLine(x0 + 1, y0, x1 + 1, y1, WHITE);
  display.drawLine(x0, y0 + 1, x1, y1 + 1, WHITE);
}

void drawBox(int cx, int top) {
  display.drawRect(cx - BOX / 2, top, BOX, BOX, WHITE);
  display.drawLine(cx - BOX / 2, top, cx + BOX / 2 - 1, top + BOX - 1, WHITE);
  display.drawLine(cx + BOX / 2 - 1, top, cx - BOX / 2, top + BOX - 1, WHITE);
}

void drawScene(float shoulder, float elbow, float grip, bool holding) {
  display.clearDisplay();

  // Ground, base and label
  display.drawFastHLine(0, GROUND_Y, SCREEN_W, WHITE);
  display.fillTriangle(54, GROUND_Y, 74, GROUND_Y, 64, SHOULDER_Y + 4, WHITE);
  display.fillRect(52, GROUND_Y - 3, 25, 3, WHITE);
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.print(F("moves:"));
  display.print(moves);

  // Forward kinematics
  float ex = SHOULDER_X + L1 * cosf(shoulder);
  float ey = SHOULDER_Y - L1 * sinf(shoulder);
  float wx = ex + L2 * cosf(shoulder + elbow);
  float wy = ey - L2 * sinf(shoulder + elbow);

  thickLine(SHOULDER_X, SHOULDER_Y, ex, ey);
  thickLine(ex, ey, wx, wy);
  display.fillCircle(SHOULDER_X, SHOULDER_Y, 4, WHITE);
  display.fillCircle(SHOULDER_X, SHOULDER_Y, 1, BLACK);
  display.fillCircle(ex, ey, 3, WHITE);
  display.fillCircle(ex, ey, 1, BLACK);

  // Gripper: always points straight down
  int gx = round(wx), gy = round(wy);
  display.fillRect(gx - 1, gy, 3, 4, WHITE);
  int open = BOX / 2 + 1 + round(grip * 3);
  display.drawFastHLine(gx - open, gy + 4, open * 2 + 1, WHITE);
  display.drawFastVLine(gx - open, gy + 4, 14, WHITE);
  display.drawFastVLine(gx + open, gy + 4, 14, WHITE);
  display.drawPixel(gx - open + 1, gy + 17, WHITE);
  display.drawPixel(gx + open - 1, gy + 17, WHITE);

  // Box: in the gripper, or resting on the ground (pick side until grabbed)
  if (holding) {
    drawBox(gx, gy + 11);
  } else {
    drawBox(sideX(step >= 3 ? +1 : -1), GROUND_Y - BOX);
  }

  display.display();
}

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

// ---- Arduino ------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  delay(100);

  if (!findDisplay()) {
    Serial.println("No OLED found. Check wiring / pins in PIN_PAIRS.");
    while (true) {  // fast blink = no screen found
      digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
      delay(100);
    }
  }

  // Splash screen
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(2);
  display.setCursor(10, 14);
  display.print(F("ROBOT ARM"));
  display.setTextSize(1);
  display.setCursor(34, 40);
  display.print(F("test v1.0"));
  display.display();
  delay(1500);
}

void loop() {
  const Keyframe &from = SEQ[(step + SEQ_LEN - 1) % SEQ_LEN];
  const Keyframe &to = SEQ[step];

  float t = easeInOut((float)frame / to.frames);

  float s0, e0, s1, e1;
  // At step 0 the previous keyframe was the last drop, which is now the pick side
  int fromSide = step == 0 ? -1 : from.side;
  solveIK(sideX(fromSide), from.wristY, s0, e0);
  solveIK(sideX(to.side), to.wristY, s1, e1);

  float shoulder = s0 + (s1 - s0) * t;
  float elbow = e0 + (e1 - e0) * t;
  float grip = from.grip + (to.grip - from.grip) * t;
  bool holding = from.holding && to.holding;

  drawScene(shoulder, elbow, grip, holding);

  if (++frame > to.frames) {
    frame = 0;
    step++;
    if (step == SEQ_LEN) {  // box delivered: next time carry it back
      step = 0;
      boxSide = -boxSide;
      moves++;
    }
  }

  digitalWrite(LED_BUILTIN, (millis() / 500) % 2);  // slow blink = running
  delay(20);
}
