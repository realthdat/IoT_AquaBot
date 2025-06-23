// ===================== BLYNK CONFIGURATION =====================
#define BLYNK_TEMPLATE_ID "YourTemplateID"
#define BLYNK_TEMPLATE_NAME "YourTemplateName"
#define BLYNK_AUTH_TOKEN "YourBlynkAuthToken"

// ===================== LIBRARIES =====================
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <HTTPClient.h>
#include <DHT.h>

// ===================== WIFI CREDENTIALS =====================
char ssid[] = "YourWiFiSSID";
char pass[] = "YourWiFiPassword";

// ===================== LCD INITIALIZATION =====================
LiquidCrystal_I2C lcd(0x27, 16, 2); // I2C address 0x27, 16x2 LCD

// ===================== PIN DEFINITIONS =====================
#define SOIL_PIN 34
#define PUMP_PIN 4
#define LIGHT_PIN 5
#define DHT_PIN 2
#define DHTTYPE DHT11

DHT dht(DHT_PIN, DHTTYPE);

// ===================== GLOBAL VARIABLES =====================
int moistureThreshold = 30;
bool pumpManualControl = false;
bool lightManualControl = false;
unsigned long previousMillis = 0;
const long interval = 10000;
bool blynkConnected = false;

// ===================== SETUP =====================
void setup() {
  Serial.begin(9600);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Welcome back");
  delay(2000);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Loading...");
  delay(2000);

  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, pass);
  int attempts = 0;
  const int maxAttempts = 10;

  while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts) {
    delay(1000);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected to WiFi");
    Blynk.config(BLYNK_AUTH_TOKEN);
    Blynk.connect();

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi Connected!");
    delay(1000);
    lcd.clear();

    blynkConnected = true;
    Blynk.virtualWrite(V5, moistureThreshold);
  } else {
    Serial.println("\nWiFi Failed");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi Failed!");
    delay(2000);
    blynkConnected = false;
  }

  pinMode(PUMP_PIN, OUTPUT);
  digitalWrite(PUMP_PIN, LOW);
  pinMode(LIGHT_PIN, OUTPUT);
  digitalWrite(LIGHT_PIN, LOW);
  dht.begin();
}

// ===================== SEND TO THINGSPEAK =====================
void sendToThingSpeak(int soilMoisturePercent, float temp, float hum) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = "https://api.thingspeak.com/update?api_key=YourThingSpeakAPIKey";
    url += "&field1=" + String(soilMoisturePercent);
    url += "&field2=" + String(temp);
    url += "&field3=" + String(hum);

    http.begin(url);
    int httpResponseCode = http.GET();
    Serial.print("ThingSpeak Response: ");
    Serial.println(httpResponseCode);
    http.end();
  } else {
    Serial.println("WiFi Disconnected");
  }
}

// ===================== SEND TO IFTTT =====================
void sendToIFTTT(int soilMoisturePercent, float temp, float hum) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = "https://maker.ifttt.com/trigger/check_soil_moisture/with/key/YourIFTTTKey";
    url += "?value1=" + String(soilMoisturePercent);
    url += "&value2=" + String(temp);
    url += "&value3=" + String(hum);

    http.begin(url);
    int httpResponseCode = http.GET();
    Serial.print("IFTTT Response: ");
    Serial.println(httpResponseCode);
    http.end();
  } else {
    Serial.println("WiFi Disconnected");
  }
}

// ===================== BLYNK VIRTUAL WRITES =====================
BLYNK_WRITE(V1) {
  if (blynkConnected) {
    pumpManualControl = param.asInt();
    digitalWrite(PUMP_PIN, pumpManualControl ? HIGH : LOW);
    Blynk.virtualWrite(V1, pumpManualControl);
    Serial.println(pumpManualControl ? "Manual Pump ON" : "Manual Pump OFF");
  }
}

BLYNK_WRITE(V2) {
  if (blynkConnected) {
    lightManualControl = param.asInt();
    digitalWrite(LIGHT_PIN, lightManualControl ? HIGH : LOW);
    Blynk.virtualWrite(V2, lightManualControl);
    Serial.println(lightManualControl ? "Manual Light ON" : "Manual Light OFF");
  }
}

BLYNK_WRITE(V5) {
  moistureThreshold = param.asInt();
  Serial.print("Updated moisture threshold: ");
  Serial.println(moistureThreshold);
}

// ===================== SENSOR MONITORING =====================
void checkSensors() {
  int soilValue = analogRead(SOIL_PIN);
  int soilMoisturePercent = map(soilValue, 4095, 0, 0, 100);
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();

  Serial.print("Soil Moisture: ");
  Serial.print(soilMoisturePercent);
  Serial.println("%");

  if (blynkConnected) {
    Blynk.virtualWrite(V0, soilMoisturePercent);
    Blynk.virtualWrite(V3, temperature);
    Blynk.virtualWrite(V4, humidity);
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("T:");
  lcd.print(temperature, 1);
  lcd.print("C H:");
  lcd.print(humidity, 1);
  lcd.print("%");
  lcd.setCursor(0, 1);
  lcd.print("Soil:");
  lcd.print(soilMoisturePercent);
  lcd.print("%");

  sendToThingSpeak(soilMoisturePercent, temperature, humidity);
  // sendToIFTTT(soilMoisturePercent, temperature, humidity); // Optional

  if (soilMoisturePercent < moistureThreshold && !pumpManualControl) {
    digitalWrite(PUMP_PIN, HIGH);
    Blynk.virtualWrite(V1, 1);
    Serial.println("Auto Pump ON");
  } else if (soilMoisturePercent >= moistureThreshold && !pumpManualControl) {
    digitalWrite(PUMP_PIN, LOW);
    Blynk.virtualWrite(V1, 0);
    Serial.println("Auto Pump OFF");
  }

  if (temperature > 30.0 && !lightManualControl) {
    digitalWrite(LIGHT_PIN, HIGH);
    Blynk.virtualWrite(V2, 1);
    Serial.println("Light ON due to high temp");
  } else if (temperature <= 30.0 && !lightManualControl) {
    digitalWrite(LIGHT_PIN, LOW);
    Blynk.virtualWrite(V2, 0);
    Serial.println("Light OFF due to temp");
  }
}

// ===================== MAIN LOOP =====================
void loop() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    checkSensors();
  }

  if (blynkConnected) {
    Blynk.run();
  }
}
