#include <ESP8266WiFi.h>
#include <TinyGPSPlus.h>
#include <SoftwareSerial.h>

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"

// =====================================================
// OLED SETTINGS
// =====================================================

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);


// =====================================================
// GPS SETTINGS
// =====================================================

static const int RXPin = D6;
static const int TXPin = D5;

static const uint32_t GPSBaud = 9600;

TinyGPSPlus gps;

SoftwareSerial softSerial(RXPin, TXPin);


// =====================================================
// WIFI PARAMETERS
// =====================================================

#define WLAN_SSID       "Xiaomi13T"
#define WLAN_PASS       "11122233"


// =====================================================
// ADAFRUIT IO
// =====================================================

#define AIO_SERVER      "io.adafruit.com"
#define AIO_SERVERPORT  1883

#define AIO_USERNAME    ""
#define AIO_KEY         ""


WiFiClient client;

Adafruit_MQTT_Client mqtt(
  &client,
  AIO_SERVER,
  AIO_SERVERPORT,
  AIO_USERNAME,
  AIO_KEY
);


// Adafruit IO GPS feed
Adafruit_MQTT_Publish GPSLocation =
  Adafruit_MQTT_Publish(
    &mqtt,
    AIO_USERNAME "/feeds/GPSLocation/csv"
  );


// =====================================================
// GPS VARIABLES
// =====================================================

float speed_mph = 0;
float alltitude = 0;
float lati = 0;
float longi = 0;

char gpsdata[120];


// =====================================================
// SETUP
// =====================================================

void setup() {

  Serial.begin(115200);

  softSerial.begin(GPSBaud);


  // ===================================================
  // START OLED
  // ===================================================

  Wire.begin(D2, D1);   // SDA = D2, SCL = D1

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {

    Serial.println(F("SSD1306 OLED not found!"));

    while (1);
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);

  display.setCursor(0, 0);
  display.println("ESP8266 GPS");

  display.setCursor(0, 15);
  display.println("Starting...");

  display.display();

  delay(1500);


  // ===================================================
  // CONNECT TO WIFI
  // ===================================================

  display.clearDisplay();

  display.setCursor(0, 0);
  display.println("Connecting WiFi...");

  display.display();


  Serial.print(F("Connecting to "));
  Serial.println(WLAN_SSID);

  WiFi.begin(WLAN_SSID, WLAN_PASS);


  while (WiFi.status() != WL_CONNECTED) {

    delay(500);

    Serial.print(F("."));
  }


  Serial.println();

  Serial.println(F("WiFi connected"));

  Serial.print(F("IP address: "));
  Serial.println(WiFi.localIP());


  // ===================================================
  // OLED WIFI STATUS
  // ===================================================

  display.clearDisplay();

  display.setCursor(0, 0);
  display.println("WiFi Connected");

  display.setCursor(0, 15);
  display.println(WiFi.localIP());

  display.display();

  delay(1500);


  // ===================================================
  // CONNECT TO ADAFRUIT IO
  // ===================================================

  connect();
}


// =====================================================
// CONNECT TO ADAFRUIT IO
// =====================================================

void connect() {

  Serial.print(F("Connecting to Adafruit IO... "));

  int8_t ret;

  while ((ret = mqtt.connect()) != 0) {

    switch (ret) {

      case 1:
        Serial.println(F("Wrong protocol"));
        break;

      case 2:
        Serial.println(F("ID rejected"));
        break;

      case 3:
        Serial.println(F("Server unavailable"));
        break;

      case 4:
        Serial.println(F("Bad user/pass"));
        break;

      case 5:
        Serial.println(F("Not authorized"));
        break;

      case 6:
        Serial.println(F("Failed to subscribe"));
        break;

      default:
        Serial.println(F("Connection failed"));
        break;
    }


    if (ret >= 0)
      mqtt.disconnect();


    Serial.println(F("Retrying connection..."));

    delay(10000);
  }


  Serial.println(F("Adafruit IO Connected!"));
}


// =====================================================
// LOOP
// =====================================================

