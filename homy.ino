#include <M5Unified.h>
#include <WiFi.h>
#include <SD.h>
#include "time.h"
#include <Preferences.h>

// Wi-Fi
const char* ssid     = "scorty";
const char* password = "15102010";

Preferences prefs;

// Палитра
const struct {
  const char* name;
  uint16_t color;
} colorPalette[] = {
  {"White",       TFT_WHITE},
  {"Black",       TFT_BLACK},
  {"Pink",        0xF9BE},
  {"Peach",       0xFDA0},
  {"Lavender",    0xE73C},
  {"Mint",        0x7E5D},
  {"Sky",         0x841C},
  {"Coral",       0xFBEA},
  {"Rose",        0xF81F},
  {"Violet",      0x881F},
  {"Cyan",        TFT_CYAN},
  {"Green",       TFT_GREEN},
  {"Lime",        0xB720},
  {"Yellow",      TFT_YELLOW},
  {"Orange",      0xFD20},
  {"Red",         TFT_RED},
  {"Purple",      0x781F},
  {"Reset Theme", 0xFFFF}
};
const int paletteSize = sizeof(colorPalette) / sizeof(colorPalette[0]);

// Настройки
int tzOffset = 7;
uint16_t timeColor = TFT_WHITE;
uint16_t secColor = 0xF9BE;
uint16_t battPercentColor = TFT_WHITE;
uint16_t battIconColor = 0xF9BE;
uint16_t bgColor = TFT_BLACK;

enum AppState {
  MAIN,
  SETTINGS_MENU,
  SET_TIMEZONE,
  SET_TIME_COLOR,
  SET_SEC_COLOR,
  SET_BATT_PERCENT_COLOR,
  SET_BATT_ICON_COLOR,
  SET_BG_COLOR
};
AppState appState = MAIN;
int menuIndex = 0;
int colorIndex = 0;

const char* menuItems[] = {
  "Timezone",
  "Time Color",
  "Seconds Color",
  "Batt % Color",
  "Batt Icon Color",
  "Background",
  "Back"
};
const int menuCount = 7;

bool firstDraw = true;
unsigned long holdStart = 0;
bool inHold = false;

// ------------------ Homy Theme ------------------
void applyRetroCraftTheme() {
  timeColor = TFT_WHITE;
  secColor = 0xF9BE;
  battPercentColor = TFT_WHITE;
  battIconColor = 0xF9BE;
  bgColor = TFT_BLACK;
  tzOffset = 7;
  saveSettings();
}

// ------------------ ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ------------------

void drawBatteryIcon(int x, int y, int percent, uint16_t iconColor) {
  auto& d = M5.Display;
  int fill = map(constrain(percent, 0, 100), 0, 100, 0, 18);
  // Контур
  d.drawRect(x, y, 20, 10, timeColor);
  d.fillRect(x + 20, y + 2, 2, 6, timeColor);
  // Заливка
  d.fillRect(x + 1, y + 1, fill, 8, iconColor);
}

void drawMainScreen(bool animate = false) {
  auto& d = M5.Display;
  d.fillScreen(bgColor);

  struct tm ti;
  if (!getLocalTime(&ti, 1000)) {
    d.setTextDatum(middle_center);
    d.setTextColor(TFT_RED);
    d.drawString("No time", d.width()/2, d.height()/2);
    return;
  }

  char tBuf[6], sBuf[3];
  strftime(tBuf, sizeof(tBuf), "%H:%M", &ti);
  strftime(sBuf, sizeof(sBuf), "%S", &ti);

  float v = M5.Power.getBatteryVoltage();
  int bp = (v <= 3.2) ? 0 : (v >= 4.2) ? 100 : (int)((v - 3.2) / 1.0 * 100);

  if (animate && firstDraw) {
    firstDraw = false;
    d.setTextDatum(middle_center);
    d.setTextSize(2);
    d.setTextColor(secColor);
    d.drawString(sBuf, d.width()/2, d.height()/2 + 16);
    delay(100);
    d.setTextSize(3);
    d.setTextColor(timeColor);
    d.drawString(tBuf, d.width()/2, d.height()/2 - 12);
  } else {
    d.setTextDatum(middle_center);
    d.setTextSize(3);
    d.setTextColor(timeColor);
    d.drawString(tBuf, d.width()/2, d.height()/2 - 12);
    d.setTextSize(2);
    d.setTextColor(secColor);
    d.drawString(sBuf, d.width()/2, d.height()/2 + 16);
  }

  drawBatteryIcon(d.width() - 25, 5, bp, battIconColor);
  d.setTextSize(1);
  d.setTextDatum(top_left);
  d.setTextColor(battPercentColor);
  d.setCursor(5, 5);
  d.printf("%d%%", bp);
}

