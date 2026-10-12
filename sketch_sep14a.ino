#include <WiFi.h>
#include <HTTPClient.h>
#include <DHT.h>

const char* WIFI_SSID = "Redmi 9C";
const char* WIFI_PASSWORD = "qwerty123";

const char* API_URL =
    "http://192.168.43.218:8000/api/telemetry";

const char* DEVICE_API_KEY = "RPXb1gGrGQyL1sZMW9wupKg7tbeGXUYBytTXBkjb6kg";
const char* DEVICE_ID = "ESP32_001";

const int pirPin = 27;
const int ledPin = 2;
const int buzzerPin = 25;

// DHT11 setup
#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// Send every 2 seconds
const unsigned long SEND_INTERVAL = 700;

// DHT11 only updates about once every 2 seconds
const unsigned long DHT_INTERVAL = 2000;

unsigned long lastSendTime = 0;
unsigned long lastDhtTime = 0;

// Last valid DHT readings (NAN until the first successful read)
float temperature = NAN;
float humidity = NAN;

void setup() {
  Serial.begin(115200);

  pinMode(pirPin, INPUT);
  pinMode(ledPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);

  digitalWrite(ledPin, LOW);
  digitalWrite(buzzerPin, LOW);

  dht.begin();

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    Serial.print(".");
  }

  Serial.println("\nWi-Fi connected!");
  Serial.println("Starting PIR + DHT11 monitoring...");
}

void loop() {
  // Read sensor immediately
  bool motionDetected = digitalRead(pirPin) == HIGH;

  // Update LED and buzzer immediately
  digitalWrite(ledPin, motionDetected ? HIGH : LOW);
  digitalWrite(buzzerPin, motionDetected ? HIGH : LOW);

  // Read back the actual LED and buzzer states
  bool ledOn = digitalRead(ledPin) == HIGH;
  bool buzzerOn = digitalRead(buzzerPin) == HIGH;

  unsigned long now = millis();

  // Read DHT11 on its own slower schedule (non-blocking)
  if (now - lastDhtTime >= DHT_INTERVAL) {
    lastDhtTime = now;
    readDht();
  }

  // Send telemetry at the chosen interval
  if (now - lastSendTime >= SEND_INTERVAL) {
    lastSendTime = now;
    sendTelemetry(motionDetected, ledOn, buzzerOn);
  }

  // No extra delay in the loop
}

void readDht() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  if (isnan(h) || isnan(t)) {
    Serial.println("Failed to read from DHT sensor! Keeping last values.");
    return;
  }

  humidity = h;
  temperature = t;
}

void sendTelemetry(bool motion, bool led, bool buzzer) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi disconnected");
    return;
  }

  HTTPClient http;

  http.setConnectTimeout(2000);
  http.setTimeout(2000);

  if (!http.begin(API_URL)) {
    Serial.println("HTTP initialization failed");
    return;
  }

  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-API-Key", DEVICE_API_KEY);

  // Order: temperature, humidity, motion, led, buzzer
  String payload = "{\"device_id\":\"";
  payload += DEVICE_ID;
  payload += "\",\"temperature\":";
  payload += isnan(temperature) ? "null" : String(temperature, 1);
  payload += ",\"humidity\":";
  payload += isnan(humidity) ? "null" : String(humidity, 1);
  payload += ",\"motion\":";
  payload += motion ? "true" : "false";
  payload += ",\"led\":";
  payload += led ? "true" : "false";
  payload += ",\"buzzer\":";
  payload += buzzer ? "true" : "false";
  payload += "}";

  int httpCode = http.POST(payload);

  // Order: Temperature, Humidity, Motion, LED, Buzzer
  Serial.print("Temperature: ");
  if (isnan(temperature)) Serial.print("--");
  else Serial.print(temperature, 1);
  Serial.print(" C | Humidity: ");
  if (isnan(humidity)) Serial.print("--");
  else Serial.print(humidity, 1);
  Serial.print(" % | Motion: ");
  Serial.print(motion ? "true" : "false");
  Serial.print(" | LED: ");
  Serial.print(led ? "true" : "false");
  Serial.print(" | Buzzer: ");
  Serial.println(buzzer ? "true" : "false");

  if (httpCode < 0) {
    Serial.println(http.errorToString(httpCode));
  }

  http.end();
}