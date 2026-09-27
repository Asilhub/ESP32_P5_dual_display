/*
  ESP32 HUB75 Matrix Panel - Carwash Display (DUAL)
  2 x P5-1921-64*32-8S-S2 (64x32, 1/8 scan / four-scan), ustma-ust -> 64x64 ekran

  p5_carwash (bitta panel) versiyasidan olingan. JSON protokoli bir xil,
  faqat ekran 64x64 bo'ldi va layout shunga moslandi.

  Ulanish: ESP32 -> tepadagi panel IN, tepadagi panel OUT -> pastdagi panel IN.
  Ikkala panel bir xil yo'nalishda (strelkalar bir tomonga) turadi.
  Boshqacha ulansa PANEL_CHAIN_TYPE ni o'zgartiring (pastga qarang).

  Kerakli kutubxonalar:
    ESP32-HUB75-MatrixPanel-I2S-DMA  3.0.14
    GFX_Lite                         2.0.0
    ArduinoJson                      7.x
*/

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <ESP32-HUB75-VirtualMatrixPanel_T.hpp>
#include <ArduinoJson.h>
#include "font16x10.h"
#include <math.h>

#define PANEL_WIDTH    64
#define PANEL_HEIGHT   32
#define PANEL_ROWS      2   // ustma-ust panellar soni
#define PANEL_COLS      1
#define PANEL_SCAN_TYPE FOUR_SCAN_32PX_HIGH

// Panellar zanjiri:
//   CHAIN_TOP_LEFT_DOWN_ZZ   - ESP32 tepadagi panelga, ikkala panel bir xil yo'nalishda (default)
//   CHAIN_BOTTOM_LEFT_UP_ZZ  - ESP32 pastdagi panelga, ikkala panel bir xil yo'nalishda
//   CHAIN_TOP_RIGHT_DOWN     - ESP32 tepadagiga, pastdagi panel 180° aylantirilgan (qisqa kabel)
// Rasm tepa-past almashib chiqsa: TOP <-> BOTTOM variantini sinab ko'ring.
#define PANEL_CHAIN_TYPE CHAIN_TOP_LEFT_DOWN_ZZ

// Umumiy (virtual) ekran o'lchami
#define DISP_W  (PANEL_WIDTH  * PANEL_COLS)   // 64
#define DISP_H  (PANEL_HEIGHT * PANEL_ROWS)   // 64

// Pinout konfiguratsiyasi (v1.0.1)
#define RL1 18
#define GL1 17
#define BL1 16
#define RL2 15
#define GL2 19
#define BL2 21
#define CH_A 4
#define CH_B 22
#define CH_C 14
#define CH_D 13
#define CH_E 5
#define CLK 27
#define LAT 26
#define OE  25

MatrixPanel_I2S_DMA *dma_display = nullptr;

using MyScanTypeMapping = ScanTypeMapping<PANEL_SCAN_TYPE>;
VirtualMatrixPanel_T<PANEL_CHAIN_TYPE, MyScanTypeMapping> *disp = nullptr;

char message[128]    = "MECANUZ";
char timeStr[6]      = "00:00";
char currencyStr[32] = "";

int   value          = 0;
int   textWidth      = 0;
int   xPos           = 0;

uint16_t textColor   = 0xFFFF;
uint16_t valueColor  = 0xFFFF;
String   displayType = "MECANUZ";
bool     isCyrillic  = false;

#define SPACE_WIDTH  5

unsigned long valueZeroSince = 0;

#define FRAME_MS 50

// ---------- Layout (64x64 uchun) ----------
// Shrift 16px balandlikda, lotin/raqamlar amalda 0..13 qatorlarni egallaydi.
// 0-qator va 63-qator animatsiya chizigi uchun band.
// Ikki qatorli rejimda matnlar har bir panelning o'rtasiga tushadi:
// tepa 9..22 (1-panel), past 41..54 (2-panel). Panellar tutashgan joy (31/32)
// bo'sh qoladi, shuning uchun harflar choki ustida bo'linib qolmaydi.
#define LINE_Y_SINGLE  ((DISP_H - FONT16X10_HEIGHT) / 2)   // 24
#define LINE_Y_TOP     9
#define LINE_Y_BOTTOM  (PANEL_HEIGHT + 9)                  // 41

