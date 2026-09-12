/*
 * DoorFeeder.ino
 * -------------------------------------------------------------
 * PERLU SERVO POSITIONAL (SG90 / MG996R). BUKAN servo 360.
 *
 * Kelakuan (AUTO, tiada butang):
 *   - Masa switch ON        : servo 90  -> PINTU TUTUP (dedak tak keluar)
 *   - Setiap 5 saat         : servo 95  -> PINTU BUKA
 *       tahan 3 saat (dedak keluar)
 *       kemudian balik 90   -> PINTU TUTUP
 *
 * Laras:
 *   DOOR_CLOSED       - sudut pintu tutup (default 90)
 *   DOOR_OPEN         - sudut pintu buka  (default 95)
 *   FEED_INTERVAL_MS  - selang antara buka (default 5000 = 5 saat)
 *   OPEN_HOLD_MS      - lama pintu terbuka (default 3000 = 3 saat)
 *
 * Sambungan (Arduino Uno):
 *   Servo SIG (oren)  -> D9
 *   Servo VCC (merah) -> 5V (bekalan luar 5-6V untuk servo besar)
 *   Servo GND (coklat)-> GND (common ground jika bekalan luar)
 * -------------------------------------------------------------
 */

#include <Servo.h>

// ---------- Pin ----------
const int SERVO_PIN = 9;

// ---------- Tetapan ----------
const int DOOR_CLOSED = 90;    // pintu tutup (sudut default)
const int DOOR_OPEN   = 180;    // pintu buka

const unsigned long FEED_INTERVAL_MS = 5000UL;    // 5 saat
const unsigned long OPEN_HOLD_MS     = 1000UL;    // 3 saat pintu terbuka

// ---------- Keadaan ----------
Servo doorServo;
unsigned long lastFeed = 0;

void openThenClose() {
  Serial.println(F("Buka pintu (auto 5 saat)"));
  doorServo.write(DOOR_OPEN);
  delay(OPEN_HOLD_MS);             // tahan 3 saat, dedak keluar

  doorServo.write(DOOR_CLOSED);
  Serial.println(F("Pintu tutup balik."));

  lastFeed = millis();
}

void setup() {
  Serial.begin(9600);

  doorServo.attach(SERVO_PIN);
  doorServo.write(DOOR_CLOSED);      // masa ON: pintu tutup

  Serial.println(F("=== DoorFeeder (AUTO) sedia ==="));
  Serial.print(F("Pintu tutup = ")); Serial.print(DOOR_CLOSED);
  Serial.print(F("  Pintu buka = ")); Serial.println(DOOR_OPEN);
  Serial.print(F("Selang buka = ")); Serial.print(FEED_INTERVAL_MS / 1000);
  Serial.println(F(" saat"));

  lastFeed = millis();
}

void loop() {
  if (millis() - lastFeed >= FEED_INTERVAL_MS) {
    openThenClose();
  }
}
