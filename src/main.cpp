#include <Arduino.h>
#include <EncButton.h>
#include <LedControl.h>
#include <LiquidCrystal_I2C.h>
#include <RTClib.h>

#define showTimeDelay 10000
#define showDateDelay 3000
#define showTempDelay 3000
#define modeDelay 10000
char fullDateFormat[] = "DD.MM.YYYY hh:mm:ss";
char dateFormat[] = "DDMM";
char timeFormat[] = "hhmm";

#define MAX_AV_LEVEL 600
#define MIN_AV_LEVEL 300
#define MAX_INTENSITY 10
#define MIN_INTENSITY 1
#define LDR_PIN A7
const uint16_t INTENSITY_STEP = round((MAX_AV_LEVEL - MIN_AV_LEVEL) / (MAX_INTENSITY - MIN_INTENSITY));

LedControl lc = LedControl(8, 10, 9, 1);  // DIN, CLK, CS, num | layout: fc16
LedControl lc2 = LedControl(7, 5, 6, 4);  // DIN, CLK, CS, num | layout: default
LiquidCrystal_I2C lcd(0x3F, 16, 2);       // custom I2C address
RTC_DS3231 rtc;                           // in real project using DS3231 instead of DS1307
Button btnMode(A0);
Button btnMinus(A1);
Button btnPlus(A2);

DateTime rtcNow;
uint32_t curMillis = 0;
uint32_t lastViewStateChange = 0;
uint32_t lastModeStateChange = 0;
uint8_t viewState = 0;
uint8_t modeState = 0;

uint8_t matrix[8] = { 60, 66, 165, 129, 165, 153, 66, 60 };  // ☺
uint8_t matrix2[4][11];

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
  for (uint8_t row = 0; row < 8; row++) {
    lc.setRow(0, row, matrix[row]);
  }
}

void printMatrix2() {
  for (uint8_t seg = 0; seg < 4; seg++) {
    for (uint8_t row = 0; row < 8; row++) {
      lc2.setRow((3 - seg), row, matrix2[seg][row]);
    }
  }
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

void setSymbol2(const char c, const uint8_t seg) {
  const uint8_t dg[3] = { 0, 0, 0 };  // default glyph is 000
  const uint8_t *g = dg;
  const uint8_t colMask[3] = { 96, 28, 3 };

  for (const auto &s : symbolMaps) {
    if (s.letter == c) {
      g = s.glyph;
      break;
    }
  }

  for (uint8_t row = 0; row < 11; row++) {
    matrix2[seg][row] = 0;
  }

  for (uint8_t col = 0; col < 3; col++) {
    const uint8_t gMask = g[col] << (3 - col);

    for (uint8_t row = 0; row < 5; row++) {
      const uint8_t bit = (1 << (7 - col)) & (gMask << row);
      if (bit) {
        matrix2[seg][row * 2] |= colMask[col];
        matrix2[seg][row * 2 + 1] |= colMask[col];
      }
    }
  }
  matrix2[seg][10] = matrix2[seg][9];
  matrix2[seg][8] = matrix2[seg][7];
}

void setMatrix(const char c[5], bool d) {
  for (uint8_t i = 0; i < 4; i++) {
    setSymbol(c[i], i);
  }

  if (d) {
    matrix[3] |= (1 << 4);  // Устанавливаем бит
  } else {
    matrix[3] &= ~(1 << 4);  // Сбрасываем бит
  }
}

void setMatrix2(const char c[5], bool d) {
  for (uint8_t seg = 0; seg < 4; seg++) {
    setSymbol2(c[seg], seg);
  }
  // TODO: if (d)
}

void printDataToMatrix(const char c[5], bool d) {
  setMatrix(c, d);
  printMatrix();
}

void printDataToMatrix2(const char c[5], bool d) {
  setMatrix2(c, d);
  printMatrix2();
}

void printDataToLcd(const char c[5], bool d) {
  lcd.setCursor(6, 1);
  lcd.print(c[0]);
  lcd.print(c[1]);
  lcd.print(":");
  lcd.print(c[2]);
  lcd.print(c[3]);
}

void showData(const char c[5], bool d) {
  if (!modeState) {
    printDataToLcd(c, d);
  }
  printDataToMatrix(c, d);
  printDataToMatrix2(c, d);
}

void printFullTime() {
  lcd.setCursor(0, 0);
  rtcNow = rtc.now();
  lcd.print(rtcNow.toString(fullDateFormat));
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
  if (lastViewStateChange > curMillis) {
    lastViewStateChange = curMillis;
  }

  switch (viewState % 3) {
    case 0:
      if (lastViewStateChange + showTimeDelay < curMillis) {
        viewState++;
        lastViewStateChange = curMillis;
        printFullTime();
        showDate();
      }
      break;
    case 1:
      if (lastViewStateChange + showDateDelay < curMillis) {
        viewState++;
        lastViewStateChange = curMillis;
        printFullTime();
        showWeather();
      }
      break;
    case 2:
      if (lastViewStateChange + showTempDelay < curMillis) {
        viewState++;
        lastViewStateChange = curMillis;
        printFullTime();
        showTime();
      }
      break;
  }
}

void updateClockMode() {
  btnMode.tick();
  btnMinus.tick();
  btnPlus.tick();

  if (btnMode.press()) {
    modeState++;
    lastModeStateChange = curMillis;
    lcd.backlight();
    lcd.setCursor(0, 1);
    lcd.print("                ");  // faster than lcd.clear()
    printFullTime();

    switch (modeState % 6) {
      case 0:
        modeState++;
      case 1:
        lcd.setCursor(14, 1);
        lcd.print("^^");
        break;
      case 2:
        lcd.setCursor(11, 1);
        lcd.print("^^");
        break;
      case 3:
        lcd.setCursor(6, 1);
        lcd.print("^^^^");
        break;
      case 4:
        lcd.setCursor(3, 1);
        lcd.print("^^");
        break;
      case 5:
        lcd.setCursor(0, 1);
        lcd.print("^^");
        break;
    }
  }

  if (modeState) {
    if (lastModeStateChange + modeDelay < curMillis || lastModeStateChange > curMillis) {
      modeState = 0;
      lcd.noBacklight();
      lcd.setCursor(0, 1);
      lcd.print("                ");  // faster than lcd.clear()
    }
  }
}

void updateIntensity() {
  uint16_t analogValue = min(max(analogRead(LDR_PIN), MIN_AV_LEVEL), MAX_AV_LEVEL);
  uint8_t intensityLevel = MAX_INTENSITY - round((analogValue - MIN_AV_LEVEL) / INTENSITY_STEP);

  lcd.setCursor(-1, 1);
  lcd.print((100 + intensityLevel));

  lc.setIntensity(0, intensityLevel);
  lc2.setIntensity(0, intensityLevel);
}

void setup() {
  lcd.begin(16, 2);
  rtc.begin();

  pinMode(LDR_PIN, INPUT);

  lc.setIntensity(0, 1);
  lc.clearDisplay(0);

  lc2.setIntensity(0, 1);
  lc2.clearDisplay(0);

  printFullTime();
  showTime();
}

void loop() {
  curMillis = millis();

  updateClockView();
  updateClockMode();
  updateIntensity();

  delay(20);
}
