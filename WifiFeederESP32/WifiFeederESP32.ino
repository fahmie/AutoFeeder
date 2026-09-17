/*
 * WifiFeederESP32.ino
 * -------------------------------------------------------------
 * Port ESP32 untuk WifiFeeder.ino (asalnya untuk ESP8266).
 * Guna WiFi + masa internet (NTP) UNTUK JADUAL, dan Blynk UNTUK APP FON
 * (butang "Feed Now" dari mana-mana).
 *
 * Servo: SG90 180 degree (positional biasa) - buka ke DOOR_OPEN, tahan,
 * tutup balik ke DOOR_CLOSED. Laras sudut ikut mekanikal flap/pintu anda.
 *
 * Kelakuan:
 *   - Sambung WiFi, dapatkan masa sebenar (waktu Malaysia UTC+8), sambung Blynk.
 *   - Pintu TUTUP (DOOR_CLOSED) semasa tidak memberi makan.
 *   - Setiap hari 07:00 dan 19:00:
 *       ulang 3 kali { buka DOOR_OPEN -> tahan -> tutup DOOR_CLOSED }
 *   - Butang GPIO 14 (fizikal) -> beri makan segera.
 *   - Butang "Feed Now" (V0) dalam app Blynk -> beri makan segera dari fon.
 *   - LED status (GPIO 2 default) -> nyala = WiFi OK, berkelip = tengah sambung.
 *   Sensor HC-SR04 -> ukur tahap dedak dalam tangki setiap 60 saat,
 *     hantar peratus (%) ke Blynk (V1) + Serial. Amaran kalau < 15%.
 *
 * SAMBUNGAN (ESP32):
 *   Servo SIG (oren)  -> GPIO 13
 *   Servo VCC (merah) -> pin 5V / VIN ESP32   (BUKAN 3V3! - servo tarik lebih arus)
 *   Servo GND (coklat)-> pin GND ESP32
 *   Butang (module push button, 3 kaki):
 *     S (Signal) -> GPIO 14
 *     + (VCC)    -> 3V3 ESP32   (BUKAN 5V/VIN - GPIO ESP32 logic 3.3V je,
 *                                5V boleh rosakkan pin!)
 *     - (GND)    -> GND ESP32
 *     (Module ni ada pull-down sendiri: diam=LOW, tekan=HIGH - INPUT biasa)
 *   Sensor HC-SR04 (ukur tahap dedak dalam tangki):
 *     VCC  -> 5V / VIN ESP32   (HC-SR04 perlukan 5V, tak stabil kat 3.3V)
 *     GND  -> GND ESP32
 *     Trig -> GPIO 26          (boleh sambung terus, tiada isu voltan)
 *     Echo -> GPIO 27 MELALUI VOLTAGE DIVIDER (⚠️ WAJIB):
 *       Echo --[R1 1k]-- (node ke GPIO27) --[R2 2k]-- GND
 *       (Echo output 5V, GPIO ESP32 logic 3.3V je - tanpa divider ni
 *        boleh rosakkan pin GPIO27!)
 *     Letak sensor di ATAS tangki, mengadap ke bawah ke arah dedak.
 *
 * NOTA KUASA:
 *   SG90 kecil OK dari VIN/5V semasa ujian (USB-C ESP32 cukup).
 *   Untuk servo besar / >1 servo, guna bekalan 5V berasingan;
 *   sambungkan GND bekalan itu ke GND ESP32 juga (common ground).
 *
 * SEBELUM UPLOAD:
 *   1. Salin "secrets.h.example" -> "secrets.h" (folder yang sama), isi
 *      WIFI_SSID_VAL, WIFI_PASS_VAL, BLYNK_TEMPLATE_ID_VAL,
 *      BLYNK_TEMPLATE_NAME_VAL, BLYNK_AUTH_TOKEN_VAL dengan nilai sebenar
 *      (dari akaun Blynk kau - lihat langkah setup Blynk dalam README.md
 *      projek ni, bahagian "3. Setup Blynk"). "secrets.h" di-gitignore -
 *      JANGAN push fail tu ke GitHub.
 *   3. Install library "ESP32Servo" DAN "Blynk" (by Volodymyr Shymanskyy)
 *      (Sketch -> Include Library -> Manage Libraries).
 *   4. Dalam app Blynk, buat DUA Datastream:
 *      - V0: Virtual Pin, Integer, Min 0 Max 1 - widget Button (mode "Push"),
 *        label "Feed Now".
 *      - V1: Virtual Pin, Integer, Min 0 Max 100 - widget Gauge/Value Display,
 *        label "Tahap Dedak (%)".
 *   5. Board: "ESP32 Dev Module". Port: ikut COM yang muncul (contoh COM6).
 *   6. LARAS DOOR_OPEN / DOOR_CLOSED ikut sudut sebenar mekanikal flap anda.
 *   7. LARAS TANK_EMPTY_CM / TANK_FULL_CM: ukur jarak sebenar sensor ke
 *      dasar tangki (kosong) dan ke permukaan dedak bila tangki baru diisi
 *      penuh (guna pembaris/measuring tape), isi nilai tu dalam kod.
 * -------------------------------------------------------------
 */

