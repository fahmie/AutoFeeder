/*
 * UltrasonicTestESP32.ino
 * -------------------------------------------------------------
 * Test cepat sensor HC-SR04 - papar jarak (cm) setiap 0.3 saat dalam
 * Serial Monitor. Guna untuk:
 *   1. Pastikan wiring betul (ada bacaan, bukan "timeout" je)
 *   2. Ukur nilai TANK_EMPTY_CM / TANK_FULL_CM untuk kod feeder utama -
 *      letak sensor macam kedudukan sebenar, baca jarak, catat nombor tu.
 *
 * Sambungan (ESP32):
 *   HC-SR04 VCC  -> 5V / VIN ESP32
 *   HC-SR04 GND  -> GND ESP32
 *   HC-SR04 Trig -> GPIO 26
 *   HC-SR04 Echo -> GPIO 27 MELALUI voltage divider (WAJIB):
 *     Echo --[R1 1k]-- (titik tengah -> GPIO27) --[R2 2k]-- GND
 *
 * Buka Serial Monitor, baud 115200.
 * -------------------------------------------------------------
 */

const int TRIG_PIN = 26;
const int ECHO_PIN = 27;

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

void setup() {
  Serial.begin(115200);
  delay(200);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);

  Serial.println(F("=== UltrasonicTestESP32 sedia ==="));
  Serial.println(F("Papar jarak setiap 0.3 saat..."));
}

void loop() {
  float d = readDistanceCm();
  if (d < 0) {
    Serial.println(F("Timeout - tiada bacaan (semak wiring)"));
  } else {
    Serial.print(F("Jarak: "));
    Serial.print(d, 1);
    Serial.println(F(" cm"));
  }
  delay(300);
}
