/*
  BOX OS 2.0 — SUPER UPDATE (base inicial)
  Arduino Uno R3 + TFT ST7735 128x160 + microSD + joystick
  RTC DS1302 por comunicacao direta (sem biblioteca RTC).

  Bibliotecas necessarias:
  - Adafruit GFX Library
  - Adafruit ST7735 and ST7789 Library
  - SD (inclusa no Arduino IDE)

  PINOS:
  Joystick: X=A0, Y=A1, SW=D2
  RTC DS1302: DAT=D3, CLK=D4, CE=A2
  Buzzer passivo: D1 (TX) — pode atrapalhar upload/Serial.
  TFT (SPI por software): CS=D10, DC=D8, RST=D7, MOSI=A4, SCK=A5
  microSD (SPI por hardware): CS=D9, MOSI=D11, MISO=D12, SCK=D13

  IMPORTANTE:
  - Confirme as etiquetas e a tensao do seu modulo antes de ligar.
  - D1 e TX: desconecte o buzzer durante uploads se houver falhas.
  - D11/D12/D13 ficam reservados ao SPI do cartao SD.
*/

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <SD.h>
#include <string.h>

// ---------- PINOS ----------
#define JOY_X A0
#define JOY_Y A1
#define JOY_SW 2

#define BUZZER_PIN 1

#define RTC_DAT 3
#define RTC_CLK 4
#define RTC_CE  A2

#define TFT_CS   10
#define TFT_DC   8
#define TFT_RST  7
#define TFT_MOSI A4
#define TFT_SCLK A5

#define SD_CS 9