// ---------- Font funksiyalari ----------

uint16_t nextUTF8(const char* &ptr) {
  uint8_t b1 = *ptr;
  if (b1 == '\0') return 0;

  ptr++; // Advance by default

  if ((b1 & 0x80) == 0) {
    return b1;
  }

  if ((b1 & 0xE0) == 0xC0 || (b1 & 0xE0) == 0xD0) {
    uint8_t b2 = *ptr;
    if (b2 == '\0') return 0;
    ptr++;
    return ((b1 & 0x1F) << 6) | (b2 & 0x3F);
  }

  if ((b1 & 0xF0) == 0xE0) {
    uint8_t b2 = *ptr; if (b2 == '\0') return 0; ptr++;
    uint8_t b3 = *ptr; if (b3 == '\0') return 0; ptr++;
    return ((b1 & 0x0F) << 12) | ((b2 & 0x3F) << 6) | (b3 & 0x3F);
  }

  return b1;
}

int getFontIndex(uint16_t code) {
  // Kirill rejimi faol bolsa, lotin harflarini avtomat kirillga ogiramiz
  if (isCyrillic) {
    if (code >= 'a' && code <= 'z') code -= 32;

    if      (code == 'A') code = 0x0410; // А
    else if (code == 'B') code = 0x0411; // Б
    else if (code == 'V') code = 0x0412; // В
    else if (code == 'G') code = 0x0413; // Г
    else if (code == 'D') code = 0x0414; // Д
    else if (code == 'E') code = 0x0415; // Е
    else if (code == 'J') code = 0x0416; // Ж
    else if (code == 'Z') code = 0x0417; // З
    else if (code == 'I') code = 0x0418; // И
    else if (code == 'K') code = 0x041A; // К
    else if (code == 'L') code = 0x041B; // Л
    else if (code == 'M') code = 0x041C; // М
    else if (code == 'N') code = 0x041D; // Н
    else if (code == 'O') code = 0x041E; // О
    else if (code == 'P') code = 0x041F; // П
    else if (code == 'R') code = 0x0420; // Р
    else if (code == 'S') code = 0x0421; // С
    else if (code == 'T') code = 0x0422; // Т
    else if (code == 'U') code = 0x0423; // У
    else if (code == 'F') code = 0x0424; // Ф
    else if (code == 'X') code = 0x0425; // Х
    else if (code == 'H') code = 0x0425; // Х
    else if (code == 'C') code = 0x0426; // Ц
    else if (code == 'W') code = 0x0412; // В
    else if (code == 'Q') code = 0x041A; // К
    else if (code == 'Y') code = 0x042B; // Ы
  }

  // ASCII / Latin
  if (code >= 'a' && code <= 'z') code -= 32;

  if (code >= '0' && code <= '9') return code - '0';
  if (code >= 'A' && code <= 'Z') return (code - 'A') + 10;
  if (code == ':') return 36;
  if (code == '-') return 37;
  if (code == '.') return 38;
  if (code == ',') return 39;
  if (code == '$') return 40;
  if (code == '%') return 41;
  if (code == '+') return 42;
  if (code == '/') return 43;
  if (code == '=') return 44;

  // Cyrillic Lowercase to Uppercase conversion
  if (code >= 0x0430 && code <= 0x044F) code -= 32;
  if (code == 0x0451) code = 0x0401; // ё -> Ё

  // Cyrillic Uppercase mapping (to match 81-character font16x10 array, skipping Ң, Ө, Ү)
  // Note: Cyrillic В uses its own glyph (index 47). The P4 version redirected it to
  // the Latin B glyph (index 11), but that one is only 13 rows tall while every
  // Cyrillic glyph is 14 - which made В sit one pixel short next to its neighbours.
  if (code >= 0x0410 && code <= 0x0415) return code - 0x0410 + 45; // А..Е
  if (code == 0x0401) return 51; // Ё
  if (code >= 0x0416 && code <= 0x041D) return code - 0x0416 + 52; // Ж..Н
  // Index 60 is Ң (skipped)
  if (code == 0x041E) return 61; // О
  // Index 62 is Ө (skipped)
  if (code >= 0x041F && code <= 0x0423) return code - 0x041F + 63; // П..У
  // Index 68 is Ү (skipped)
  if (code >= 0x0424 && code <= 0x042F) return code - 0x0424 + 69; // Ф..Я

  return -1; // Unknown character
}

