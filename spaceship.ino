#include <Adafruit_GFX.h>     // Core graphics library
#include <Adafruit_ST7789.h>  // Hardware-specific library for ST7789
#include <WiFi.h>             // Thư viện Wi-Fi
#include <PubSubClient.h>     // Thư viện MQTT
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <TimeLib.h>
#include <Fonts/FreeMono9pt7b.h>  // Thêm thư viện font
#include <Fonts/FreeMonoBold24pt7b.h>
// ST7789 TFT module connections
#define TFT_DC 2
#define TFT_RST 4
#define TFT_CS 15
#define COLOR_LINE 0x630C
#define COLOR_HEADER_R 0x07E0
#define COLOR_HEADER_L 0xec43
#define COLOR_CLOCK 0xec43
#define COLOR_WEATHER 0xec43
#define COLOR_TEMP 0xec43
#define COLOR_DEVICE 0xf800
#define COLOR_STATE 0x201f
#define COLOR_FOOTER 0xF81F

#include "rainy.h"
#include "cloudy.h"
#include "sunny.h"
#include "lightning.h"
#include "private.h"

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

WiFiClient espClient;

const uint8_t HEIGHT = 240;
const uint8_t WIDTH = 240;

// MQTT variable
int temperatureExternal;
int humidityExternal;
int power;
String weather;
String oldWeather;
String device;
String state;
char dateStr[20];

uint8_t minutes = 0;
uint8_t hours = 0;
uint8_t seconds = 0;

bool blinkColon;
int8_t position = 0;
int8_t step = 6;
bool isWifiConnected;
bool isMqttConnected;

unsigned long previousStatus = 0;
const long intervalStatus = 200;

// For timer (seconds)
unsigned long previousClock = 0;
const long intervalClock = 1000;

// For colon
unsigned long previousColon = 0;
const long intervalColon = 500;

// For wifi check connection
unsigned long previousWifi = 0;
const long intervalWifi = 5000;

// MQTT Object
PubSubClient mqttClient(espClient);

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

// Touch pin
const int touchPin = T6;
bool touchDetected = true;
bool touchToggle = false;
// Led anten
const int ledPin = 16;
void setup(void) {
  Serial.begin(9600);
  // init touch pin
  touchAttachInterrupt(touchPin, touchCallback, 40);

  pinMode(ledPin, OUTPUT);

  tft.init(WIDTH, HEIGHT, SPI_MODE2);  // Init ST7789 display 240x240 pixel
  tft.setRotation(2);
  tft.setCursor(1, 1);
  tft.fillScreen(ST77XX_BLACK);
  drawLayout();
  drawConnectStatus();

  // Init wifi
  WiFi.begin(ssid, password);
  previousWifi = millis();
  delay(3000);
  isWifiConnected = false;

  // Check wifi status
  if (WiFi.status() == WL_CONNECTED) {
    isWifiConnected = true;

    // Init mqtt connection
    mqttClient.setServer(mqttServer, mqttPort);
    mqttClient.setCallback(callback);

    // Init clock from internet
    timeClient.begin();
    timeClient.setTimeOffset(25200);  // Adjust this offset as needed

    // Init clock to screen
    initClock();
  }
}

void touchCallback() {
  if (touchDetected) {
    drawHeaderRight();
    touchDetected = false;
    Serial.println("Cảm ứng đã được nhận!");
  }
}

