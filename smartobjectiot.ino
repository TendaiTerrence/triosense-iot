#include <Wire.h>
#include <LiquidCrystal_PCF8574.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "DHT.h"
#include <WebServer.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>


// Wi-Fi Credentials
const char* ssid = "Machaya";
const char* password = "machaya1312";

// Server Details
const char* serverURL = "http://192.168.114.21/final/smart_object_api.php";

// MQTT Broker Details
const char* mqttServer = "192.168.114.21"; 
const int mqttPort = 1883;
const char* mqttTopic = "sensor/data/";

// Pin Definitions
#define DHTPIN 4
#define DHTTYPE DHT22
#define LDRPIN 34
#define LEDPIN 2        
#define RELAYPIN 25     

// Object Initialization
DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_PCF8574 lcd(0x27);
WebServer server(80);
Preferences preferences;
WiFiClient espClient;
PubSubClient mqttClient(espClient);

// Variables
float temperature = 0.0;
float humidity = 0.0;
int lightIntensity = 0;
unsigned long lastDHTUpdate = 0;
unsigned long lastHumidityUpdate = 0;
unsigned long lastHeartbeatTime = 0;  
unsigned long lastDataPost = 0; 
const unsigned long postInterval = 6000; 
bool useHTTP = true; 
bool fanStatus = false;  
bool isAutoMode = true;
String fanMode = "auto";  
String deviceName = "Smart_Object_1";
String dLocation = "indoors";
int deviceID = 1;
String protocol = "http";
float triggerTemp = 30.0;
int ldrSensorValues[15] = {0}; 
int ldrValueIndex = 0; 


// Function Declarations
void connectToWiFi();
void handleConfigPage();
void handleSaveConfig();
void controlFan();
void updateLCD();
void handleHeartbeat();
void publishData();
void sendHTTPData();
void publishMQTTData();
void setupOTA();
void handleRoot();
void handleLdrData();
void handleFanControl();
void handleSensorData();
void readSensors();

// Connect to WiFi and update the LCD with the IP address
void connectToWiFi() {
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");
  Serial.println(WiFi.localIP());

  // Clear LCD and display IP address
  lcd.setCursor(0, 0);
  lcd.print("Connected!");
  lcd.setCursor(0, 1);
  lcd.print(WiFi.localIP().toString());
}

String createPayload() {
    return "{\"deviceName\":\"" + deviceName + "\",\"dLocation\":\"" + dLocation + 
           "\",\"Temperature\":" + String(temperature) + 
           ",\"Humidity\":" + String(humidity) + 
           ",\"LightIntensity\":" + String(lightIntensity) + "}";
}


void connectToMQTT() {
  mqttClient.setServer(mqttServer, mqttPort);
  while (!mqttClient.connected()) {
    Serial.print("Connecting to MQTT...");
    if (mqttClient.connect("ESP32Client")) {
      Serial.println("Connected to MQTT broker.");
    } else {
      Serial.print("Failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" Retrying...");
      delay(2000);
    }
  }
}

void sendHTTPData() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    if (!http.begin(serverURL)) {
      Serial.println("Failed to initialize HTTP client");
      return;
    }

    String payload = "{";
    payload += "\"deviceName\":\"" + deviceName + "\","; 
    payload += "\"dLocation\":\"" + dLocation + "\","; 
    payload += "\"deviceID\":\"" + String(deviceID) + "\",";
    payload += "\"Temperature\":" + String(temperature) + ","; 
    payload += "\"Humidity\":" + String(humidity) + ","; 
    payload += "\"LightIntensity\":" + String(lightIntensity); 
    payload += "}";

    Serial.println("Payload: " + payload);
    http.addHeader("Content-Type", "application/json");

    int httpResponseCode = http.POST(payload);
    String response = http.getString();
    Serial.print("HTTP Response Code: ");
    Serial.println(httpResponseCode);
    Serial.print("Response: ");
    Serial.println(response);

    http.end();
  } else {
    Serial.println("WiFi not connected");
  }
}

void publishMQTTData() {
  if (!mqttClient.connected()) {
    connectToMQTT();
  }

  // Create JSON payload
  String payload = "{";
  payload += "\"deviceName\":\"" + deviceName + "\","; 
  payload += "\"dLocation\":\"" + dLocation + "\","; 
  payload += "\"deviceID\":\"" + String(deviceID) + "\",";
  payload += "\"Temperature\":" + String(temperature) + ","; 
  payload += "\"Humidity\":" + String(humidity) + ","; 
  payload += "\"LightIntensity\":" + String(lightIntensity);
  payload += "}";

  // Publish to MQTT topic
  mqttClient.publish(mqttTopic, payload.c_str());
  Serial.println("MQTT Publish Success: " + payload);
}