Adafruit_ST7735 tft(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

// ---------- CORES ----------
#define C_BG       0x0841
#define C_PANEL    0x10A3
#define C_CYAN     0x07FF
#define C_WHITE    0xFFFF
#define C_GRAY     0xBDF7
#define C_DARK     0x4208
#define C_GREEN    0x07E0
#define C_YELLOW   0xFFE0
#define C_RED      0xF800
#define C_BLUE     0x249F

// ---------- SISTEMA ----------
const char* apps[] = {
  "TERMINAL", "MONITOR", "CALCULADORA", "SNAKE",
  "DINO RUN", "BOX FILES", "NOTIFICACOES", "TEMAS",
  "GAME CENTER", "SYSTEM INFO", "BOX STORE", "CONTROL CENTER"
};
const uint8_t APP_COUNT = sizeof(apps) / sizeof(apps[0]);

uint8_t selectedApp = 0;
uint8_t menuPage = 0;
const uint8_t APPS_PER_PAGE = 4;

enum ScreenMode { SCR_MENU, SCR_APP, SCR_INFO };
ScreenMode screenMode = SCR_MENU;

unsigned long bootMillis = 0;
unsigned long lastMove = 0;
unsigned long lastButton = 0;
bool lastButtonState = HIGH;
bool sdReady = false;
bool rtcReady = false;
bool backlightState = true;
uint8_t themeIndex = 0;

uint16_t bgColor() { return themeIndex == 1 ? 0x0000 : (themeIndex == 2 ? 0x2104 : C_BG); }
uint16_t accentColor() { return themeIndex == 1 ? C_GREEN : (themeIndex == 2 ? C_YELLOW : C_CYAN); }

// ---------- BUZZER ----------
void beep(uint16_t freq = 1500, uint16_t duration = 35) {
  tone(BUZZER_PIN, freq, duration);
}

// ---------- RTC DS1302: FUNCOES BASICAS ----------
uint8_t bcdToDec(uint8_t v) { return ((v >> 4) * 10) + (v & 0x0F); }

void rtcBegin() {
  pinMode(RTC_CE, OUTPUT);
  pinMode(RTC_CLK, OUTPUT);
  pinMode(RTC_DAT, INPUT_PULLUP);
  digitalWrite(RTC_CE, LOW);
  digitalWrite(RTC_CLK, LOW);
}

uint8_t rtcReadRegister(uint8_t address) {
  uint8_t value = 0;
  pinMode(RTC_DAT, OUTPUT);
  digitalWrite(RTC_CE, HIGH);
  uint8_t command = (address | 0x01);
  for (uint8_t i = 0; i < 8; i++) {
    digitalWrite(RTC_DAT, (command >> i) & 1);
    digitalWrite(RTC_CLK, HIGH);
    digitalWrite(RTC_CLK, LOW);
  }
  pinMode(RTC_DAT, INPUT);
  for (uint8_t i = 0; i < 8; i++) {
    if (digitalRead(RTC_DAT)) value |= (1 << i);
    digitalWrite(RTC_CLK, HIGH);
    digitalWrite(RTC_CLK, LOW);
  }
  digitalWrite(RTC_CE, LOW);
  return value;
}

void rtcWriteRegister(uint8_t address, uint8_t value) {
  pinMode(RTC_DAT, OUTPUT);
  digitalWrite(RTC_CE, HIGH);
  uint8_t command = (address & 0xFE);
  for (uint8_t i = 0; i < 8; i++) {
    digitalWrite(RTC_DAT, (command >> i) & 1);
    digitalWrite(RTC_CLK, HIGH);
    digitalWrite(RTC_CLK, LOW);
  }
  for (uint8_t i = 0; i < 8; i++) {
    digitalWrite(RTC_DAT, (value >> i) & 1);
    digitalWrite(RTC_CLK, HIGH);
    digitalWrite(RTC_CLK, LOW);
  }
  digitalWrite(RTC_CE, LOW);
  pinMode(RTC_DAT, INPUT_PULLUP);
}

bool rtcReadTime(uint8_t &ss, uint8_t &mm, uint8_t &hh,
                 uint8_t &day, uint8_t &month, uint8_t &year) {
  uint8_t seconds = rtcReadRegister(0x80);
  if (seconds & 0x80) return false; // oscilador parado
  ss = bcdToDec(seconds & 0x7F);
  mm = bcdToDec(rtcReadRegister(0x82) & 0x7F);
  uint8_t hourRaw = rtcReadRegister(0x84);
  if (hourRaw & 0x80) {
    uint8_t h = bcdToDec(hourRaw & 0x1F);
    bool pm = hourRaw & 0x20;
    hh = (h % 12) + (pm ? 12 : 0);
  } else {
    hh = bcdToDec(hourRaw & 0x3F);
  }
  day = bcdToDec(rtcReadRegister(0x86) & 0x3F);
  month = bcdToDec(rtcReadRegister(0x88) & 0x1F);
  year = bcdToDec(rtcReadRegister(0x8C));
  return true;
}

// ---------- DESENHO ----------
void drawHeader(const char* title) {
  tft.fillScreen(bgColor());
  tft.fillRect(0, 0, 160, 22, C_PANEL);
  tft.setTextColor(accentColor());
  tft.setTextSize(1);
  tft.setCursor(7, 7);
  tft.print("BOX OS 2.0");
  tft.setTextColor(C_WHITE);
  int16_t x1, y1; uint16_t w, h;
  tft.getTextBounds(title, 0, 0, &x1, &y1, &w, &h);
  tft.setCursor(121 - w, 7);
  tft.print(title);
}

void drawStartup() {
  tft.fillScreen(bgColor());
  tft.drawRoundRect(34, 35, 92, 80, 12, accentColor());
  tft.drawRoundRect(43, 44, 74, 62, 8, C_WHITE);
  tft.setTextColor(accentColor());
  tft.setTextSize(3);
  tft.setCursor(55, 63);
  tft.print("B");
  tft.setTextSize(1);
  tft.setTextColor(C_WHITE);
  tft.setCursor(47, 126);
  tft.print("INICIANDO BOX OS");
  for (uint8_t i = 0; i < 5; i++) {
    tft.fillRect(42, 141, i * 18 + 4, 4, accentColor());
    delay(100);
  }
}

void drawMenu() {
  drawHeader("MENU");
  uint8_t first = menuPage * APPS_PER_PAGE;
  for (uint8_t i = 0; i < APPS_PER_PAGE; i++) {
    uint8_t app = first + i;
    if (app >= APP_COUNT) break;
    uint8_t col = i % 2;
    uint8_t row = i / 2;
    int16_t x = 3 + col * 62;
    int16_t y = 29 + row * 57;
    bool selected = (app == selectedApp);
    tft.fillRoundRect(x, y, 58, 50, 7, selected ? C_PANEL : bgColor());
    tft.drawRoundRect(x, y, 58, 50, 7, selected ? accentColor() : C_DARK);
    tft.setTextColor(selected ? accentColor() : C_WHITE);
    tft.setTextSize(2);
    tft.setCursor(x + 23, y + 7);
    tft.print(i + 1);
    tft.setTextSize(1);
    tft.setTextColor(C_WHITE);
    int16_t tx = x + 3;
    if (strlen(apps[app]) > 8) tx = x + 1;
    tft.setCursor(tx, y + 32);
    tft.print(apps[app]);
  }
  tft.setTextColor(C_GRAY);
  tft.setCursor(8, 145);
  tft.print("SW: abrir   CIMA/BAIXO: paginas");
  tft.setCursor(112, 7);
  tft.print(menuPage + 1);
}

void drawApp(uint8_t app) {
  drawHeader(apps[app]);
  tft.setTextColor(C_WHITE);
  tft.setTextSize(1);
  tft.setCursor(8, 34);

  switch (app) {
    case 0: // TERMINAL
      tft.print(F("BOX TERMINAL"));
      tft.setCursor(8, 52); tft.print(F("> Terminal pronto"));
      tft.setCursor(8, 68); tft.print(F("> Teclado virtual:"));
      tft.setCursor(8, 80); tft.print(F("em desenvolvimento"));
      tft.setCursor(8, 112); tft.setTextColor(accentColor());
      tft.print(F("SW para voltar"));
      break;
    case 1: // MONITOR
      tft.print(F("TEMPO LIGADO"));
      tft.setTextSize(2);
      tft.setCursor(8, 58); tft.print((millis() - bootMillis) / 1000);
      tft.setTextSize(1); tft.print(F(" s"));
      tft.setCursor(8, 92); tft.print(F("Memoria livre: "));
      tft.print(freeRam());
      tft.print(F(" B"));
      break;
    case 2: // CALCULADORA
      tft.print(F("CALCULADORA"));
      tft.setCursor(8, 54); tft.print(F("Modulo reservado."));
      tft.setCursor(8, 70); tft.print(F("Interface sera"));
      tft.setCursor(8, 82); tft.print(F("adicionada na"));
      tft.setCursor(8, 94); tft.print(F("proxima etapa."));
      break;
    case 3:
    case 4:
      tft.print(app == 3 ? F("SNAKE") : F("DINO RUN"));
      tft.setCursor(8, 54); tft.print(F("Jogo selecionado!"));
      tft.setCursor(8, 70); tft.print(F("Mecanica do jogo"));
      tft.setCursor(8, 82); tft.print(F("sera adicionada"));
      tft.setCursor(8, 94); tft.print(F("na proxima etapa."));
      break;
    case 5: // BOX FILES
      tft.print(F("BOX FILES / microSD"));
      tft.setCursor(8, 54); tft.print(F("Cartao: "));
      tft.setTextColor(sdReady ? C_GREEN : C_RED);
      tft.print(sdReady ? F("CONECTADO") : F("NAO DETECTADO"));
      tft.setTextColor(C_WHITE);
      if (sdReady) {
        File root = SD.open("/");
        uint16_t count = 0;
        if (root) {
          while (true) {
            File entry = root.openNextFile();
            if (!entry) break;
            count++;
            entry.close();
          }
          root.close();
        }
        tft.setCursor(8, 72); tft.print(F("Itens na raiz: ")); tft.print(count);
      }
      tft.setCursor(8, 112); tft.setTextColor(accentColor());
      tft.print(F("Leitura basica SD"));
      break;
    case 6: // NOTIFICACOES
      tft.print(F("CENTRAL DE AVISOS"));
      tft.setCursor(8, 54); tft.print(F("Nenhum aviso novo."));
      break;
    case 7: // TEMAS
      tft.print(F("BOX THEMES"));
      tft.setCursor(8, 54); tft.print(F("Tema atual: "));
      tft.print(themeIndex == 0 ? F("CYAN") : (themeIndex == 1 ? F("GREEN") : F("GOLD")));
      tft.setCursor(8, 76); tft.print(F("Mova joystick"));
      tft.setCursor(8, 88); tft.print(F("esquerda/direita"));
      tft.setCursor(8, 100); tft.print(F("para trocar tema."));
      break;
    case 8: // GAME CENTER
      tft.print(F("BOX GAME CENTER"));
      tft.setCursor(8, 54); tft.print(F("SNAKE + DINO RUN"));
      tft.setCursor(8, 72); tft.print(F("Selecione os jogos"));
      tft.setCursor(8, 84); tft.print(F("no menu principal."));
      break;
    case 9: // SYSTEM INFO
      tft.print(F("ARDUINO UNO R3"));
      tft.setCursor(8, 52); tft.print(F("Flash: 32 KB"));
      tft.setCursor(8, 66); tft.print(F("SRAM: 2 KB"));
      tft.setCursor(8, 80); tft.print(F("EEPROM: 1 KB"));
      tft.setCursor(8, 100); tft.print(F("SD: "));
      tft.print(sdReady ? F("OK") : F("OFF"));
      tft.setCursor(8, 114); tft.print(F("RTC: "));
      tft.print(rtcReady ? F("OK") : F("VERIFICAR"));
      break;
    case 10: // STORE
      tft.print(F("BOX STORE"));
      tft.setCursor(8, 54); tft.print(F("Loja de aplicativos"));
      tft.setCursor(8, 70); tft.print(F("local planejada."));
      break;
    case 11: // CONTROL CENTER
      tft.print(F("BOX CONTROL CENTER"));
      tft.setCursor(8, 50); tft.print(F("Tema: "));
      tft.print(themeIndex == 0 ? F("CYAN") : (themeIndex == 1 ? F("GREEN") : F("GOLD")));
      tft.setCursor(8, 66); tft.print(F("Luz: "));
      tft.print(backlightState ? F("LIGADA") : F("DESLIGADA"));
      tft.setCursor(8, 88); tft.print(F("Esq/Dir: tema"));
      tft.setCursor(8, 102); tft.print(F("Cima: luz logica"));
      break;
  }
  tft.setTextColor(C_GRAY);
  tft.setCursor(8, 145);
  tft.print(F("SW: voltar"));
}

int freeRam() {
  extern int __heap_start, *__brkval;
  int v;
  return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
}

void enterApp() {
  screenMode = SCR_APP;
  if (selectedApp == 7) { /* tema pode ser alterado pelo joystick */ }
  drawApp(selectedApp);
}

void handleAppInput(int x, int y) {
  if (selectedApp == 7) {
    if (x < 350) themeIndex = (themeIndex + 2) % 3;
    else if (x > 670) themeIndex = (themeIndex + 1) % 3;
    drawApp(selectedApp);
  } else if (selectedApp == 11) {
    if (x < 350) themeIndex = (themeIndex + 2) % 3;
    else if (x > 670) themeIndex = (themeIndex + 1) % 3;
    else if (y < 350) backlightState = !backlightState;
    drawApp(selectedApp);
  }
}

void setup() {
  pinMode(JOY_SW, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  rtcBegin();
  uint8_t rs, rm, rh, rd, rmo, ry;
  rtcReady = rtcReadTime(rs, rm, rh, rd, rmo, ry);

  tft.initR(INITR_BLACKTAB);
  tft.setRotation(0); // 128 x 160
  tft.setTextWrap(false);
  drawStartup();

  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);
  sdReady = SD.begin(SD_CS);

  bootMillis = millis();
  drawMenu();
  beep(1200, 45);
}

void loop() {
  int x = analogRead(JOY_X);
  int y = analogRead(JOY_Y);
  bool buttonState = digitalRead(JOY_SW);
  unsigned long now = millis();

  if (screenMode == SCR_MENU) {
    if (now - lastMove > 190) {
      bool changed = false;
      if (x < 350) { if (selectedApp % 2 == 1) selectedApp--; changed = true; }
      else if (x > 670) { if (selectedApp % 2 == 0 && selectedApp + 1 < APP_COUNT) selectedApp++; changed = true; }
      if (y < 350) { if (selectedApp >= 2) selectedApp -= 2; changed = true; }
      else if (y > 670) { if (selectedApp + 2 < APP_COUNT) selectedApp += 2; changed = true; }
      if (changed) {
        menuPage = selectedApp / APPS_PER_PAGE;
        drawMenu();
        lastMove = now;
      }
    }
    if (lastButtonState == HIGH && buttonState == LOW && now - lastButton > 250) {
      lastButton = now;
      beep();
      enterApp();
    }
  } else if (screenMode == SCR_APP) {
    if (now - lastMove > 220 && (selectedApp == 7 || selectedApp == 11)) {
      if (x < 350 || x > 670 || (selectedApp == 11 && y < 350)) {
        handleAppInput(x, y);
        lastMove = now;
      }
    }
    if (lastButtonState == HIGH && buttonState == LOW && now - lastButton > 250) {
      lastButton = now;
      beep(1000, 30);
      screenMode = SCR_MENU;
      drawMenu();
    }
  }
  lastButtonState = buttonState;
  delay(8);
}
