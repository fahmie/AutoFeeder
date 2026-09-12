/*
 * ServoDiag.ino
 * -------------------------------------------------------------
 * Tujuan: kenal pasti jenis servo — SUDUT biasa atau PUTARAN BERTERUSAN.
 *
 * Kod ini hantar SATU nilai sahaja ke servo, kemudian diam.
 * Guna Serial Monitor (baud 9600) untuk tukar nilai:
 *
 *   90  (taip "90")  -> nilai neutral / "berhenti"
 *   0   (taip "0")   -> hujung satu / laju satu arah
 *   180 (taip "180") -> hujung lain / laju arah bertentangan
 *   +   -> nilai +1  (halus, untuk cari titik berhenti sebenar)
 *   -   -> nilai -1
 *   u<us> -> hantar mikrosaat terus, cth "u1500"
 *
 * TAFSIRAN:
 *   - Taip 90, servo BERHENTI diam        -> servo SUDUT biasa (elok untuk pintu)
 *   - Taip 90, servo MASIH pusing perlahan -> servo PUTARAN BERTERUSAN
 *     (cari nilai +/- sampai betul-betul diam = titik neutral sebenar)
 *   - Taip 0 / 90 / 180, servo pergi ke 3 kedudukan berbeza & berhenti
 *                                          -> servo SUDUT biasa
 *   - Taip 0 / 180, servo pusing tanpa henti (arah berbeza)
 *                                          -> servo PUTARAN BERTERUSAN
 * -------------------------------------------------------------
 */

#include <Servo.h>

const int SERVO_PIN = 9;

Servo s;
int   val = 90;          // nilai semasa (0..180)
long  num = -1;          // nombor ditaip
bool  useMicros = false; // mod 'u'

void apply() {
  s.write(val);
  Serial.print(F("write("));
  Serial.print(val);
  Serial.print(F(")  ~ "));
  Serial.print(map(val, 0, 180, 1000, 2000));
  Serial.println(F(" us"));
}

void setup() {
  Serial.begin(9600);
  s.attach(SERVO_PIN);
  s.write(val);

  Serial.println(F("=== ServoDiag ==="));
  Serial.println(F("Taip: 90 / 0 / 180  |  + -  |  u1500"));
  Serial.println(F("90 = neutral. Servo sudut -> berhenti. Putaran berterusan -> masih pusing."));
  apply();
}

void loop() {
  while (Serial.available()) {
    char c = Serial.read();

    if (c >= '0' && c <= '9') {
      if (num < 0) num = 0;
      num = num * 10 + (c - '0');
      continue;
    }

    switch (c) {
      case '+':
        val = constrain(val + 1, 0, 180);
        apply();
        break;
      case '-':
        val = constrain(val - 1, 0, 180);
        apply();
        break;
      case 'u': case 'U':
        useMicros = true;
        break;
      case '\n': case '\r':
        if (num >= 0) {
          if (useMicros) {
            int us = constrain((int)num, 500, 2500);
            s.writeMicroseconds(us);
            Serial.print(F("writeMicroseconds("));
            Serial.print(us);
            Serial.println(F(")"));
          } else {
            val = constrain((int)num, 0, 180);
            apply();
          }
        }
        num = -1;
        useMicros = false;
        break;
      default:
        break;
    }
  }
}
