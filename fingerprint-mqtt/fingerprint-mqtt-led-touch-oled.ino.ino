#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_Fingerprint.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Wifi Settings
#define SSID                          "Your Wifi SSID Here"
#define PASSWORD                      "Your WiFi Password Here"

// MQTT Settings
#define HOSTNAME                      "fingerprint-sensor-1"
#define MQTT_SERVER                   "Your MQTT Server here"														
#define STATE_TOPIC                   "fingerprint_sensor/1/state"        //Subscribe here to receive state updates
#define REQUEST_TOPIC                 "fingerprint_sensor/1/request"      //Publish here to make learn and delete requests
#define REPLY_TOPIC                   "fingerprint_sensor/1/reply"        //Publish here to display your action on OLED
#define AVAILABILITY_TOPIC            "fingerprint_sensor/1/available"
#define mqtt_username                 "Your MQTT Username"
#define mqtt_password                 "Your MQTT Password"

// Fingerprint Sensor
#define SENSOR_TX 13                  //GPIO Pin for WEMOS RX, SENSOR TX
#define SENSOR_RX 12                  //GPIO Pin for WEMOS TX, SENSOR RX

// OLED
#define OLED_RESET -1
Adafruit_SSD1306 display(128, 64, &Wire, OLED_RESET);

SoftwareSerial mySerial(SENSOR_TX, SENSOR_RX);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

WiFiClient wifiClient;                // Initiate WiFi library
PubSubClient client(wifiClient);      // Initiate PubSubClient library

int id = 0;                       //Stores the current fingerprint ID
int lastID = 0;                   //Stores the last matched ID
int lastConfidenceScore = 0;      //Stores the last matched confidence score

int i;                                // misc for loop use

//Declare JSON variables
DynamicJsonDocument mqttMessage(200);
char mqttBuffer[200];

const unsigned char fingerprint_bmp [] PROGMEM = {
  // 'fingerprint, 128x64px
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x1f, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x01, 0xff, 0xff, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x0f, 0xff, 0xff, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x3f, 0xe0, 0x07, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0xfe, 0x00, 0x00, 0x7f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x01, 0xf0, 0x00, 0x00, 0x0f, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0xc0, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x7f, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x03, 0xff, 0xff, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x0f, 0xff, 0xff, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x7f, 0x80, 0x01, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0xfc, 0x00, 0x00, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x03, 0xe0, 0x00, 0x00, 0x07, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x07, 0xc0, 0x0f, 0xf0, 0x03, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x0f, 0x00, 0xff, 0xff, 0x00, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x1e, 0x03, 0xff, 0xff, 0xc0, 0x78, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x3c, 0x0f, 0xe0, 0x07, 0xf0, 0x3c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x78, 0x3f, 0x80, 0x01, 0xf8, 0x1e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0xf0, 0x7c, 0x00, 0x00, 0x7e, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0xe0, 0xf0, 0x00, 0x00, 0x1f, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x41, 0xe0, 0x1f, 0xf8, 0x07, 0x82, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x03, 0xc0, 0x7f, 0xfe, 0x03, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x07, 0x81, 0xff, 0xff, 0x81, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x07, 0x03, 0xe0, 0x07, 0xc0, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x0f, 0x07, 0x80, 0x01, 0xe0, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x0e, 0x0f, 0x00, 0x00, 0xf0, 0x70, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x1e, 0x1e, 0x03, 0xc0, 0x78, 0x70, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x1c, 0x1c, 0x1f, 0xf0, 0x38, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x1c, 0x38, 0x3f, 0xfc, 0x1c, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x18, 0x38, 0x7c, 0x3e, 0x1c, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x38, 0x70, 0x70, 0x0e, 0x0e, 0x1c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x38, 0x70, 0xe0, 0x07, 0x0e, 0x1c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x38, 0x70, 0xe0, 0x07, 0x0e, 0x1c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x38, 0x70, 0xe1, 0x87, 0x0e, 0x1c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x38, 0x70, 0xe1, 0x87, 0x0e, 0x1c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x38, 0x70, 0xe1, 0x87, 0x00, 0x1c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x38, 0x70, 0xe1, 0xc3, 0x80, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x38, 0x70, 0xe1, 0xc3, 0xc0, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x38, 0x70, 0xe1, 0xc1, 0xe0, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x18, 0x70, 0xe0, 0xe0, 0xff, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x1c, 0x30, 0x70, 0xe0, 0x7f, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x1c, 0x38, 0x70, 0x70, 0x1f, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x1c, 0x38, 0x78, 0x78, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x0e, 0x1c, 0x38, 0x3e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x0c, 0x1c, 0x1c, 0x1f, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x0e, 0x1e, 0x0f, 0xff, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x0f, 0x0f, 0x03, 0xff, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x07, 0x07, 0x80, 0x7f, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x07, 0x83, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x03, 0xc1, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x01, 0xe0, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0xe0, 0x3f, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x60, 0x0f, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};



