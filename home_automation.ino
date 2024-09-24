#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <time.h>
#include <KMESerial.h>
#include <DHT.h>

// Wi-Fi credentials
const char* ssid = "INTECH 2.4G";
const char* password = "Rawale@123";

// NTP server and time zone details
const char* ntpServer = "time.google.com";
const long gmtOffset_sec = 19800;  // GMT+5:30 (Indian Standard Time)
const int daylightOffset_sec = 0;  // No daylight saving in India

// Telegram Bot credentials
#define BOTtoken "7215863945:AAHTJMy49kovUyq7DjXFMRlMaXxDuTPllO8" // Replace with your bot token
#define CHAT_ID "7166272891"    // Replace with your chat ID

// Custom characters for Wi-Fi and power status
byte wifiOn[8] = {
  0b00111,
  0b01111,
  0b11111,
  0b10101,
  0b10101,
  0b10001,
  0b10001,
  0b00000
};

byte wifiOff[8] = {
  0b00000,
  0b00000,
  0b01110,
  0b10001,
  0b10001,
  0b01110,
  0b00000,
  0b00000
};

byte Check[8] = {
  0b00000,
  0b00001,
  0b00011,
  0b10110,
  0b11100,
  0b01000,
  0b00000,
  0b00000
};

byte MiniX[8] = {
  0b00000,
  0b00000,
  0b01010,
  0b00100,
  0b01010,
  0b00000,
  0b00000,
  0b00000
};

// LCD setup
const int lcdColumns = 16;
const int lcdRows = 2;
LiquidCrystal_I2C lcd(0x27, lcdColumns, lcdRows);

// Power cut detection setup
const int powerPin = D0;  // GPIO pin to detect power status
const int dhtPin = D6;    // GPIO pin for DHT11 sensor
bool powerState = true; // Variable to store current power state

// DHT sensor setup
#define DHTTYPE DHT11
DHT dht(dhtPin, DHTTYPE);

// Telegram bot setup
WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);

// KMESerial setup
KMESerial KMESerial;

unsigned long previousMillis = 0;
const long interval = 10000; // Interval to switch displays (10 seconds)
bool displayTempHumidity = false; // Toggle for display window

// Debouncing setup
const int debounceDelay = 50;  // Debounce delay in milliseconds
unsigned long lastDebounceTime = 0;  // Last time the pin state was checked
bool lastPowerState = HIGH;  // Previous stable state of the power pin

void switchstate(KME State) {
  KMESerial.SetSwitch(State.id, State.value == 1 ? true : false);
}

void setup() {
  // Initialize Serial Monitor
  Serial.begin(74880); // Set the baud rate to match your communication settings

  // Initialize KMESerial
  // No KMESerial.begin() call needed
  KMESerial.setCallback(switchstate);

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  int attempt = 0;
  while (WiFi.status() != WL_CONNECTED && attempt < 10) {
    delay(500);
    Serial.print(".");
    attempt++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nFailed to connect to WiFi");
    return;
  }

  // Initialize LCD
  lcd.init();
  lcd.backlight();
  lcd.createChar(0, wifiOn);
  lcd.createChar(1, wifiOff);
  lcd.createChar(2, Check);   // Power Available
  lcd.createChar(3, MiniX);   // Power Not Available

  // Initialize DHT sensor
  dht.begin();

  // Check if the DHT sensor is working
  if (isDHTWorking()) {
    // Initialize and get the time
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    if (!attemptTimeFetch()) {
      Serial.println("Failed to obtain time.");
    }

    // Set up pins
    pinMode(powerPin, INPUT_PULLUP); // Set the power pin as input with an internal pull-up resistor

    // Initialize Telegram client
    client.setInsecure(); // Disable SSL certificate verification

    // Notify on Telegram
    bot.sendMessage(CHAT_ID, "WiFi Connected!", "");
    bot.sendMessage(CHAT_ID, "System has Started!!", "");

    // Show loading animation while booting
    showLoadingScreen();

    // Show welcome screen
    showWelcomeScreen();

    // Display the time window initially
    displayTempHumidity = false; // Ensure we start with the time window
    updateDisplay();
  } else {
    // Display error message if DHT sensor is not working
    showErrorScreen();
    while (true); // Stop further execution
  }
}

