#define TINY_GSM_MODEM_SIM800  // Define the modem model

#include <ESP8266WiFi.h>
#include <TinyGsmClient.h>
#include <Adafruit_MQTT.h>
#include <Adafruit_MQTT_Client.h>
#include <SoftwareSerial.h>
#include "config.h"  // Include your configuration file

// Create WiFi and MQTT clients
WiFiClient client;
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, IO_USERNAME, IO_KEY);

// MQTT Subscribe objects
Adafruit_MQTT_Subscribe Loc = Adafruit_MQTT_Subscribe(&mqtt, IO_USERNAME "/feeds/location");
Adafruit_MQTT_Subscribe GA = Adafruit_MQTT_Subscribe(&mqtt, IO_USERNAME "/feeds/google-assistance");

// MQTT Publish objects
Adafruit_MQTT_Publish locReset = Adafruit_MQTT_Publish(&mqtt, IO_USERNAME "/feeds/location");
Adafruit_MQTT_Publish gaReset = Adafruit_MQTT_Publish(&mqtt, IO_USERNAME "/feeds/google-assistance");

// Power cut detection setup
const int powerPin = D0;
bool powerState = HIGH;  // Assume the power is on at the start

// Output control setup
const int ToCont = 4;

// SIM800 module setup
SoftwareSerial sim800(D5, D6); // RX, TX for SIM800
TinyGsm modem(sim800);  // Create modem instance

// Debouncing setup
const int debounceDelay = 50;
unsigned long lastDebounceTime = 0;
bool lastPowerState = HIGH;

// Global state variables
int Loc_State = 0;
int GA_State = 0;

// Setup function
void setup() {
  Serial.begin(115200);
  sim800.begin(9600);  // Initialize SIM800 module
  modem.restart();     // Restart the modem
  
  // Connect to Wi-Fi
  connectToWiFi();

  // Initialize MQTT and subscribe to feeds
  mqtt.subscribe(&Loc);
  mqtt.subscribe(&GA);

  // Set up pins
  pinMode(powerPin, INPUT_PULLUP);  // Ensure INPUT_PULLUP is correct for power detection
  pinMode(ToCont, OUTPUT);

  // Send initial status SMS
  sendSMS("System has started and connected to WiFi!");

  Serial.println("Setup complete. Monitoring power status...");
}

void loop() {
  MQTT_connect();
  checkMQTTSubscriptions();
  debouncePowerStatus();
  Send_Out();
}

void connectToWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void sendSMS(const char* message) {
  modem.sendSMS(PHONE_NUMBER, message);
  Serial.println("SMS sent to " + String(PHONE_NUMBER) + ": " + message);
}

void checkMQTTSubscriptions() {
  Adafruit_MQTT_Subscribe *subscription;
  while ((subscription = mqtt.readSubscription(100))) {
    if (subscription == &Loc) {
      Loc_State = atoi((char *)Loc.lastread);
      Serial.print(F("Got_Loc: "));
      Serial.println(Loc_State);
    } else if (subscription == &GA) {
      GA_State = atoi((char *)GA.lastread);
      Serial.print(F("Got_GA: "));
      Serial.println(GA_State);
    }

    // Check the condition and perform actions
    if (GA_State == 1 && Loc_State == 1) {
      triggerOutput();
    }
  }
}

void debouncePowerStatus() {
  bool reading = digitalRead(powerPin);  // Read power pin status

  Serial.print("Power Pin State: ");
  Serial.println(reading);

  if (reading != lastPowerState) {
    lastDebounceTime = millis();  // Reset debounce timer
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != powerState) {
      powerState = reading;  // Update the power state

      if (powerState == LOW) {
        Serial.println("Power Cut Detected!");
        sendSMS("ALERT! Power Cut Detected!!✂");
      } else {
        Serial.println("Power Restored!");
        sendSMS("Power Restored!⚡");
      }
    }
  }

  lastPowerState = reading;  // Update the last power state
}

void MQTT_connect() {
  if (mqtt.connected()) {
    return;
  }

  Serial.print("Connecting to MQTT... ");

  uint8_t retries = 3;
  while ((mqtt.connect()) != 0 && retries > 0) {
    Serial.println(mqtt.connectErrorString(mqtt.connect()));
    Serial.println("Retrying MQTT connection in 5 seconds...");
    mqtt.disconnect();
    delay(5000);
    retries--;
  }

  if (retries == 0) {
    Serial.println("Failed to connect to MQTT. Halting execution.");
    while (1);
  }

  Serial.println("MQTT Connected!");
}

void triggerOutput() {
  digitalWrite(ToCont, HIGH);
  delay(100);
  digitalWrite(ToCont, LOW);
  Serial.println("ToCont pulsed");

  // Reset both feeds
  if (locReset.publish(0) && gaReset.publish(0)) {
    Serial.println("Both feeds reset to 0");
  } else {
    Serial.println("Failed to reset feeds");
  }
}

void Send_Out() {
  if (GA_State == 1 && Loc_State == 1) {
    digitalWrite(ToCont, HIGH);
    Serial.println("ToCont HIGH");
  } else {
    digitalWrite(ToCont, LOW);
    Serial.println("ToCont LOW");
  }
  
  Serial.print("Loc State: ");
  Serial.println(Loc_State);
  Serial.print("GA State: ");
  Serial.println(GA_State);
}
