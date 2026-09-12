# Fish Auto Feeder — ESP32 + Servo + Blynk

Auto feeder ikan: jadual makan sampai 6x sehari, set durasi servo buka, dan
butang "Feed Now" dari phone (control dari mana-mana melalui Blynk Cloud).

---

## 1. Barang yang perlu

| Barang | Nota |
|---|---|
| NodeMCU ESP32 | yang kau ada |
| Servo SG90 (kecil) | cukup untuk flap/pintu ringan, boleh ambil kuasa dari USB-C |
| Kabel USB-C + adapter dinding 5V/2A | jangan guna port USB laptop (had ~500mA) |
| Wayar jumper, bekas makanan | botol + tudung berlubang / auger 3D print |

### Wiring — satu kabel USB-C je (untuk SG90)

```
USB-C 5V  ->  ESP32                       (kuasakan ESP32 macam biasa)

Servo signal (oren)  ->  GPIO 13
Servo VCC   (merah)  ->  pin 5V / VIN  ESP32
Servo GND   (coklat) ->  pin GND       ESP32
```

Tak perlu power supply berasingan untuk satu servo SG90 — pin `5V/VIN` ESP32
bekalkan semula servo dari punca USB-C yang sama. Jangan sambung servo VCC ke
pin **3V3**, tak cukup arus.

> **Bila baru perlu power supply luar:** kalau tukar ke servo besar
> (**MG996R**) atau nak pasang lebih dari satu servo, arusnya boleh melebihi
> had regulator ESP32 dan buat ia asyik reset. Di situ baru tambah power
> supply 5V 2A berasingan — servo VCC/GND terus ke supply tu, dan **GND
> supply mesti disambung ke GND ESP32 juga** (common ground). Kapasitor
> 470µF–1000µF antara 5V dan GND servo bantu redam lonjakan arus.

### Mekanik ringkas
- **Cara paling senang:** botol air terbalik, tudung botol potong lubang segi
  empat. Lekatkan "flap"/plat pada horn servo. Servo pusing 0° = lubang tutup,
  90° = lubang buka → pelet jatuh. `feedDurationSec` = berapa saat flap terbuka.
- **Cara lebih tepat:** auger screw (skru 3D print) — tapi tu guna continuous
  rotation servo, bukan SG90.

---

## 2. Setup Arduino IDE

1. **File → Preferences → Additional Boards Manager URLs**, tambah:
   `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
2. **Tools → Board → Boards Manager** → cari **esp32** (Espressif) → Install.
3. **Tools → Board** → pilih **ESP32 Dev Module**.
4. **Sketch → Include Library → Manage Libraries**, install:
   - `Blynk` (by Volodymyr Shymanskyy)
   - `ESP32Servo` (by Kevin Harrington)
5. Buka `FishFeeder.ino`.

---

## 3. Setup Blynk (percuma)

### A. Blynk Console (web: https://blynk.cloud)

1. Sign up → **New Template** → nama "Fish Feeder", hardware **ESP32**, connection **WiFi**.
2. Copy **Template ID**, **Template Name**, dan nanti **Auth Token** device.
3. Masuk tab **Datastreams** → **New Datastream → Virtual Pin**, buat semua ni:

| Virtual Pin | Name | Type | Min | Max | Default |
|---|---|---|---|---|---|
| V0  | Feed Now       | Integer | 0 | 1 | 0 |
| V1  | Duration (s)   | Integer | 1 | 30 | 3 |
| V2  | Open Angle     | Integer | 0 | 180 | 90 |
| V10 | Feed Time 1    | String  | — | — | — |
| V11 | Feed Time 2    | String  | — | — | — |
| V12 | Feed Time 3    | String  | — | — | — |
| V13 | Feed Time 4    | String  | — | — | — |
| V14 | Feed Time 5    | String  | — | — | — |
| V15 | Feed Time 6    | String  | — | — | — |
| V20 | Status         | String  | — | — | — |
| V21 | Feeding LED    | Integer | 0 | 255 | 0 |

> Kalau free plan hadkan bilangan datastream, kurangkan slot (contoh guna V10–V12
> je untuk 3x sehari). Edit `NUM_SLOTS` dalam kod ikut jumlah slot.

4. Tab **Web Dashboard** (optional) — drag widget kalau nak control dari web juga.

### B. Blynk app (phone — iOS/Android)

1. Login akaun sama → buka template "Fish Feeder" → **New Device**.
2. Susun widget dan link ke datastream:
   - **Button** → V0, mode **Push**, label "Feed Now"
   - **Slider** → V1 (1–30), label "Durasi (saat)"
   - **Slider** → V2 (0–180), label "Sudut buka"
   - **Time Input** × 6 → V10, V11, V12, V13, V14, V15 (satu untuk setiap masa makan)
   - **Value Display** / **Label** → V20 ("Status")
   - **LED** → V21 ("Feeding")

### C. Masukkan token dalam kod

Dalam `FishFeeder.ino`, edit bahagian atas:
```cpp
#define BLYNK_TEMPLATE_ID   "TMPLxxxxxxxx"
#define BLYNK_TEMPLATE_NAME "Fish Feeder"
#define BLYNK_AUTH_TOKEN    "xxxxxxxx"        // dari Device Info
char ssid[] = "NAMA_WIFI_KAU";
char pass[] = "PASSWORD_WIFI_KAU";
```

---

## 4. Upload & guna

1. Sambung ESP32 ke USB → pilih **Port** yang betul → **Upload**.
2. Buka **Serial Monitor** (115200 baud) — patut nampak WiFi connect, Blynk ready.
3. Dalam app Blynk:
   - Tekan **Feed Now** → servo patut buka `feedDurationSec` saat, lepas tu tutup.
   - Set **Time Input** contoh 08:00 dan 18:00 → nanti jam tu feeder jalan sendiri.
   - Kalau nak matikan satu slot, clear/off Time Input widget tu.

---

## 5. Macam mana ia berfungsi

- ESP32 dapat **waktu sebenar** dari NTP (server internet) sekali masa boot,
  lepas tu jam dalaman ESP32 jalan sendiri.
- Tiap 15 saat ESP32 semak: jam:minit sekarang sama dengan mana-mana slot aktif?
  Kalau ya → buka servo. Ada guard supaya tak bagi makan 2x dalam minit yang sama.
- **Jadual tetap jalan walau internet putus**, asalkan ESP32 pernah sync NTP dan
  tak hilang power. Butang Feed Now pula perlukan internet (sebab lalu Blynk Cloud).
- Semua tetapan (durasi, sudut, masa slot) disimpan dalam flash (`Preferences`),
  jadi kekal walau restart.

---

## 6. Naik taraf (optional)

| Nak | Buat |
|---|---|
| Jadual reliable walau kerap putus letrik | tambah modul RTC **DS3231** (I2C) |
| Feed Now tanpa internet | tambah web server local dalam ESP32 ( layan HTML page) |
| Alert "makanan habis" | sensor jarak / IR di dalam bekas → Blynk Event notification |
| Bagi makan ikut kuantiti tepat | tukar ke continuous rotation servo + auger, kira pusingan |
| Elak servo "bergegar" | detach servo selepas tutup: `feederServo.detach();` dalam `serviceFeed()` |
