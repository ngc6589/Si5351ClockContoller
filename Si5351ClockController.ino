#include <SPI.h>
#include <Wire.h>
#include <EEPROM.h>
#include <Adafruit_GFX.h>
#include <gfxfont.h>
#include <Adafruit_ILI9341.h>
#include <Adafruit_STMPE610.h>
#include <si5351.h>

void dispCLK(unsigned long long freq, enum si5351_clock clk, int y, uint16_t color);
void cleartextField();

Si5351 si5351;

// The STMPE610 uses hardware SPI on the shield, and #8
#define STMPE_CS 8
Adafruit_STMPE610 ts = Adafruit_STMPE610(STMPE_CS);

// The display also uses hardware SPI, plus #9 & #10
#define TFT_CS 10
#define TFT_DC 9
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);

#define TEXT_X 20
#define TEXT_Y 120
#define TEXT_W 220
#define TEXT_H 50
#define TEXT_TSIZE 3
#define TEXT_TCOLOR ILI9341_YELLOW
#define TEXT_LEN 11
#define BUTTON_X 30
#define BUTTON_Y 180
#define BUTTON_W 45
#define BUTTON_H 25
#define BUTTON_SPACING_X 15
#define BUTTON_SPACING_Y 15
#define BUTTON_TEXTSIZE 2
char buttonlabels[16][3] = {"1", "2", "3", "C0", "4", "5", "6", "C1", "7", "8", "9", "C2", "0", " ", "C", "BS" };
uint16_t buttoncolors[16] = {ILI9341_BLUE, ILI9341_BLUE, ILI9341_BLUE, ILI9341_BLUE,
                             ILI9341_BLUE, ILI9341_BLUE, ILI9341_BLUE, ILI9341_BLUE,
                             ILI9341_BLUE, ILI9341_BLUE, ILI9341_BLUE, ILI9341_BLUE,
                             ILI9341_BLUE, ILI9341_BLUE, ILI9341_BLUE, ILI9341_BLUE
                            };

Adafruit_GFX_Button buttons[16];

struct freq {
  unsigned long long clk0Freq;
  unsigned long long clk1Freq;
  unsigned long long clk2Freq;
  unsigned int clk0Enable;
  unsigned int clk1Enable;
  unsigned int clk2Enable;
} f;
char textField[TEXT_LEN + 1];
unsigned int enableDsiableClockCount = 0;

