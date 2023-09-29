#include <Adafruit_GFX.h>     // Core graphics library
#include <Adafruit_ST7789.h>  // Hardware-specific library for ST7789
#include <WiFi.h>             // Thư viện Wi-Fi
#include <PubSubClient.h>     // Thư viện MQTT
#include <ArduinoJson.h>
#include <Fonts/FreeSansBoldOblique9pt7b.h>;
#include <Fonts/FreeMono9pt7b.h>;
#include <Fonts/FreeSansBold12pt7b.h>;
#include <Fonts/FreeSerif9pt7b.h>;
#include <Fonts/FreeMonoBold12pt7b.h>;
#include <Fonts/FreeMonoBold24pt7b.h>;
// ST7789 TFT module connections
#define TFT_DC 2
#define TFT_RST 4
#define TFT_CS 15
#include "rainy.h"
#include "cloudy.h"
#include "sunny.h"
#include "lightning.h"
#include "private.h"
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

String wifiStatus = "";
const uint8_t HEIGHT = 240;
const uint8_t WIDTH = 240;
// Đối tượng kết nối Wi-Fi
WiFiClient espClient;

// Đối tượng MQTT
PubSubClient client(espClient);

void setup(void) {
  Serial.begin(9600);
  // if the display has CS pin try with SPI_MODE0
  tft.init(WIDTH, HEIGHT, SPI_MODE2);  // Init ST7789 display 240x240 pixel

  tft.setRotation(2);
  tft.fillScreen(ST77XX_BLACK);
  // Kết nối Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  wifiStatus = ssid;
  Serial.println("Connected to WiFi");

  drawLayout();
  drawface();
  // drawMessage();
  // Kết nối MQTT Broker
  client.setServer(mqttServer, 1883);
  client.setCallback(callback);
  Serial.println("done");
  delay(5000);
  // clearScreen();
}
void callback(char *topic, byte *payload, unsigned int length) {
  // Xử lý dữ liệu nhận được từ MQTT
  Serial.print("Message arrived on topic: ");
  Serial.println(topic);
  Serial.print("Payload: ");
  // Serial.print(payload)
  // for (int i = 0; i < length; i++) {
  //   Serial.print((char)payload[i]);
  // }
  Serial.println();

  DynamicJsonDocument doc(256);  // Kích thước đối tượng JSON
  deserializeJson(doc, payload, length);
  // process data here
  String weather = doc["weather"];
  String tem = doc["tem"];
  String hum = doc["hum"];
  String power = doc["power"];
  drawFooter(tem, hum, power);
  // process for weather
  unsigned short weatherData;
  if (weather == "rainy") {
    showWeather("Rainy");
    tft.drawRGBBitmap(180, 30, (const uint16_t *)rainy, 48, 48);
  } else if (weather == "cloudy") {
    showWeather("Cloudy");
    tft.drawRGBBitmap(180, 30, (const uint16_t *)cloudy, 48, 48);
  } else if (weather == "sunny") {
    showWeather("Sunny");
    tft.drawRGBBitmap(180, 30, (const uint16_t *)sunny, 48, 48);
  } else {
    showWeather("Lightning");
    tft.drawRGBBitmap(180, 30, (const uint16_t *)lightning, 48, 48);
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.println("Attempting MQTT connection...");
    if (client.connect("SpaceshipDevice", mqttUser, mqttPassword)) {
      Serial.println("Connected to MQTT Broker");
      client.subscribe("spaceship");  // Đăng ký theo chủ đề bạn quan tâm
    } else {
      Serial.print("Failed to connect to MQTT Broker, rc=");
      Serial.print(client.state());
      delay(5000);
    }
  }
}
void loop() {
  // tft.invertDisplay(true);
  // delay(500);
  // tft.invertDisplay(false);
  // delay(500);
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
}

void drawLayout() {
  uint16_t colorLine = ST77XX_ORANGE;
  // tft.drawRect(0, 0, 240, 240, colorLine);
  // tft.drawLine(0, 20, WIDTH, 20, colorLine);
  // tft.drawLine(170, 20, 170, 110, colorLine);
  // tft.drawLine(0, 80, 170, 80, colorLine);
  // tft.drawLine(0, 110, WIDTH, 110, colorLine);
  // tft.drawLine(0, 210, WIDTH, 210, colorLine);
}

void clearScreen() {
  // fill header
  tft.fillRect(1, 1, 238, 19, ST77XX_BLACK);
  // fill time
  tft.fillRect(1, 21, 169, 59, ST77XX_BLACK);
  // fill Temp, Hum
  tft.fillRect(1, 81, 169, 29, ST77XX_BLACK);
  // fill weather
  tft.fillRect(171, 21, 68, 89, ST77XX_BLACK);
  // fill message
  tft.fillRect(1, 111, 238, 99, ST77XX_BLACK);
  // fill footer
  tft.fillRect(1, 211, 238, 28, ST77XX_BLACK);
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
  const char *text_day = "Mon, 25 SEP 2023";
  tft.setCursor(5, 7);
  tft.setTextColor(ST77XX_BLUE);
  tft.setTextWrap(false);
  tft.setFont(&FreeMono9pt7b);
  tft.setTextSize(1);
  tft.print(text_day);
  // Draw time
  const char *text_time = "20:55";
  // Set text align = center
  tft.setCursor(16, 64);
  tft.setFont(&FreeMonoBold24pt7b);
  tft.setTextColor(ST77XX_RED);
  tft.setTextWrap(false);
  tft.print(text_time);
  // Draw temp and hum
  tft.setFont();
  tft.setCursor(15, 88);
  tft.setTextColor(ST77XX_GREEN);
  tft.setTextSize(2);
  tft.print("BN-25");
  tft.setCursor(tft.getCursorX(), 88 - 2);
  tft.setTextSize(1);
  tft.print("o");
  tft.setCursor(tft.getCursorX(), 88);
  tft.setTextSize(2);
  tft.print("C|");
  tft.print("98%");

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
  const char *text_device = "Light ON: 6, Device ON: 10";
  tft.setCursor(5, 120 + 5);
  tft.setFont(&FreeMono9pt7b);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_YELLOW);
  tft.setTextWrap(true);
  tft.print(text_device);

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

void drawFooter(String tem, String hum, String power) {
  tft.fillRect(1, 211, 238, 28, ST77XX_BLACK);
  tft.setFont();
  tft.setCursor(5, 218);
  tft.setTextColor(ST77XX_GREEN);
  tft.setTextSize(2);
  tft.print(tem);
  tft.setCursor(tft.getCursorX(), 218 - 2);
  tft.setTextSize(1);
  tft.print("o");
  tft.setCursor(tft.getCursorX(), 218);
  tft.setTextSize(2);
  tft.print("C|");
  tft.print(hum);
  tft.print("%|");
  tft.print(power);
  tft.print("W");
}
void showWeather(String status) {
  // tft.drawRGBBitmap(180, 30, (const uint16_t *)data, 48, 48);
  tft.fillRect(171, 21, 68, 89, ST77XX_BLACK);
  tft.setFont();
  tft.setCursor(185, 95);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(1);
  tft.print(status);
}