/* ---- WiFi + Blynk: nilai sebenar dalam secrets.h (JANGAN commit fail tu) ----
 * Kalau "secrets.h" tak wujud lagi: salin "secrets.h.example" jadi
 * "secrets.h" (folder yang sama), isi nilai sebenar kau.
 */
#include "secrets.h"

#define BLYNK_TEMPLATE_ID   BLYNK_TEMPLATE_ID_VAL
#define BLYNK_TEMPLATE_NAME BLYNK_TEMPLATE_NAME_VAL
#define BLYNK_AUTH_TOKEN    BLYNK_AUTH_TOKEN_VAL
#define BLYNK_PRINT Serial

#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <time.h>
#include <ESP32Servo.h>

// ====== Diambil dari secrets.h ======
const char* WIFI_SSID = WIFI_SSID_VAL;
const char* WIFI_PASS = WIFI_PASS_VAL;
// =====================================

// ---------- Pin ----------
const int SERVO_PIN  = 13;
const int BUTTON_PIN = 14;
const int TRIG_PIN   = 26;
const int ECHO_PIN   = 27;   // MELALUI voltage divider (5V->3.3V), lihat wiring di atas

#ifndef LED_BUILTIN
#define LED_BUILTIN 2   // kebanyakan board ESP32 Dev Module guna GPIO2
#endif

// ---------- Kalibrasi tangki dedak (HC-SR04) ----------
// Ukur jarak sebenar (cm) dari sensor ke dasar tangki (kosong) dan ke
// permukaan dedak bila tangki baru diisi penuh - laras ikut tangki anda.
const float TANK_EMPTY_CM = 20.0;   // jarak bila tangki KOSONG
const float TANK_FULL_CM  = 3.0;    // jarak bila tangki PENUH
const unsigned long LEVEL_CHECK_MS = 5000UL;    // check tahap dedak setiap 5 saat (testing - naikkan balik ke 60000+ untuk guna harian)
const int LEVEL_WARNING_PERCENT    = 15;        // amaran bila bawah 15%

// ---------- Sudut pintu (servo positional 180) ----------
const int DOOR_CLOSED = 90;    // pintu tutup (sudut default)
const int DOOR_OPEN   = 150;   // pintu buka

const unsigned long OPEN_HOLD_MS = 1000UL;   // berapa lama pintu terbuka setiap kali
const unsigned long PAUSE_MS     = 400UL;    // jeda antara setiap bukaan
const int PORTIONS               = 1;        // ulang 1 kali

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

int lastReading = LOW, stableState = LOW;   // module push button: diam=LOW, tekan=HIGH
unsigned long lastDebounce = 0;
const unsigned long DEBOUNCE_MS = 50;

// Ukur jarak (cm) guna HC-SR04. Pulang -1 kalau gagal baca (timeout).
float readDistanceCm() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  unsigned long durationUs = pulseIn(ECHO_PIN, HIGH, 30000UL);   // timeout 30ms (~5m)
  if (durationUs == 0) return -1.0f;
  return durationUs * 0.0343f / 2.0f;   // kelajuan bunyi ~343 m/s
}

// Tukar jarak jadi peratus tahap dedak (0% kosong - 100% penuh). Pulang -1 kalau gagal baca.
int readTankPercent() {
  float d = readDistanceCm();
  if (d < 0) return -1;
  d = constrain(d, TANK_FULL_CM, TANK_EMPTY_CM);
  float percent = (TANK_EMPTY_CM - d) / (TANK_EMPTY_CM - TANK_FULL_CM) * 100.0f;
  return (int)(percent + 0.5f);
}