int getActiveCharWidth(uint16_t code) {
  if (code == ' ') return SPACE_WIDTH;
  int index = getFontIndex(code);
  if (index < 0) return 0;

  int left = FONT16X10_WIDTH;
  int right = -1;
  for (int row = 0; row < FONT16X10_HEIGHT; row++) {
    uint16_t line = pgm_read_word(&font16x10[index][row]);
    for (int col = 0; col < FONT16X10_WIDTH; col++) {
      if (line & (1 << (FONT16X10_WIDTH - 1 - col))) {
        if (col < left) left = col;
        if (col > right) right = col;
      }
    }
  }

  if (right < left) return 0;
  return (right - left + 1);
}

void drawChar(int16_t x, int16_t y, uint16_t code, uint16_t color) {
  int index = getFontIndex(code);
  if (index < 0) return;

  int left = FONT16X10_WIDTH;
  int right = -1;
  for (int row = 0; row < FONT16X10_HEIGHT; row++) {
    uint16_t line = pgm_read_word(&font16x10[index][row]);
    for (int col = 0; col < FONT16X10_WIDTH; col++) {
      if (line & (1 << (FONT16X10_WIDTH - 1 - col))) {
        if (col < left) left = col;
        if (col > right) right = col;
      }
    }
  }

  if (right < left) return;

  for (int row = 0; row < FONT16X10_HEIGHT; row++) {
    uint16_t line = pgm_read_word(&font16x10[index][row]);
    for (int col = left; col <= right; col++) {
      int16_t px = x + (col - left);
      int16_t py = y + row;
      if (px >= 0 && px < DISP_W && py >= 0 && py < DISP_H) {
        if (line & (1 << (FONT16X10_WIDTH - 1 - col))) {
          disp->drawPixel(px, py, color);
        }
      }
    }
  }
}

int drawText(int16_t x, int16_t y, const char* str, uint16_t color) {
  int startX = x;
  const char* ptr = str;
  while (*ptr) {
    uint16_t code = nextUTF8(ptr);
    if (code == 0) break;

    if (code == ' ') {
      x += SPACE_WIDTH;
    } else {
      drawChar(x, y, code, color);
      int charWidth = getActiveCharWidth(code);
      if (charWidth > 0) {
        x += charWidth + 1;
      }
    }
  }
  return x - startX;
}

int calculateTextWidth(const char* str) {
  int width = 0;
  const char* ptr = str;
  while (*ptr) {
    uint16_t code = nextUTF8(ptr);
    if (code == 0) break;

    if (code == ' ') {
      width += SPACE_WIDTH;
    } else {
      int charWidth = getActiveCharWidth(code);
      if (charWidth > 0) {
        width += charWidth + 1;
      }
    }
  }
  return width;
}

// ---------- Yordamchi funksiyalar ----------

void formatTime(int rawValue, char* output) {
  int leftPart  = rawValue / 100;
  int rightPart = rawValue % 100;
  snprintf(output, 6, "%02d:%02d", leftPart, rightPart);
}

// 64px enda tort tomonlama ramka matnga joy qoldirmaydi (MECANUZ = 63px),
// shuning uchun animatsiya faqat tepa (0) va past (63) qatorlar boylab yuguradi.
#define ANIM_PERIMETER  (DISP_W * 2)
#define ANIM_SNAKE_LEN  16
#define ANIM_STEP        2

