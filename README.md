# ESP32 Break beam Mailbox Sensor

## Overview

This project involves an ESP32-based mailbox sensor that detects when a letter is inserted into a mailbox. When a letter is detected, the system sends an email notification to a recipient (or multiple).

A break beam sensor is used for mailboxes which do not have a flap and only an opening for a letter to enter.

## Features

- Detects if a letter has been sent in your mailbox
- Sends email notifications when a letter is detected in the mailbox.
- Configurable to send emails to multiple recipients.

## Hardware Required
- ESP32 Development Board
- Break beam sensor
  
## Software Required

- [Arduino IDE](https://www.arduino.cc/en/software)
- ESP32 Core for Arduino
- ESP Mail Client library

## Installation

1. **Install the Arduino IDE**:
   Download and install the Arduino IDE.

2. **Install ESP32 Core**:
   Open the Arduino IDE and navigate to `File` > `Preferences`. Add the following URL to the `Additional Boards Manager URLs` field: `https://dl.espressif.com/dl/package_esp32_index.json`

Then, go to `Tools` > `Board` > `Boards Manager`, search for `ESP32`, and install it.

3. **Install the ESP Mail Client Library**:
Go to `Sketch` > `Include Library` > `Manage Libraries...`. Search for `ESP Mail Client` and install it.

4. **Clone or Download the Repository**:
Clone this repository or download it as a ZIP file and extract it.

5. **Upload the Code**:
Open the `Mailbox_Sensor.ino` file in the Arduino IDE. Connect your ESP32 board, select the correct board and port from `Tools`, and upload the code.

## Configuration

In the code, update the following configuration settings:

- **WiFi Credentials**:
```cpp
const char* ssid = "<Your-WiFi-SSID>";
const char* password = "<Your-WiFi-password>";
```
- **Email Setup**:
```cpp
#define EMAIL_SENDER "<sender-email>"
#define EMAIL_SENDER_PASSWORD "<Gmail-app-password>"
#define EMAIL_RECIPIENT "<recipient-email>"
#define USER_DOMAIN "<Your-public-IP-address>"
```
- **GPIO Pins**:
  
  **Make sure your pin supports RTC or it will not wake up**

  I have set my pin to 12 because that worked for me, however depending on your board it may not work for you so please try different pins.
```cpp
const int sensorPin = <pin>; // Set your GPIO Pin to match the break beam sensor pin
const int ledPin = 2; // Usually the led on the board is at pin 2
```
## Usage
- Connect the ESP32 to your computer and flash the code onto it.
- Upon restarting and correct configuration, you will notice the LED on the board flashes fast, which means it has connected to WiFi.
- Open the Serial Monitor in `Tools` > `Serial Monitor` to view the connection status and debug information.
- Break the sensor beam to test if the system sends an email notification.

## Gmail
  If you are using a Gmail account to send the confirmation email, you will need to turn on two-factor authentication and [set an app-specific password](https://myaccount.google.com/apppasswords) which you will use to replace your password in the `Mailbox_Sensor.ino` file.
  For example, if your app password looks like this:
  ```
  abcd efgh ijkl mnop
  ```
  then set the EMAIL_SENDER_PASSWORD variable to this:
  ```cpp
  #define EMAIL_SENDER_PASSWORD "abcdefghijklmnop"
  ```
## Troubleshooting
- **WiFi Connection Issues**:
Ensure your WiFi credentials are correct and the ESP32 is within range of your network.

- **Email Sending Problems**:
Verify SMTP settings and ensure that the email sender's account uses an app password.

- **NTP Time Sync Issues**:
Check the network connectivity and NTP server settings and retry.