void callback(char *topic, byte *payload, unsigned int length) {
  // Message arrived, turn on led
  digitalWrite(ledPin, HIGH);
  // Xử lý dữ liệu nhận được từ MQTT
  Serial.print("Message arrived on topic: ");
  Serial.println(topic);

  DynamicJsonDocument doc(256);  // Kích thước đối tượng JSON
  deserializeJson(doc, payload, length);

  // process data here
  weather = doc["weather"].as<String>();
  temperatureExternal = doc["tem"].as<int>();
  humidityExternal = doc["hum"].as<int>();
  power = doc["power"].as<int>();
  device = doc["device"].as<String>();
  state = doc["state"].as<String>();

  if (device != nullptr && !device.isEmpty() && device != "null") {
    drawMessage();
  }

  drawTemHum();
  drawFooter();

  // process for weather
  const uint16_t *weatherImage = nullptr;
  String newWeather;

  if (weather == "rainy" || weather == "lightning-rainy") {
    newWeather = "Rainy";
    weatherImage = (const uint16_t *)rainy;
  } else if (weather == "cloudy" || weather == "partlycloudy") {
    newWeather = "Cloudy";
    weatherImage = (const uint16_t *)cloudy;
  } else if (weather == "lightning") {
    newWeather = "Lightning";
    weatherImage = (const uint16_t *)lightning;
  // } else if (weather == "sunny") {
  //   newWeather = "Sunny";
  //   weatherImage = (const uint16_t *)sunny;
  } else {
    // set default to weather
    newWeather = "Sunny";
    weatherImage = (const uint16_t *)sunny;
  }

  drawBattery();

  if (weatherImage != nullptr && oldWeather != newWeather) {
    drawWeather(newWeather);
    tft.drawRGBBitmap(180, 30, weatherImage, 48, 48);
    oldWeather = newWeather;
  }

  newWeather.clear();
  digitalWrite(ledPin, LOW);
}

void reconnectMqtt() {
  if (!mqttClient.connected()) {
    isMqttConnected = false;
    Serial.println("MQTT connection starting");
    if (mqttClient.connect(mqttDevice, mqttUser, mqttPassword)) {
      Serial.println("Connected to MQTT Broker");
      mqttClient.subscribe(mqttTopic);
      isMqttConnected = true;
    } else {
      Serial.print("Can't connect to MQTT Broker, MQTT state: ");
      Serial.println(mqttClient.state());
      isMqttConnected = false;
    }
  }
}

bool reconnectWifi() {
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Wifi connected");
    return true;
  }

  Serial.println("Can't connect to wifi");
  return false;
}

void loop() {
  unsigned long currentMillis = millis();

  // Clock
  if (currentMillis - previousClock >= intervalClock) {
    previousClock = currentMillis;
    touchDetected = true;
    drawClock(false);
  }

  // Update colon
  if (currentMillis - previousColon >= intervalColon) {
    previousColon = currentMillis;
    drawColon();
  }

  // Check wifi connection
  if (currentMillis - previousWifi >= intervalWifi) {
    previousWifi = currentMillis;
    isWifiConnected = reconnectWifi();

    if (isWifiConnected) {
      reconnectMqtt();
      updateClock();
    }
  }

  // Update wifi status to screen
  if (currentMillis - previousStatus >= intervalStatus) {
    previousStatus = currentMillis;
    drawConnectStatus();
  }

  mqttClient.loop();
}

String formatNumber(int num) {
  if (num < 10) {
    return "0" + String(num);
  }
  if (num >= 60) {
    return "00";
  }
  return String(num);
}

void initClock() {
  updateClock();

  tft.setFont(&FreeMonoBold24pt7b);
  tft.setTextSize(1);
  tft.setTextColor(COLOR_CLOCK);
  tft.fillRect(1, 21, 169, 59, ST77XX_BLACK);
  tft.setCursor(8, 64);
  tft.print(formatNumber(hours));
  tft.setCursor(63, 60);
  tft.print(":");
  tft.setCursor(91, 64);
  tft.print(formatNumber(minutes));
}
void updateClock() {
  timeClient.update();
  timeClient.setTimeOffset(25200);
  seconds = timeClient.getSeconds();
  minutes = timeClient.getMinutes();
  hours = timeClient.getHours();

  time_t epochTime = timeClient.getEpochTime();
  tmElements_t tm;
  breakTime(epochTime, tm);
  const char *monthNames[] = { "", "JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC" };

  sprintf(dateStr, "%s %02d %s %04d", dayShortStr(tm.Wday), tm.Day, monthNames[tm.Month], tmYearToCalendar(tm.Year));
  // After get clock, re-draw date
  drawDate(String(dateStr));
  drawClock(true);
}
// DRAW
void drawLayout() {
  uint16_t colorLine = COLOR_LINE;
  tft.drawRect(0, 0, 240, 240, colorLine);
  tft.drawLine(0, 20, WIDTH, 20, colorLine);
  tft.drawLine(170, 20, 170, 110, colorLine);
  tft.drawLine(0, 80, 170, 80, colorLine);
  tft.drawLine(0, 110, WIDTH, 110, colorLine);
  tft.drawLine(0, 210, WIDTH, 210, colorLine);
}