// setup
void setup() {

  Serial.begin(9600);
  ts.begin();
  tft.begin();
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextColor(ILI9341_WHITE);
  tft.setRotation(0);

  // create buttons
  for (unsigned char row = 0; row < 4; row++) {
    for (unsigned char col = 0; col < 4; col++) {
      buttons[col + row * 4].initButton(&tft, BUTTON_X + col * (BUTTON_W + BUTTON_SPACING_X),
                                        BUTTON_Y + row * (BUTTON_H + BUTTON_SPACING_Y),
                                        BUTTON_W, BUTTON_H,
                                        ILI9341_WHITE,
                                        buttoncolors[col + row * 3],
                                        ILI9341_WHITE,
                                        buttonlabels[col + row * 4],
                                        BUTTON_TEXTSIZE);
      buttons[col + row * 4].drawButton();
    }
  }

  tft.setTextColor(TEXT_TCOLOR, ILI9341_YELLOW);
  tft.setTextSize(1);
  tft.setCursor(55, 156);
  tft.print("MHz");
  tft.setCursor(110, 156);
  tft.print("KHz");
  tft.setCursor(169, 156);
  tft.print("Hz");
  tft.setCursor(195, 156);
  tft.print("Cent");
  tft.drawFastHLine(19, 125, 201, ILI9341_YELLOW);
  tft.drawFastHLine(19, 154, 201, ILI9341_YELLOW);
  tft.drawFastVLine(19, 125, 29, ILI9341_YELLOW);
  tft.drawFastVLine(220, 125, 29, ILI9341_YELLOW);

  EEPROM.get(0, f);

  si5351.init(SI5351_CRYSTAL_LOAD_8PF, 0, 0);
  si5351.set_correction(10150, SI5351_PLL_INPUT_XO);
  si5351.set_freq(f.clk0Freq, SI5351_CLK0);
  si5351.set_freq(f.clk1Freq, SI5351_CLK1);
  si5351.set_freq(f.clk2Freq, SI5351_CLK2);
  si5351.output_enable(SI5351_CLK0, f.clk0Enable);
  si5351.output_enable(SI5351_CLK1, f.clk1Enable);
  si5351.output_enable(SI5351_CLK2, f.clk2Enable);
  si5351.update_status();

  if (f.clk0Enable == 1) dispCLK(f.clk0Freq, SI5351_CLK0, 10, ILI9341_CYAN);
  if (f.clk0Enable == 0) dispCLK(f.clk0Freq, SI5351_CLK0, 10, ILI9341_LIGHTGREY);
  if (f.clk1Enable == 1) dispCLK(f.clk1Freq, SI5351_CLK1, 48, ILI9341_CYAN);
  if (f.clk1Enable == 0) dispCLK(f.clk1Freq, SI5351_CLK1, 48, ILI9341_LIGHTGREY);
  if (f.clk1Enable == 1) dispCLK(f.clk2Freq, SI5351_CLK2, 84, ILI9341_CYAN);
  if (f.clk1Enable == 0) dispCLK(f.clk2Freq, SI5351_CLK2, 84, ILI9341_LIGHTGREY);
  
  cleartextField();
}