// Read Sensors
void readSensors() {
    if (millis() - lastDHTUpdate >= 6000) {
        temperature = dht.readTemperature();
        lightIntensity = analogRead(LDRPIN);
        lastDHTUpdate = millis();
    }
    if (millis() - lastHumidityUpdate >= 3000) {
        humidity = dht.readHumidity();
        lastHumidityUpdate = millis();
    }
}

void handleHeartbeat() {
  static int blinkState = 0; 

  const unsigned long interval = 2000; 
  const unsigned long blinkDelay = 200; 

  unsigned long currentMillis = millis();

  // Determine the time delay for the current state
  unsigned long delayTime = (blinkState == 0) ? interval : blinkDelay;

  // Check if the delay time has passed
  if (currentMillis - lastHeartbeatTime >= delayTime) {
    // Update LED state and transition to the next state
    if (blinkState == 0 || blinkState == 2) {
      digitalWrite(LEDPIN, HIGH);
    } else {
      digitalWrite(LEDPIN, LOW); 
    }

    // Move to the next state (looping back to 0 after 3)
    blinkState = (blinkState + 1) % 4;

    // Update the time for the current state
    lastHeartbeatTime = currentMillis;
  }
}

// Automatically control the fan
void controlFan() {
  if (fanMode == "auto") {
    if (temperature > triggerTemp) {
      digitalWrite(RELAYPIN, LOW); 
      fanStatus = false;
      Serial.println("Fan ON (Auto Mode): Temperature exceeds threshold.");
    } else {
      digitalWrite(RELAYPIN, HIGH); 
      fanStatus = true;
      Serial.println("Fan OFF (Auto Mode): Temperature below threshold.");
    }
  }
}

// Handle manual fan Functions
void handleStartFan() {
  if (fanMode == "manual") {
    digitalWrite(RELAYPIN, HIGH); 
    fanStatus = true;
    Serial.println("Fan OFF (Manual Mode): User turned off the fan.");
    server.send(200, "text/plain", "Fan stopped in manual mode.");
  } else {
    server.send(403, "text/plain", "Cannot stop fan. Automatic mode is active.");
  }
}

// Handle manual fan 
void handleStopFan() {
  if (fanMode == "manual") {
    digitalWrite(RELAYPIN, LOW); 
    fanStatus = false;
    Serial.println("Fan ON (Manual Mode): User turned on the fan.");
    server.send(200, "text/plain", "Fan started in manual mode.");
  } else {
    server.send(403, "text/plain", "Cannot start fan. Automatic mode is active.");
  }

}


void handleResetConfig() {
    preferences.clear(); 
    server.send(200, "text/plain", "All configurations have been reset to defaults.");
    ESP.restart(); 
}


