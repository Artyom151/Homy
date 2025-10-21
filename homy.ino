//by dexile

#include <M5Unified.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include "time.h"
#include <ArduinoJson.h>

const char* ssid = "scorty";
const char* password = "15102010";
const char* WEATHER_API_KEY = "458faf9c88514be79a674849252110";

Preferences prefs;

uint16_t timeColor = TFT_WHITE;
uint16_t secColor = 0xF9BE;
uint16_t battPercentColor = TFT_WHITE;
uint16_t battIconColor = 0xF9BE;
uint16_t bgColor = TFT_BLACK;

int tzOffset = 7;
String city = "Krasnoyarsk";

enum AppState {
  MAIN,
  SETTINGS_MENU,
  SET_TIME_COLOR,
  SET_SEC_COLOR,
  SET_BATT_PERCENT_COLOR,
  SET_BATT_ICON_COLOR,
  SET_BG_COLOR,
  SET_CITY
};
AppState appState = MAIN;

int menuIndex = 0;
int colorIndex = 0;
int mainTab = 0;  // 0: Time, 1: Weather, 2: Calendar
int holdStart = 0;
bool inHold = false;

const struct {
  const char* name;
  uint16_t color;
} colorPalette[] = {
  {"White", TFT_WHITE}, {"Black", TFT_BLACK}, {"Pink", 0xF9BE}, {"Peach", 0xFDA0},
  {"Lavender", 0xE73C}, {"Mint", 0x7E5D}, {"Sky", 0x841C}, {"Coral", 0xFBEA},
  {"Rose", 0xF81F}, {"Violet", 0x881F}, {"Cyan", TFT_CYAN}, {"Green", TFT_GREEN},
  {"Lime", 0xB720}, {"Yellow", TFT_YELLOW}, {"Orange", 0xFD20}, {"Red", TFT_RED},
  {"Purple", 0x781F}, {"Reset Theme", 0xFFFF}
};
const int paletteSize = sizeof(colorPalette) / sizeof(colorPalette[0]);

const char* menuItems[] = {
  "Timezone", "Time Color", "Seconds Color", "Batt % Color",
  "Batt Icon Color", "Background", "City", "Back"
};
const int menuCount = 8;

const char* russianCities[] = {
  "Moscow", "Saint Petersburg", "Novosibirsk", "Yekaterinburg", "Nizhny Novgorod",
  "Kazan", "Chelyabinsk", "Omsk", "Samara", "Rostov-on-Don",
  "Ufa", "Krasnoyarsk", "Perm", "Voronezh", "Volgograd"
};
const int russianCitiesCount = sizeof(russianCities) / sizeof(russianCities[0]);

int cityScrollIndex = 0;
int citySelectedIndex = 0;

String weatherCached = "Loading...";
unsigned long lastWeatherUpdate = 0;
const unsigned long weatherInterval = 1 * 60 * 1000; // 1 minute

float getBatteryVoltage() { return M5.Power.getBatteryVoltage(); }

int getBatteryPercent(float voltage) {
  if (voltage < 3.2) return 0;
  if (voltage > 4.2) return 100;
  return (int)((voltage - 3.2) * 100);
}

String fetchWeather(const String& city) {
  WiFiClient client;
  HTTPClient http;
  String url = "http://api.weatherapi.com/v1/current.json?key=" + String(WEATHER_API_KEY) + "&q=" + city + "&lang=en";
  http.begin(client, url);
  int httpCode = http.GET();
  String payload = "";
  if (httpCode == 200) payload = http.getString();
  http.end();
  return payload;
}

String parseWeather(const String& json) {
  StaticJsonDocument<2048> doc;
  DeserializationError err = deserializeJson(doc, json);
  if (err) return "Failed to get data";
  String name = doc["location"]["name"].as<String>();
  float tempC = doc["current"]["temp_c"].as<float>();
  String cond = doc["current"]["condition"]["text"].as<String>();
  return name + ": " + String(tempC, 1) + "°C, " + cond;
}

void drawBatteryIcon(int x, int y, int percent, uint16_t iconColor) {
  auto& d = M5.Display;
  int fill = map(constrain(percent, 0, 100), 0, 100, 0, 18);
  d.drawRect(x, y, 20, 10, timeColor);
  d.fillRect(x + 20, y + 2, 2, 6, timeColor);
  d.fillRect(x + 1, y + 1, fill, 8, iconColor);
}

