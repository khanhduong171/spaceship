#include <Adafruit_GFX.h>     // Core graphics library
#include <Adafruit_ST7789.h>  // Hardware-specific library for ST7789
#include <WiFi.h>             // Thư viện Wi-Fi
#include <PubSubClient.h>     // Thư viện MQTT
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <TimeLib.h>
#include <Fonts/FreeSansBoldOblique9pt7b.h>;
#include <Fonts/FreeMono9pt7b.h>;
#include <Fonts/FreeSansBold12pt7b.h>;
#include <Fonts/FreeSerif9pt7b.h>;
#include <Fonts/FreeMonoBold12pt7b.h>;
#include <Fonts/FreeMonoBold24pt7b.h>;
#include <Fonts/FreeMonoOblique9pt7b.h>;
// ST7789 TFT module connections
#define TFT_DC 2
#define TFT_RST 4
#define TFT_CS 15
#define CUSTOM_LINE 0x630C
#include "rainy.h"
#include "cloudy.h"
#include "sunny.h"
#include "lightning.h"
#include "private.h"
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

WiFiClient espClient;

String wifiStatus = "";
const uint8_t HEIGHT = 240;
const uint8_t WIDTH = 240;
/* Global variable */
// MQTT variable
int temperatureExternal;
int humidityExternal;
int power;
String weather;
String oldWeather;
char dateStr[20];

uint8_t minutes = 0;
uint8_t hours = 0;
uint8_t seconds = 0;


bool blinkColon;
uint8_t position;

// Timmer
unsigned long previousClock = 0;
const long interval3600 = 3600;
const long interval1000 = 1000;
const long interval1m = 10000;
const long interval300 = 300;
const long interval500 = 500;

unsigned long previousMillis = 0;
const long intervalFooter = 400;
/* Global variable */
// MQTT Object
PubSubClient client(espClient);

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
// Touch pin
const int touchPin = T6;
bool touchDetected = true;
bool touchToggle = false;

void setup(void) {
  Serial.begin(9600);
  // init touch pin
  touchAttachInterrupt(touchPin, touchCallback, 40);

  tft.init(WIDTH, HEIGHT, SPI_MODE2);  // Init ST7789 display 240x240 pixel

  tft.setRotation(2);
  tft.fillScreen(ST77XX_BLACK);

  // Connect Wi-Fi
  drawHeader(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    drawHeader("Connecting to WiFi...");
    Serial.println("Connecting to WiFi...");
  }
  wifiStatus = ssid;
  Serial.println("Connected to WiFi");
  drawHeader("Connected to WiFi");
  delay(3000);
  tft.fillScreen(ST77XX_BLACK);
  // Connect MQTT Broker
  client.setServer(mqttServer, mqttPort);
  client.setCallback(callback);
  Serial.println("MQTT connected");
  timeClient.begin();
  delay(1000);
  drawLayout();
  initDrawClock();
}

void touchCallback() {
  if (touchDetected) {
    drawHeaderRight();
    touchDetected = false;
    Serial.println("Cảm ứng đã được nhận!");
  }
}
void callback(char *topic, byte *payload, unsigned int length) {
  // Xử lý dữ liệu nhận được từ MQTT
  Serial.print("Message arrived on topic: ");
  Serial.println(topic);
  Serial.print("Payload: ");

  DynamicJsonDocument doc(256);  // Kích thước đối tượng JSON
  deserializeJson(doc, payload, length);
  // process data here
  weather = doc["weather"].as<String>();
  temperatureExternal = doc["tem"].as<int>();
  humidityExternal = doc["hum"].as<int>();
  power = doc["power"].as<int>();
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
  } else if (weather == "sunny") {
    newWeather = "Sunny";
    weatherImage = (const uint16_t *)sunny;
  } else if (weather == "lightning") {
    newWeather = "Lightning";
    weatherImage = (const uint16_t *)lightning;
  } else {
    // TODO:
  }
  drawBattery();
  if (weatherImage != nullptr && oldWeather != newWeather) {
    drawWeather(newWeather);
    tft.drawRGBBitmap(180, 30, weatherImage, 48, 48);
    oldWeather = newWeather;
  }
  newWeather.clear();
}