void drawBorderAnimation() {
  if (displayType == "ERROR") {
    uint16_t errColor = ((millis() / 250) % 2 == 0) ? disp->color444(15, 0, 0) : 0;
    for (int i = 0; i < DISP_W; i++) {
      disp->drawPixel(i, 0, errColor);
      disp->drawPixel(i, DISP_H - 1, errColor);
    }
    return;
  }

  // Premium chiziq animatsiyasi (Snake)
  static int pos = 0;
  static uint8_t colorIndex = 0;
  uint16_t colors[] = {
    disp->color444(15, 0,  0),
    disp->color444(0,  15, 0),
    disp->color444(0,  0,  15),
    disp->color444(15, 15, 0),
    disp->color444(0,  15, 15),
    disp->color444(15, 0,  15)
  };

  uint16_t animColor1 = colors[colorIndex];
  uint16_t animColor2 = colors[(colorIndex + 2) % 6];

  // 0..63   -> tepa qator, chapdan ongga
  // 64..127 -> past qator, ongdan chapga (uzluksiz halqa hosil boladi)
  auto drawLinePixel = [&](int p, uint16_t c) {
    if (p < DISP_W) {
      disp->drawPixel(p, 0, c);
    } else {
      disp->drawPixel(ANIM_PERIMETER - 1 - p, DISP_H - 1, c);
    }
  };

  for (int i = 0; i < ANIM_SNAKE_LEN; i++) {
    drawLinePixel((pos + i) % ANIM_PERIMETER, animColor1);
    drawLinePixel((pos + i + (ANIM_PERIMETER / 2)) % ANIM_PERIMETER, animColor2);
  }

  pos += ANIM_STEP;
  if (pos >= ANIM_PERIMETER) {
    pos -= ANIM_PERIMETER;
    colorIndex = (colorIndex + 1) % 6;
  }
}

// Nafas oluvchi effekt
void animateMecanuz() {
  float breath = (sin(millis() / 400.0) + 1.0) / 2.0;
  int intensity = 2 + (13 * breath);
  textColor = disp->color444(intensity, intensity, intensity);
}

// Xato miltillashi
void animateError() {
  float breath = (sin(millis() / 200.0) + 1.0) / 2.0;
  int r = 5 + (10 * breath);
  textColor = disp->color444(r, 0, 0);
}

// ---------- JSON ----------

