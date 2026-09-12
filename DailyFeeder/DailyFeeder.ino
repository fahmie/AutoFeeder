/*
 * DailyFeeder.ino
 * -------------------------------------------------------------
 * Fish Feeder - selang 8 jam guna millis() (TIADA RTC diperlukan), 3x sehari.
 *
 * CARA GUNA:
 *   Hidupkan feeder TEPAT pada pukul 7:00 (pagi ATAU malam).
 *     -> beri makan sekali (selepas ~10 saat)
 *     -> 8 jam kemudian beri makan lagi
 *     -> berulang (7am -> 3pm -> 11pm -> 7am -> ...)
 *
 *   Kalau kuasa terputus, hidupkan semula pada pukul 7 (pagi/malam).
 *   Jam Arduino hanyut ~1-2 minit seminggu - betulkan sekali-sekala.
 *
 * Sambungan (Arduino Uno):
 *   Servo SIG (oren)  -> D9      (servo POSITIONAL, cth SG90)
 *   Servo VCC (merah) -> 5V
 *   Servo GND (coklat)-> GND
 *   Butang (pilihan)  -> D2 dan GND (INPUT_PULLUP)
 *
 * feed = ulang PORTIONS kali { buka DOOR_OPEN -> tahan OPEN_HOLD_MS -> tutup DOOR_CLOSED }
 *
 * Laras di bawah:
 *   DOOR_CLOSED / DOOR_OPEN  - sudut pintu
 *   OPEN_HOLD_MS             - lama pintu terbuka setiap ulangan
 *   PORTIONS                 - bilangan ulangan setiap sesi makan
 *   FEED_INTERVAL_MS         - selang antara makan (default 12 jam)
 *   FIRST_FEED_MS            - lengah sebelum makan pertama (default 10 saat)
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
const int PORTIONS               = 1;        // ulang 1 kali

// ---------- Masa ----------
const unsigned long FEED_INTERVAL_MS = 8UL * 60UL * 60UL * 1000UL;  // 8 jam = 3x sehari (guna sebenar). Ujian: 5UL * 60UL * 1000UL = 5 minit
const unsigned long FIRST_FEED_MS    = 10UL * 1000UL;                // 10 saat selepas ON

// ---------- Keadaan ----------
Servo doorServo;
unsigned long nextFeedAt = 0;

int lastReading = HIGH, stableState = HIGH;
unsigned long lastDebounce = 0;
const unsigned long DEBOUNCE_MS = 50;

void feedSession(const char *sebab) {
  Serial.print(F("=== BERI MAKAN (")); Serial.print(sebab); Serial.println(F(") ==="));
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
  nextFeedAt = millis() + FEED_INTERVAL_MS;
  Serial.println(F("Makan seterusnya: 8 jam lagi."));
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

  nextFeedAt = FIRST_FEED_MS;

  Serial.println(F("=== DailyFeeder (selang 8 jam, 3x sehari) sedia ==="));
  Serial.println(F("Hidupkan feeder pada pukul 7:00 (pagi/malam)."));
  Serial.print(F("Pintu tutup=")); Serial.print(DOOR_CLOSED);
  Serial.print(F(" buka=")); Serial.print(DOOR_OPEN);
  Serial.print(F(" ulang=")); Serial.println(PORTIONS);
  Serial.println(F("Makan pertama: ~10 saat lagi."));
}

void loop() {
  // makan berjadual (8 jam) - tolak bertanda supaya selamat dari overflow millis()
  if ((long)(millis() - nextFeedAt) >= 0) {
    feedSession("jadual 8 jam");
  }

  // butang manual
  if (buttonPressed()) {
    feedSession("butang");
  }

  // papar baki masa setiap 60 saat
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint >= 60000UL) {
    lastPrint = millis();
    long sisaMin = (long)(nextFeedAt - millis()) / 60000L;
    if (sisaMin < 0) sisaMin = 0;
    Serial.print(F("Baki ke makan seterusnya: "));
    Serial.print(sisaMin);
    Serial.println(F(" minit"));
  }
}
