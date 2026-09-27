# P5 Carwash Display DUAL — ESP32 + 2 x HUB75 (64x64)

Avtomoyka shoxobchalari uchun LED tablo dasturi, **ikki panelli** versiya. Ikkita 64x32 P5 panel ustma-ust qo'yilib, bitta **64x64** ekran hosil qiladi. ESP32 Serial port orqali keladigan JSON buyruqlarni ko'rsatadi: xizmat nomi, qolgan vaqt, balans, valyuta, xatolik holati.

Bitta panelli asl loyiha: [Asilhub/ESP32_P5_display](https://github.com/Asilhub/ESP32_P5_display). **JSON protokoli bir xil**, shuning uchun boshqaruv dasturini o'zgartirish shart emas.

**Versiya:** 1.0.0  
**Panel:** 2 x P5-1921-64\*32-8S-S2 (64x32, 1/8 scan, four-scan)  
**Ekran:** 64x64 (ustma-ust)  
**Kontroller:** ESP32 (original, 2016)

---

## 1. Apparat va ulanish

### Panellarni ulash

```
   ESP32 ──HUB75──► [ TEPADAGI panel ]  IN
                    [   64 x 32       ]  OUT ──┐
                                              │ (panel bilan kelgan qisqa HUB75 kabel)
                    [ PASTDAGI panel  ]  IN ◄─┘
                    [   64 x 32       ]
```

- ESP32 **tepadagi** panelning `IN` razyomiga ulanadi.
- Tepadagi panelning `OUT` razyomi pastdagi panelning `IN` razyomiga ulanadi.
- Ikkala panel **bir xil yo'nalishda** turadi (orqa tomondagi strelkalar bir tomonga qaragan).

Agar boshqacha ulangan bo'lsa, [`p5_carwash_dual.ino`](p5_carwash_dual.ino) dagi `PANEL_CHAIN_TYPE` ni o'zgartiring:

| Ulanish | `PANEL_CHAIN_TYPE` |
| :--- | :--- |
| ESP32 → tepadagi, panellar bir xil yo'nalishda (**default**) | `CHAIN_TOP_LEFT_DOWN_ZZ` |
| ESP32 → pastdagi, panellar bir xil yo'nalishda | `CHAIN_BOTTOM_LEFT_UP_ZZ` |
| ESP32 → tepadagi, pastdagi panel 180° aylantirilgan | `CHAIN_TOP_RIGHT_DOWN` |

> **Belgi:** matnning tepa va past qismi almashib chiqsa, `TOP` ↔ `BOTTOM` variantini sinab ko'ring. Pastdagi yarmi teskari chiqsa, `CHAIN_TOP_RIGHT_DOWN` kerak.

### Pinout (ESP32 → tepadagi panel IN)

Bitta panelli versiya bilan bir xil:

| HUB75 pin | ESP32 GPIO | Vazifasi |
| :--- | :---: | :--- |
| **R1** | 18 | Qizil, yuqori yarim |
| **G1** | 17 | Yashil, yuqori yarim |
| **B1** | 16 | Ko'k, yuqori yarim |
| **R2** | 15 | Qizil, quyi yarim |
| **G2** | 19 | Yashil, quyi yarim |
| **B2** | 21 | Ko'k, quyi yarim |
| **A** | 4 | Satr tanlash A |
| **B** | 22 | Satr tanlash B |
| **C** | 14 | Satr tanlash C |
| **D / NC** | 13 | Satr tanlash D |
| **E** | 5 | Satr tanlash E (1/8 scan panelda ulanmasa ham bo'ladi) |
| **LAT / STB** | 26 | Latch |
| **OE** | 25 | Output Enable |
| **CLK** | 27 | Taktlash |
| **GND** | GND | Umumiy yer — kamida 2 ta GND simini ulang |

### Quvvat

Panellar ESP32 dan emas, **alohida 5 V manbadan** oziqlanadi.

- Bitta 64x32 P5 panel to'liq oq rangda ~3.5–4 A tortadi. Ikkitasi uchun kamida **5 V / 10 A** blok qo'ying.
- Har bir panelga quvvat simini **alohida** manbadan torting (bir panel orqali ikkinchisiga ulamang).
- ESP32 va ikkala panelning **GND** lari birlashtirilgan bo'lishi shart.
- Yorqinlik `setBrightness8(120)` (255 dan).

---

## 2. Dasturni yuklash

### A) Tayyor firmware bilan (4 ta .bin fayl)

[`release/parts/`](release/parts/) papkasidagi 4 ta fayl:

| Fayl | Manzil (Address) |
| :--- | :--- |
| `bootloader.bin` | `0x1000` |
| `partitions.bin` | `0x8000` |
| `boot_app0.bin` | `0xe000` |
| `firmware.bin` | `0x10000` |

**Windows skripti bilan:**

```cmd
cd release
flash.bat COM3
```

**Qo'lda esptool:**

```bash
esptool --chip esp32 -p COM3 -b 921600 write_flash 0x1000 parts/bootloader.bin 0x8000 parts/partitions.bin 0xe000 parts/boot_app0.bin 0x10000 parts/firmware.bin
```

*(Ixtiyoriy: 0x0 manziliga birdan yozish uchun `p5_carwash_dual_v1.0.0_FULL.bin` ham bor)*

Espressif Flash Download Tool sozlamalari: Chip **ESP32**, SPI **80 MHz**, **DIO**, Flash **32Mbit**.

### B) Manbadan kompilyatsiya qilish

Arduino IDE da **ESP32 Dev Module** boardini tanlang.

| Kutubxona | Versiya |
| :--- | :--- |
| ESP32 HUB75 LED MATRIX PANEL DMA Display | **3.0.14** |
| GFX_Lite | 2.0.0 |
| ArduinoJson | 7.x |
| esp32 board core | 3.3.7 |

> HUB75 kutubxonasining **3.0.13 dan eski** versiyalari ishlamaydi (`VirtualMatrixPanel_T`, `FOUR_SCAN_32PX_HIGH` yo'q).

---

## 3. Ekran layouti (64x64)

```
 qator 0      ▬▬▬▬ animatsiya chizig'i ▬▬▬▬
 qator 9..22        TEPA MATN              ← 1-panel o'rtasi
 ---------- panellar choki (31/32) ----------
 qator 41..54       PAST MATN / VAQT       ← 2-panel o'rtasi
 qator 63     ▬▬▬▬ animatsiya chizig'i ▬▬▬▬
```

- **Ikki qatorli rejimlar** (matn + vaqt, valyuta): har bir qator o'z panelining o'rtasida turadi, panellar choki ustidan harf o'tmaydi.
- **Bir qatorli rejimlar** (`MECANUZ`, `SUM`, `ERROR`, `TEST`, vaqt tugagan matn): ekranning qoq o'rtasida (qator 24..37).

---

## 4. JSON protokoli

Bitta panelli versiya bilan **to'liq bir xil**. Serial port, **115200 baud**, har bir buyruq `\n` bilan tugaydi.

| Kalit | Turi | Tavsif |
| :--- | :--- | :--- |
| `type` | matn | Rejim yoki ko'rsatiladigan matn (lotin) |
| `typeuz` / `textuz` | matn | Xuddi `type` kabi, lotin |
| `typekg` / `textkg` | matn | Kirill rejimi (qirg'izcha) |
| `typekr` / `textkr` | matn | Kirill rejimi |
| `value` | son | Balans yoki vaqt — **MMSS formatida** (`230` → `02:30`) |
| `colorR1` `colorG1` `colorB1` | 0–255 | Matn rangi |
| `colorR2` `colorG2` `colorB2` | 0–255 | Raqam / vaqt rangi |

| Kalit so'z | Rejim |
| :--- | :--- |
| `MECANUZ`, `CARWASH`, `KGCARWASH`, `KG` | Logotip, nafas oluvchi oq |
| `PP<matn>PP`, `KG<matn>KG` | Valyuta: tepada raqam, pastda `<matn>` |
| `SUM` | Faqat raqam, markazda |
| `SOM`, `СОМ` | Tepada raqam, pastda `SOM` / `СОМ` |
| `TEST` | Butun alifbo skroll |
| boshqa matn | Tepada matn, pastda `MM:SS` |

### Misollar

```json
{"type":"MECANUZ"}
{"typeuz":"PPsumPP","value":10000,"colorR1":255,"colorG1":180,"colorB1":0,"colorR2":0,"colorG2":255,"colorB2":0}
{"typeuz":"KOPIK","value":300}
{"typekg":"ШАМПУНЬ","value":230}
{"type":"TEST"}
```

Batafsil tavsif va ma'lum cheklovlar (`Ө`, `Ү`, `Ң` harflari, `SH` → `Ш` va h.k.) uchun asl loyiha README siga qarang — ular bu versiyada ham o'zgarmagan.

---

## 5. Bitta panelli versiyadan farqlar

| Nima | 1 panel (64x32) | 2 panel (64x64) |
| :--- | :--- | :--- |
| Chain | `CHAIN_NONE` | `CHAIN_TOP_LEFT_DOWN_ZZ` |
| DMA konfiguratsiya | `HUB75_I2S_CFG(128, 16, 1)` | `HUB75_I2S_CFG(128, 16, 2)` |
| Virtual panel | `(1, 1, 64, 32)` | `(2, 1, 64, 32)` |
| Bir qatorli matn Y | 8 | 24 |
| Ikki qatorli matn Y | 1 / 17 | 9 / 41 |
| Animatsiya chiziqlari | qator 0 va 31 | qator 0 va 63 |
| Quvvat bloki | 5 V / 5 A | 5 V / 10 A |

---

## 6. Fayl tuzilmasi

```
p5_carwash_dual/
├── p5_carwash_dual.ino                    Asosiy dastur
├── font16x10.h                            Proporsional shrift, 81 ta belgi
├── README.md
└── release/
    ├── p5_carwash_dual_v1.0.0_FULL.bin    Tayyor firmware (0x0 ga)
    ├── flash.bat                          Windows flash skripti
    └── parts/
        ├── bootloader.bin                 0x1000
        ├── partitions.bin                 0x8000
        ├── boot_app0.bin                  0xe000
        └── firmware.bin                   0x10000
```
