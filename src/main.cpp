#include <Arduino.h>
#include <LedControl.h>
#include <LiquidCrystal_I2C.h>
#include <RTClib.h>

#define showTimeDelay 10000
#define showDateDelay 3000
#define showTempDelay 3000

char fullDateFormat[] = "DD.MM.YYYY hh:mm:ss";
char dateFormat[] = "DDMM";
char timeFormat[] = "hhmm";

LedControl lc = LedControl(8, 10, 9, 1);  // DIN, CLK, CS, num | layout: fc16
LiquidCrystal_I2C lcd(0x3F, 16, 2);       // custom I2C address
RTC_DS3231 rtc;                           // in real project using DS3231 instead of DS1307

DateTime rtcNow;
uint32_t curMillis = 0;
uint32_t lastViewStateChange = 0;
uint32_t lastSetStateChange = 0;
uint8_t viewState = 0;

uint8_t matrix[8] = { 60, 66, 165, 129, 165, 153, 66, 60 };  // ☺

struct Symbol {
  char letter;
  uint8_t glyph[3];
};

const Symbol symbolMaps[] = {
  { '+', { 4, 14, 4 } },
  { '-', { 4, 4, 4 } },
  { 'C', { 31, 17, 17 } },
  { '0', { 31, 17, 31 } },
  { '1', { 0, 0, 31 } },
  { '2', { 23, 21, 29 } },
  { '3', { 17, 21, 31 } },
  { '4', { 28, 4, 31 } },
  { '5', { 29, 21, 23 } },
  { '6', { 31, 21, 23 } },
  { '7', { 16, 16, 31 } },
  { '8', { 31, 21, 31 } },
  { '9', { 29, 21, 31 } },
};

void printMatrix() {
  for (uint8_t i = 0; i < 8; i++) {
    lc.setRow(0, i, matrix[i]);
  }
}

void printDataToLcd(const char c[6]) {
  lcd.setCursor(5, 1);
  lcd.print('[');
  lcd.print(c);
  lcd.print(']');
}

void setSymbol(const char c, const uint8_t p) {
  const uint8_t dg[3] = { 0, 0, 0 };  // default glyph is 000
  const uint8_t *g = dg;
  uint8_t rowMask;

  for (const auto &s : symbolMaps) {
    if (s.letter == c) {
      g = s.glyph;
      break;
    }
  }

  for (uint8_t col = 0; col < 3; col++) {
    const uint8_t gBits = g[col];
    const uint8_t gMask = gBits << (3 - col);

    switch (p) {
      case 0:
        // вертикальное заполнение от 0x0
        rowMask = 1 << (7 - col);
        for (uint8_t row = 0; row < 5; row++) {
          matrix[row] &= ~rowMask;
          matrix[row] |= rowMask & (gMask << row);
        }
        break;
      case 1:
        // горизонтальное заполнение от 4x0
        matrix[2 - col] &= 0b11100000;
        matrix[2 - col] |= gBits;
        break;
      case 2:
        // вертикальное заполнение от 5x4
        rowMask = 1 << (2 - col);
        for (uint8_t row = 3; row < 8; row++) {
          matrix[row] &= ~rowMask;
          matrix[row] |= rowMask & (gMask >> (8 - row));
        }
        break;
      case 3:
        // горизонтальное заполнение от 0x5
        matrix[7 - col] &= 0b00000111;
        matrix[7 - col] |= gBits << 3;
        break;
    }
  }
}

void setMatrix(char c[6]) {
  setSymbol(c[0], 0);
  setSymbol(c[1], 1);
  setSymbol(c[3], 2);
  setSymbol(c[4], 3);

  if (c[2] == '.') {
    matrix[3] |= (1 << 4);  // Устанавливаем бит
  } else {
    matrix[3] &= ~(1 << 4);  // Сбрасываем бит
  }
}

void printDataToMatrix(char c[6]) {
  setMatrix(c);
  printMatrix();
}

void showData(const char i[5], bool d) {
  char o[6];
  snprintf(o, 6, "%c%c%c%c%c", i[0], i[1], d ? '.' : ' ', i[2], i[3]);
  printDataToLcd(o);
  printDataToMatrix(o);
}

void showTime() {
  rtcNow = rtc.now();
  showData(rtcNow.toString(timeFormat), rtcNow.second() % 2 == 0);
}

void showDate() {
  rtcNow = rtc.now();
  showData(rtcNow.toString(dateFormat), true);
}

void showWeather() {
  float fTemp = rtc.getTemperature();
  uint8_t iTemp = round(abs(fTemp));
  char sTemp[5];

  if (iTemp == 0) {
    snprintf(sTemp, 5, "  0C");
  } else if (iTemp < 10) {
    snprintf(sTemp, 5, " %c%d%c", fTemp < 0 ? '-' : '+', iTemp, 'C');
  } else {
    snprintf(sTemp, 5, " %c%02d", fTemp < 0 ? '-' : '+', iTemp);
  }
  showData(sTemp, false);
}

void updateClockView() {
  switch (viewState % 3) {
    case 0:
      showTime();
      if (lastViewStateChange + showTimeDelay < curMillis) {
        viewState++;
        lastViewStateChange = curMillis;
        showDate();
      }
      break;
    case 1:
      if (lastViewStateChange + showDateDelay < curMillis) {
        viewState++;
        lastViewStateChange = curMillis;
        showWeather();
      }
      break;
    case 2:
      if (lastViewStateChange + showTempDelay < curMillis) {
        viewState++;
        lastViewStateChange = curMillis;
        showTime();
      }
      break;
  }
}



void setup() {
  lcd.begin(16, 2);
  rtc.begin();

  rtcNow = rtc.now();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print(rtcNow.toString(fullDateFormat));

  lc.shutdown(0, false);
  lc.setIntensity(0, 10);
  lc.clearDisplay(0);

  printMatrix();
}

void loop() {
  curMillis = millis();

  updateClockView();

  delay(100);
}