void saveSettings() {
  prefs.putInt("tz", tzOffset);
  prefs.putUShort("timeC", timeColor);
  prefs.putUShort("secC", secColor);
  prefs.putUShort("battPercC", battPercentColor);
  prefs.putUShort("battIconC", battIconColor);
  prefs.putUShort("bgC", bgColor);
}

void loadSettings() {
  tzOffset = prefs.getInt("tz", 7);
  timeColor = prefs.getUShort("timeC", TFT_WHITE);
  secColor = prefs.getUShort("secC", 0xF9BE);
  battPercentColor = prefs.getUShort("battPercC", TFT_WHITE);
  battIconColor = prefs.getUShort("battIconC", 0xF9BE);
  bgColor = prefs.getUShort("bgC", TFT_BLACK);
  configTime(tzOffset * 3600, 0, "pool.ntp.org");
}

void drawMenu() {
  auto& d = M5.Display;
  d.fillScreen(TFT_BLACK);
  d.setTextDatum(top_center);
  d.setTextSize(2);
  d.setTextColor(0xF9BE);
  d.drawString("Settings", d.width()/2, 8);

  d.setTextSize(1);
  for (int i = 0; i < menuCount; i++) {
    int y = 35 + i * 18;
    if (i == menuIndex) {
      d.fillRect(8, y, d.width() - 16, 14, 0x3186); // тёмно-серый
      d.setTextColor(0xF9BE);
    } else {
      d.setTextColor(TFT_WHITE);
    }
    d.setTextDatum(top_left);
    d.setCursor(12, y + 1);
    d.print(menuItems[i]);
  }
}

void drawColorPicker(const char* title) {
  auto& d = M5.Display;
  d.fillScreen(TFT_BLACK);
  d.setTextDatum(top_center);
  d.setTextSize(2);
  d.setTextColor(0xF9BE);
  d.drawString(title, d.width()/2, 10);

  // Показываем текущий цвет
  uint16_t c = colorPalette[colorIndex].color;
  if (c == 0xFFFF) c = 0xF9BE; // для preview "Reset"
  d.fillRect((d.width() - 50)/2, 40, 50, 50, c);
  d.drawRect((d.width() - 50)/2, 40, 50, 50, TFT_WHITE);

  d.setTextSize(1);
  d.setTextDatum(top_center);
  d.setTextColor(TFT_WHITE);
  d.drawString(colorPalette[colorIndex].name, d.width()/2, 95);

  d.setTextDatum(bottom_center);
  d.drawString("[B] Next  [C] Prev  [A] Select", d.width()/2, d.height() - 5);
}

void drawTimezonePicker() {
  auto& d = M5.Display;
  d.fillScreen(TFT_BLACK);
  d.setTextDatum(middle_center);
  d.setTextSize(2);
  d.setTextColor(0xF9BE);
  d.drawString("Timezone", d.width()/2, 20);

  String tz = "UTC";
  if (tzOffset >= 0) tz += "+";
  tz += String(tzOffset);
  d.setTextSize(3);
  d.setTextColor(TFT_WHITE);
  d.drawString(tz, d.width()/2, d.height()/2);

  d.setTextSize(1);
  d.setTextDatum(bottom_center);
  d.drawString("[B]+  [C]-  [A] Save", d.width()/2, d.height() - 5);
}

// ------------------ SETUP & LOOP ------------------

void setup() {
  auto cfg = M5.config();
  cfg.clear_display = true;
  M5.begin(cfg);
  M5.Display.setRotation(3);

  prefs.begin("RetroCraft", false);
  loadSettings();

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextDatum(middle_center);
    M5.Display.setTextColor(TFT_RED);
    M5.Display.setTextSize(1);
    M5.Display.drawString("Wi-Fi...", M5.Display.width()/2, M5.Display.height()/2);
    delay(500);
  }

  configTime(tzOffset * 3600, 0, "pool.ntp.org");
  drawMainScreen(true);
}

