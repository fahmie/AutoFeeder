/*
 * FishFeeder.ino
 * -------------------------------------------------------------
 * Projek: Automatic Fish Feeder (Arduino Uno + Servo)
 *
 * Fungsi:
 *   - Beri makan ikan secara berjadual (default: setiap 8 jam / 3x sehari).
 *   - Butang tekan untuk beri makan segera (manual feed).
 *   - Servo pusing untuk buka/tutup pintu bekas makanan.
 *   - LED status berkelip semasa memberi makan.
 *
 * Sambungan (Arduino Uno):
 *   Servo VCC (Merah)        -> 5V (guna bekalan luar 5V jika servo besar)
 *   Servo GND (Coklat/Hitam) -> GND (common ground dengan Uno)
 *   Servo SIG (Oren/Kuning)  -> D9
 *   Butang                   -> D2 dan GND (guna INPUT_PULLUP, tak perlu perintang)
 *   LED status (+ perintang 220R) -> D13 dan GND (D13 = LED atas board pun boleh)
 *
 * Pelarasan:
 *   - FEED_INTERVAL_MS : selang masa antara makan berjadual
 *   - FEED_PORTIONS    : berapa kali servo "goyang" setiap sesi makan
 *   - DOOR_OPEN_ANGLE / DOOR_CLOSED_ANGLE : sudut pintu ikut mekanikal anda
 *
 * NOTA: Arduino Uno tiada jam sebenar (RTC). Masa dikira guna millis(),
 *       jadi ia reset bila kuasa terputus. Untuk jadual tepat harian,
 *       tambah modul RTC DS3231 kemudian.
 * -------------------------------------------------------------
 */

#include <Servo.h>

// ---------- Konfigurasi pin ----------
const int SERVO_PIN  = 9;
const int BUTTON_PIN = 2;
const int LED_PIN    = 13;

// ---------- Konfigurasi kelakuan ----------
const int DOOR_CLOSED_ANGLE = 0;      // pintu tutup
const int DOOR_OPEN_ANGLE   = 90;     // pintu buka
const int FEED_PORTIONS      = 1;     // bilangan goyang setiap sesi
const unsigned long OPEN_HOLD_MS   = 500;   // lama pintu terbuka setiap goyang
const unsigned long CLOSE_HOLD_MS  = 400;   // jeda antara goyang

// Selang makan berjadual (8 jam = 3 kali sehari). Tukar ikut keperluan.
// Contoh: 8 jam = 8UL * 60 * 60 * 1000
const unsigned long FEED_INTERVAL_MS = 8UL * 60UL * 60UL * 1000UL;

// ---------- Pemboleh ubah keadaan ----------
Servo doorServo;
unsigned long lastFeedTime = 0;
int  lastButtonState = HIGH;
unsigned long lastButtonDebounce = 0;
const unsigned long DEBOUNCE_MS = 50;

void blink(int times, int onMs, int offMs) {
  for (int i = 0; i < times; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(onMs);
    digitalWrite(LED_PIN, LOW);
    delay(offMs);
  }
}

void feedNow(const char *reason) {
  Serial.print(F("Beri makan ikan - sebab: "));
  Serial.println(reason);

  digitalWrite(LED_PIN, HIGH);

  for (int i = 0; i < FEED_PORTIONS; i++) {
    doorServo.write(DOOR_OPEN_ANGLE);
    delay(OPEN_HOLD_MS);
    doorServo.write(DOOR_CLOSED_ANGLE);
    delay(CLOSE_HOLD_MS);
    Serial.print(F("  goyang "));
    Serial.print(i + 1);
    Serial.print(F("/"));
    Serial.println(FEED_PORTIONS);
  }

  digitalWrite(LED_PIN, LOW);
  lastFeedTime = millis();
  Serial.println(F("Selesai memberi makan."));
}

bool buttonPressed() {
  int reading = digitalRead(BUTTON_PIN);
  if (reading != lastButtonState) {
    lastButtonDebounce = millis();
  }
  bool pressed = false;
  if ((millis() - lastButtonDebounce) > DEBOUNCE_MS) {
    // butang guna INPUT_PULLUP: LOW = ditekan
    static int stableState = HIGH;
    if (reading != stableState) {
      stableState = reading;
      if (stableState == LOW) pressed = true;
    }
  }
  lastButtonState = reading;
  return pressed;
}

void setup() {
  Serial.begin(9600);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  doorServo.attach(SERVO_PIN);
  doorServo.write(DOOR_CLOSED_ANGLE);   // mula dengan pintu tutup

  Serial.println(F("=== Fish Feeder sedia ==="));
  Serial.print(F("Selang makan berjadual (jam): "));
  Serial.println(FEED_INTERVAL_MS / 3600000.0, 2);
  blink(3, 100, 100);

  // Beri makan sekali semasa mula (pilihan - buang jika tak mahu)
  feedNow("permulaan sistem");
}

void loop() {
  // 1. Makan berjadual
  if (millis() - lastFeedTime >= FEED_INTERVAL_MS) {
    feedNow("jadual");
  }

  // 2. Butang manual
  if (buttonPressed()) {
    feedNow("butang manual");
  }

  // 3. Denyut LED perlahan tanda sistem hidup
  static unsigned long lastHeartbeat = 0;
  if (millis() - lastHeartbeat >= 2000) {
    lastHeartbeat = millis();
    digitalWrite(LED_PIN, HIGH);
    delay(15);
    digitalWrite(LED_PIN, LOW);
  }
}
