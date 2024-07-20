#include <WiFi.h>
#include <ESP_Mail_Client.h>

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
const int sensorPin = 18; //Set your GPIO Pin to match the break beam sensor pin
const int ledPin = 2;
int sensorState = 0, lastState = 0;

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

void setup() {
  pinMode(ledPin, OUTPUT);
  pinMode(sensorPin, INPUT);
  digitalWrite(sensorPin, HIGH);
  Serial.begin(9600);

  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Connected to WiFi");
  ledBlink(1);
  MailClient.networkReconnect(true);

  // Initialise email client
  smtp.debug(1);
  smtp.callback(smtpCallback);
  
  // Email configuration setup
  config.server.host_name = SMTP_HOST;
  config.server.port = SMTP_PORT;
  config.login.email = EMAIL_SENDER;
  config.login.password = EMAIL_SENDER_PASSWORD;
  config.login.user_domain = USER_DOMAIN;
  config.time.ntp_server = "pool.ntp.org,time.nist.gov";
  config.time.gmt_offset = 1;  // 1 hour
  config.time.day_light_offset = 0;
  smtp.connect(&config);
  email(0);
  /*Email config:
    Startup Email: 0
    Mail Notification Email: 1
  */
}

void loop() {
  sensorState = digitalRead(sensorPin);
  digitalWrite(ledPin, sensorState == LOW ? HIGH : LOW);

  if (!sensorState && lastState) {
    Serial.println("Letter Detected!");
    email(1);
  }

  lastState = sensorState;
}

void connectWifi() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Connected to WiFi");
  ledBlink(1);
  MailClient.networkReconnect(true);
}

void email(int mode) {  //initialises the email process, then sends an email specific to the mode parameter it was given.
  if (WiFi.status() != WL_CONNECTED) connectWifi();

  if (!smtp.connect(&config)) {
    Serial.printf("Connection error, Status Code: %d, Error Code: %d, Reason: %s\n", smtp.statusCode(), smtp.errorCode(), smtp.errorReason().c_str());
    return;
  }

  if (mode == 0) {
    sendStartupEmail();
  } else if (mode == 1) {
    sendMailEmail();
  }
}

void ledBlink(int mode) {
  /*LED modes:
    Fast Blink: 0
    Slow Blink: 1
  */
  int delayInMilliseconds = 200;
  if (mode == 0) delayInMilliseconds = 500;
  for (int i = 0; i < 3; i++) {
    if (i != 0) delay(delayInMilliseconds);
    digitalWrite(ledPin, HIGH);
    delay(delayInMilliseconds);
    digitalWrite(ledPin, LOW);
  }
}

void sendMailEmail() {
  SMTP_Message message;
  message.sender.name = "Mailbox Sensor";
  message.sender.email = EMAIL_SENDER;
  message.subject = "You've Got Mail!";
  message.addRecipient( F("<recipient-name>"), EMAIL_RECIPIENT);
  /* Optional Extra recipients
  message.addRecipient( F("<recipient-name>"), EMAIL_RECIPIENT2);
  message.addRecipient( F("<recipient-name>"), EMAIL_RECIPIENT3);
  message.addRecipient( F("<recipient-name>"), EMAIL_RECIPIENT4);
*/
  String htmlMsg = "<div style=\"color:#000000;\"><h1>(⌐■_■)</h1><p>A new letter has been detected in your mailbox.</p></div>";
  message.html.content = htmlMsg.c_str();
  message.html.charSet = F("utf-8");
  message.html.transfer_encoding = "base64";

  if (!MailClient.sendMail(&smtp, &message)) {
    Serial.printf("Error, Status Code: %d, Error Code: %d, Reason: %s\n", smtp.statusCode(), smtp.errorCode(), smtp.errorReason().c_str());
  }
  ledBlink(0);
}

void sendStartupEmail() {
  SMTP_Message message;
  message.sender.name = "Mailbox Sensor";
  message.sender.email = EMAIL_SENDER;
  message.subject = "Mailbox Sensor Has Booted Up!";
  message.addRecipient( F("<recipient-name>"), EMAIL_RECIPIENT);
  /* Optional Extra recipients
  message.addRecipient( F("<recipient-name>"), EMAIL_RECIPIENT2);
  message.addRecipient( F("<recipient-name>"), EMAIL_RECIPIENT3);
  message.addRecipient( F("<recipient-name>"), EMAIL_RECIPIENT4);
*/

  String htmlMsg = "<p>\\(ˆ˚ˆ)/</p>";
  message.html.content = htmlMsg.c_str();
  message.html.charSet = F("utf-8");
  message.html.transfer_encoding = "base64";

  if (!MailClient.sendMail(&smtp, &message)) {
    Serial.printf("Error, Status Code: %d, Error Code: %d, Reason: %s\n", smtp.statusCode(), smtp.errorCode(), smtp.errorReason().c_str());
  }
  ledBlink(0);
}