void setup(){
  // prep OLED
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.cp437(true);

  pinMode(D5, INPUT); // Connect D5 to T-Out (pin 5 on reader), T-3v to 3v

  display.setCursor(0, 0);
  display.println(F("Looking for sensor..."));
  display.display();
  
  finger.begin(57600);
  delay(5);
  if (finger.verifyPassword()) {
    display.setCursor(0, 10);
    display.println(F("Sensor Found!"));
    display.display();
  } else {
    display.setCursor(0, 10);
    display.println(F("Sensor not found!"));
    display.display();
    while (1) {
      delay(1);
    }
  }

  // prep wifi
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASSWORD);
  display.setCursor(0, 20);
  display.println(F("Connecting WiFi..."));
  display.display();
  while (WiFi.status() != WL_CONNECTED) {       // Wait till Wifi connected
    delay(1000);
  }
  display.setCursor(0, 30);
  display.println(F("IP is "));
  display.setCursor(35, 30);
  display.println(WiFi.localIP());
  display.display();
  delay(500);

  // connect to mqtt server
  display.setCursor(0, 40);
  display.println(F("Connecting MQTT..."));
  display.display();
  client.setServer(MQTT_SERVER, 1883);                // Set MQTT server and port number
  client.setCallback(callback);
  reconnect();                                        //Connect to MQTT server
  display.setCursor(0, 50);
  display.println(F("Connected"));
  display.display();
  // publish initial state
  mqttMessage["state"] = "idle";
  mqttMessage["id"] = 0;
  size_t mqttMessageSize = serializeJson(mqttMessage, mqttBuffer);
  client.publish(STATE_TOPIC, mqttBuffer, mqttMessageSize);
  delay(500);

  // done
  display.clearDisplay();
  display.setCursor(20, 30);
  display.println(F("BOOT SUCCESSFUL!"));
  display.display();
  delay(1000);
}




void loop() {
  if (!client.connected()) {
    reconnect();                //Just incase we get disconnected from MQTT server
  }

  display.clearDisplay();
  display.setCursor(50, 30);
  display.setTextSize(1);
  display.println(F("READY"));
  display.display();
    
  int fingerState = digitalRead(D5); // Read T-Out, normally HIGH (when no finger)
  
  if (fingerState == HIGH) {
    finger.CloseLED();
  } else {
    display.clearDisplay();
    display.drawBitmap(70, 0, fingerprint_bmp, 128, 64, WHITE);
    display.setCursor(0, 0);
    display.setTextSize(1);
    display.println(F("SCANNING"));
    display.display();
    uint8_t result = getFingerprintID();
    if (result == FINGERPRINT_OK) {
      mqttMessage["state"] = "matched";
      mqttMessage["id"] = lastID;
      size_t mqttMessageSize = serializeJson(mqttMessage, mqttBuffer);
      client.publish(STATE_TOPIC, mqttBuffer, mqttMessageSize);
      delay(2000);
    } else if (result == FINGERPRINT_NOTFOUND) {
      mqttMessage["state"] = "not matched";
      mqttMessage["id"] = lastID;
      size_t mqttMessageSize = serializeJson(mqttMessage, mqttBuffer);
      client.publish(STATE_TOPIC, mqttBuffer, mqttMessageSize);
      delay(2000);
    } else {
      // do nothing
    }
    mqttMessage["state"] = "idle";
    mqttMessage["id"] = 0;
    size_t mqttMessageSize = serializeJson(mqttMessage, mqttBuffer);
    client.publish(STATE_TOPIC, mqttBuffer, mqttMessageSize);
  }

  client.loop();
  delay(100);            //don't need to run this at full speed.
}