void loop() {

  // Keep MQTT connection alive
  if (!mqtt.ping(3)) {

    if (!mqtt.connected())
      connect();
  }


  // Read GPS data
  readGPSData();


  // ===================================================
  // CHECK FOR VALID GPS LOCATION
  // ===================================================

  if (gps.location.isValid()) {

    // Get GPS values
    lati = gps.location.lat();
    longi = gps.location.lng();

    // Speed in MPH
    if (gps.speed.isValid())
      speed_mph = gps.speed.mph();
    else
      speed_mph = 0;

    // Altitude
    if (gps.altitude.isValid())
      alltitude = gps.altitude.meters();
    else
      alltitude = 0;


    // =================================================
    // DISPLAY DATA ON OLED
    // =================================================

    displayGPSData();


    // =================================================
    // CREATE CSV DATA
    // =================================================

    getCoordinates();


    // =================================================
    // SEND DATA TO ADAFRUIT IO
    // =================================================

    Serial.println();
    Serial.println(F("Valid GPS location found!"));

    Serial.print(F("Latitude: "));
    Serial.println(lati, 6);

    Serial.print(F("Longitude: "));
    Serial.println(longi, 6);

    Serial.print(F("Altitude: "));
    Serial.print(alltitude);
    Serial.println(F(" m"));

    Serial.print(F("Speed: "));
    Serial.print(speed_mph);
    Serial.println(F(" mph"));

    Serial.print(F("Sending to Adafruit IO... "));


    if (!GPSLocation.publish(gpsdata)) {

      Serial.println(F("Failed"));

    }
    else {

      Serial.println(F("Sent!"));
    }

  }

  else {

    // =================================================
    // NO VALID GPS LOCATION
    // =================================================

    display.clearDisplay();

    display.setTextSize(1);

    display.setCursor(0, 0);
    display.println("GPS STATUS");

    display.setCursor(0, 18);
    display.println("Waiting for");

    display.setCursor(0, 32);
    display.println("valid location...");

    display.setCursor(0, 50);
    display.println("Searching GPS");

    display.display();


    Serial.println(F("Waiting for valid GPS location..."));
  }


  delay(5000);
}


// =====================================================
// CREATE GPS CSV DATA
// =====================================================

void getCoordinates() {

  char *p = gpsdata;


  // Speed
  dtostrf(speed_mph, 2, 6, p);

  p += strlen(p);

  p[0] = ',';
  p++;


  // Latitude
  dtostrf(lati, 2, 6, p);

  p += strlen(p);

  p[0] = ',';
  p++;


  // Longitude
  dtostrf(longi, 3, 6, p);

  p += strlen(p);

  p[0] = ',';
  p++;


  // Altitude
  dtostrf(alltitude, 2, 6, p);

  p += strlen(p);


  // Null terminate
  p[0] = 0;


  Serial.print(F("CSV: "));
  Serial.println(gpsdata);
}


// =====================================================
// DISPLAY GPS DATA ON OLED
// =====================================================

void displayGPSData() {

  display.clearDisplay();

  display.setTextColor(SSD1306_WHITE);


  // Title
  display.setTextSize(1);

  display.setCursor(0, 0);
  display.println("GPS LOCATION");


  // Latitude
  display.setCursor(0, 14);
  display.print("Lat:");
  display.println(lati, 6);


  // Longitude
  display.setCursor(0, 27);
  display.print("Lon:");
  display.println(longi, 6);


  // Speed
  display.setCursor(0, 40);
  display.print("Speed:");
  display.print(speed_mph, 1);
  display.println(" mph");


  // Altitude
  display.setCursor(0, 53);
  display.print("Alt:");
  display.print(alltitude, 1);
  display.println(" m");


  display.display();
}


// =====================================================
// READ GPS DATA
// =====================================================

void readGPSData() {

  unsigned long start = millis();


  // Read GPS for 1 second
  while (millis() - start < 1000) {

    while (softSerial.available()) {

      gps.encode(softSerial.read());
    }
  }


  // Check whether GPS module is sending data
  if (millis() > 5000 && gps.charsProcessed() < 10) {

    Serial.println(F("Waiting for GPS data..."));
  }
}