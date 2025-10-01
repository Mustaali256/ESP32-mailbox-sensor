#include <WiFi.h>
#include <ESP_Mail_Client.h>
#include "esp_sleep.h"

const char* ssid = "<your-wifi-ssid-here>";
const char* password = "<your-wifi-password-here>";

// Email setup
#define SMTP_HOST "smtp.gmail.com"
#define SMTP_PORT 465
#define EMAIL_SENDER "<your-sender-email-here>"
#define EMAIL_SENDER_PASSWORD "<your-app-password-here>"
#define EMAIL_RECIPIENT "<your-recipient-email-here>"
#define EMAIL_RECIPIENT2 "<your-second-recipient-here>"
#define EMAIL_RECIPIENT3 "<your-third-recipient-here>"
#define USER_DOMAIN "<your-domain-here>"

SMTPSession smtp;
Session_Config config;

// Pin Setup
const int sensorPin = 12;  // GPIO 12 for testing with button
const int ledPin = 2;      // Built-in LED
// Setup wakeup on GPIO 12 (Active LOW) - button press pulls to ground

RTC_DATA_ATTR int lastState = HIGH;  // Retain sensor state across sleep
RTC_DATA_ATTR int bootCount = 0;
RTC_DATA_ATTR int consecutiveWakeups = 0;  // Track consecutive wakeups from same trigger
RTC_DATA_ATTR unsigned long lastEmailTime = 0;  // Track when last email was sent
RTC_DATA_ATTR bool stuckMailDetected = false;  // Flag for stuck mail condition

// Constants for stuck mail detection
const int MAX_CONSECUTIVE_WAKEUPS = 5;  // Max wakeups before considering mail stuck
const unsigned long STUCK_MAIL_SLEEP_TIME = 300000;  // 5 minutes in milliseconds
const unsigned long MIN_EMAIL_INTERVAL = 600000;  // 10 minutes between emails (in milliseconds)

// Test mode - set to true for testing without emails
const bool TEST_MODE = false;  // Change to false for production