void handleRoot() {
  String html = "<html><head>";
  html += "<style>";
  // Reset and base styles
  html += "* { box-sizing: border-box; margin: 0; padding: 0; }";
  html += "body { font-family: Arial, sans-serif; background-color: #1e1e2f; color: #f4f4f9; line-height: 1.375; padding: 20px; }";
  
  // Header styles with improved spacing and shadow
  html += "header { background-color: #2c2c3e; padding: 2rem; text-align: center; border-radius: 10px; margin-bottom: 2rem; box-shadow: 0 4px 6px rgba(0, 0, 0, 0.1); }";
  html += "h1 { color: #00d1b2; font-size: 2.5em; letter-spacing: 0.5px; }";
  
  // Container for better content organization
  html += ".container { max-width: 800px; margin: 0 auto; }";
  
  // Enhanced paragraph styles for data display
  html += "p { background: #2c2c3e; padding: 1rem 1.5rem; border-radius: 8px; margin: 0.8rem 0; display: flex; justify-content: space-between; align-items: center; }";
  html += "p span { font-weight: 500; }";
  
  // Form styles with improved spacing and interactions
  html += "form { max-width: 600px; margin: 2rem auto; background: #2c2c3e; padding: 2rem; border-radius: 10px; box-shadow: 0 4px 6px rgba(0, 0, 0, 0.1); }";
  html += "label { display: block; margin-bottom: 0.5rem; color: #f4f4f9; font-size: 1.2em; }";
  html += "input, select { width: 100%; padding: 0.8rem; margin-bottom: 1rem; border: 2px solid #444; border-radius: 6px; background: #1e1e2f; color: #f4f4f9; transition: border-color 0.3s ease; }";
  html += "input:focus, select:focus { outline: none; border-color: #00d1b2; }";
  
  // Button container and styles
  html += ".button-container { display: flex; gap: 1rem; justify-content: center; margin: 2rem 0; flex-wrap: wrap; }";
  html += "button { background-color: #00d1b2; color: white; padding: 0.8rem 1.5rem; font-size: 1rem; border: none; border-radius: 6px; cursor: pointer; transition: all 0.3s ease; }";
  html += "button:hover { background-color: #00a894; transform: translateY(-2px); }";
  html += "button:active { transform: translateY(0); }";
  
  // Data display section
  html += "#ldrData { background: #2c2c3e; padding: 1.5rem; border-radius: 8px; margin-top: 1.5rem; }";
  
  // Configuration link styles
  html += ".config-link { display: block; text-align: center; color: #00d1b2; text-decoration: none; padding: 1rem; margin-top: 1.5rem; font-weight: 500; transition: color 0.3s ease; }";
  html += ".config-link:hover { color: #00a894; }";
  
  // Responsive design
  html += "@media (max-width: 768px) {";
  html += "  body { padding: 10px; }";
  html += "  header { padding: 1.5rem; }";
  html += "  .button-container { flex-direction: column; }";
  html += "  button { width: 100%; }";
  html += "  p { flex-direction: column; text-align: center; gap: 0.5rem; }";
  html += "}";
  html += "</style>";
  html += "<link rel='stylesheet' href='https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.0.0-beta3/css/all.min.css'>";
  html += "</head><body>";
  
  // Modified HTML structure with new classes
  html += "<header><h1>Smart Object Dashboard</h1></header>";
  html += "<div class='container'>";
  html += "<p>Device Name: <span>" + String(deviceName) + "</span></p>";
  html += "<p>Temperature: <span id='temperature'>" + String(temperature) + " C</span></p>";
  html += "<p>Humidity: <span id='humidity'>" + String(humidity) + " %</span></p>";
  html += "<p>Light Intensity: <span id='lightIntensity'>" + String(lightIntensity) + "</span></p>";
  html += "<div class='button-container'>";
  html += "<button id='startFan'>Start Fan</button>";
  html += "<button id='stopFan'>Stop Fan</button>";
  html += "<button id='showLdrData'>Show Last 15 LDR Readings</button>";
  html += "</div>";
  html += "<div id='ldrData'></div>";
  html += "<a href='/config' class='config-link'>Configuration</a>";
  html += "</div>";


  html += "<script>";
  html += "function updateSensorData() {";
  html += "  fetch('/sensor-data')";
  html += "    .then(response => response.json())";
  html += "    .then(data => {";
  html += "      document.getElementById('temperature').textContent = data.temperature;";
  html += "      document.getElementById('humidity').textContent = data.humidity;";
  html += "      document.getElementById('lightIntensity').textContent = data.lightIntensity;";
  html += "    });";
  html += "}";
  html += "setInterval(updateSensorData, 5000);";
  html += "document.getElementById('startFan').addEventListener('click', () => {";
  html += "  fetch('/stop-fan', { method: 'POST' })";
  html += "    .then(response => console.log('Fan started'));";
  html += "});";
  html += "document.getElementById('stopFan').addEventListener('click', () => {";
  html += "  fetch('/start-fan', { method: 'POST' })";
  html += "    .then(response => console.log('Fan stopped'));";
  html += "});";
  html += "document.getElementById('showLdrData').addEventListener('click', () => {";
  html += "  fetch('/ldr-data')";
  html += "    .then(response => response.json())";
  html += "    .then(data => {";
  html += "      let ldrDataHtml = '<h2>Last 15 LDR Readings</h2><ul>';";
  html += "      data.forEach(value => {";
  html += "        ldrDataHtml += '<li>' + value + '</li>';";
  html += "      });";
  html += "      ldrDataHtml += '</ul>';";
  html += "      document.getElementById('ldrData').innerHTML = ldrDataHtml;";
  html += "    });";
  html += "});";
  html += "</script>";
  html += "</body></html>";

  server.send(200, "text/html", html);
}