void drawTimeScreen() {
  auto& d = M5.Display;
  d.fillScreen(bgColor);

  struct tm ti;
  if (!getLocalTime(&ti)) {
    d.setTextDatum(middle_center);
    d.setTextSize(3);
    d.setTextColor(TFT_RED);
    d.drawString("no time", d.width() / 2, d.height() / 2);
    return;
  }
  char hm[6], s[3];
  strftime(hm, sizeof(hm), "%H:%M", &ti);
  strftime(s, sizeof(s), "%S", &ti);

  float v = getBatteryVoltage();
  int bp = getBatteryPercent(v);

  d.setTextDatum(middle_center);
  d.setTextSize(4);
  d.setTextColor(timeColor);
  d.drawString(hm, d.width() / 2, d.height() / 2 - 20);

  d.setTextSize(2);
  d.setTextColor(secColor);
  d.drawString(s, d.width() / 2, d.height() / 2 + 10);

  drawBatteryIcon(d.width() - 25, 5, bp, battIconColor);

  d.setTextSize(2);
  d.setTextDatum(top_left);
  d.setTextColor(battPercentColor);
  d.setCursor(10, 5);
  d.printf("%d%%", bp);
}

void drawWeatherScreen() {
  auto& d = M5.Display;
  d.fillScreen(bgColor);

  d.setTextDatum(middle_center);
  d.setTextSize(2);
  d.setTextColor(timeColor);
  d.drawString(city, d.width() / 2, d.height() / 2 - 20);

  d.setTextSize(1);
  d.drawString(weatherCached, d.width() / 2, d.height() / 2 + 10);
}

void drawCalendarScreen() {
  auto& d = M5.Display;
  d.fillScreen(bgColor);

  struct tm ti;
  if (!getLocalTime(&ti)) {
    d.setTextDatum(middle_center);
    d.setTextColor(TFT_RED);
    d.setTextSize(2);
    d.drawString("No time", d.width() / 2, d.height() / 2);
    return;
  }
  char dateStr[16], dayStr[10];
  strftime(dateStr, sizeof(dateStr), "%Y-%m-%d", &ti);
  strftime(dayStr, sizeof(dayStr), "%A", &ti); // weekday

  d.setTextDatum(middle_center);
  d.setTextColor(timeColor);
  d.setTextSize(3);
  d.drawString(dateStr, d.width() / 2, d.height() / 2 - 20);
  d.setTextSize(2);
  d.drawString(dayStr, d.width() / 2, d.height() / 2 + 20);
}

void drawSettingsMenu() {
  auto& d = M5.Display;
  d.fillScreen(TFT_BLACK);
  d.setTextDatum(top_center);
  d.setTextSize(2);
  d.setTextColor(0xF9BE);
  d.drawString("Settings", d.width() / 2, 8);
  int maxVisible = 5;
  int start = (menuIndex / maxVisible) * maxVisible;
  d.setTextSize(1);
  for (int i = 0; i < maxVisible && (start + i) < menuCount; i++) {
    int idx = start + i;
    int y = 35 + i * 18;
    if (idx == menuIndex) {
      d.fillRect(8, y, d.width() - 16, 14, 0x3186);
      d.setTextColor(0xF9BE);
    }
    else {
      d.setTextColor(TFT_WHITE);
    }
    d.setCursor(12, y + 1);
    if (idx == 6) {
      d.print(": ");
      d.print(city);
    }
    else {
      d.print(menuItems[idx]);
    }
  }
}

void drawCitySelectionMenu() {
  auto& d = M5.Display;
  d.fillScreen(TFT_BLACK);
  int maxVisible = 5;
  int start = cityScrollIndex - cityScrollIndex % maxVisible;
  d.setTextDatum(top_left);
  d.setTextSize(1);
  for (int i = 0; i < maxVisible && start + i < russianCitiesCount; i++) {
    int idx = start + i;
    int y = 40 + i * 14;
    if (idx == citySelectedIndex) {
      d.fillRect(5, y - 2, d.width() - 10, 14, 0x3186);
      d.setTextColor(TFT_WHITE);
    }
    else {
      d.setTextColor(0x7BEF);
    }
    d.setCursor(10, y);
    d.print(russianCities[idx]);
  }
  d.setTextSize(1);
  d.setTextColor(TFT_WHITE);
  d.setTextDatum(bottom_center);
  d.drawString("[B]Down  [C]Up  [A]Select", d.width() / 2, d.height() - 5);
}

