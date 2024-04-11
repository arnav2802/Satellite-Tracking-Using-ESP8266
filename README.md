# Satellite-Tracking-Using-ESP8266

**Satellite Tracker**
This repository contains the code for a satellite tracker using Arduino and ESP8266. The system uses the Celestrak API to fetch TLE data for multiple satellites, calculates the next pass for each satellite, and moves two stepper motors to track the satellite during the pass.

**NOTE**
This code was originally written to work on #ESP32 but it was repurposed to run on #NODEMCU-ESP8266.

**Features**
Real-time satellite tracking using stepper motors
Serial communication with Celestrak API
Data saving to a text file
Technologies Used
Arduino
ESP8266
AccelStepper library
Adafruit_LEDBackpack library
Sgp4 library
MD_Parola library
MD_MAX72xx library
SPI library


**Setup**
To use this code for your own satellite tracker, follow these steps:

Install the required libraries on your Arduino IDE.
Connect the stepper motors to your Arduino board and upload the provided Arduino code.
Adjust the serial communication settings in the script to match your Celestrak API's settings.

**License**
This project is licensed under the MIT License - see the LICENSE file for details.

**Contact**
If you have any questions or comments about this project, please contact me at arnav171103@gmial.com