void parseJSON(String jsonStr) {
#if ARDUINOJSON_VERSION_MAJOR >= 7
  JsonDocument doc;
#else
  StaticJsonDocument<256> doc;
#endif
  DeserializationError error = deserializeJson(doc, jsonStr);

  if (error) {
    Serial.print("Buzilgan JSON: ");
    Serial.println(error.c_str());

    displayType = "ERROR";
    strcpy(message, "ERROR");
    strcpy(timeStr, "");

    textWidth = calculateTextWidth(message);
    xPos = (DISP_W - textWidth) / 2;
    return;
  }

  // Tilni aniqlash (kirill/lotin kalitlari orqali)
  const char* typeRaw = nullptr;

  if (doc["typekr"].is<const char*>()) {
    typeRaw = doc["typekr"];
    isCyrillic = true;
  } else if (doc["textkr"].is<const char*>()) {
    typeRaw = doc["textkr"];
    isCyrillic = true;
  } else if (doc["typekg"].is<const char*>()) {
    typeRaw = doc["typekg"];
    isCyrillic = true;
  } else if (doc["textkg"].is<const char*>()) {
    typeRaw = doc["textkg"];
    isCyrillic = true;
  } else if (doc["typeuz"].is<const char*>()) {
    typeRaw = doc["typeuz"];
    isCyrillic = false;
  } else if (doc["textuz"].is<const char*>()) {
    typeRaw = doc["textuz"];
    isCyrillic = false;
  } else {
    typeRaw = doc["type"] | "MECANUZ";
    isCyrillic = false;
  }

  int newValue = doc["value"] | 0;

  int r1 = doc["colorR1"] | 255;
  int g1 = doc["colorG1"] | 255;
  int b1 = doc["colorB1"] | 255;
  int r2 = doc["colorR2"] | 255;
  int g2 = doc["colorG2"] | 255;
  int b2 = doc["colorB2"] | 255;

  String typeStr = String(typeRaw);
  typeStr.toUpperCase();

  char newMsg[128] = "";
  String newDisplayType = "";

  if (typeStr == "CARWASH" || typeStr == "MECANUZ" || typeStr == "KGCARWASH" || typeStr == "KG") {
    newDisplayType = "MECANUZ";
    strcpy(newMsg, "MECANUZ");
  }
  else if (typeStr.startsWith("PP") && typeStr.endsWith("PP") && typeStr.length() > 4) {
    newDisplayType = "VALYUTA";
    String cleanType = typeStr.substring(2, typeStr.length() - 2);

    // Kirill rejimi uchun pul birligi avtomatik tarjimasi
    if (isCyrillic) {
      if (cleanType == "SUM" || cleanType == "SOM") {
        cleanType = "СОМ";
      }
    }

    strncpy(currencyStr, cleanType.c_str(), sizeof(currencyStr) - 1);
    snprintf(newMsg, sizeof(newMsg), "%d", newValue);
  }
  else if (typeStr.startsWith("KG") && typeStr.endsWith("KG") && typeStr.length() > 4) {
    newDisplayType = "VALYUTA";
    String cleanType = typeStr.substring(2, typeStr.length() - 2);
    strncpy(currencyStr, cleanType.c_str(), sizeof(currencyStr) - 1);
    snprintf(newMsg, sizeof(newMsg), "%d", newValue);
  }
  else if (typeStr == "SUM") {
    newDisplayType = "SUM";
    snprintf(newMsg, sizeof(newMsg), "%d", newValue);
  }
  else if (typeStr == "SOM") {
    newDisplayType = "VALYUTA";
    strcpy(currencyStr, isCyrillic ? "СОМ" : "SOM");
    snprintf(newMsg, sizeof(newMsg), "%d", newValue);
  }
  else if (typeStr == "СОМ" || typeStr == "сом") {
    newDisplayType = "VALYUTA";
    strcpy(currencyStr, "СОМ");
    snprintf(newMsg, sizeof(newMsg), "%d", newValue);
  }
  else if (typeStr == "TEST") {
    newDisplayType = "TEST";
    strcpy(newMsg, "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ:-.,$%+=АБВГДЕЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯ");
  }
  else {
    newDisplayType = "TEXT";

    // Kirill tili uchun sozlar tarjimasi
    if (isCyrillic && typeStr == "SHAMPUN") {
      strcpy(newMsg, "ШАМПУНЬ");
    }
    else if (isCyrillic && typeStr == "PAUZA") {
      strcpy(newMsg, "ПАУЗА");
    }
    else {
      strncpy(newMsg, typeStr.c_str(), sizeof(newMsg) - 1);
    }

    newMsg[sizeof(newMsg) - 1] = '\0';
    formatTime(newValue, timeStr);
  }

  textColor   = disp->color444(r1 / 17, g1 / 17, b1 / 17);
  valueColor  = disp->color444(r2 / 17, g2 / 17, b2 / 17);
  displayType = newDisplayType;
  value       = newValue;

  if (newDisplayType == "TEXT" && newValue == 0) {
    if (valueZeroSince == 0) valueZeroSince = millis();
  } else {
    valueZeroSince = 0;
  }

  // Matn ozgarganda xPos ni moslash
  if (strcmp(message, newMsg) != 0) {
    strcpy(message, newMsg);
    textWidth = calculateTextWidth(message);
    xPos = (textWidth <= DISP_W) ? (DISP_W - textWidth) / 2 : 0;

    // P4 da MECANUZ markazdan +2 px surilardi. 64px enda u aynan 63px -
    // qoshimcha surish oxirgi ustunni kesib yuboradi, shuning uchun olib tashlandi.

    // TEST rejimi uchun scroll ongdan boshlanadi
    if (displayType == "TEST") {
      xPos = DISP_W;
    }
  }
}

