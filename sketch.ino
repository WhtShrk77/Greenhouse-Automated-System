#define BLYNK_TEMPLATE_ID "TMPL3MARnncxv"
#define BLYNK_TEMPLATE_NAME "Greenhouse Automated System"
#define BLYNK_AUTH_TOKEN "G7OMLu7U4ZV_Wg2MIXLMm0QH_blilcRt"

#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <DHT.h>
#include <ESP32Servo.h>
#include <BlynkSimpleEsp32.h>

#define dht_pin 19
#define moisture_pin 32
#define relay_pin 5
#define windowMotorPin 23
#define watterMotorPin 16

// Thresholds
const float temperatureThreshold = 25.0; // Threshold for temperature (in Celsius)
const float humidityThreshold = 70.0; // Threshold for humidity (in percentage)
const float maxMoistureThreshold = 60.0; // Max Threshold for soil moisture (in percentage)
const float minMoistureThreshold = 20.0; // Min Threshold for soil moisture (in percentage)

boolean isWindowOpen = false; // Green House Window is closed initially
boolean isWaterpumpOpen = false; // Water pump is closed initially

DHT my_dht(dht_pin, DHT22);
Servo windowServo;
Servo waterServo;

LiquidCrystal_I2C LCD = LiquidCrystal_I2C(0x27, 16, 2);

char auth[] = BLYNK_AUTH_TOKEN;
char ssid[] = "Wokwi-GUEST";
char pass[] = "";

void setup() {
  Serial.begin(9600);
  pinMode(relay_pin, OUTPUT);  // Initialize relay pin
  digitalWrite(relay_pin, LOW); // Ensure relay is off initially
  
  windowServo.attach(windowMotorPin);
  waterServo.attach(watterMotorPin);
  
  // Initialize servos to closed position
  windowServo.write(0);
  waterServo.write(0);
  
  LCD.init();
  LCD.backlight();
  
  connectToWifi(ssid, pass);  
  Blynk.begin(auth, ssid, pass);
  
  // Sync with Blynk server on connection
  Blynk.syncVirtual(V0, V4);
}

void loop() { 
  Blynk.run();  // Keep Blynk connection alive
  
  // Read sensors
  float temperature = my_dht.readTemperature();
  float humidity = my_dht.readHumidity();
  int moistureValue = analogRead(moisture_pin);
  
  // Check for sensor errors
  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }
  
  // Normalizing the moisture value to a percentage (0 to 100)
  float moisture = map(moistureValue, 0, 4095, 0, 100);
  
  Serial.println("Temperature: " + String(temperature) + "°C");
  Serial.println("Humidity: " + String(humidity) + "%");
  Serial.println("Moisture: " + String(moisture) + "%");

  // Send data to Blynk
  Blynk.virtualWrite(V1, temperature);
  Blynk.virtualWrite(V2, humidity);
  Blynk.virtualWrite(V3, moisture);

  // Update LCD display
  LCD.clear();
  displayOnScreen(temperature, humidity, moisture);

  // Auto-control logic (only runs if manual override is not active)
  // You can add flags to disable auto-control when manually controlled
  
  // Water pump control based on soil moisture
  if (moisture < minMoistureThreshold && !isWaterpumpOpen) {
    digitalWrite(relay_pin, HIGH);
    openWaterPump();
    isWaterpumpOpen = true;
    Blynk.virtualWrite(V4, 1);  // Update app switch
    Serial.println("Auto: Water pump ON (low moisture)");
  } 
  else if (moisture > maxMoistureThreshold && isWaterpumpOpen) {
    closeWaterPump();
    isWaterpumpOpen = false;
    Blynk.virtualWrite(V4, 0);  // Update app switch
    Serial.println("Auto: Water pump OFF (high moisture)");
  }

  // Window control based on temperature and humidity
  if ((humidity > humidityThreshold || temperature > temperatureThreshold) && !isWindowOpen) {
    digitalWrite(relay_pin, HIGH);
    openWindow();
    isWindowOpen = true;
    Blynk.virtualWrite(V0, 1);  // Update app switch
    Serial.println("Auto: Window OPEN (high temp/humidity)");
  } 
  else if (humidity <= humidityThreshold && temperature <= temperatureThreshold && isWindowOpen) {
    closeWindow();
    isWindowOpen = false;
    Blynk.virtualWrite(V0, 0);  // Update app switch
    Serial.println("Auto: Window CLOSED (normal conditions)");
  }

  // Turn off relay if both systems are off
  if (!isWindowOpen && !isWaterpumpOpen) {
    digitalWrite(relay_pin, LOW);
  }

  delay(3000);
}

// Function to connect to WiFi
void connectToWifi(const char* ssid, const char* pass) {
  WiFi.begin(ssid, pass, 6);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  LCD.println("WiFi Connected!");
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
}

// Function to show sensor data on LCD
void displayOnScreen(float temperature, float humidity, int moisture) {
  LCD.setCursor(0, 0);
  LCD.print("T:" + String(temperature, 1) + "C ");
  LCD.print("H:" + String(humidity, 0) + "%");
  LCD.setCursor(0, 1);
  LCD.print("Soil:" + String(moisture) + "%");
  if (isWindowOpen) LCD.print(" W:ON");
  if (isWaterpumpOpen) LCD.print(" P:ON");
}

// Function to open the Window
void openWindow() {
  for (int i = 0; i <= 180; i += 5) {
    windowServo.write(i);
    delay(50);
  }
}

// Function to close the Window
void closeWindow() {
  for (int i = 180; i >= 0; i -= 5) {
    windowServo.write(i);
    delay(50);
  }
}

// Function to open the Water Pump
void openWaterPump() {
  for (int i = 0; i <= 180; i += 5) {
    waterServo.write(i);
    delay(50);
  }
}

// Function to close the Water Pump
void closeWaterPump() {
  for (int i = 180; i >= 0; i -= 5) {
    waterServo.write(i);
    delay(50);
  }
}

// Manual control from Blynk app - Window
BLYNK_WRITE(V0) {
  int value = param.asInt();
  if (value == 1 && !isWindowOpen) {
    openWindow();
    isWindowOpen = true;
    digitalWrite(relay_pin, HIGH);
    Serial.println("Manual: Window OPENED");
  } 
  else if (value == 0 && isWindowOpen) {
    closeWindow();
    isWindowOpen = false;
    // Only turn off relay if water pump is also off
    if (!isWaterpumpOpen) {
      digitalWrite(relay_pin, LOW);
    }
    Serial.println("Manual: Window CLOSED");
  }
}

// Manual control from Blynk app - Water Pump
BLYNK_WRITE(V4) {
  int value = param.asInt();
  if (value == 1 && !isWaterpumpOpen) {
    openWaterPump();
    isWaterpumpOpen = true;
    digitalWrite(relay_pin, HIGH);
    Serial.println("Manual: Water Pump STARTED");
  } 
  else if (value == 0 && isWaterpumpOpen) {
    closeWaterPump();
    isWaterpumpOpen = false;
    // Only turn off relay if window is also closed
    if (!isWindowOpen) {
      digitalWrite(relay_pin, LOW);
    }
    Serial.println("Manual: Water Pump STOPPED");
  }
}