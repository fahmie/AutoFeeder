/*
 * FeedTest.ino
 * -------------------------------------------------------------
 * Untuk SERVO PUTARAN BERTERUSAN (360) - contoh MG995 360 / SG90 360.
 *
 *   write(90)  = BERHENTI
 *   write(0)   = pusing laju arah A
 *   write(180) = pusing laju arah B
 *
 * Fungsi:
 *   - Tekan BUTANG di D2  -> servo pusing DISPENSE_MS milisaat,
 *                            kemudian BERHENTI. (satu hidangan makanan)
 *   - Serial Monitor (baud 9600, Line ending = New Line) untuk laras:
 *
 *       g   = keluarkan makanan sekarang (sama macam tekan butang)
 *       s   = berhenti serta-merta
 *       +   = tambah masa pusing 100 ms
 *       -   = kurang masa pusing 100 ms
 *       f   = arah A (forward)
 *       b   = arah B (backward)
 *       [   = kurang kelajuan 10
 *       ]   = tambah kelajuan 10
 *       n<nombor> = set masa pusing terus, cth "n1200"  -> 1200 ms
 *       t<nombor> = set nilai BERHENTI (trim), cth "t90"
 *       p   = papar tetapan
 *
 * Sambungan (Arduino Uno):
 *   Servo SIG (oren)  -> D9
 *   Servo VCC (merah) -> bekalan luar 5-6V  (MG995 arus tinggi!)
 *   Servo GND (coklat)-> GND bekalan luar + GND Arduino (common ground)
 *   Butang            -> D2 dan GND (INPUT_PULLUP)
 * -------------------------------------------------------------
 */

#include <Servo.h>

// ---------- Pin ----------
const int SERVO_PIN  = 9;
const int BUTTON_PIN = 2;

// ---------- Tetapan boleh laras ----------
int stopValue    = 90;      // nilai "berhenti" (trim). 90 biasa.
int spinSpeed    = 90;      // 0..90 : jarak dari stopValue (90 = paling laju)
int spinDir      = 1;       // 1 = arah A, -1 = arah B
unsigned long dispenseMs = 800;   // tempoh pusing setiap hidangan

// ---------- Keadaan ----------
Servo feeder;

int  lastReading  = HIGH;
int  stableState  = HIGH;
unsigned long lastDebounce = 0;
const unsigned long DEBOUNCE_MS = 50;

long num = -1;

int spinValue() {
  int v = stopValue + spinDir * spinSpeed;
  return constrain(v, 0, 180);
}

void servoStop() {
  feeder.write(stopValue);
}

void printSettings() {
  Serial.println(F("---- FeedTest ----"));
  Serial.print(F("  Nilai berhenti (trim) : ")); Serial.println(stopValue);
  Serial.print(F("  Kelajuan (0-90)       : ")); Serial.println(spinSpeed);
  Serial.print(F("  Arah                  : ")); Serial.println(spinDir > 0 ? F("A (f)") : F("B (b)"));
  Serial.print(F("  Masa pusing (ms)      : ")); Serial.println(dispenseMs);
  Serial.print(F("  -> write() masa pusing: ")); Serial.println(spinValue());
  Serial.println(F("  Arahan: g s + - f b [ ] n<ms> t<trim> p"));
  Serial.println(F("------------------"));
}

void dispense(const char *sebab) {
  Serial.print(F("Keluar makanan ("));
  Serial.print(sebab);
  Serial.print(F(") - pusing "));
  Serial.print(dispenseMs);
  Serial.println(F(" ms"));

  feeder.write(spinValue());
  delay(dispenseMs);
  servoStop();

  Serial.println(F("  -> berhenti."));
}

bool buttonPressed() {
  int reading = digitalRead(BUTTON_PIN);
  if (reading != lastReading) lastDebounce = millis();
  bool pressed = false;
  if (millis() - lastDebounce > DEBOUNCE_MS) {
    if (reading != stableState) {
      stableState = reading;
      if (stableState == LOW) pressed = true;   // INPUT_PULLUP
    }
  }
  lastReading = reading;
  return pressed;
}

void handleSerial() {
  while (Serial.available()) {
    char c = Serial.read();

    if (c >= '0' && c <= '9') {
      if (num < 0) num = 0;
      num = num * 10 + (c - '0');
      continue;
    }

    switch (c) {
      case 'g': case 'G': dispense("serial g"); break;
      case 's': case 'S': servoStop(); Serial.println(F("BERHENTI")); break;

      case '+': dispenseMs += 100; Serial.print(F("Masa pusing = ")); Serial.println(dispenseMs); break;
      case '-': dispenseMs = (dispenseMs > 100) ? dispenseMs - 100 : 0;
                Serial.print(F("Masa pusing = ")); Serial.println(dispenseMs); break;

      case 'f': case 'F': spinDir = 1;  Serial.println(F("Arah = A")); break;
      case 'b': case 'B': spinDir = -1; Serial.println(F("Arah = B")); break;

      case '[': spinSpeed = constrain(spinSpeed - 10, 0, 90);
                Serial.print(F("Kelajuan = ")); Serial.println(spinSpeed); break;
      case ']': spinSpeed = constrain(spinSpeed + 10, 0, 90);
                Serial.print(F("Kelajuan = ")); Serial.println(spinSpeed); break;

      case 'n': case 'N':
        if (num >= 0) { dispenseMs = num; Serial.print(F("Masa pusing = ")); Serial.println(dispenseMs); }
        break;
      case 't': case 'T':
        if (num >= 0) { stopValue = constrain((int)num, 0, 180); servoStop();
                        Serial.print(F("Nilai berhenti = ")); Serial.println(stopValue); }
        break;

      case 'p': case 'P': printSettings(); break;
      default: break;
    }
    num = -1;
  }
}

void setup() {
  Serial.begin(9600);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  feeder.attach(SERVO_PIN);
  servoStop();

  Serial.println(F("=== FeedTest (servo putaran berterusan) ==="));
  Serial.println(F("Tekan butang D2 -> pusing sekejap -> berhenti."));
  Serial.println(F("Set Line ending = New Line untuk arahan nombor."));
  printSettings();
}

void loop() {
  handleSerial();
  if (buttonPressed()) dispense("butang");
}