void handleConfig() {
  String html = "<html><head>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; background-color: #1e1e2f; margin: 0; padding: 0; color: #f4f4f9; }";
  html += "header { background-color: #2c2c3e; padding: 20px 0; text-align: center; }";
  html += "h1 { color: #00d1b2; font-size: 2.5em; }";
  html += "form { max-width: 600px; margin: auto; background: #2c2c3e; padding: 20px; border-radius: 10px; }";
  html += "label { display: block; margin-bottom: 5px; color: #f4f4f9; font-size: 1.2em; }";
  html += "input, select { width: 100%; padding: 10px; margin-bottom: 15px; border: 1px solid #444; border-radius: 5px; background: #1e1e2f; color: #f4f4f9; }";
  html += "button { background-color: #00d1b2; color: white; padding: 10px 20px; font-size: 16px; border: none; border-radius: 5px; cursor: pointer; }";
  html += "button:hover { background-color: #00a894; }";
  html += "@media (max-width: 768px) { form { padding: 15px; } }"; // Added responsive form styles
  html += "</style>";
  html += "<link rel='stylesheet' href='https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.0.0-beta3/css/all.min.css'>";
  html += "</head><body>";

  html += "<header><h1><i class='fas fa-cogs'></i> Device Configuration</h1></header>";

  html += "<main>";

  html += "<form action='/save-config' method='POST'>";
  html += "<label for='deviceName'>Device Name:</label>";
  html += "<input type='text' id='deviceName' name='deviceName' value='" + deviceName + "' required>";

  html += "<label for='dLocation'>Device Location:</label>";
  html += "<input type='text' id='dLocation' name='dLocation' value='" + dLocation + "' required>";

  html += "<label for='deviceID'>Device ID:</label>";
  html += "<input type='text' id='deviceID' name='deviceID' value='" + String(deviceID) + "' required>";

  html += "<label for='protocol'>Communication Protocol:</label>";
  html += "<select id='protocol' name='protocol'>";
  html += "<option value='http' " + String(protocol == "http" ? "selected" : "") + ">HTTP</option>";
  html += "<option value='mqtt' " + String(protocol == "mqtt" ? "selected" : "") + ">MQTT</option>";
  html += "</select>";

  html += "<label for='fanMode'>Fan Control Mode:</label>";
  html += "<select id='fanMode' name='fanMode'>";
  html += "<option value='manual' " + String(fanMode == "manual" ? "selected" : "") + ">Manual</option>";
  html += "<option value='auto' " + String(fanMode == "auto" ? "selected" : "") + ">Auto</option>";
  html += "</select>";

  html += "<label for='triggerTemp'>Trigger Temperature (°C):</label>";
  html += "<input type='number' id='triggerTemp' name='triggerTemp' value='" + String(triggerTemp) + "' min='15' max='35' step='0.1' required>";

  html += "<button type='submit'><i class='fas fa-save'></i> Save Configuration</button>";
  html += "</form>";

  html += "</main>";

  html += "<br><a href='/' style='color: #00d1b2; text-align: center; display: block; margin-top: 20px;'><i class='fas fa-arrow-left'></i> Back to Dashboard</a>";

  html += "</body></html>";

  server.send(200, "text/html", html);
}



void handleSaveConfig() {
    if (server.hasArg("deviceName")) {
        deviceName = server.arg("deviceName");
        preferences.putString("deviceName", deviceName); // Save to flash
    }
    if (server.hasArg("dLocation")) {
        dLocation = server.arg("dLocation");
        preferences.putString("dLocation", dLocation);
    }
    if (server.hasArg("deviceID")) {
        deviceID = server.arg("deviceID").toInt();
        preferences.putInt("deviceID", deviceID);
    }
    if (server.hasArg("protocol")) {
        protocol = server.arg("protocol");
        preferences.putString("protocol", protocol);
    }
    if (server.hasArg("fanMode")) {
        fanMode = server.arg("fanMode");
        preferences.putString("fanMode", fanMode);
    }
    if (server.hasArg("triggerTemp")) {
        triggerTemp = server.arg("triggerTemp").toFloat();
        preferences.putFloat("triggerTemp", triggerTemp);
    }

    Serial.println("Configurations Saved!");

    // Redirect back to the dashboard or configuration page
    server.sendHeader("Location", "/");
    server.send(303, "text/plain", "");
}

