#include <WiFi.h>
#include <WebServer.h>
#include "DHT.h"

// DHT setup
#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// Pin Definitions
#define HEATER_PIN 5
#define FAN_PIN 12
#define LDR_PIN 34
#define PIR_PIN 32
#define LIGHT_PIN 2
#define AO_PIN 35
#define BUZZER_PIN 18

// Thresholds
float tempThresholdHigh = 25.0;
float tempThresholdLow = 18.0;
float humidityThresholdHigh = 60.0;
float humidityThresholdLow = 30.0;
int lightThreshold = 20;
#define THRESHOLD_SMOKE 20
#define THRESHOLD_LPG 40
         
// Timing variables
unsigned long lightOnTime = 0;
unsigned long lightTimeout = 5 * 60 * 1000; // 5 minutes
unsigned long deviceTimeout = 5 * 60 * 1000; // 5 minutes for heater and fan

// WiFi Credentials
const char *ssid = "Redmi";
const char *password = "123456789";

// Web server on port 80
WebServer server(80);

// Variables for controlling devices
bool autoMode = true;
bool heaterStatus = false;
bool fanStatus = false;
bool lightStatus = false;
bool gasDetected = false;
bool motionDetected = false;

unsigned long lastMotionTime = 0; // Track the last time motion was detected

void setup() {
  Serial.begin(115200);
  //  IPAddress IP = WiFi.softAPIP();
  // Serial.print("AP IP address: ");
  // Serial.println(IP);

  // Initialize sensors and outputs
  dht.begin();
  pinMode(HEATER_PIN, OUTPUT);
  pinMode(FAN_PIN, OUTPUT);
  pinMode(LIGHT_PIN, OUTPUT);
  pinMode(PIR_PIN, INPUT);
  pinMode(AO_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  // Connect to Wi-Fi
  // WiFi.softAP(ssid, password);
  // Serial.println("Access Point Started");

  WiFi.begin(ssid, password);
  Serial.println("Connecting to Wi-Fi...");
  
  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting...");
  }

  Serial.println("Connected to Wi-Fi");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());


  // Define web server routes
  server.on("/", handleRoot);
  server.on("/toggleMode", handleToggleMode);
  server.on("/controlDevices", handleManualControl);
  server.begin();
  Serial.println("HTTP server started");

  // Ensure all devices are off at startup
  digitalWrite(HEATER_PIN, LOW);
  digitalWrite(FAN_PIN, LOW);
  digitalWrite(LIGHT_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);
}

void loop() {
  server.handleClient(); // Handle incoming HTTP requests

  if (autoMode) {
    // Automatic control based on sensors
    handleAutoControl();
  }
}

void handleAutoControl() {
  // Read temperature and humidity
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  if (isnan(h) || isnan(t)) {
    Serial.println(F("Failed to read from DHT sensor!"));
    return;
  }

  // Read light intensity from LDR
  int rawLightIntensity = analogRead(LDR_PIN);
  int l = map(rawLightIntensity, 0, 4095, 0, 100);
  int lightIntensity = (100 - l);

  // PIR motion detection
  int pirState = digitalRead(PIR_PIN);
  motionDetected = (pirState == HIGH);

  if (motionDetected) {
    lastMotionTime = millis(); // Update the last motion time
  }

  // Gas detection from MQ2
  int rawGasLevel = analogRead(AO_PIN);
  int gasLevel = map(rawGasLevel, 0, 4095, 0, 100);
  gasDetected = (gasLevel > THRESHOLD_SMOKE || gasLevel > THRESHOLD_LPG);

  // Control Heater
  if (t < tempThresholdLow && motionDetected) {
    heaterStatus = true;
    digitalWrite(HEATER_PIN, HIGH);
  } else if (t > tempThresholdHigh || (millis() - lastMotionTime > deviceTimeout)) {
    heaterStatus = false;
    digitalWrite(HEATER_PIN, LOW);
  }

  // Control Fan
  if (h > humidityThresholdHigh && motionDetected) {
    fanStatus = true;
    digitalWrite(FAN_PIN, HIGH);
  } else if (h < humidityThresholdLow || (millis() - lastMotionTime > deviceTimeout)) {
    fanStatus = false;
    digitalWrite(FAN_PIN, LOW);
  }

  // Control Light
  if (lightIntensity < lightThreshold && motionDetected) {
    lightStatus = true;
    digitalWrite(LIGHT_PIN, HIGH);
    lightOnTime = millis();
  } else if (millis() - lightOnTime > lightTimeout) {
    lightStatus = false;
    digitalWrite(LIGHT_PIN, LOW);
  }

  // Handle Gas/Fire Alarm
  if (gasDetected) {
    digitalWrite(BUZZER_PIN, HIGH);
  } else {
    digitalWrite(BUZZER_PIN, LOW);
  }

  delay(1000);
}

