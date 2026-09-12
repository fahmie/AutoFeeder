/*
 * SimpleServo.ino
 * -------------------------------------------------------------
 * Contoh PALING RINGKAS: gerak servo pakai board ESP32.
 *
 * Guna library "ESP32Servo" (BUKAN library Servo.h biasa),
 * sebab Servo.h standard Arduino tak serasi dengan ESP32.
 *
 * Install dulu: Sketch -> Include Library -> Manage Libraries
 *   -> cari "ESP32Servo" (by Kevin Harrington) -> Install
 *
 * Kelakuan:
 *   Servo bergerak dari 0 -> 180 darjah, tunggu sekejap,
 *   pastu balik 180 -> 0 darjah, ulang selama-lamanya.
 *
 * Sambungan (ESP32):
 *   Servo Signal (oren/kuning) -> GPIO 13
 *   Servo VCC     (merah)      -> pin 5V / VIN pada ESP32
 *   Servo GND     (coklat)     -> pin GND pada ESP32
 *
 *   * Untuk servo kecil (SG90), kuasa dari USB-C ESP32 cukup.
 *   * Untuk servo besar (MG996R) atau lebih dari 1 servo, guna
 *     power supply 5V berasingan dan sambung GND sama (common ground).
 * -------------------------------------------------------------
 */

#include <ESP32Servo.h>

const int SERVO_PIN = 13;   // pin signal servo

Servo myServo;

void setup() {
  Serial.begin(115200);

  myServo.attach(SERVO_PIN);   // sambung servo ke pin
  Serial.println(F("=== SimpleServo sedia ==="));
}

void loop() {
  // Gerak 0 -> 180 darjah, sikit-sikit
  for (int angle = 0; angle <= 180; angle += 1) {
    myServo.write(angle);
    delay(15);   // laju gerakan (ms setiap 1 darjah)
  }

  delay(500);   // tahan sekejap di 180

  // Gerak balik 180 -> 0 darjah
  for (int angle = 180; angle >= 0; angle -= 1) {
    myServo.write(angle);
    delay(15);
  }

  delay(500);   // tahan sekejap di 0
}