void saveSettings() {
  prefs.putInt("tz", tzOffset);
  prefs.putUShort("timeColor", timeColor);
  prefs.putUShort("secColor", secColor);
  prefs.putUShort("battPercentColor", battPercentColor);
  prefs.putUShort("battIconColor", battIconColor);
  prefs.putUShort("bgColor", bgColor);
  prefs.putString("city", city);
}

void loadSettings() {
  tzOffset = prefs.getInt("tz", 7);
  timeColor = prefs.getUShort("timeColor", TFT_WHITE);
  secColor = prefs.getUShort("secColor", 0xF9BE);
  battPercentColor = prefs.getUShort("battPercentColor", TFT_WHITE);
  battIconColor = prefs.getUShort("battIconColor", 0xF9BE);
  bgColor = prefs.getUShort("bgColor", TFT_BLACK);
  city = prefs.getString("city", "Krasnoyarsk");

  citySelectedIndex = 0;
  cityScrollIndex = 0;
  for(int i = 0; i < russianCitiesCount; i++){
    if(city == russianCities[i]){
      citySelectedIndex = i;
      if(citySelectedIndex >= 5)
        cityScrollIndex = citySelectedIndex - 4;
      break;
    }
  }
  configTime(tzOffset * 3600, 0, "pool.ntp.org");
}

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
    M5.Display.drawString("Booting", M5.Display.width() / 2, M5.Display.height() / 2);
    delay(500);
  }

  configTime(tzOffset * 3600, 0, "pool.ntp.org");

  struct tm ti;
  int retries = 0;
  while (!getLocalTime(&ti) && retries++ < 10) {
    delay(1000);
  }

  weatherCached = parseWeather(fetchWeather(city));
  lastWeatherUpdate = millis();

  drawTimeScreen();
}