uint8_t getFingerprintID() {
  uint8_t p = finger.getImage();
  switch (p) {
    case FINGERPRINT_OK:
      display.setCursor(0, 10);
      display.println(F("IMAGED"));
      display.drawRect(83, 5, 8, 8, SSD1306_WHITE);
      display.display();
      break;
    default:
      lastID = 0;
      display.clearDisplay();
      display.setTextSize(2);
      for(i=0; i<4; i++){
        display.setCursor(15, 25);
        display.print(F("TRY AGAIN"));
        display.display();
        delay(150);
        display.clearDisplay();
        display.display();
        delay(100);
      }
      display.setCursor(15, 25);
      display.print(F("TRY AGAIN"));
      display.display();
      return p;
  }
  // image taken
  p = finger.image2Tz();
  switch (p) {
    case FINGERPRINT_OK:
      display.setCursor(0, 20);
      display.println(F("PROCESSED"));
      display.drawRect(112, 20, 8, 8, SSD1306_WHITE);
      display.display();
      break;
    default:
      lastID = 0;
      display.clearDisplay();
      display.setTextSize(2);
      int i;
      for(i=0; i<4; i++){
        display.setCursor(15, 25);
        display.print(F("TRY AGAIN"));
        display.display();
        delay(150);
        display.clearDisplay();
        display.display();
        delay(100);
      }
      display.setCursor(15, 25);
      display.print(F("TRY AGAIN"));
      display.display();
      return p;
  }
  // converted
  p = finger.fingerFastSearch();
  switch (p) {
    case FINGERPRINT_OK:
      lastID = finger.fingerID;
      lastConfidenceScore = finger.confidence;
      display.setCursor(0, 30);
      display.println(F("MATCHED"));
      display.drawRect(78, 45, 8, 8, SSD1306_WHITE);
      display.display();
      display.setCursor(0, 47);
      display.println(F("ID #"));
      display.drawRect(92, 52, 8, 8, SSD1306_WHITE);
      display.display();
      display.setCursor(32, 47);
      display.println(finger.fingerID);
      display.drawRect(112, 42, 8, 8, SSD1306_WHITE);
      display.setCursor(0, 57);
      display.println(F("CONFIDENCE "));
      display.drawRect(95, 30, 8, 8, SSD1306_WHITE);
      display.setCursor(65, 57);
      display.println(finger.confidence);
      display.drawRect(87, 18, 8, 8, SSD1306_WHITE);
      display.display();
      delay(1000);
      return p;
    case FINGERPRINT_NOTFOUND:
      lastID = 0;
      display.clearDisplay();
      display.setTextSize(2);
      for(i=0; i<4; i++){
        display.setCursor(15, 25);
        display.print(F("NO MATCH"));
        display.display();
        delay(150);
        display.clearDisplay();
        display.display();
        delay(100);
      }
      display.setCursor(15, 25);
      display.print(F("NO MATCH"));
      display.display();
      return p;
    default:
      lastID = 0;
      display.clearDisplay();
      display.setTextSize(2);
      for(i=0; i<4; i++){
        display.setCursor(15, 25);
        display.print(F("TRY AGAIN"));
        display.display();
        delay(150);
        display.clearDisplay();
        display.display();
        delay(100);
      }
      display.setCursor(15, 25);
      display.print(F("TRY AGAIN"));
      display.display();
      return p;
  }
}