void loop() {
  M5.update();

  // Вход в меню: удержание A
  if (appState == MAIN) {
    if (M5.BtnA.wasPressed()) {
      holdStart = millis();
      inHold = true;
    }
    if (inHold && M5.BtnA.isPressed()) {
      if (millis() - holdStart > 1200) {
        inHold = false;
        appState = SETTINGS_MENU;
        menuIndex = 0;
        drawMenu();
        while (M5.BtnA.isPressed()) { M5.update(); delay(10); }
      }
    } else if (inHold && !M5.BtnA.isPressed()) {
      inHold = false;
    }
  }

  if (appState == MAIN) {
    static time_t last = 0;
    time_t now; time(&now);
    if (now != last) {
      last = now;
      drawMainScreen();
    }
    delay(20);
    return;
  }

  // Меню
  if (appState == SETTINGS_MENU) {
    if (M5.BtnB.wasPressed()) { menuIndex = (menuIndex + 1) % menuCount; drawMenu(); }
    if (M5.BtnC.wasPressed()) { menuIndex = (menuIndex + menuCount - 1) % menuCount; drawMenu(); }
    if (M5.BtnA.wasPressed()) {
      switch (menuIndex) {
        case 0: appState = SET_TIMEZONE; drawTimezonePicker(); break;
        case 1: appState = SET_TIME_COLOR; colorIndex = 0; drawColorPicker("Time Color"); break;
        case 2: appState = SET_SEC_COLOR; colorIndex = 1; drawColorPicker("Seconds Color"); break;
        case 3: appState = SET_BATT_PERCENT_COLOR; colorIndex = 0; drawColorPicker("Batt % Color"); break;
        case 4: appState = SET_BATT_ICON_COLOR; colorIndex = 1; drawColorPicker("Batt Icon Color"); break;
        case 5: appState = SET_BG_COLOR; colorIndex = 1; drawColorPicker("Background"); break;
        case 6: appState = MAIN; firstDraw = true; drawMainScreen(true); return;
      }
    }
  }

  // Timezone
  else if (appState == SET_TIMEZONE) {
    if (M5.BtnB.wasPressed()) { tzOffset = (tzOffset + 13) % 25 - 12; drawTimezonePicker(); }
    if (M5.BtnC.wasPressed()) { tzOffset = (tzOffset + 11) % 25 - 12; drawTimezonePicker(); }
    if (M5.BtnA.wasPressed()) {
      configTime(tzOffset * 3600, 0, "pool.ntp.org");
      saveSettings();
      appState = SETTINGS_MENU;
      drawMenu();
    }
  }

  // Цвета (общая логика)
  else if (appState >= SET_TIME_COLOR && appState <= SET_BG_COLOR) {
    if (M5.BtnB.wasPressed()) { colorIndex = (colorIndex + 1) % paletteSize; drawColorPicker(""); }
    if (M5.BtnC.wasPressed()) { colorIndex = (colorIndex + paletteSize - 1) % paletteSize; drawColorPicker(""); }
    if (M5.BtnA.wasPressed()) {
      uint16_t selected = colorPalette[colorIndex].color;
      if (selected == 0xFFFF) {
        applyRetroCraftTheme();
        appState = MAIN;
        firstDraw = true;
        drawMainScreen(true);
        return;
      }
      switch (appState) {
        case SET_TIME_COLOR: timeColor = selected; break;
        case SET_SEC_COLOR: secColor = selected; break;
        case SET_BATT_PERCENT_COLOR: battPercentColor = selected; break;
        case SET_BATT_ICON_COLOR: battIconColor = selected; break;
        case SET_BG_COLOR: 
          // Разрешаем только тёмные фоны
          if (selected == TFT_BLACK || selected == 0x3186 || selected == 0x18E3) {
            bgColor = selected;
          }
          break;
      }
      saveSettings();
      appState = SETTINGS_MENU;
      drawMenu();
    }
  }

  delay(20);
}