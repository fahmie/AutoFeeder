/*
 * WifiFeederESP32.ino
 * -------------------------------------------------------------
 * Port ESP32 untuk WifiFeeder.ino (asalnya untuk ESP8266).
 * Guna WiFi + masa internet (NTP) UNTUK JADUAL, dan Blynk UNTUK APP FON
 * (butang "Feed Now" dari mana-mana).
 *
 * NOTA PENTING: Servo ni jenis SG90 "360 degree" (continuous-rotation),
 * BUKAN positional. Dicalibrate guna ServoCalibrateESP32.ino:
 *   1500us = STOP (diam)   |   1470us = PUSING (untuk keluarkan makanan)
 * Sebab tu logik dia BUKAN "buka ke sudut X", tapi "PUSING X saat -> STOP".
 *
 * Kelakuan:
 *   - Sambung WiFi, dapatkan masa sebenar (waktu Malaysia UTC+8), sambung Blynk.
 *   - Servo diam (STOP) semasa tidak memberi makan.
 *   - Setiap hari 07:00 dan 19:00:
 *       ulang 3 kali { pusing FEED_SPIN_MS -> stop -> tahan sekejap }
 *   - Butang GPIO 14 (fizikal) -> beri makan segera.
 *   - Butang "Feed Now" (V0) dalam app Blynk -> beri makan segera dari fon.
 *   - LED status (GPIO 2 default) -> nyala = WiFi OK, berkelip = tengah sambung.
 *
 * SAMBUNGAN (ESP32):
 *   Servo SIG (oren)  -> GPIO 13
 *   Servo VCC (merah) -> pin 5V / VIN ESP32   (BUKAN 3V3!)
 *   Servo GND (coklat)-> pin GND ESP32
 *   Butang fizikal    -> GPIO 14 dan GND (INPUT_PULLUP)
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
 *   4. Dalam app Blynk, buat SATU Datastream: Virtual Pin V0, jenis Integer,
 *      Min 0 Max 1. Letak widget Button (mode "Push") kat V0, label "Feed Now".
 *   5. Board: "ESP32 Dev Module". Port: ikut COM yang muncul (contoh COM6).
 *   6. LARAS FEED_SPIN_MS ikut berapa banyak makanan yang keluar bila diuji -
 *      makin lama pusing, makin banyak makanan (bergantung mekanikal auger anda).
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

#ifndef LED_BUILTIN
#define LED_BUILTIN 2   // kebanyakan board ESP32 Dev Module guna GPIO2
#endif

// ---------- Kalibrasi servo continuous-rotation (dari ServoCalibrateESP32) ----------
const int STOP_PULSE_US = 1500;   // servo diam (sudah disahkan)
const int FEED_PULSE_US = 1470;   // servo pusing (sudah disahkan) - untuk keluarkan makanan

const unsigned long FEED_SPIN_MS = 800UL;    // berapa lama pusing setiap kali (laras ikut ujian)
const unsigned long PAUSE_MS     = 400UL;    // jeda antara setiap pusingan
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
    doorServo.writeMicroseconds(FEED_PULSE_US);
    Serial.print(F("  pusing (")); Serial.print(i + 1); Serial.print('/'); Serial.print(PORTIONS); Serial.println(')');
    delay(FEED_SPIN_MS);
    doorServo.writeMicroseconds(STOP_PULSE_US);
    Serial.println(F("  stop"));
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
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // ESP32Servo: peruntuk timer PWM + attach dengan julat pulse yang betul
  ESP32PWM::allocateTimer(0);
  doorServo.setPeriodHertz(50);
  doorServo.attach(SERVO_PIN, 500, 2500);
  doorServo.writeMicroseconds(STOP_PULSE_US);   // masa ON: servo diam

  Serial.println(F("\n=== WifiFeederESP32 sedia ==="));
  Serial.print(F("Jadual: ")); Serial.print(FEED_HOUR_1);
  Serial.print(F(":00 & ")); Serial.print(FEED_HOUR_2); Serial.println(F(":00 (waktu Malaysia)"));
  Serial.print(F("Stop=")); Serial.print(STOP_PULSE_US);
  Serial.print(F("us  Pusing=")); Serial.print(FEED_PULSE_US);
  Serial.print(F("us  Tempoh pusing=")); Serial.print(FEED_SPIN_MS);
  Serial.print(F("ms  ulang=")); Serial.println(PORTIONS);

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

  delay(50);
}