uint8_t getFingerprintEnroll() {
  int p = -1;
  display.setCursor(0, 20);
  display.println(F("PLACE FINGER..."));
  display.display();
  delay(250);
  // start routine
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
      case FINGERPRINT_OK:
        display.setCursor(0, 30);
        display.println(F("IMAGED"));
        display.display();
        delay(250);
        break;
      case FINGERPRINT_NOFINGER:
        // finger hasn't been placed yet, do nothing
        break;
      case FINGERPRINT_PACKETRECIEVEERR:
        // communication error, do nothing
        break;
      case FINGERPRINT_IMAGEFAIL:
        // imaging error, do nothing
        break;
      default:
        // unknown error, do nothing
        break;
    }
  }
  //
  // got fingerprint_ok, continue
  p = finger.image2Tz(1);
  switch (p) {
    case FINGERPRINT_OK:
      display.setCursor(0, 40);
      display.print(F("CONVERTED"));
      display.display();
      delay(250);
      break;
    case FINGERPRINT_IMAGEMESS:
      display.setCursor(0, 40);
      display.print(F("IMAGE MESSY"));
      display.display();
      delay(250);
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      display.setCursor(0, 40);
      display.print(F("COMM ERROR"));
      display.display();
      delay(250);
      return p;
    case FINGERPRINT_FEATUREFAIL:
      display.setCursor(0, 40);
      display.print(F("BAD IMAGE"));
      display.display();
      delay(250);
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      display.setCursor(0, 40);
      display.print(F("BAD IMAGE"));
      display.display();
      delay(250);
      return p;
    default:
      display.setCursor(0, 40);
      display.print(F("UNKNOWN ERROR"));
      display.display();
      delay(250);
      return p;
  }
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(20, 30);
  display.print(F("REMOVE FINGER"));
  display.display();
  delay(2000);
  
  p = 0;
  while (p != FINGERPRINT_NOFINGER) {
    p = finger.getImage();
  }
  p = -1;

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(15, 30);
  display.print(F("PLACE SAME FINGER"));
  display.display();
  
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
      case FINGERPRINT_OK:
        display.clearDisplay();
        display.setCursor(0, 0);
        display.print(F("IMAGED"));
        display.display();
        delay(250);
        break;
      case FINGERPRINT_NOFINGER:
        //Serial.print(".");
        break;
      case FINGERPRINT_PACKETRECIEVEERR:
        //Serial.println("Communication error");
        break;
      case FINGERPRINT_IMAGEFAIL:
        //Serial.println("Imaging error");
        break;
      default:
        //Serial.println("Unknown error");
        break;
    }
  }

  // OK success!

  p = finger.image2Tz(2);
  switch (p) {
    case FINGERPRINT_OK:
      display.setCursor(0, 10);
      display.print(F("CONVERTED"));
      display.display();
      delay(250);
      break;
    case FINGERPRINT_IMAGEMESS:
      display.setCursor(0, 10);
      display.print(F("IMAGE MESSY"));
      display.display();
      delay(250);
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      display.setCursor(0, 10);
      display.print(F("COMM ERROR"));
      display.display();
      delay(250);
      return p;
    case FINGERPRINT_FEATUREFAIL:
      display.setCursor(0, 10);
      display.print(F("BAD IMAGE"));
      display.display();
      delay(250);
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      display.setCursor(0, 10);
      display.print(F("BAD IMAGE"));
      display.display();
      delay(250);
      return p;
    default:
      display.setCursor(0, 10);
      display.print(F("UNKNOWN ERROR"));
      display.display();
      delay(250);
      return p;
  }

  // OK converted!
  display.setCursor(0, 20);
  display.print(F("CREATING MODEL"));
  display.display();
  delay(250);
  
  p = finger.createModel();
  if (p == FINGERPRINT_OK) {
    display.setCursor(0, 30);
    display.print(F("PRINTS MATCH"));
    display.display();
    delay(250);
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    display.setCursor(0, 30);
    display.print(F("COMM ERROR"));
    display.display();
    delay(250);
    return p;
  } else if (p == FINGERPRINT_ENROLLMISMATCH) {
    display.setCursor(0, 30);
    display.print(F("PRINTS DONT MATCH"));
    display.display();
    delay(250);
    return p;
  } else {
    display.setCursor(0, 30);
    display.print(F("UNKNOWN ERROR"));
    display.display();
    delay(250);
    return p;
  }
  delay(250);
  
  p = finger.storeModel(id);
  if (p == FINGERPRINT_OK) {
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(20, 25);
    display.print(F("SUCCESS"));
    display.display();
    delay(1000);
    return true;
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(30, 30);
    display.print(F("COMM ERROR"));
    display.display();
    return p;
  } else if (p == FINGERPRINT_BADLOCATION) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(10, 30);
    display.print(F("LOCATION ERROR"));
    display.display();
    return p;
  } else if (p == FINGERPRINT_FLASHERR) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(10, 30);
    display.print(F("FLASH ERROR"));
    display.display();
    return p;
  } else {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(20, 30);
    display.print(F("UNKNOWN ERROR"));
    display.display();
    return p;
  }
  delay(250);
}


uint8_t deleteFingerprint() {
  uint8_t p = -1;

  p = finger.deleteModel(id);
  if (p == FINGERPRINT_OK) {
    display.setCursor(0, 20);
    display.print(F("DELETED"));
    display.display();
    delay(250);
    mqttMessage["state"] = "deleting";
    mqttMessage["id"] = id;
    size_t mqttMessageSize = serializeJson(mqttMessage, mqttBuffer);
    client.publish(STATE_TOPIC, mqttBuffer, mqttMessageSize);
    return true;
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    display.setCursor(0, 20);
    display.print(F("PACKET ERROR"));
    display.display();
    delay(250);
    return p;
  } else if (p == FINGERPRINT_BADLOCATION) {
    display.setCursor(0, 20);
    display.print(F("BAD LOCATION"));
    display.display();
    delay(250);
    return p;
  } else if (p == FINGERPRINT_FLASHERR) {
    display.setCursor(0, 20);
    display.print(F("FLASH ERROR"));
    display.display();
    delay(250);
    return p;
  } else {
    Serial.print("Unknown error: 0x"); Serial.println(p, HEX);
    return p;
  }
}