void setup() {
  // Initialize pins first
  pinMode(ledPin, OUTPUT);
  pinMode(sensorPin, INPUT_PULLUP);
  
  // Initialize serial and wait a moment for it to be ready
  Serial.begin(115200);  // Higher baud rate for better debugging
  delay(1000);
  Serial.println();
  Serial.println("=== ESP32 Mailbox Sensor Starting ===");
  
  bootCount++;
  Serial.printf("Boot count: %d\n", bootCount);
  
  // Read sensor state
  int currentSensorState = digitalRead(sensorPin);
  Serial.printf("Current sensor reading: %d\n", currentSensorState);
  
  // Check reset reason
  esp_reset_reason_t reset_reason = esp_reset_reason();
  Serial.printf("Reset reason: %d\n", reset_reason);
  
  // Add LED indicator for boot
  digitalWrite(ledPin, HIGH);
  delay(500);
  digitalWrite(ledPin, LOW);
  
  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0) {
    Serial.println("Wakeup from Break Beam Sensor!");
    Serial.printf("Consecutive wakeups: %d\n", consecutiveWakeups);
    
    if (currentSensorState == LOW) {
      consecutiveWakeups++;
      
      // Check if mail might be stuck
      if (consecutiveWakeups >= MAX_CONSECUTIVE_WAKEUPS) {
        if (!stuckMailDetected) {
          Serial.println("STUCK MAIL DETECTED - Multiple consecutive wakeups!");
          stuckMailDetected = true;
          
          // Send stuck mail notification email
          ledBlink(0);  // Fast blink to indicate issue
          if (TEST_MODE) {
            Serial.println("TEST MODE: Skipping stuck mail email");
          } else {
            sendStuckMailEmail();
          }
          
          Serial.printf("Going to extended sleep for %d minutes due to stuck mail\n", STUCK_MAIL_SLEEP_TIME / 60000);
          esp_sleep_enable_timer_wakeup(STUCK_MAIL_SLEEP_TIME * 1000);  // Convert to microseconds
          esp_deep_sleep_start();
        } else {
          Serial.println("Already notified about stuck mail, going to extended sleep");
          esp_sleep_enable_timer_wakeup(STUCK_MAIL_SLEEP_TIME * 1000);
          esp_deep_sleep_start();
        }
      } else {
        // Normal mail detection
        if (lastState == HIGH) {  // Only send email if previous state was clear
          unsigned long currentTime = millis();
          // Add basic time-based protection (rough estimate since we don't have RTC)
          if (lastEmailTime == 0 || (bootCount - lastEmailTime > 5)) {  // Use bootCount as rough time estimate
            Serial.println("Mail Detected!");
            ledBlink(0);
            if (TEST_MODE) {
              Serial.println("TEST MODE: Skipping email sending");
            } else {
              Serial.println("Sending Email...");
              sendMailEmail();
            }
            lastEmailTime = bootCount;
            delay(3000);
          } else {
            Serial.println("Email recently sent, skipping to avoid spam");
          }
        }
        
        // Wait a bit to see if beam clears quickly (mail passing through)
        Serial.println("Waiting to see if mail clears the beam...");
        delay(2000);  // Wait 2 seconds
        
        if (digitalRead(sensorPin) == HIGH) {
          Serial.println("Mail cleared quickly - normal delivery");
          consecutiveWakeups = 0;  // Reset counter
          stuckMailDetected = false;  // Reset stuck flag
          lastState = HIGH;
        } else {
          Serial.println("Mail still blocking beam");
          lastState = LOW;
          // Don't reset consecutive wakeups yet
        }
      }
    } else {
      // Sensor is HIGH (beam not broken)
      Serial.println("Sensor clear on wakeup - resetting stuck mail detection");
      consecutiveWakeups = 0;
      stuckMailDetected = false;
      lastState = HIGH;
    }
  } else {
    // First boot or timer wakeup - add delay to see what's happening
    Serial.print("Wakeup cause: ");
    Serial.println(esp_sleep_get_wakeup_cause());
    
    if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER) {
      Serial.println("Wakeup from timer (stuck mail check)");
      if (currentSensorState == HIGH) {
        Serial.println("Mail no longer stuck - resuming normal operation");
        consecutiveWakeups = 0;
        stuckMailDetected = false;
      } else {
        Serial.println("Mail still stuck - continuing extended sleep");
      }
    } else {
      Serial.println("First boot or other wakeup cause - checking for boot loop");
      // Reset all RTC variables on unexpected boot
      consecutiveWakeups = 0;
      stuckMailDetected = false;
      
      // Add a delay to prevent rapid rebooting
      Serial.println("Adding 5 second delay to prevent boot loop...");
      delay(5000);
    }
    lastState = currentSensorState;
    Serial.printf("Sensor state: %s\n", lastState == HIGH ? "HIGH (beam clear)" : "LOW (beam blocked)");
  }
  
  Serial.println("Going to sleep...");
  Serial.flush();  // Ensure all serial data is sent before sleep
  delay(1000);     // Increased delay to ensure everything is ready
  
  // Disable all wakeup sources first
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  
  // Choose sleep mode based on stuck mail status
  if (stuckMailDetected && digitalRead(sensorPin) == LOW) {
    Serial.println("Using timer wakeup due to stuck mail");
    esp_sleep_enable_timer_wakeup(STUCK_MAIL_SLEEP_TIME * 1000ULL);  // Extended sleep (ULL for microseconds)
  } else {
    Serial.println("Using sensor wakeup for normal operation");
    // Check if GPIO 12 is available for EXT0 wakeup
    Serial.printf("Current GPIO 12 state before sleep: %d\n", digitalRead(sensorPin));
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_12, 0);  // Wake up when GPIO 12 goes LOW
  }
  
  Serial.println("Entering deep sleep now...");
  Serial.flush();
  delay(100);
  
  esp_deep_sleep_start();
}

void sendMailEmail() {
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 30) {  // Wait up to 15 seconds
    delay(500);
    Serial.print(".");
    retries++;
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Failed to connect to WiFi!");
    // Clean up WiFi before returning
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    return;  // Exit if no WiFi
  }
  
  Serial.println("\nConnected to WiFi");
  ledBlink(1);
  smtp.debug(1);
  smtp.callback(smtpCallback);

  config.server.host_name = SMTP_HOST;
  config.server.port = SMTP_PORT;
  config.login.email = EMAIL_SENDER;
  config.login.password = EMAIL_SENDER_PASSWORD;
  config.login.user_domain = USER_DOMAIN;
  config.time.ntp_server = "pool.ntp.org,time.nist.gov";
  smtp.connect(&config);

  SMTP_Message message;
  message.sender.name = "Mailbox Sensor";
  message.sender.email = EMAIL_SENDER;
  message.subject = "You've Got Mail!";
  message.addRecipient( F("<recipient-name-1>"), EMAIL_RECIPIENT);
  message.addRecipient( F("<recipient-name-2>"), EMAIL_RECIPIENT2);
  message.addRecipient( F("<recipient-name-3>"), EMAIL_RECIPIENT3);

  String htmlMsg = "<div style=\"color:#000000;\"><h1>(⌐■_■)</h1>"
                    "<p>A new letter has been detected in your mailbox!</p>"
                    "<p>📬 Mailbox wake-up count: <b>" + String(bootCount) + 
                    "</b><br><br>If this is 1 and is not the first time you started me up,"
                    " then I have lost power at some point.</p></div>";
  message.html.content = htmlMsg.c_str();
  message.html.charSet = F("utf-8");
  message.html.transfer_encoding = "base64";

  // Set a timeout for email sending (30 seconds)
  unsigned long emailStartTime = millis();
  bool emailSent = false;
  
  if (!MailClient.sendMail(&smtp, &message)) {
    Serial.printf("Error, Status Code: %d, Error Code: %d, Reason: %s\n",
                  smtp.statusCode(), smtp.errorCode(), smtp.errorReason().c_str());
  } else {
    emailSent = true;
    Serial.println("Email sent successfully!");
  }
  
  // Check if email process took too long
  if (millis() - emailStartTime > 30000) {
    Serial.println("Email sending timeout reached");
  }
  
  // Clean up WiFi and SMTP connection
  smtp.closeSession();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);  // Turn off WiFi to save power
  delay(100);  // Give time for WiFi to properly shut down
}

