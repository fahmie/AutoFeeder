/*
 * =====================================================================
 *  FISH AUTO FEEDER  -  ESP32 + Servo SG90/MG996R + Blynk IoT
 * =====================================================================
 *
 *  Ciri-ciri:
 *   - Jadual makan sampai 6 kali sehari (set masa dari app Blynk)
 *   - Set berapa lama servo buka (saat) + sudut buka servo
 *   - Butang "Feed Now" untuk bagi makan segera dari phone (dari mana-mana)
 *   - Jadual disimpan dalam flash (kekal walau ESP32 restart / putus letrik)
 *   - Guna waktu sebenar Malaysia (NTP, UTC+8). Jadual tetap jalan
 *     walaupun internet putus, ASALKAN ESP32 pernah dapat masa NTP.
 *
 *  Library yang perlu install (Arduino IDE -> Library Manager):
 *   1. "Blynk"        by Volodymyr Shymanskyy
 *   2. "ESP32Servo"   by Kevin Harrington / John K. Bennett
 *
 *  Board: "ESP32 Dev Module"  (install "esp32" by Espressif dari Board Manager)
 *
 *  Wiring (satu servo SG90, satu kabel USB-C je):
 *   Servo signal (oren/kuning) -> GPIO 13
 *   Servo VCC    (merah)       -> pin 5V / VIN  ESP32   (BUKAN 3V3!)
 *   Servo GND    (coklat/hitam)-> pin GND        ESP32
 *   Guna adapter dinding USB-C 5V/2A untuk ESP32, bukan port USB laptop.
 *   (Kalau tukar ke MG996R atau >1 servo, arus boleh melebihi had ESP32 ->
 *    barulah tambah power supply 5V luar berasingan, dengan GND supply
 *    disambung ke GND ESP32 juga. Kapasitor 470uF-1000uF antara 5V dan GND
 *    servo bantu redam lonjakan arus.)
 * =====================================================================
 */

/* ---- 1. Isi 3 baris ni dari Blynk (Device Info) ---- */
#define BLYNK_TEMPLATE_ID   "TMPLxxxxxxxx"
#define BLYNK_TEMPLATE_NAME  "Fish Feeder"
#define BLYNK_AUTH_TOKEN    "PASTE_AUTH_TOKEN_KAU_SINI"

/* ---- 2. Isi WiFi kau ---- */
char ssid[] = "NAMA_WIFI_KAU";
char pass[] = "PASSWORD_WIFI_KAU";

/* ---- 3. Tetapan hardware ---- */
#define BLYNK_PRINT Serial
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <ESP32Servo.h>
#include <Preferences.h>
#include <time.h>

const int  SERVO_PIN          = 13;
const int  SERVO_CLOSED_ANGLE = 0;          // sudut "tutup"
const long GMT_OFFSET_SEC     = 8 * 3600;   // Malaysia UTC+8
const int  DST_OFFSET_SEC     = 0;          // Malaysia takde DST
#define    NUM_SLOTS          6             // max jadual sehari

/* ===================================================================== */

Servo       feederServo;
Preferences prefs;
BlynkTimer  timer;

struct Slot { bool enabled; int hour; int minute; };
Slot slots[NUM_SLOTS];

int  feedDurationSec = 3;    // V1 - berapa saat servo buka
int  servoOpenAngle  = 90;   // V2 - sudut servo bila buka

bool          feeding          = false;
unsigned long feedStartMs      = 0;
int           lastFedMinuteKey = -1;   // elak bagi makan 2x dalam minit sama

/* ---------------- SIMPAN / BACA TETAPAN (flash) ---------------- */
void saveConfig() {
  prefs.begin("feeder", false);
  prefs.putInt("dur",   feedDurationSec);
  prefs.putInt("angle", servoOpenAngle);
  for (int i = 0; i < NUM_SLOTS; i++) {
    char k[10];
    sprintf(k, "s%d_en", i); prefs.putBool(k, slots[i].enabled);
    sprintf(k, "s%d_h",  i); prefs.putInt (k, slots[i].hour);
    sprintf(k, "s%d_m",  i); prefs.putInt (k, slots[i].minute);
  }
  prefs.end();
}

void loadConfig() {
  prefs.begin("feeder", true);
  feedDurationSec = prefs.getInt("dur",   3);
  servoOpenAngle  = prefs.getInt("angle", 90);
  for (int i = 0; i < NUM_SLOTS; i++) {
    char k[10];
    sprintf(k, "s%d_en", i); slots[i].enabled = prefs.getBool(k, false);
    sprintf(k, "s%d_h",  i); slots[i].hour    = prefs.getInt (k, 8);
    sprintf(k, "s%d_m",  i); slots[i].minute  = prefs.getInt (k, 0);
  }
  prefs.end();
}