void reconnect() {
  while (!client.connected()) {       // Loop until connected to MQTT server
    if (client.connect(HOSTNAME, mqtt_username, mqtt_password, AVAILABILITY_TOPIC, 1, true, "offline")) {       //Connect to MQTT server
      client.publish(AVAILABILITY_TOPIC, "online");         // Once connected, publish online to the availability topic
      client.subscribe(REQUEST_TOPIC);
      client.subscribe(REPLY_TOPIC);
    } else {
      delay(5000);  // Will attempt connection again in 5 seconds
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {          //The MQTT callback which listens for incoming messages on the subscribed topics
  
  //check incoming topic
  if (strcmp(topic, REQUEST_TOPIC) == 0){
    // first convert payload to char array
    char payloadChar[length+1];
    for (int i = 0; i < length; i++) {
      payloadChar[i] = payload[i];
    }
    // second deserialize json payload and save variables
    StaticJsonDocument<200> doc;
    deserializeJson(doc, payloadChar);
    const char* requestValue = doc["request"];
    id = atoi(doc["id"]);
    //if learning...
    if (strcmp(requestValue, "learn") == 0) {
      display.clearDisplay();
      display.setTextSize(1);
      display.setCursor(0, 0);
      display.print(F("LEARNING MODE ACTIVE"));
      display.display();
      delay(250);
      if (id > 0 && id < 128) {
        display.setCursor(0, 10);
        display.print(F("ID SELECTED:"));
        display.setCursor(75, 10);
        display.print(id);
        display.display();
        delay(250);
        // MQTT
        mqttMessage["state"] = "learning";
        mqttMessage["id"] = id;
        size_t mqttMessageSize = serializeJson(mqttMessage, mqttBuffer);
        client.publish(STATE_TOPIC, mqttBuffer, mqttMessageSize);
        while (!getFingerprintEnroll());
        delay(250);
        display.clearDisplay();
        display.setTextSize(1);
        display.setCursor(10, 30);
        display.println(F("EXITING LEARN MODE"));
        display.display();
        delay(1000);
        id = 0;
      } else {
        display.clearDisplay();
        display.setTextSize(1);
        display.setCursor(0, 10);
        display.println(F("INVALID ID"));
        display.display();
        id = 0;
      }
    }
    // if deleting...
    if (strcmp(requestValue, "delete") == 0) {
      display.clearDisplay();
      display.setTextSize(1);
      display.setCursor(0, 0);
      display.print(F("DELETE MODE ACTIVE"));
      display.display();
      delay(250);
      if (id > 0 && id < 128) {
        display.setCursor(0, 10);
        display.print(F("ID SELECTED:"));
        display.setCursor(75, 10);
        display.print(id);
        display.display();
        delay(250);
        mqttMessage["state"] = "deleting";
        mqttMessage["id"] = id;
        size_t mqttMessageSize = serializeJson(mqttMessage, mqttBuffer);
        client.publish(STATE_TOPIC, mqttBuffer, mqttMessageSize);
        while (! deleteFingerprint());
        display.setTextSize(1);
        display.setCursor(0, 30);
        display.println(F("EXITING DELETE MODE"));
        display.display();
        delay(250);
        id = 0;
      }
    }
  }
  // display the reply from the server
  if (strcmp(topic, REPLY_TOPIC) == 0){
    // first convert payload to char array
    char payloadChar[length+1];
    for (int i = 0; i < length; i++) {
      payloadChar[i] = payload[i];
    }
    // second deserialize json payload and save variables
    StaticJsonDocument<200> doc;
    deserializeJson(doc, payloadChar);
    const char* line1Val = doc["line1"];
    const char* line2Val = doc["line2"];
    //then display it
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(oledXstart(line1Val,7), 20);
    display.print(line1Val);
    display.setCursor(oledXstart(line2Val,7), 40);
    display.print(line2Val);
    display.display();
    delay(1000);
  }
  // wrap up
  mqttMessage["state"] = "idle";
  mqttMessage["id"] = 0;
  size_t mqttMessageSize = serializeJson(mqttMessage, mqttBuffer);
  client.publish(STATE_TOPIC, mqttBuffer, mqttMessageSize);  
}

uint8_t oledXstart(String text, uint8_t pixels_per){
  //super janky method to approximate the start position to center the string on the OLED display
  uint8_t oled_x_pixels = 128;
  return ((oled_x_pixels/2)-((text.length()/2)*pixels_per));
}
