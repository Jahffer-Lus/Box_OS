#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>

#define TFT_CS   10
#define TFT_DC    9
#define TFT_RST   8
#define TFT_MOSI  4
#define TFT_SCLK  5

Adafruit_ST7735 tft(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

void setup() {
  tft.initR(INITR_REDTAB);
  tft.setRotation(1);

  tft.fillScreen(ST77XX_BLACK);
}

void loop() {
  tft.fillScreen(ST77XX_BLACK);
  delay(2000);

  tft.fillScreen(ST77XX_RED);
  delay(2000);

  tft.fillScreen(ST77XX_GREEN);
  delay(2000);

  tft.fillScreen(ST77XX_BLUE);
  delay(2000);

  tft.fillScreen(ST77XX_WHITE);
  delay(2000);
}