void handleSensorData() {
  // Fetch the latest sensor data and return it as a JSON response
  String json = "{";
  json += "\"temperature\":" + String(temperature) + ",";
  json += "\"humidity\":" + String(humidity) + ",";
  json += "\"lightIntensity\":" + String(lightIntensity);
  json += "}";
  server.send(200, "application/json", json);
}

void handleLdrData() {
  // Return the last 15 LDR sensor values as a JSON response
  String json = "[";
  for (int i = 0; i < 15; i++) {
    json += String(ldrSensorValues[(ldrValueIndex + i) % 15]);
    if (i < 14) {
      json += ",";
    }
  }
  json += "]";
  server.send(200, "application/json", json);
}

// Update LCD
void updateLCD() {
    lcd.setCursor(0, 0);
    lcd.print("Temp: " + String(temperature) + "C");
    lcd.setCursor(0, 1);
    lcd.print("H:" + String(humidity) + "% L:" + String(lightIntensity));
}

void publishData() {
  // Store the latest LDR sensor value in the circular buffer
  ldrSensorValues[ldrValueIndex] = lightIntensity;
  ldrValueIndex = (ldrValueIndex + 1) % 15;

  if (protocol == "http") {
    sendHTTPData();
  } else if (protocol == "mqtt") {
    publishMQTTData();
  }
}

// Setup OTA
void setupOTA() {
  ArduinoOTA.setHostname("MautoESP32");
  ArduinoOTA.setPassword("mauto25");
  ArduinoOTA.begin();
}

void loadConfig() {
    // Load configurations from preferences
    deviceName = preferences.getString("deviceName", "Smart_Object_1");
    dLocation = preferences.getString("dLocation", "indoors");
    deviceID = preferences.getInt("deviceID", 1);
    protocol = preferences.getString("protocol", "http");
    fanMode = preferences.getString("fanMode", "auto");
    triggerTemp = preferences.getFloat("triggerTemp", 30.0);

    Serial.println("Loaded Configurations:");
    Serial.println("Device Name: " + deviceName);
    Serial.println("Location: " + dLocation);
    Serial.println("Device ID: " + String(deviceID));
    Serial.println("Protocol: " + protocol);
    Serial.println("Fan Mode: " + fanMode);
    Serial.println("Trigger Temperature: " + String(triggerTemp));
}

void setup() {
  Serial.begin(115200);  

  // Initialize the LCD 
  lcd.begin(16, 2); 
  lcd.setBacklight(255); 

  // Start connecting to Wi-Fi
  lcd.setCursor(0, 0);
  lcd.print("Connecting to");
  lcd.setCursor(0, 1);
  lcd.print("WiFi...");
  Serial.println("Connecting to WiFi...");

  WiFi.begin(ssid, password);

  // Wait until connected
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  // Clear the LCD and print the IP address
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("IP Address:");
  lcd.setCursor(0, 1);
  lcd.print(WiFi.localIP().toString());

  // Print IP address to Serial for debugging
  Serial.println("\nWiFi Connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Allow time for the user to see the IP address
  delay(8000); // Display the IP address for 8 seconds

  // Initialize peripherals
  pinMode(LEDPIN, OUTPUT);
  pinMode(RELAYPIN, OUTPUT);
  digitalWrite(RELAYPIN, LOW); 

  // Connect to Wi-Fi
  connectToWiFi();

  //enable OTA
  setupOTA();

  // Start Web Server
  server.on("/", handleRoot);
  server.on("/config", HTTP_GET, handleConfig);
  server.on("/save-config", HTTP_POST, handleSaveConfig);
  server.on("/sensor-data", handleSensorData);
  server.on("/start-fan", handleStartFan);
  server.on("/stop-fan", handleStopFan);
  server.on("/ldr-data", handleLdrData);
  server.on("/reset-config", handleResetConfig);
  server.begin();


  // Initialize Preferences
  preferences.begin("smart-config", false); 
  loadConfig(); 

  // Initialize the DHT sensor
  dht.begin();
}

void loop() {
  //handle OTA
  ArduinoOTA.handle();

  //READ SENSORS 
  readSensors();

  // Display on LCD
  updateLCD();

  // Control the Fan
  controlFan();

  // Publish data at the 6 seconds interval
  if (millis() - lastDataPost >= postInterval) {
    publishData();
    lastDataPost = millis();
  }

  // Handle HTTP requests
  server.handleClient();

  // Handle the heartbeat signal
  handleHeartbeat();
}