void drawConnectStatus() {
  tft.setFont();
  tft.setTextSize(1);
  if (isWifiConnected) {
    tft.setTextColor(COLOR_HEADER_R);
    tft.setCursor(200, 3);
    tft.print("ooooo");
  }
  if (isMqttConnected) {
    tft.setTextColor(ST77XX_MAGENTA);
    tft.setCursor(200, 9);
    tft.print("ooooo");
  }

  position += step;
  if (position > 29) {
    step = -6;
  }
  if (position < 1) {
    step = 6;
  }
  if (!isWifiConnected) {
    isMqttConnected = false;
    tft.fillRect(171, 1, 68, 10, ST77XX_BLACK);
    tft.setTextColor(COLOR_HEADER_R);
    tft.setCursor(200 + position, 3);
    tft.print("o");
  }
  if (!isMqttConnected) {
    tft.fillRect(171, 10, 68, 10, ST77XX_BLACK);
    tft.setTextColor(ST77XX_MAGENTA);
    tft.setCursor(200 + position, 9);
    tft.print("o");
  }
}

void drawHeader(String str) {
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(5, 100);
  tft.setTextSize(1);
  tft.setTextColor(COLOR_HEADER_L);
  tft.setFont(&FreeMono9pt7b);
  tft.print(str);
  str.clear();
}

void drawHeaderRight() {
  touchToggle = !touchToggle;
  tft.setTextColor(COLOR_HEADER_R);
  tft.setFont();
  tft.setCursor(205, 5);
  tft.setTextSize(1);

  if (touchToggle) {
    tft.print(".oO");
  } else {
    tft.fillRect(171, 1, 68, 19, ST77XX_BLACK);
  }
}
void drawDate(String str) {
  tft.fillRect(1, 1, 170, 19, ST77XX_BLACK);
  tft.setFont(&FreeMono9pt7b);
  tft.setTextColor(COLOR_HEADER_L);
  tft.setTextSize(1);
  tft.setCursor(5, 15);
  tft.print(str);
  str.clear();
}

void drawColon() {
  blinkColon = !blinkColon;
  tft.setFont(&FreeMonoBold24pt7b);
  tft.setCursor(63, 60);

  if (blinkColon) {
    tft.setTextColor(ST77XX_BLACK);
  } else {
    tft.setTextColor(COLOR_CLOCK);
  }

  tft.print(":");
}

void drawClock(bool isWriteAll) {
  tft.setFont(&FreeMono9pt7b);
  tft.setTextSize(1);
  tft.setTextColor(COLOR_CLOCK);
  tft.fillRect(146, 45, 24, 24, ST77XX_BLACK);  // clear seconds
  tft.setCursor(147, 64);
  tft.print(formatNumber(seconds));

  if (isWriteAll) {
    tft.setCursor(8, 64);
    tft.fillRect(8, 21, 55, 59, ST77XX_BLACK);  // clear hours
    tft.setFont(&FreeMonoBold24pt7b);
    tft.setTextColor(COLOR_CLOCK);
    tft.print(formatNumber(hours));

    tft.setCursor(91, 64);
    tft.fillRect(91, 21, 55, 59, ST77XX_BLACK);  // clear minutes
    tft.setFont(&FreeMonoBold24pt7b);
    tft.setTextColor(COLOR_CLOCK);
    tft.print(formatNumber(minutes));
  }

  if (seconds >= 60) {
    seconds = 0;
    minutes++;

    if (minutes >= 60) {
      minutes = 0;
      hours++;

      if (hours >= 24) {
        hours = 0;
      }

      tft.setCursor(8, 64);
      tft.fillRect(8, 21, 55, 59, ST77XX_BLACK);  // clear hours
      tft.setFont(&FreeMonoBold24pt7b);
      tft.setTextColor(COLOR_CLOCK);
      tft.print(formatNumber(hours));

      // Update clock after 1 hours
      updateClock();
    }

    tft.setCursor(91, 64);
    tft.fillRect(91, 21, 55, 59, ST77XX_BLACK);  // clear minutes
    tft.setFont(&FreeMonoBold24pt7b);
    tft.setTextColor(COLOR_CLOCK);
    tft.print(formatNumber(minutes));
  }

  seconds++;
}

