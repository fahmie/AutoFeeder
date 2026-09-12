/*
 * HoldSpin.ino
 * -------------------------------------------------------------
 * Untuk SERVO PUTARAN BERTERUSAN (360).
 *
 *   Tekan & TAHAN butang D2  -> servo pusing.
 *   Lepas butang             -> servo berhenti.
 *
 * Serial Monitor (baud 9600, Line ending = New Line) untuk laras:
 *   f          = arah A
 *   b          = arah B
 *   [          = perlahankan
 *   ]          = lajukan
 *   t<nombor>  = set nilai BERHENTI (trim), cth "t90"
 *   p          = papar tetapan
 *
 * Sambungan (Arduino Uno):
 *   Servo SIG (oren)  -> D9
 *   Servo VCC (merah) -> bekalan luar 5-6V
 *   Servo GND (coklat)-> GND bekalan luar + GND Arduino (common ground)
 *   Butang            -> D2 dan GND (INPUT_PULLUP)
 * -------------------------------------------------------------
 */

#include <Servo.h>

const int SERVO_PIN  = 9;
const int BUTTON_PIN = 2;

int stopValue = 90;    // nilai berhenti (trim)
int spinSpeed = 90;    // 0..90 (90 = paling laju)
int spinDir   = 1;     // 1 = arah A, -1 = arah B

Servo feeder;
long num = -1;
bool spinningNow = false;

int spinValue() {
  return constrain(stopValue + spinDir * spinSpeed, 0, 180);
}

void printSettings() {
  Serial.println(F("---- HoldSpin ----"));
  Serial.print(F("  Nilai berhenti : ")); Serial.println(stopValue);
  Serial.print(F("  Kelajuan (0-90): ")); Serial.println(spinSpeed);
  Serial.print(F("  Arah           : ")); Serial.println(spinDir > 0 ? F("A") : F("B"));
  Serial.print(F("  write() pusing  : ")); Serial.println(spinValue());
  Serial.println(F("  Arahan: f b [ ] t<trim> p"));
  Serial.println(F("------------------"));
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
      case 'f': case 'F': spinDir = 1;  Serial.println(F("Arah = A")); break;
      case 'b': case 'B': spinDir = -1; Serial.println(F("Arah = B")); break;
      case '[': spinSpeed = constrain(spinSpeed - 10, 0, 90);
                Serial.print(F("Kelajuan = ")); Serial.println(spinSpeed); break;
      case ']': spinSpeed = constrain(spinSpeed + 10, 0, 90);
                Serial.print(F("Kelajuan = ")); Serial.println(spinSpeed); break;
      case 't': case 'T':
        if (num >= 0) { stopValue = constrain((int)num, 0, 180);
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
  feeder.write(stopValue);          // mula: berhenti

  Serial.println(F("=== HoldSpin (servo putaran berterusan) ==="));
  Serial.println(F("Tahan butang D2 -> pusing. Lepas -> berhenti."));
  printSettings();
}

void loop() {
  handleSerial();

  // LOW = butang ditekan (INPUT_PULLUP)
  bool pressed = (digitalRead(BUTTON_PIN) == LOW);

  if (pressed && !spinningNow) {
    feeder.write(spinValue());
    spinningNow = true;
    Serial.println(F("pusing..."));
  } else if (!pressed && spinningNow) {
    feeder.write(stopValue);
    spinningNow = false;
    Serial.println(F("berhenti."));
  }
}
