/*
 * WifiFeeder.ino
 * -------------------------------------------------------------
 * Fish Feeder untuk NodeMCU / Wemos LoLin V3 (ESP8266).
 * TIADA Arduino Uno. TIADA RTC. Guna WiFi + masa internet (NTP).
 *
 * Kelakuan:
 *   - Sambung WiFi, dapatkan masa sebenar (waktu Malaysia UTC+8).
 *   - Pintu TUTUP di 90 (default).
 *   - Setiap hari 07:00 dan 19:00:
 *       ulang 3 kali { buka 150 -> tahan 1 saat -> tutup 90 }
 *   - Butang D6 (pilihan) -> beri makan segera.
 *   - Lampu biru board = petunjuk status.
 *
 * SAMBUNGAN (NodeMCU):
 *   Servo SIG (oren)  -> D1
 *   Servo VCC (merah) -> VIN  (5V dari USB)   *lihat nota kuasa di bawah
 *   Servo GND (coklat)-> G (GND)
 *   Butang (pilihan)  -> D6 dan G (GND)
 *
 * NOTA KUASA:
 *   SG90 kecil OK dari VIN semasa ujian. Untuk operasi sebenar / servo besar,
 *   guna bekalan 5V berasingan; sambungkan GND bekalan itu ke GND NodeMCU.
 *
 * SEBELUM UPLOAD:
 *   1. Isi WIFI_SSID dan WIFI_PASS di bawah.
 *   2. Board: "NodeMCU 1.0 (ESP-12E Module)".  Upload speed 115200.
 *   3. Pasang driver CH340 kalau port tak muncul.
 * -------------------------------------------------------------
 */

#include <ESP8266WiFi.h>
#include <time.h>
#include <Servo.h>

// ====== ISI DI SINI ======
const char* WIFI_SSID = "NAMA_WIFI_ANDA";
const char* WIFI_PASS = "KATA_LALUAN_WIFI";
// =========================

// ---------- Pin ----------
const int SERVO_PIN  = D1;
const int BUTTON_PIN = D6;

// ---------- Sudut pintu ----------
const int DOOR_CLOSED = 90;
const int DOOR_OPEN   = 150;
const unsigned long OPEN_HOLD_MS = 1000UL;   // 1 saat
const int PORTIONS               = 3;        // ulang 3 kali

// ---------- Jadual (jam 24, waktu Malaysia) ----------
const int FEED_HOUR_1 = 7;    // 07:00 pagi
const int FEED_HOUR_2 = 19;   // 19:00 (7 malam)
const int FEED_MINUTE = 0;

// ---------- Zon waktu ----------
const long GMT_OFFSET_SEC = 8 * 3600;   // Malaysia UTC+8
const int  DST_OFFSET_SEC = 0;          // tiada DST

// ---------- Keadaan ----------
Servo doorServo;
int lastFedYday = -1;
int lastFedHour = -1;
bool timeReady = false;

int lastReading = HIGH, stableState = HIGH;
unsigned long lastDebounce = 0;
const unsigned long DEBOUNCE_MS = 50;

void feedSession(const char* sebab) {
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

void connectWifi() {
  Serial.print(F("Sambung WiFi ke "));
  Serial.println(WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(500);
    Serial.print('.');
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print(F("WiFi OK. IP: "));
    Serial.println(WiFi.localIP());
    digitalWrite(LED_BUILTIN, LOW);   // LED nyala = WiFi ok (aktif rendah)
  } else {
    Serial.println(F("WiFi GAGAL - akan cuba semula dalam loop."));
    digitalWrite(LED_BUILTIN, HIGH);
  }
}

void syncTime() {
  configTime(GMT_OFFSET_SEC, DST_OFFSET_SEC, "pool.ntp.org", "time.nist.gov", "time.google.com");
  Serial.print(F("Dapatkan masa NTP"));
  time_t now = time(nullptr);
  unsigned long start = millis();
  while (now < 8 * 3600 * 2 && millis() - start < 15000) {
    delay(500);
    Serial.print('.');
    now = time(nullptr);
  }
  Serial.println();

  if (now > 8 * 3600 * 2) {
    timeReady = true;
    struct tm* t = localtime(&now);
    Serial.printf("Masa sekarang: %02d:%02d:%02d  (%04d-%02d-%02d)\n",
                  t->tm_hour, t->tm_min, t->tm_sec,
                  t->tm_year + 1900, t->tm_mon + 1, t->tm_mday);
  } else {
    Serial.println(F("NTP gagal - akan cuba semula."));
  }
}

void setup() {
  Serial.begin(9600);
  delay(200);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  doorServo.attach(SERVO_PIN);
  doorServo.write(DOOR_CLOSED);      // masa ON: pintu tutup

  Serial.println(F("\n=== WifiFeeder (ESP8266) sedia ==="));
  Serial.print(F("Jadual: ")); Serial.print(FEED_HOUR_1);
  Serial.print(F(":00 & ")); Serial.print(FEED_HOUR_2); Serial.println(F(":00 (waktu Malaysia)"));
  Serial.print(F("Pintu tutup=")); Serial.print(DOOR_CLOSED);
  Serial.print(F(" buka=")); Serial.print(DOOR_OPEN);
  Serial.print(F(" ulang=")); Serial.println(PORTIONS);

  connectWifi();
  if (WiFi.status() == WL_CONNECTED) syncTime();
}

void loop() {
  // cuba semula WiFi / NTP kalau belum sedia
  static unsigned long lastRetry = 0;
  if ((WiFi.status() != WL_CONNECTED || !timeReady) && millis() - lastRetry > 30000) {
    lastRetry = millis();
    if (WiFi.status() != WL_CONNECTED) connectWifi();
    if (WiFi.status() == WL_CONNECTED && !timeReady) syncTime();
  }

  // ---- Jadual ----
  if (timeReady) {
    time_t now = time(nullptr);
    struct tm* t = localtime(&now);

    bool jamMakan   = (t->tm_hour == FEED_HOUR_1 || t->tm_hour == FEED_HOUR_2);
    bool minitBetul = (t->tm_min == FEED_MINUTE);
    bool belumMakan = !(t->tm_yday == lastFedYday && t->tm_hour == lastFedHour);

    if (jamMakan && minitBetul && belumMakan) {
      feedSession("jadual");
      lastFedYday = t->tm_yday;
      lastFedHour = t->tm_hour;
    }

    // papar masa setiap 30 saat
    static unsigned long lastPrint = 0;
    if (millis() - lastPrint >= 30000) {
      lastPrint = millis();
      Serial.printf("Masa: %02d:%02d:%02d\n", t->tm_hour, t->tm_min, t->tm_sec);
    }
  }

  // ---- Butang manual ----
  if (buttonPressed()) {
    feedSession("butang");
  }

  delay(50);
}
