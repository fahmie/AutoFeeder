/*
 * ServoCalibrateESP32.ino
 * -------------------------------------------------------------
 * Tujuan: cari titik "STOP" yang tepat untuk servo continuous-rotation
 *         (360) yang disambung ke ESP32 (GPIO 13).
 *
 * Continuous-rotation servo tak boleh "hold" kat sudut tertentu - dia
 * cuma pusing ikut lebar pulsa (microseconds). Biasanya:
 *   ~1500us         = stop (tapi selalu meleset sikit ikut unit, cth 1470-1530)
 *   < titik stop     = pusing satu arah (makin jauh dari stop = makin laju)
 *   > titik stop     = pusing arah bertentangan
 *
 * Guna Serial Monitor (baud 115200):
 *   Taip nombor terus + Enter   -> tetapkan pulsa (contoh: taip "1500" Enter)
 *   +                            -> tambah 10us
 *   -                            -> tolak 10us
 *   p                            -> papar nilai semasa
 *
 * MATLAMAT: cari nilai di mana servo BENAR-BENAR DIAM (tak berpusing
 *           langsung, tak juga bergegar). Nilai tu = titik STOP servo kau.
 *           Tulis nilai tu, nanti kita guna dalam kod feeder sebenar.
 *
 * Sambungan (ESP32):
 *   Servo Signal (oren) -> GPIO 13
 *   Servo VCC   (merah) -> 5V / VIN
 *   Servo GND   (coklat)-> GND
 * -------------------------------------------------------------
 */

#include <ESP32Servo.h>

const int SERVO_PIN = 13;

Servo myServo;
int pulseUs = 1500;      // anggaran awal titik stop
long serialNumber = -1;
bool hasNumber = false;

void applyPulse() {
  myServo.writeMicroseconds(pulseUs);
  Serial.print(F("Pulsa = "));
  Serial.print(pulseUs);
  Serial.println(F(" us"));
}

void setup() {
  Serial.begin(115200);
  delay(200);

  ESP32PWM::allocateTimer(0);
  myServo.setPeriodHertz(50);
  myServo.attach(SERVO_PIN, 500, 2500);

  Serial.println(F("=== ServoCalibrateESP32 ==="));
  Serial.println(F("Taip nombor (cth 1500) + Enter untuk tetapkan pulsa (us)."));
  Serial.println(F("+ / - untuk laras 10us. 'p' untuk papar nilai semasa."));
  Serial.println(F("Cari nilai di mana servo BENAR-BENAR diam."));

  applyPulse();
}

void loop() {
  while (Serial.available()) {
    char c = Serial.read();

    if (c >= '0' && c <= '9') {
      if (!hasNumber) { serialNumber = 0; hasNumber = true; }
      serialNumber = serialNumber * 10 + (c - '0');
      continue;
    }

    switch (c) {
      case '\n': case '\r':
        if (hasNumber) {
          pulseUs = constrain((int)serialNumber, 500, 2500);
          applyPulse();
        }
        hasNumber = false;
        serialNumber = -1;
        break;

      case '+':
        pulseUs = constrain(pulseUs + 10, 500, 2500);
        applyPulse();
        break;

      case '-':
        pulseUs = constrain(pulseUs - 10, 500, 2500);
        applyPulse();
        break;

      case 'p': case 'P':
        applyPulse();
        break;

      default:
        break;
    }
  }
}