void reconnect() {
  while (!client.connected()) {
    Serial.println("Attempting MQTT connection...");
    if (client.connect("SpaceshipDevice", mqttUser, mqttPassword)) {
      Serial.println("Connected to MQTT Broker");
      client.subscribe(mqttTopic);
    } else {
      Serial.print("Failed to connect to MQTT Broker, rc=");
      Serial.print(client.state());
      delay(5000);
    }
  }
}
void loop() {
  unsigned long currentMillis = millis();
  // Footer
  // if (currentMillis - previousMillis >= intervalFooter) {
  //   previousMillis = currentMillis;
  //   drawMqttFooter();
  // }
  // Clock
  if (currentMillis - previousClock >= interval1000) {
    previousClock = currentMillis;
    touchDetected = true;
    drawClock();
  }
  // Update touch
  // if (currentMillis - previousClock >= interval300) {
  //   previousClock = currentMillis;
    
  // }
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  delay(1000);
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

void initDrawClock() {
  updateClock();

  tft.setFont(&FreeMonoBold24pt7b);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_RED);
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
}
// DRAW
void drawLayout() {
  uint16_t colorLine = CUSTOM_LINE;
  tft.drawRect(0, 0, 240, 240, colorLine);
  tft.drawLine(0, 20, WIDTH, 20, colorLine);
  tft.drawLine(170, 20, 170, 110, colorLine);
  tft.drawLine(0, 80, 170, 80, colorLine);
  tft.drawLine(0, 110, WIDTH, 110, colorLine);
  tft.drawLine(0, 210, WIDTH, 210, colorLine);
}

void drawHeader(String str) {
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(5, 100);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_BLUE);
  tft.setFont(&FreeMono9pt7b);
  tft.print(str);
  str.clear();
}

void drawHeaderRight() {
  touchToggle = !touchToggle;
  if (touchToggle) {
    tft.setTextColor(ST77XX_GREEN);
    tft.setFont();
    tft.setCursor(205, 5);
    tft.setTextSize(1);
    tft.print(".oO");
  } else {
    tft.fillRect(171, 1, 68, 19, ST77XX_BLACK);
  }
}

void drawDate(String str) {
  tft.fillRect(1, 1, 170, 19, ST77XX_BLACK);
  tft.setCursor(5, 15);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_BLUE);
  tft.setFont(&FreeMono9pt7b);
  tft.print(str);
  str.clear();
}
void drawClock() {
  blinkColon = !blinkColon;
  // tft.setFont();
  tft.setFont(&FreeMono9pt7b);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_RED);
  tft.fillRect(146, 45, 24, 24, ST77XX_BLACK);  // clear seconds
  // tft.setCursor(150, 59);
  tft.setCursor(147, 64);
  tft.print(formatNumber(seconds));

  if (blinkColon) {
    tft.setTextColor(ST77XX_BLACK);
  }
  tft.setFont(&FreeMonoBold24pt7b);
  tft.setCursor(63, 60);
  tft.print(":");

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
      tft.setTextColor(ST77XX_RED);
      tft.print(formatNumber(hours));
      // Update clock after 1 hours
      updateClock();
    }
    tft.setCursor(91, 64);
    tft.fillRect(91, 21, 55, 59, ST77XX_BLACK);  // clear minutes
    tft.setFont(&FreeMonoBold24pt7b);
    tft.setTextColor(ST77XX_RED);
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
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.setCursor(5, 88);
  tft.print(temperatureExternal);
  tft.setTextSize(1);
  tft.setCursor(tft.getCursorX(), 86);
  tft.print("o");
  tft.setCursor(tft.getCursorX(), 88);
  tft.setTextSize(2);
  String str = "C " + String(humidityExternal) + "% ";
  tft.print(str);
  str.clear();
}
void drawWeather(String status) {
  // tft.drawRGBBitmap(180, 30, (const uint16_t *)data, 48, 48);
  tft.fillRect(171, 21, 68, 89, ST77XX_BLACK);
  tft.setFont();
  tft.setCursor(185, 95);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(1);
  tft.print(status);
}