void loop() {
  unsigned char i;
  unsigned long long calFreq;

  TS_Point p;

  if (ts.bufferSize()) {
    p = ts.getPoint();
  } else {
    p.x = p.y = p.z = -1;
    enableDsiableClockCount = 0;
  }

  // Scale from ~0->4000 to tft.width using the calibration #'s
  if (p.z != -1) {
    p.x = map(p.x, 150, 3800, 0, tft.width());
    p.y = map(p.y, 130, 3800, 0, tft.height());
    //    Serial.print("("); Serial.print(p.x); Serial.print(", ");
    //    Serial.print(p.y); Serial.print(", ");
    //    Serial.print(p.z); Serial.println(") ");
    ts.touched();
  }

  // enbla/disable clockout

  if ((enableDsiableClockCount == 0) && (p.y > 0 & p.y <= 110)) {
    //Serial.println(enableDsiableClockCount);
    if ((p.y > 30 & p.y < 50)) {
      if (f.clk0Enable == 1) {
        si5351.output_enable(SI5351_CLK0, 0);
        f.clk0Enable = 0;
        dispCLK(f.clk0Freq, SI5351_CLK0, 10, ILI9341_LIGHTGREY);
      } else {
        si5351.output_enable(SI5351_CLK0, 1);
        f.clk0Enable = 1;
        dispCLK(f.clk0Freq, SI5351_CLK0, 10, ILI9341_CYAN);
      }
      EEPROM.put(0, f);
    }
    if ((p.y > 60 & p.y < 80)) {
      if (f.clk1Enable == 1) {
        si5351.output_enable(SI5351_CLK1, 0);
        f.clk1Enable = 0;
        dispCLK(f.clk1Freq, SI5351_CLK1, 48, ILI9341_LIGHTGREY);
      } else {
        si5351.output_enable(SI5351_CLK1, 1);
        f.clk1Enable = 1;
        dispCLK(f.clk1Freq, SI5351_CLK1, 48, ILI9341_CYAN);
      }
      EEPROM.put(0, f);
    }
    if ((p.y > 90 & p.y < 110)) {
      if (f.clk2Enable == 1) {
        si5351.output_enable(SI5351_CLK2, 0);
        f.clk2Enable = 0;
        dispCLK(f.clk2Freq, SI5351_CLK2, 84, ILI9341_LIGHTGREY);
      } else {
        si5351.output_enable(SI5351_CLK2, 1);
        f.clk2Enable = 1;
        dispCLK(f.clk2Freq, SI5351_CLK2, 84, ILI9341_CYAN);
      }
      EEPROM.put(0, f);
    }
    enableDsiableClockCount++;
  }

  // go thru all the buttons, checking if they were pressed
  for (unsigned char b = 0; b < 16; b++) {
    if (buttons[b].contains(p.x, p.y)) {
      buttons[b].press(true);  // tell the button it is pressed
    } else {
      buttons[b].press(false);  // tell the button it is NOT pressed
    }
  }

  for (unsigned char btn = 0; btn < 16; btn++) {
    if (buttons[btn].justReleased()) {
      buttons[btn].drawButton();
    }
    if (buttons[btn].justPressed()) {
      buttons[btn].drawButton(true);

      // if a numberpad button, append the relevant # to the textField
      if ((btn >= 0 & btn <= 2) || (btn >= 4 & btn <= 6) || (btn >= 8 & btn <= 10) || (btn == 12)) {
        for (i = 0; i < 11; i++) {
          textField[i] = textField[i + 1];
        }
        textField[10] = buttonlabels[btn][0];
      }

      // BackSpace Button
      if (btn == 15) {
        for (i = 10; i > 0; i--) {
          textField[i] = textField[i - 1];
        }
        textField[0] = ' ';
      }

      // set Si5351 frequency
      if (btn == 3 || btn == 7 || btn == 11) {
        calFreq = 0;
        for (i = 0; i < 11; i++) {
          if (textField[i] >= '0' && textField[i] <= '9') {
            calFreq *= 10;
            calFreq += (textField[i] - 0x30);
          }
        }
        if (btn == 3) {
          f.clk0Freq = calFreq;
          si5351.set_freq(f.clk0Freq, SI5351_CLK0);
          dispCLK(f.clk0Freq, SI5351_CLK0, 10, ILI9341_CYAN);
        }
        if (btn == 7) {
          f.clk1Freq = calFreq;
          si5351.set_freq(f.clk1Freq, SI5351_CLK1);
          dispCLK(f.clk1Freq, SI5351_CLK1, 48, ILI9341_CYAN);
        }
        if (btn == 11) {
          f.clk2Freq = calFreq;
          si5351.set_freq(f.clk2Freq, SI5351_CLK2);
          dispCLK(f.clk2Freq, SI5351_CLK2, 84, ILI9341_CYAN);
        }
        cleartextField();
        EEPROM.put(0, f);
      }

      // Clear Button
      if (btn == 14) {
        cleartextField();
      }

      // update the current text field
      tft.setCursor(TEXT_X + 2, TEXT_Y + 10);
      tft.setTextColor(TEXT_TCOLOR, ILI9341_BLACK);
      tft.setTextSize(TEXT_TSIZE);
      tft.print(textField);
      delay(100); // UI debouncing
    }
  }
  //delay(50); // UI debouncing
}

void dispCLK(unsigned long long freq, enum si5351_clock clk, int y, uint16_t color) {

  char str[12];

  for (int i = 10; i >= 0; i--) {
    str[i] = (freq % 10 + 0x30);
    freq /= 10;
  }
  str[11] = '\0';
  tft.setCursor(23, y);
  tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  tft.setTextSize(1);
  if (clk == SI5351_CLK0) {
    tft.print("CLK0");
  }
  if (clk == SI5351_CLK1) {
    tft.print("CLK1");
  }
  if (clk == SI5351_CLK2) {
    tft.print("CLK2");
  }
  tft.setCursor(23, y + 10);

  tft.setTextColor(color, ILI9341_BLACK);
  tft.setTextSize(TEXT_TSIZE);
  tft.print(str);
}

void cleartextField() {

  for (int i = 0; i < TEXT_LEN; i++) {
    textField[i] = ' ';
  }
  textField[TEXT_LEN] = '\0';
}

