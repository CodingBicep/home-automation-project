# Home Automation Project

This project implements a home automation system using an ESP8266 microcontroller and integrates with Adafruit IO for MQTT communication.

## Features
- Wi-Fi connectivity
- Power cut detection and SMS alert
- MQTT subscription to control outputs

## Setup
1. Install the required libraries:
   - ESP8266WiFi
   - TinyGsmClient
   - Adafruit_MQTT

2. Configure your Wi-Fi credentials and Adafruit IO settings in the `HomeAutomation.ino` file.

3. Upload the code to your ESP8266.

4. Monitor your power status and control outputs via MQTT.
