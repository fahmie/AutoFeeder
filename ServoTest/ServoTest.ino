/*
 * DoorFeeder.ino
 * -------------------------------------------------------------
 * PERLU SERVO POSITIONAL (SG90 / MG996R / MG995 positional).
 * BUKAN untuk servo putaran berterusan (360).
 *
 * Kelakuan:
 *   - Default (dan masa switch ON): servo di 90  -> PINTU TUTUP (dedak tak keluar)
 *   - Setiap 1 minit: servo gerak ke 95 -> PINTU BUKA
 *       tahan 3 saat (dedak keluar)
 *       kemudian balik ke 90 -> PINTU TUTUP
 *   - Butang D2 (pilihan): buka pintu segera (manual), guna kelakuan sama
 *
 * Laras di bawah:
 *   DOOR_CLOSED   - sudut pintu tutup (default 90)
 *   DOOR_OPEN     - sudut pintu buka  (default 95)
 *   FEED_INTERVAL_MS - selang antara buka (default 60000 = 1 minit)
 *   OPEN_HOLD_MS  - lama pintu terbuka (default 3000 = 3 saat)
 *
 * Sambungan (Arduino Uno):
 *   Servo SIG (oren)  -> D9
 *   Servo VCC (merah) -> 5V (atau bekalan luar 5-6V untuk servo besar)
 *   Servo GND (coklat)-> GND (common ground jika guna bekalan luar)
 *   Butang (pilihan)  -> D2 dan GND (INPUT_PULLUP)
 * -------------------------------------------------------------
 */

#include <Servo.h>

// ---------- Pin ----------
const int SERVO_PIN  = 9;
const int BUTTON_PIN = 2;

// ---------- Tetapan ----------
const int DOOR_CLOSED = 90;    // pintu tutup (sudut default)
const int DOOR_OPEN   = 95;    // pintu buka

const unsigned long FEED_INTERVAL_MS = 60000UL;   // 1 minit
const unsigned long OPEN_HOLD_MS     = 3000UL;    // 3 saat pintu terbuka

// ---------- Keadaan ----------
Servo doorServo;
unsigned long lastFeed = 0;

int lastReading = HIGH;
int stableState = HIGH;
unsigned long lastDebounce = 0;
const unsigned long DEBOUNCE_MS = 50;

void openThenClose(const char *sebab) {
  Serial.print(F("Buka pintu (sebab: "));
  Serial.print(sebab);
  Serial.println(F(")"));

  doorServo.write(DOOR_OPEN);
  delay(OPEN_HOLD_MS);            // tahan 3 saat, dedak keluar

  doorServo.write(DOOR_CLOSED);
  Serial.println(F("Pintu tutup balik."));

  lastFeed = millis();
}

bool buttonPressed() {
  int reading = digitalRead(BUTTON_PIN);
  if (reading != lastReading) lastDebounce = millis();
  bool pressed = false;
  if (millis() - lastDebounce > DEBOUNCE_MS) {
    if (reading != stableState) {
      stableState = reading;
      if (stableState == LOW) pressed = true;   // INPUT_PULLUP: LOW = ditekan
    }
  }
  lastReading = reading;
  return pressed;
}

void setup() {
  Serial.begin(9600);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  doorServo.attach(SERVO_PIN);
  doorServo.write(DOOR_CLOSED);      // masa ON: pintu tutup

  Serial.println(F("=== DoorFeeder sedia ==="));
  Serial.print(F("Pintu tutup = ")); Serial.print(DOOR_CLOSED);
  Serial.print(F("  Pintu buka = ")); Serial.println(DOOR_OPEN);
  Serial.print(F("Selang buka = ")); Serial.print(FEED_INTERVAL_MS / 1000);
  Serial.println(F(" saat"));

  lastFeed = millis();
}

void loop() {
  // 1. Buka berjadual setiap 1 minit
  if (millis() - lastFeed >= FEED_INTERVAL_MS) {
    openThenClose("jadual 1 minit");
  }

  // 2. Butang manual (pilihan)
  if (buttonPressed()) {
    openThenClose("butang");
  }
}