// ---------- Setup ----------

void setup() {
  Serial.begin(115200);

  HUB75_I2S_CFG::i2s_pins pins = { RL1, GL1, BL1, RL2, GL2, BL2, CH_A, CH_B, CH_C, CH_D, CH_E, LAT, OE, CLK };

  // four-scan: DMA ga har bir panel eni x2, boyi /2 beriladi -> 128 x 16,
  // zanjirda 2 ta panel -> DMA jami 256 x 16
  HUB75_I2S_CFG cfg(PANEL_WIDTH * 2, PANEL_HEIGHT / 2, PANEL_ROWS * PANEL_COLS, pins);
  cfg.i2sspeed    = HUB75_I2S_CFG::HZ_10M;   // 20M da bu panelda ghosting bolishi mumkin
  cfg.clkphase    = false;
  cfg.double_buff = true;

  dma_display = new MatrixPanel_I2S_DMA(cfg);
  dma_display->begin();
  dma_display->setBrightness8(120);          // P5 outdoor uchun 200 juda kop tok tortadi
  dma_display->clearScreen();

  disp = new VirtualMatrixPanel_T<PANEL_CHAIN_TYPE, MyScanTypeMapping>(
           PANEL_ROWS, PANEL_COLS, PANEL_WIDTH, PANEL_HEIGHT);
  disp->setDisplay(*dma_display);

  strcpy(message, "MECANUZ");
  displayType = "MECANUZ";
  textWidth = calculateTextWidth(message);
  xPos = (DISP_W - textWidth) / 2;
}

// ---------- Loop ----------

void loop() {
  static String inputBuffer = "";
  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      inputBuffer.trim();
      if (inputBuffer.length() > 0) parseJSON(inputBuffer);
      inputBuffer = "";
    } else {
      inputBuffer += c;
    }
  }

  static unsigned long lastFrame = 0;
  if (millis() - lastFrame < FRAME_MS) return;
  lastFrame = millis();

  disp->fillScreen(0);
  drawBorderAnimation();

  if (displayType == "MECANUZ") {
    animateMecanuz();
    drawText(xPos, LINE_Y_SINGLE, message, textColor);
  }
  else if (displayType == "ERROR") {
    animateError();
    drawText(xPos, LINE_Y_SINGLE, message, textColor);
  }
  else if (displayType == "SUM") {
    drawText((DISP_W - textWidth) / 2, LINE_Y_SINGLE, message, valueColor);
  }
  else if (displayType == "VALYUTA") {
    drawText((DISP_W - textWidth) / 2, LINE_Y_TOP, message, valueColor);
    int currWidth = calculateTextWidth(currencyStr);
    drawText((DISP_W - currWidth) / 2, LINE_Y_BOTTOM, currencyStr, textColor);
  }
  else if (displayType == "TEST") {
    drawText(xPos, LINE_Y_SINGLE, message, textColor);
    xPos--;
    if (xPos < -textWidth) {
      xPos = DISP_W;
    }
  }
  else {
    bool hideTime = (value == 0 && valueZeroSince != 0 && (millis() - valueZeroSince >= 1000));

    int textY = hideTime ? LINE_Y_SINGLE : LINE_Y_TOP;
    drawText(xPos, textY, message, textColor);

    if (!hideTime && strcmp(message, "ERROR") != 0) {
      int tWidth = calculateTextWidth(timeStr);
      drawText((DISP_W - tWidth) / 2, LINE_Y_BOTTOM, timeStr, valueColor);
    }

    // SKROLL MANTIQI: Uzun matnlar qotib qolmasligi uchun uzluksiz skroll boladi
    if (textWidth > DISP_W) {
      xPos--;
      // Matn toliq chiqib ketgach, yana ong tarafdan kirib kelishni boshlaydi
      if (xPos < -textWidth) {
        xPos = DISP_W;
      }
    }
  }

  disp->flipDMABuffer();
}