void loop() {
  unsigned long currentMillis = millis();

  // KMESerial loop
  KMESerial.loop();

  // Read the power pin state with debounce
  bool reading = digitalRead(powerPin);

  if (reading != lastPowerState) {
    lastDebounceTime = currentMillis;  // Reset the debounce timer
  }

  if ((currentMillis - lastDebounceTime) > debounceDelay) {
    if (reading != powerState) {
      powerState = reading;  // Update the stable state
      if (powerState == LOW) {
        // Power cut detected
        bot.sendMessage(CHAT_ID, "ALERT! Power Cut Detected!!", "");
      } else {
        // Power restored
        bot.sendMessage(CHAT_ID, "Power Restored!", "");
      }
    }
  }

  lastPowerState = reading;  // Save the current state for the next loop iteration

  // Switch display every 10 seconds
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    displayTempHumidity = !displayTempHumidity;
    lcd.clear(); // Clear the screen before switching windows
    updateDisplay(); // Update the display based on the current window
  }

  if (displayTempHumidity) {
    displayTemperatureHumidity();
  } else {
    updateDisplay();
  }

  // Update KMESerial with temperature and humidity data
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();

  if (!isnan(humidity) && !isnan(temperature)) {
    int intTemperature = static_cast<int>(temperature);
    int intHumidity = static_cast<int>(humidity);

    KMESerial.setSensor(0, intTemperature);
    KMESerial.setSensor(1, intHumidity);
  } else {
    Serial.println("Failed to read DHT sensor");
  }

  delay(1000); // Delay for stability
}

bool attemptTimeFetch() {
  struct tm timeinfo;
  for (int i = 0; i < 5; i++) { // Reduce attempts to 5
    if (getLocalTime(&timeinfo)) {
      return true;
    }
    delay(2000); // Wait for 2 seconds before retrying
  }
  return false;
}

bool isDHTWorking() {
  // Attempt to read from DHT sensor
  float temp = dht.readTemperature();
  float humidity = dht.readHumidity();

  // Check if the readings are valid
  if (isnan(temp) || isnan(humidity)) {
    return false; // DHT sensor not working
  }
  return true; // DHT sensor is working
}

void updateDisplay() {
  if (!displayTempHumidity) {
    printLocalTime();
    displayWiFiStatus();
    displayPowerStatus();
  }
}

void printLocalTime() {
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    char dateString[17];
    strftime(dateString, sizeof(dateString), "%a %d-%m-%y", &timeinfo);

    char timeString[17];
    strftime(timeString, sizeof(timeString), "%H:%M:%S", &timeinfo);

    lcd.setCursor(0, 0);
    lcd.print(timeString);
    lcd.setCursor(0, 1);
    lcd.print(dateString);
  } else {
    Serial.println("Failed to obtain time");
    lcd.setCursor(0, 0);
    lcd.print("Time not found");
  }
}

void displayWiFiStatus() {
  lcd.setCursor(14, 0);
  if (WiFi.status() == WL_CONNECTED) {
    lcd.write(byte(0)); // Wi-Fi on custom character
  } else {
    lcd.write(byte(1)); // Wi-Fi off custom character
  }
}

void displayPowerStatus() {
  lcd.setCursor(15, 1);
  if (powerState == HIGH) {
    lcd.write(byte(2)); // Power available custom character
  } else {
    lcd.write(byte(3)); // Power not available custom character
  }
}

void displayTemperatureHumidity() {
  lcd.setCursor(0, 0);
  lcd.print("Temp: ");
  lcd.print(dht.readTemperature());
  lcd.print((char)223);
  lcd.print("C");

  lcd.setCursor(0, 1);
  lcd.print("Hum: ");
  lcd.print(dht.readHumidity());
  lcd.print("%");
}

void showLoadingScreen() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("System Starting...");
  lcd.setCursor(0, 1);
  lcd.print("Please wait...");
  delay(2000);  // Show loading screen for 2 seconds
}

void showWelcomeScreen() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Welcome to the");
  lcd.setCursor(0, 1);
  lcd.print("Home Automation");
  delay(2000);  // Show welcome screen for 2 seconds
}

void showErrorScreen() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("DHT Sensor Error");
  lcd.setCursor(0, 1);
  lcd.print("Check Wiring!");
}