void sendStuckMailEmail() {
  Serial.println("Connecting to WiFi for stuck mail notification...");
  WiFi.begin(ssid, password);
  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 30) {
    delay(500);
    Serial.print(".");
    retries++;
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Failed to connect to WiFi for stuck mail notification!");
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    return;
  }
  
  Serial.println("\nConnected to WiFi");
  ledBlink(1);
  
  smtp.debug(1);
  smtp.callback(smtpCallback);

  config.server.host_name = SMTP_HOST;
  config.server.port = SMTP_PORT;
  config.login.email = EMAIL_SENDER;
  config.login.password = EMAIL_SENDER_PASSWORD;
  config.login.user_domain = USER_DOMAIN;
  config.time.ntp_server = "pool.ntp.org,time.nist.gov";
  smtp.connect(&config);

  SMTP_Message message;
  message.sender.name = "Mailbox Sensor";
  message.sender.email = EMAIL_SENDER;
  message.subject = "⚠️ Mailbox Issue - Mail May Be Stuck";
  message.addRecipient(F("<recipient-name-1>"), EMAIL_RECIPIENT);
  message.addRecipient(F("<recipient-name-2>"), EMAIL_RECIPIENT2);
  message.addRecipient(F("<recipient-name-3>"), EMAIL_RECIPIENT3);

  String htmlMsg = "<div style=\"color:#ff6600;\"><h1>⚠️ Mailbox Alert</h1>"
                    "<p><strong>Issue detected:</strong> Mail appears to be stuck in your mailbox!</p>"
                    "<p>📬 The sensor has been triggered <b>" + String(consecutiveWakeups) + 
                    "</b> times consecutively, suggesting mail is jammed in the slot.</p>"
                    "<p>🔧 <strong>Action needed:</strong> Please check the mailbox and clear any stuck mail.</p>"
                    "<p>💤 The sensor will now sleep for 5-minute intervals to conserve battery until the issue is resolved.</p>"
                    "<p>📊 Total wake-up count: <b>" + String(bootCount) + "</b></p></div>";
  message.html.content = htmlMsg.c_str();
  message.html.charSet = F("utf-8");
  message.html.transfer_encoding = "base64";

  if (!MailClient.sendMail(&smtp, &message)) {
    Serial.printf("Error sending stuck mail notification, Status Code: %d, Error Code: %d, Reason: %s\n",
                  smtp.statusCode(), smtp.errorCode(), smtp.errorReason().c_str());
  } else {
    Serial.println("Stuck mail notification sent successfully!");
  }

  // Clean up
  smtp.closeSession();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  delay(100);
}

// Callback function to get the Email sending status
void smtpCallback(SMTP_Status status) {
  Serial.println(status.info());

  if (status.success()) {
    Serial.println("----------------");
    MailClient.printf("Message sent success: %d\n", status.completedCount());
    MailClient.printf("Message sent failed: %d\n", status.failedCount());
    Serial.println("----------------\n");

    for (size_t i = 0; i < smtp.sendingResult.size(); i++) {
      SMTP_Result result = smtp.sendingResult.getItem(i);
      MailClient.printf("Message No: %d\n", i + 1);
      MailClient.printf("Status: %s\n", result.completed ? "success" : "failed");
      MailClient.printf("Date/Time: %s\n", MailClient.Time.getDateTimeString(result.timestamp, "%B %d, %Y %H:%M:%S").c_str());
      MailClient.printf("Recipient: %s\n", result.recipients.c_str());
      MailClient.printf("Subject: %s\n", result.subject.c_str());
    }
    Serial.println("----------------\n");

    smtp.sendingResult.clear();
  }
}

// Function to blink LED in different modes
void ledBlink(int mode) {
  /* LED modes:
     Fast Blink: 0
     Slow Blink: 1
  */
  int delayInMilliseconds = (mode == 0) ? 200 : 500;
  for (int i = 0; i < 3; i++) {
    if (i != 0) delay(delayInMilliseconds);
    digitalWrite(ledPin, HIGH);
    delay(delayInMilliseconds);
    digitalWrite(ledPin, LOW);
  }
}
void loop() {
  // This function should never be reached since we use deep sleep
  // If we somehow get here, go back to sleep
  Serial.println("Unexpected loop() execution - going back to sleep");
  esp_deep_sleep_start();
}