void checkTankLevel() {
  int pct = readTankPercent();
  if (pct < 0) {
    Serial.println(F("Sensor tangki: gagal baca (timeout)"));
    return;
  }
  Serial.print(F("Tahap dedak: ")); Serial.print(pct); Serial.println(F("%"));
  if (Blynk.connected()) Blynk.virtualWrite(V1, pct);
  if (pct < LEVEL_WARNING_PERCENT) {
    Serial.println(F("AMARAN: dedak dalam tangki hampir habis!"));
  }
}

void feedSession(const char* sebab) {
  Serial.print(F("=== BERI MAKAN (")); Serial.print(sebab); Serial.println(F(") ==="));
  for (int i = 0; i < PORTIONS; i++) {
    doorServo.write(DOOR_OPEN);
    Serial.print(F("  buka ")); Serial.print(DOOR_OPEN);
    Serial.print(F(" (")); Serial.print(i + 1); Serial.print('/'); Serial.print(PORTIONS); Serial.println(')');
    delay(OPEN_HOLD_MS);
    doorServo.write(DOOR_CLOSED);
    Serial.print(F("  tutup ")); Serial.println(DOOR_CLOSED);
    delay(PAUSE_MS);
  }
  Serial.println(F("=== Selesai ==="));
}

BLYNK_WRITE(V0) {                       // butang "Feed Now" dalam app Blynk
  if (param.asInt() == 1) feedSession("app");
}

bool buttonPressed() {
  int reading = digitalRead(BUTTON_PIN);
  if (reading != lastReading) lastDebounce = millis();
  bool pressed = false;
  if (millis() - lastDebounce > DEBOUNCE_MS) {
    if (reading != stableState) {
      stableState = reading;
      if (stableState == HIGH) pressed = true;   // module push button: tekan = HIGH
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
    digitalWrite(LED_BUILTIN, HIGH);   // LED nyala = WiFi ok
  } else {
    Serial.println(F("WiFi GAGAL - akan cuba semula dalam loop."));
    digitalWrite(LED_BUILTIN, LOW);
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
  Serial.begin(115200);
  delay(200);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  pinMode(BUTTON_PIN, INPUT);   // module push button dah ada pull-down sendiri
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);

  // ESP32Servo: peruntuk timer PWM + attach dengan julat pulse yang betul
  ESP32PWM::allocateTimer(0);
  doorServo.setPeriodHertz(50);
  doorServo.attach(SERVO_PIN, 500, 2400);   // julat pulse SG90
  doorServo.write(DOOR_CLOSED);             // masa ON: pintu tutup

  Serial.println(F("\n=== WifiFeederESP32 sedia ==="));
  Serial.print(F("Jadual: ")); Serial.print(FEED_HOUR_1);
  Serial.print(F(":00 & ")); Serial.print(FEED_HOUR_2); Serial.println(F(":00 (waktu Malaysia)"));
  Serial.print(F("Pintu tutup=")); Serial.print(DOOR_CLOSED);
  Serial.print(F(" buka=")); Serial.print(DOOR_OPEN);
  Serial.print(F(" ulang=")); Serial.println(PORTIONS);

  connectWifi();
  if (WiFi.status() == WL_CONNECTED) {
    syncTime();
    Blynk.config(BLYNK_AUTH_TOKEN);
    Blynk.connect(3000);   // cuba sambung Blynk cloud, timeout 3 saat
  }
}

void loop() {
  // cuba semula WiFi / NTP kalau belum sedia
  static unsigned long lastRetry = 0;
  if ((WiFi.status() != WL_CONNECTED || !timeReady) && millis() - lastRetry > 30000) {
    lastRetry = millis();
    if (WiFi.status() != WL_CONNECTED) connectWifi();
    if (WiFi.status() == WL_CONNECTED && !timeReady) syncTime();
  }

  // ---- Blynk (app fon) ----
  if (WiFi.status() == WL_CONNECTED) {
    if (Blynk.connected()) {
      Blynk.run();
    } else {
      static unsigned long lastBlynkRetry = 0;
      if (millis() - lastBlynkRetry > 10000) {
        lastBlynkRetry = millis();
        Blynk.connect(3000);
      }
    }
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

  // ---- Tahap dedak dalam tangki (HC-SR04) ----
  static unsigned long lastLevelCheck = 0;
  if (millis() - lastLevelCheck >= LEVEL_CHECK_MS) {
    lastLevelCheck = millis();
    checkTankLevel();
  }

  delay(50);
}
