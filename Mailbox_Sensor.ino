#include <WiFi.h>
#include <ESP_Mail_Client.h>
#include "esp_sleep.h"

const char* ssid = "<Your-WiFi-SSID>";
const char* password = "<Your-WiFi-password>";
// Email setup
#define SMTP_HOST "smtp.gmail.com"
#define SMTP_PORT 465
#define EMAIL_SENDER "<sender-email>"
#define EMAIL_SENDER_PASSWORD "<Gmail-app-password>"
#define EMAIL_RECIPIENT "<recipient-email>"
#define USER_DOMAIN "<Your-public-IP-address>"
/* Optional extra recipients
#define EMAIL_RECIPIENT2 "<recipient-email>"
#define EMAIL_RECIPIENT3 "<recipient-email>"
#define EMAIL_RECIPIENT4 "<recipient-email>"
*/
SMTPSession smtp;
Session_Config config;

// Pin Setup
const int sensorPin = 12;
const int ledPin = 2;
// Setup wakeup on GPIO 12 (Active LOW)

RTC_DATA_ATTR int lastState = HIGH;  // Retain button state across sleep
RTC_DATA_ATTR int bootCount= 0;
void setup() {
  pinMode(ledPin, OUTPUT);
  pinMode(sensorPin, INPUT_PULLUP);
  bootCount++;
  int currentState = digitalRead(sensorPin);  // Read button state
  Serial.begin(9600);
  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0) {
    Serial.println("Wakeup from Mail Detection!");
    if (currentState == LOW && lastState == HIGH) {  
      Serial.println("Sending Email: Mail Detected!");
      ledBlink(0);
      sendMailEmail();
      // Add a delay to prevent immediate re-triggering if button is still LOW
      delay(3000);  
    }
    else {
      Serial.println("Ignored wake-up due to button still being LOW.");
      delay(5000);
    }
  }
  currentState = digitalRead(sensorPin);
  lastState = currentState;  // Store last state in RTC memory
  Serial.println("Going to sleep...");
  delay(500);
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_12, 0);  // Wake up when GPIO 12 goes LOW
  esp_deep_sleep_start();
}

void sendMailEmail() {
  WiFi.begin(ssid, password);
  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 30) {  // Wait up to 15 seconds
    delay(500);
    Serial.print(".");
    retries++;
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Failed to connect to WiFi!");
    return;  // Exit if no WiFi
  }
  Serial.println("Connected to WiFi");
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
  message.addRecipient( F"<recipient-name>"), EMAIL_RECIPIENT);
  /* Optional Extra recipients
  message.addRecipient( F("<recipient-name>"), EMAIL_RECIPIENT2);
  message.addRecipient( F("<recipient-name>"), EMAIL_RECIPIENT3);
  message.addRecipient( F("<recipient-name>"), EMAIL_RECIPIENT4);
*/


  String htmlMsg = "<div style=\"color:#000000;\"><h1>(⌐■_■)</h1>"
                    "<p>A new letter has been detected in your mailbox!</p>"
                    "<p>📬 Mailbox wake-up count: <b>" + String(bootCount) + 
                    "</b><br><br>If this is 1 and is not the first time you started me up,"
                    " then I have lost power at some point.</p></div>";
  message.html.content = htmlMsg.c_str();
  message.html.charSet = F("utf-8");
  message.html.transfer_encoding = "base64";

  if (!MailClient.sendMail(&smtp, &message)) {
    Serial.printf("Error, Status Code: %d, Error Code: %d, Reason: %s\n",
                  smtp.statusCode(), smtp.errorCode(), smtp.errorReason().c_str());
  }
  
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);  // Turn off WiFi to save power
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
void loop(){};