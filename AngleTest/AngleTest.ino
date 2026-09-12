/*
 * AngleTest.ino
 * -------------------------------------------------------------
 * Tujuan: uji sudut servo untuk pintu Fish Feeder.
 *
 *   - Tekan BUTANG di D2  -> servo gerak ke SUDUT SASARAN,
 *                            tahan 3 saat, kemudian balik ke SUDUT REHAT.
 *   - Guna Serial Monitor (baud 9600) untuk laras sudut:
 *
 *        +   = sudut sasaran +5
 *        -   = sudut sasaran -5
 *        t   = tetapkan sudut sasaran = sudut semasa yang ditaip
 *              (contoh: taip "t90" -> sasaran jadi 90)
 *        r   = tetapkan sudut REHAT ikut nombor selepasnya
 *              (contoh: "r0"  -> rehat jadi 0)
 *        h   = tahan (hold) di sasaran, JANGAN balik  (toggle)
 *        g   = gerak sekali ke sasaran sekarang (sama macam tekan butang)
 *        p   = papar tetapan semasa
 *
 * Sambungan (Arduino Uno):
 *   Servo Oren/Kuning (SIG) -> D9
 *   Servo Merah (VCC)       -> 5V (bekalan luar 5V jika servo besar)
 *   Servo Coklat/Hitam(GND) -> GND (common ground)
 *   Butang                  -> D2 dan GND  (INPUT_PULLUP, tiada perintang)
 * -------------------------------------------------------------
 */

#include <Servo.h>

// ---------- Pin ----------
const int SERVO_PIN  = 9;
const int BUTTON_PIN = 2;

// ---------- Tetapan boleh laras ----------
int targetAngle = 90;              // sudut bila butang ditekan
int restAngle   = 0;               // sudut selepas tahan 3 saat
const unsigned long HOLD_MS = 3000;   // tempoh tahan di sasaran (3 saat)

// ---------- Keadaan ----------
Servo doorServo;
bool holdForever = false;           // kalau true, tak balik ke rehat

int  lastButtonReading = HIGH;
int  buttonStable      = HIGH;
unsigned long lastDebounce = 0;
const unsigned long DEBOUNCE_MS = 50;

long serialNumber = -1;             // nombor yang ditaip selepas huruf arahan

void printSettings() {
  Serial.println(F("---- Tetapan AngleTest ----"));
  Serial.print(F("  Sudut sasaran : ")); Serial.println(targetAngle);
  Serial.print(F("  Sudut rehat   : ")); Serial.println(restAngle);
  Serial.print(F("  Tahan (ms)    : ")); Serial.println(HOLD_MS);
  Serial.print(F("  Hold selamanya: ")); Serial.println(holdForever ? F("YA") : F("TIDAK"));
  Serial.println(F("  Arahan: + - t<nombor> r<nombor> h g p"));
  Serial.println(F("---------------------------"));
}

void moveToTarget(const char *sebab) {
  Serial.print(F("Gerak ke sasaran "));
  Serial.print(targetAngle);
  Serial.print(F(" (sebab: "));
  Serial.print(sebab);
  Serial.println(F(")"));

  doorServo.write(targetAngle);

  if (holdForever) {
    Serial.println(F("  -> hold selamanya, tak balik."));
    return;
  }

  delay(HOLD_MS);                 // tahan 3 saat

  doorServo.write(restAngle);
  Serial.print(F("  -> balik ke rehat "));
  Serial.println(restAngle);
}

bool buttonPressed() {
  int reading = digitalRead(BUTTON_PIN);
  if (reading != lastButtonReading) {
    lastDebounce = millis();
  }
  bool pressed = false;
  if ((millis() - lastDebounce) > DEBOUNCE_MS) {
    if (reading != buttonStable) {
      buttonStable = reading;
      if (buttonStable == LOW) pressed = true;   // INPUT_PULLUP: LOW = ditekan
    }
  }
  lastButtonReading = reading;
  return pressed;
}

void handleSerial() {
  while (Serial.available()) {
    char c = Serial.read();

    if (c >= '0' && c <= '9') {
      if (serialNumber < 0) serialNumber = 0;
      serialNumber = serialNumber * 10 + (c - '0');
      continue;
    }

    switch (c) {
      case '+':
        targetAngle = constrain(targetAngle + 5, 0, 180);
        doorServo.write(targetAngle);
        Serial.print(F("Sasaran = ")); Serial.println(targetAngle);
        break;

      case '-':
        targetAngle = constrain(targetAngle - 5, 0, 180);
        doorServo.write(targetAngle);
        Serial.print(F("Sasaran = ")); Serial.println(targetAngle);
        break;

      case 't': case 'T':
        if (serialNumber >= 0) {
          targetAngle = constrain((int)serialNumber, 0, 180);
          doorServo.write(targetAngle);
          Serial.print(F("Sasaran ditetapkan = ")); Serial.println(targetAngle);
        }
        break;

      case 'r': case 'R':
        if (serialNumber >= 0) {
          restAngle = constrain((int)serialNumber, 0, 180);
          Serial.print(F("Rehat ditetapkan = ")); Serial.println(restAngle);
        }
        break;

      case 'h': case 'H':
        holdForever = !holdForever;
        Serial.print(F("Hold selamanya: "));
        Serial.println(holdForever ? F("YA") : F("TIDAK"));
        break;

      case 'g': case 'G':
        moveToTarget("arahan serial g");
        break;

      case 'p': case 'P':
        printSettings();
        break;

      default:
        break;   // abaikan newline / aksara lain
    }

    serialNumber = -1;   // reset selepas setiap huruf arahan
  }
}

void setup() {
  Serial.begin(9600);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  doorServo.attach(SERVO_PIN);
  doorServo.write(restAngle);

  Serial.println(F("=== AngleTest - Fish Feeder ==="));
  Serial.println(F("Tekan butang D2 -> servo ke sasaran, tahan 3s, balik."));
  printSettings();
}

void loop() {
  handleSerial();

  if (buttonPressed()) {
    moveToTarget("butang");
  }
}