void loop() {
  M5.update();

  if (appState == MAIN) {
    if (M5.BtnB.wasPressed()) {
      mainTab = (mainTab + 1) % 3; // Time, Weather, Calendar
      if (mainTab == 0)
        drawTimeScreen();
      else if (mainTab == 1)
        drawWeatherScreen();
      else if (mainTab == 2)
        drawCalendarScreen();
    }

    if (M5.BtnA.wasPressed()) {
      holdStart = millis();
      inHold = true;
    }
    if (inHold && M5.BtnA.isPressed() && (millis() - holdStart > 1200)) {
      inHold = false;
      appState = SETTINGS_MENU;
      menuIndex = 0;
      drawSettingsMenu();
      while (M5.BtnA.isPressed()) {
        M5.update();
        delay(10);
      }
    } else if (inHold && !M5.BtnA.isPressed()) inHold = false;

    if (millis() - lastWeatherUpdate > weatherInterval) {
      weatherCached = parseWeather(fetchWeather(city));
      lastWeatherUpdate = millis();
      if (mainTab == 1) drawWeatherScreen();
    }

    static time_t lastTime = 0;
    time_t now;
    time(&now);
    if (now != lastTime) {
      lastTime = now;
      if (mainTab == 0) drawTimeScreen();
      if (mainTab == 2) drawCalendarScreen();
    }
    delay(20);
    return;
  }

  if (appState == SETTINGS_MENU) {
    if (M5.BtnB.wasPressed()) {
      menuIndex++;
      if (menuIndex >= menuCount) menuIndex = 0;
      drawSettingsMenu();
    }
    if (M5.BtnC.wasPressed()) {
      menuIndex--;
      if (menuIndex < 0) menuIndex = menuCount - 1;
      drawSettingsMenu();
    }
    if (M5.BtnA.wasPressed()) {
      switch (menuIndex) {
      case 0:
        tzOffset = (tzOffset % 12) + 1;
        configTime(tzOffset * 3600, 0, "pool.ntp.org");
        saveSettings();
        drawSettingsMenu();
        break;
      case 1:
        appState = SET_TIME_COLOR;
        colorIndex = 0;
        drawColorPicker("Time Color");
        break;
      case 2:
        appState = SET_SEC_COLOR;
        colorIndex = 0;
        drawColorPicker("Seconds Color");
        break;
      case 3:
        appState = SET_BATT_PERCENT_COLOR;
        colorIndex = 0;
        drawColorPicker("Batt % Color");
        break;
      case 4:
        appState = SET_BATT_ICON_COLOR;
        colorIndex = 0;
        drawColorPicker("Batt Icon Color");
        break;
      case 5:
        appState = SET_BG_COLOR;
        colorIndex = 1;
        drawColorPicker("Background");
        break;
      case 6:
        appState = SET_CITY;
        citySelectedIndex = 0;
        cityScrollIndex = 0;
        drawCitySelectionMenu();
        break;
      case 7:
        appState = MAIN;
        drawTimeScreen();
        break;
      }
    }
  } else if (appState == SET_CITY) {
    if (M5.BtnB.wasPressed()) {
      citySelectedIndex++;
      if (citySelectedIndex >= russianCitiesCount)
        citySelectedIndex = 0;
      if (citySelectedIndex >= cityScrollIndex + 5) {
        cityScrollIndex++;
        if (cityScrollIndex >= russianCitiesCount - 4)
          cityScrollIndex = 0;
      }
      drawCitySelectionMenu();
    }
    if (M5.BtnC.wasPressed()) {
      if (citySelectedIndex == 0)
        citySelectedIndex = russianCitiesCount - 1;
      else
        citySelectedIndex--;
      if (citySelectedIndex < cityScrollIndex) {
        if (cityScrollIndex == 0)
          cityScrollIndex = russianCitiesCount - 5;
        else
          cityScrollIndex--;
      }
      drawCitySelectionMenu();
    }
    if (M5.BtnA.wasPressed()) {
      city = russianCities[citySelectedIndex];
      saveSettings();
      appState = SETTINGS_MENU;
      drawSettingsMenu();
    }
  } else if (appState >= SET_TIME_COLOR && appState <= SET_BG_COLOR) {
    if (M5.BtnB.wasPressed()) {
      colorIndex = (colorIndex + 1) % paletteSize;
      drawColorPicker("");
    }
    if (M5.BtnC.wasPressed()) {
      colorIndex = (colorIndex + paletteSize - 1) % paletteSize;
      drawColorPicker("");
    }
    if (M5.BtnA.wasPressed()) {
      uint16_t selected = colorPalette[colorIndex].color;
      if (selected == 0xFFFF) {
        timeColor = TFT_WHITE;
        secColor = 0xF9BE;
        battPercentColor = TFT_WHITE;
        battIconColor = 0xF9BE;
        bgColor = TFT_BLACK;
      } else {
        switch (appState) {
        case SET_TIME_COLOR:
          timeColor = selected;
          break;
        case SET_SEC_COLOR:
          secColor = selected;
          break;
        case SET_BATT_PERCENT_COLOR:
          battPercentColor = selected;
          break;
        case SET_BATT_ICON_COLOR:
          battIconColor = selected;
          break;
        case SET_BG_COLOR:
          bgColor = selected;
          break;
        }
      }
      saveSettings();
      appState = SETTINGS_MENU;
      drawSettingsMenu();
    }
  }
  delay(20);
}

void drawColorPicker(const char* title) {
  auto& d = M5.Display;
  d.fillScreen(TFT_BLACK);
  d.setTextDatum(top_center);
  d.setTextSize(2);
  d.setTextColor(0xF9BE);
  d.drawString(title, d.width() / 2, 10);
  uint16_t c = colorPalette[colorIndex].color;
  if (c == 0xFFFF)
    c = 0xF9BE;
  d.fillRect((d.width() - 50) / 2, 40, 50, 50, c);
  d.drawRect((d.width() - 50) / 2, 40, 50, 50, TFT_WHITE);
  d.setTextSize(1);
  d.setTextDatum(top_center);
  d.setTextColor(TFT_WHITE);
  d.drawString(colorPalette[colorIndex].name, d.width() / 2, 95);
  d.setTextDatum(bottom_center);
  d.drawString("[B] Next  [C] Prev  [A] Select", d.width() / 2, d.height() - 5);
}