/* ---------------- KAWALAN SERVO (non-blocking) ---------------- */
void startFeed(const char* source) {
  if (feeding) return;
  feeding     = true;
  feedStartMs = millis();
  feederServo.write(servoOpenAngle);
  Serial.printf("FEED start (%s)\n", source);
  Blynk.virtualWrite(V20, String("Feeding... (") + source + ")");
  Blynk.virtualWrite(V21, 255);
}

void serviceFeed() {
  if (!feeding) return;
  if (millis() - feedStartMs >= (unsigned long)feedDurationSec * 1000UL) {
    feederServo.write(SERVO_CLOSED_ANGLE);
    feeding = false;
    Blynk.virtualWrite(V21, 0);

    struct tm ti;
    if (getLocalTime(&ti, 50)) {
      char buf[40];
      strftime(buf, sizeof(buf), "Last fed: %d/%m %H:%M", &ti);
      Blynk.virtualWrite(V20, buf);
      Serial.println(buf);
    } else {
      Blynk.virtualWrite(V20, "Fed (waktu belum sync)");
    }
  }
}

/* ---------------- SEMAK JADUAL ---------------- */
void checkSchedule() {
  struct tm ti;
  if (!getLocalTime(&ti, 50)) return;          // waktu belum sync, skip

  int minuteKey = ti.tm_hour * 60 + ti.tm_min;

  for (int i = 0; i < NUM_SLOTS; i++) {
    if (!slots[i].enabled) continue;
    if (slots[i].hour == ti.tm_hour && slots[i].minute == ti.tm_min) {
      if (lastFedMinuteKey != minuteKey) {
        lastFedMinuteKey = minuteKey;
        startFeed("jadual");
      }
    }
  }
}

/* ---------------- HANDLER BLYNK ---------------- */
BLYNK_WRITE(V0) {                       // butang Feed Now
  if (param.asInt() == 1) startFeed("manual");
}

BLYNK_WRITE(V1) {                       // slider: durasi (saat)
  feedDurationSec = constrain(param.asInt(), 1, 30);
  saveConfig();
}

BLYNK_WRITE(V2) {                       // slider: sudut buka servo
  servoOpenAngle = constrain(param.asInt(), 0, 180);
  saveConfig();
}

void handleTimeInput(int idx, const BlynkParam& param) {
  TimeInputParam t(param);
  if (t.hasStartTime()) {
    slots[idx].enabled = true;
    slots[idx].hour    = t.getStartHour();
    slots[idx].minute  = t.getStartMinute();
    Serial.printf("Slot %d -> %02d:%02d ON\n", idx, slots[idx].hour, slots[idx].minute);
  } else {
    slots[idx].enabled = false;
    Serial.printf("Slot %d OFF\n", idx);
  }
  saveConfig();
}
BLYNK_WRITE(V10) { handleTimeInput(0, param); }
BLYNK_WRITE(V11) { handleTimeInput(1, param); }
BLYNK_WRITE(V12) { handleTimeInput(2, param); }
BLYNK_WRITE(V13) { handleTimeInput(3, param); }
BLYNK_WRITE(V14) { handleTimeInput(4, param); }
BLYNK_WRITE(V15) { handleTimeInput(5, param); }

BLYNK_CONNECTED() {
  // minta Blynk hantar balik nilai terkini bila sambung
  Blynk.syncVirtual(V1, V2, V10, V11, V12, V13, V14, V15);
}

/* ---------------- SETUP / LOOP ---------------- */
void setup() {
  Serial.begin(115200);
  delay(200);
  loadConfig();

  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  feederServo.setPeriodHertz(50);
  feederServo.attach(SERVO_PIN, 500, 2400);   // SG90/MG996R pulse range
  feederServo.write(SERVO_CLOSED_ANGLE);

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);   // tunggu WiFi + Blynk

  configTime(GMT_OFFSET_SEC, DST_OFFSET_SEC,
             "pool.ntp.org", "time.google.com", "time.nist.gov");

  timer.setInterval(1000L,  serviceFeed);      // tutup servo bila cukup masa
  timer.setInterval(15000L, checkSchedule);    // semak jadual tiap 15 saat
}

void loop() {
  Blynk.run();
  timer.run();
}
