/*
 * TestFeeder.ino
 * -------------------------------------------------------------
 * VERSI UJIAN - TIADA RTC. Guna kiraan detik (millis()).
 *
 * Servo POSITIONAL di D9.
 *
 * Kelakuan:
 *   - Masa switch ON: pintu TUTUP di 90.
 *   - FIRST_FEED_S saat selepas ON  -> feed sekali.
 *   - Kemudian setiap REPEAT_S saat  -> feed lagi (berulang).
 *   - Butang D2                       -> feed segera bila-bila.
 *
 *   feed = ulang 3 kali { buka 120 -> tahan 1 saat -> tutup 90 }
 *
 * Serial Monitor (baud 9600) akan kira detik & beritahu bila feed seterusnya.
 * -------------------------------------------------------------
 */

#include <Servo.h>

// ---------- Pin ----------
const int SERVO_PIN  = 9;
const int BUTTON_PIN = 2;

// ---------- Sudut pintu ----------
const int DOOR_CLOSED = 90;
const int DOOR_OPEN   = 150;
const unsigned long OPEN_HOLD_MS = 1000UL;   // 1 saat terbuka
const int PORTIONS               = 3;        // ulang 3 kali

// ---------- Masa ujian (saat) ----------
const unsigned long FIRST_FEED_S = 15;       // feed kali pertama: 15 saat selepas ON
const unsigned long REPEAT_S     = 60;       // lepas tu feed setiap 60 saat

// ---------- Keadaan ----------
Servo doorServo;
unsigned long nextFeedMs = 0;

int lastReading = HIGH, stableState = HIGH;
unsigned long lastDebounce = 0;
const unsigned long DEBOUNCE_MS = 50;

void feedSession(const char *sebab) {
  Serial.print(F("=== FEED (")); Serial.print(sebab); Serial.println(F(") ==="));
  for (int i = 0; i < PORTIONS; i++) {
    doorServo.write(DOOR_OPEN);
    Serial.print(F("  buka ")); Serial.print(DOOR_OPEN);
    Serial.print(F(" (")); Serial.print(i + 1); Serial.print('/'); Serial.print(PORTIONS); Serial.println(')');
    delay(OPEN_HOLD_MS);
    doorServo.write(DOOR_CLOSED);
    Serial.print(F("  tutup ")); Serial.println(DOOR_CLOSED);
    delay(400);
  }
  Serial.println(F("=== Selesai ==="));
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

void setup() {
  Serial.begin(9600);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  doorServo.attach(SERVO_PIN);
  doorServo.write(DOOR_CLOSED);          // masa ON: pintu tutup

  nextFeedMs = FIRST_FEED_S * 1000UL;

  Serial.println(F("=== TestFeeder (kira detik) ==="));
  Serial.print(F("Feed pertama dalam ")); Serial.print(FIRST_FEED_S); Serial.println(F(" saat"));
  Serial.print(F("Lepas tu setiap ")); Serial.print(REPEAT_S); Serial.println(F(" saat"));
  Serial.print(F("Pintu tutup=")); Serial.print(DOOR_CLOSED);
  Serial.print(F(" buka=")); Serial.print(DOOR_OPEN);
  Serial.print(F(" ulang=")); Serial.println(PORTIONS);
}

void loop() {
  unsigned long ms = millis();

  // feed berjadual
  if (ms >= nextFeedMs) {
    feedSession("jadual");
    nextFeedMs = millis() + REPEAT_S * 1000UL;
  }

  // butang manual
  if (buttonPressed()) {
    feedSession("butang");
    nextFeedMs = millis() + REPEAT_S * 1000UL;
  }

  // papar kiraan detik setiap 5 saat
  static unsigned long lastPrint = 0;
  if (ms - lastPrint >= 5000) {
    lastPrint = ms;
    long sisa = (long)(nextFeedMs - millis()) / 1000L;
    if (sisa < 0) sisa = 0;
    Serial.print(F("Hidup ")); Serial.print(ms / 1000);
    Serial.print(F(" saat | feed seterusnya dalam ")); Serial.print(sisa);
    Serial.println(F(" saat"));
  }
}