/*
 Draw temperature and humidity
*/
void drawTemHum() {
  tft.fillRect(1, 81, 169, 29, ST77XX_BLACK);
  tft.setFont();
  tft.setTextColor(COLOR_TEMP);
  tft.setTextSize(2);
  tft.setCursor(30, 88);
  tft.print(temperatureExternal);
  tft.setTextSize(1);
  tft.setCursor(tft.getCursorX(), 86);
  tft.print("o");
  tft.setCursor(tft.getCursorX(), 88);
  tft.setTextSize(2);
  String str = "C | " + String(humidityExternal) + "% ";
  tft.print(str);
  str.clear();
}

void drawWeather(String status) {
  tft.fillRect(171, 21, 68, 89, ST77XX_BLACK);
  tft.setFont();
  tft.setCursor(185, 95);
  tft.setTextColor(COLOR_WEATHER);
  tft.setTextSize(1);
  tft.print(status);
}

void drawMessage() {
  tft.setTextWrap(true);
  tft.fillRect(1, 111, 238, 99, ST77XX_BLACK);
  tft.setTextSize(1);
  tft.setFont(&FreeMono9pt7b);
  // Print time
  tft.setCursor(80, 135);
  tft.setTextColor(ST77XX_GREEN);
  char timeTmp[8];
  sprintf(timeTmp, "%s:%s:%s", formatNumber(hours), formatNumber(minutes), formatNumber(seconds));
  tft.print(timeTmp);
  // Print device
  tft.setCursor((WIDTH - device.length() * 10) / 2, 160);
  tft.setTextColor(COLOR_DEVICE);
  tft.print(device);
  // Print state
  tft.setCursor((WIDTH - state.length() * 10) / 2, 185);
  tft.setTextColor(COLOR_STATE);
  tft.print(state);
  device.clear();
  state.clear();
}

void drawFooter() {
  tft.fillRect(1, 211, 238, 28, ST77XX_BLACK);
  tft.setFont(&FreeMono9pt7b);
  tft.setTextSize(1);
  tft.setCursor(5, 230);
  tft.setTextColor(COLOR_FOOTER);
  String strFooter = "Power: " + String(power) + "W";
  tft.print(strFooter);
  strFooter.clear();
}

void clearScreen() {
  // fill header left
  tft.fillRect(1, 1, 170, 19, ST77XX_BLACK);
  // fill header right
  tft.fillRect(171, 1, 68, 19, ST77XX_BLACK);
  // fill all time
  tft.fillRect(1, 21, 169, 59, ST77XX_BLACK);
  // fill hours
  tft.fillRect(8, 21, 55, 59, ST77XX_BLACK);
  // fill minute
  tft.fillRect(91, 21, 55, 59, ST77XX_BLACK);
  // fill second
  tft.fillRect(146, 56, 24, 24, ST77XX_BLACK);
  // fill colon
  tft.fillRect(63, 21, 28, 59, ST77XX_BLACK);

  // fill Temp, Hum
  tft.fillRect(1, 81, 169, 29, ST77XX_BLACK);
  // fill weather
  tft.fillRect(171, 21, 68, 89, ST77XX_BLACK);
  // fill message
  tft.fillRect(1, 111, 238, 99, ST77XX_BLACK);
  // fill footer
  tft.fillRect(1, 211, 238, 28, ST77XX_BLACK);
}

void drawBattery() {
  float voltage = analogRead(A0) / 4095.0 * 3.3;

  Serial.print("Điện áp: ");
  Serial.print(voltage, 2);  // Hiển thị 2 chữ số sau dấu thập phân
  Serial.println(" V");
}

void drawface() {
}