// Webserver root page
void handleRoot() {
  String html = "<html><head><title>Smart Home Automation</title>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<meta http-equiv='refresh' content='120'>";
  html += "<style>body {font-family: Arial; text-align: center; background-color: #f4f4f9; color: #333; padding: 50px;} ";
  html += ".status {display: inline-block; padding: 10px; margin: 10px; border-radius: 5px; color: white;} ";
  html += ".on {background-color: #4CAF50;} .off {background-color: #F44336;} .safe {background-color: #2196F3;} .warn {background-color: #FF9800;} ";
  html += "</style></head><body><h1>Smart Home Automation</h1>";

  // Display Control Mode
  html += "<h2>Control Mode: " + String(autoMode ? "Automatic" : "Manual") + "</h2>";
  html += "<form action='/toggleMode' method='POST'><button type='submit'>Switch to " + String(autoMode ? "Manual" : "Automatic") + " Mode</button></form>";

  // Display Sensor Readings
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  html += "<h2>Sensor Readings</h2>";
  html += "<p>Temperature: " + String(t) + " &deg;C</p>";
  html += "<p>Humidity: " + String(h) + " %</p>";

  // Display Device Status
  html += "<h2>Device Status</h2>";
  html += "<p>Heater: <span class='status " + String(heaterStatus ? "on" : "off") + "'>" + String(heaterStatus ? "On" : "Off") + "</span></p>";
  html += "<p>Fan: <span class='status " + String(fanStatus ? "on" : "off") + "'>" + String(fanStatus ? "On" : "Off") + "</span></p>";
  html += "<p>Light: <span class='status " + String(lightStatus ? "on" : "off") + "'>" + String(lightStatus ? "On" : "Off") + "</span></p>";

  // Display Warning Messages
  html += "<h2>Warnings</h2>";
  html += "<p><span class='status " + String(gasDetected ? "warn" : "safe") + "'>" + String(gasDetected ? "Warning: Gas Leak/Fire Detected" : "Environment Safe") + "</span></p>";

  // Manual Device Control
  if (!autoMode) {
    html += "<h2>Manual Control</h2>";
    html += "<form action='/controlDevices' method='POST'>";
    html += "<button name='heater' value='" + String(heaterStatus ? "Off" : "On") + "'>Turn " + String(heaterStatus ? "Off" : "On") + " Heater</button>";
    html += "<button name='fan' value='" + String(fanStatus ? "Off" : "On") + "'>Turn " + String(fanStatus ? "Off" : "On") + " Fan</button>";
    html += "<button name='light' value='" + String(lightStatus ? "Off" : "On") + "'>Turn " + String(lightStatus ? "Off" : "On") + " Light</button>";
    html += "</form>";
  }

  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleToggleMode() {
  autoMode = !autoMode;
  handleRoot();
}

void handleManualControl() {
  if (!autoMode) {
    if (server.hasArg("heater")) {
      heaterStatus = (server.arg("heater") == "On");
      digitalWrite(HEATER_PIN, heaterStatus ? HIGH : LOW);
    }
    if (server.hasArg("fan")) {
      fanStatus = (server.arg("fan") == "On");
      digitalWrite(FAN_PIN, fanStatus ? HIGH : LOW);
    }
    if (server.hasArg("light")) {
      lightStatus = (server.arg("light") == "On");
      digitalWrite(LIGHT_PIN, lightStatus ? HIGH : LOW);
    }
  }
  handleRoot();
}