void drawFooter() {
  // uint8_t lenF = footer.length();
  // if (position >= lenF) {
  //   position = 0;
  // }
  // String newFooter = footer.substring(position, lenF);
  // position++;
  // Clear border => 239, keep border 238
  tft.fillRect(1, 211, 238, 28, ST77XX_BLACK);
  tft.setFont(&FreeMonoOblique9pt7b);
  tft.setTextSize(1);
  tft.setCursor(5, 230);
  tft.setTextColor(ST77XX_GREEN);
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

void drawMessage() {
  tft.drawRect(0, 160, 199, 179, ST77XX_BLACK);
  tft.setCursor(0, 160);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_RED);
  tft.setTextWrap(true);
  tft.print(wifiStatus);
}


void drawface() {


  // Draw text
  // const char *text_day = "Mon, 25 SEP 2023";
  // tft.setCursor(5, 7);
  // tft.setTextColor(ST77XX_BLUE);
  // tft.setTextWrap(false);
  // tft.setFont(&FreeMono9pt7b);
  // tft.setTextSize(1);
  // tft.print(text_day);
  // Draw time
  // const char *text_time = "20:55";
  // // Set text align = center
  // tft.setCursor(16, 64);
  // tft.setFont(&FreeMonoBold24pt7b);
  // tft.setTextColor(ST77XX_RED);
  // tft.setTextWrap(false);
  // tft.print(text_time);
  // Draw temp and hum
  // tft.setFont();
  // tft.setCursor(15, 88);
  // tft.setTextColor(ST77XX_GREEN);
  // tft.setTextSize(2);
  // tft.print("BN-25");
  // tft.setCursor(tft.getCursorX(), 88 - 2);
  // tft.setTextSize(1);
  // tft.print("o");
  // tft.setCursor(tft.getCursorX(), 88);
  // tft.setTextSize(2);
  // tft.print("C|");
  // tft.print("98%");

  // showWeather();
  // tft.setCursor(80, 83);
  // tft.setTextColor(ST77XX_GREEN);
  // tft.setTextSize(2);
  // tft.print("25");
  // tft.setCursor(tft.getCursorX(), 83 - 2);
  // tft.setTextSize(1);
  // tft.print("o");
  // tft.setCursor(tft.getCursorX(), 83);
  // tft.setTextSize(2);
  // tft.print("C|");
  // tft.print("80%");
  // Draw line
  // tft.drawLine(0, 105, 240, 105, ST77XX_BLUE);

  // Draw smarthome device
  // const char *text_device = "Light ON: 6, Device ON: 10";
  // tft.setCursor(5, 120 + 5);
  // tft.setFont(&FreeMono9pt7b);
  // tft.setTextSize(1);
  // tft.setTextColor(ST77XX_YELLOW);
  // tft.setTextWrap(true);
  // tft.print(text_device);

  // Draw message
  // const char *text_msg = "The door is opening...";
  // tft.setCursor(0, 160);
  // tft.setTextSize(1);
  // tft.setTextColor(ST77XX_RED);
  // tft.setTextWrap(true);
  // tft.print(wifiStatus);

  // Draw line
  // tft.drawLine(0, 200, 240, 200, ST77XX_BLUE);
  // Draw TH
  // const char *text_tem = "16";
  // const char *text_hum = "89";
  // tft.setCursor(5, 200 + 6);
  // tft.setTextSize(2);
  // tft.setTextColor(ST77XX_ORANGE);
  // tft.print(text_tem);
  // tft.print("\xF7");
  // tft.print("C");
  // tft.setCursor(tft.getCursorX(), 200 + 20 - 2);
  // tft.setTextSize(1);
  // tft.print("o");
  // tft.setCursor(tft.getCursorX(), 200 + 20);
  // tft.setTextSize(2);
  // tft.print("C");
  // tft.print("  ");
  // tft.print(text_hum);
  // tft.print("%");
  // Draw temp and hum
}
