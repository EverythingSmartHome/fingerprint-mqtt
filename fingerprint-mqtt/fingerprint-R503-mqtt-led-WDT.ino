/*************************************
 * https://everythingsmarthome.co.uk
 * 
 * This is an MQTT connected fingerprint sensor which can 
 * used to connect to your home automation software of choice.
 * 
 * You can add and remove fingerprints using MQTT topics by
 * sending the ID through the topic.
 * 
 * Simply configure the Wifi and MQTT parameters below to get
 * started!
 *
 */

#include <ArduinoJson.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_Fingerprint.h>
#include <esp_task_wdt.h>

//12 seconds WatchDog Timer
#define WDT_TIMEOUT 12 

// Wifi Settings
#define SSID                          "Your Wifi SSID"
#define PASSWORD                      "Wifi Password"

// MQTT Settings
#define HOSTNAME                      "fingerprint-sensor"
#define MQTT_SERVER                   "MQTT Broker Server"
#define STATE_TOPIC                   "/fingerprint/mode/status"
#define MODE_LEARNING                 "/fingerprint/mode/learning"
#define MODE_READING                  "/fingerprint/mode/reading"
#define MODE_DELETE                   "/fingerprint/mode/delete"
#define AVAILABILITY_TOPIC            "/fingerprint/available"
#define mqtt_username                 "MQTT Username"
#define mqtt_password                 "MQTT Password"

#define MQTT_INTERVAL 5000            //MQTT rate limiting when no finger present, in ms

#define SENSOR_TX 4                  //GPIO Pin for RX
#define SENSOR_RX 5                  //GPIO Pin for TX

#define mySerial Serial1             //Hardware Serial
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

WiFiClient wifiClient;                // Initiate WiFi library
PubSubClient client(wifiClient);      // Initiate PubSubClient library

uint8_t id = 0;                       //Stores the current fingerprint ID
uint8_t lastID = 0;                   //Stores the last matched ID
uint8_t lastConfidenceScore = 0;      //Stores the last matched confidence score
boolean modeLearning = false;
boolean modeReading = true;
boolean modeDelete = false;
unsigned long lastMQTTmsg = 0;	      //Stores millis since last MQTT message

//Declare JSON variables
DynamicJsonDocument mqttMessage(100);
char mqttBuffer[100];

void setup()
{
  Serial.begin(57600);

  //Relay if needed
  pinMode(7,OUTPUT);
  digitalWrite(7,HIGH);

  while (!Serial);
  delay(100);
  Serial.println("\n\nWelcome to the MQTT Fingerprint Sensor program!");

  //Hardware Serial for FPS
  Serial1.begin(57600,SERIAL_8N1, SENSOR_TX, SENSOR_RX); 

  // set the data rate for the sensor serial port
  finger.begin(57600);
  delay(5);
  if (finger.verifyPassword()) {
    Serial.println("Found fingerprint sensor!");
  } else {
    Serial.println("Did not find fingerprint sensor :(");
    while (1) {
      delay(1);
    }
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASSWORD);
  Serial.print("Connecting...");

  while (WiFi.status() != WL_CONNECTED) {       // Wait till Wifi connected
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  Serial.print("Connected, IP address: ");
  Serial.println(WiFi.localIP());                     // Print IP address

  Serial.println("Configuring WDT...");
  esp_task_wdt_init(WDT_TIMEOUT, true); //enable panic so ESP32 restarts
  esp_task_wdt_add(NULL); //add current thread to WDT watch

  client.setServer(MQTT_SERVER, 1883);                // Set MQTT server and port number
  client.setCallback(callback);
}

int last = millis();

void loop() {

  if (millis() - last <= 11000) {
      //Serial.println("Resetting WDT...");
      esp_task_wdt_reset(); //timer reset
      last = millis();
  }

    
  if (!client.connected()) {
    reconnect();                //Just incase we get disconnected from MQTT server
  }
  if (modeReading == true && modeLearning == false) {
    uint8_t result = getFingerprintID();
    if (result == FINGERPRINT_OK) {
      mqttMessage["mode"] = "reading";
      mqttMessage["id"] = lastID;
      mqttMessage["state"] = "Matched";
      mqttMessage["confidence"] = lastConfidenceScore;
      size_t mqttMessageSize = serializeJson(mqttMessage, mqttBuffer);
      client.publish(STATE_TOPIC, mqttBuffer, mqttMessageSize);
      lastMQTTmsg = millis();
      delay(500);
    } else if (result == FINGERPRINT_NOTFOUND) {
      mqttMessage["mode"] = "reading";
      mqttMessage["match"] = false;
      mqttMessage["id"] = id;
      mqttMessage["state"] = "Not matched";
      size_t mqttMessageSize = serializeJson(mqttMessage, mqttBuffer);
      client.publish(STATE_TOPIC, mqttBuffer, mqttMessageSize);
      lastMQTTmsg = millis();
      delay(500);
    } else if (result == FINGERPRINT_NOFINGER) {
      if ((millis() - lastMQTTmsg) > MQTT_INTERVAL){
        mqttMessage["mode"] = "reading";
        mqttMessage["id"] = id;
        mqttMessage["state"] = "Waiting";
        size_t mqttMessageSize = serializeJson(mqttMessage, mqttBuffer);
        client.publish(STATE_TOPIC, mqttBuffer, mqttMessageSize);
        lastMQTTmsg = millis();
      }
      if ((millis() - lastMQTTmsg) < 0){
        lastMQTTmsg = millis();     //Just in case millis ever rolls over
      }
    } else {

    }
  }
  client.loop();
  delay(100);            //don't need to run this at full speed.
}

uint8_t getFingerprintID() {
  uint8_t p = finger.getImage();
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image taken");
      finger.LEDcontrol(FINGERPRINT_LED_GRADUAL_ON, 200, FINGERPRINT_LED_PURPLE);
      break;
    case FINGERPRINT_NOFINGER:
      //Serial.println("No finger detected");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      finger.LEDcontrol(FINGERPRINT_LED_FLASHING, 50, FINGERPRINT_LED_RED, 5);
      return p;
    case FINGERPRINT_IMAGEFAIL:
      Serial.println("Imaging error");
      finger.LEDcontrol(FINGERPRINT_LED_FLASHING, 50, FINGERPRINT_LED_RED, 10);
      return p;
    default:
      Serial.println("Unknown error");
      finger.LEDcontrol(FINGERPRINT_LED_FLASHING, 50, FINGERPRINT_LED_RED, 15);
      return p;
  }

  // OK success!

  p = finger.image2Tz();
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Image too messy");
      finger.LEDcontrol(FINGERPRINT_LED_FLASHING, 18, FINGERPRINT_LED_PURPLE, 5);
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      finger.LEDcontrol(FINGERPRINT_LED_FLASHING, 50, FINGERPRINT_LED_RED, 5);
      return p;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");
      finger.LEDcontrol(FINGERPRINT_LED_FLASHING, 50, FINGERPRINT_LED_PURPLE, 10);
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Could not find fingerprint features");
      finger.LEDcontrol(FINGERPRINT_LED_FLASHING, 50, FINGERPRINT_LED_PURPLE, 15);
      return p;
    default:
      Serial.println("Unknown error");
      finger.LEDcontrol(FINGERPRINT_LED_FLASHING, 50, FINGERPRINT_LED_RED, 15);
      return p;
  }

  // OK converted!
  p = finger.fingerSearch();
    else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    finger.LEDcontrol(FINGERPRINT_LED_FLASHING, 50, FINGERPRINT_LED_RED, 5);
    return p;
  } else if (p == FINGERPRINT_NOTFOUND) {
    Serial.println("Did not find a match");
    finger.LEDcontrol(FINGERPRINT_LED_FLASHING, 24, FINGERPRINT_LED_RED, 4);
    return p;
  } else {
    Serial.println("Unknown error");
    finger.LEDcontrol(FINGERPRINT_LED_FLASHING, 50, FINGERPRINT_LED_RED, 15);
    return p;
  }
  
  // found a match!
  Serial.print("Found ID #"); Serial.print(finger.fingerID);
  Serial.print(" with confidence of "); Serial.println(finger.confidence);
  finger.LEDcontrol(FINGERPRINT_LED_FLASHING, 18, FINGERPRINT_LED_BLUE, 5);

  digitalWrite(7,LOW);
  delay(500);
  digitalWrite(7,HIGH);
  
  return p;
}

uint8_t getFingerprintEnroll() {
  int p = -1;
  mqttMessage["mode"] = "learning";
  mqttMessage["id"] = id;
  mqttMessage["state"] = "Place finger..";
  size_t mqttMessageSize = serializeJson(mqttMessage, mqttBuffer);
  client.publish(STATE_TOPIC, mqttBuffer, mqttMessageSize);
  Serial.print("Waiting for valid finger to enroll as #"); Serial.println(id);
  finger.LEDcontrol(FINGERPRINT_LED_BREATHING, 100, FINGERPRINT_LED_PURPLE);
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
      case FINGERPRINT_OK:
        Serial.println("Image taken");
        break;
      case FINGERPRINT_NOFINGER:
        Serial.print(".");
        break;
      case FINGERPRINT_PACKETRECIEVEERR:
        Serial.println("Communication error");
        finger.LEDcontrol(FINGERPRINT_LED_FLASHING, 50, FINGERPRINT_LED_RED, 5);
        break;
      case FINGERPRINT_IMAGEFAIL:
        Serial.println("Imaging error");
        finger.LEDcontrol(FINGERPRINT_LED_FLASHING, 50, FINGERPRINT_LED_RED, 10);
        break;
      default:
        Serial.println("Unknown error");
        finger.LEDcontrol(FINGERPRINT_LED_FLASHING, 50, FINGERPRINT_LED_RED, 15);
        break;
    }
  }

  // OK success!

  p = finger.image2Tz(1);
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Image too messy");
      finger.LEDcontrol(FINGERPRINT_LED_FLASHING, 18, FINGERPRINT_LED_PURPLE, 5);
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      finger.LEDcontrol(FINGERPRINT_LED_FLASHING, 50, FINGERPRINT_LED_RED, 5);
      return p;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");
      finger.LEDcontrol(FINGERPRINT_LED_FLASHING, 50, FINGERPRINT_LED_PURPLE, 10);
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Could not find fingerprint features");
      finger.LEDcontrol(FINGERPRINT_LED_FLASHING, 50, FINGERPRINT_LED_PURPLE, 15);
      return p;
    default:
      Serial.println("Unknown error");
      finger.LEDcontrol(FINGERPRINT_LED_FLASHING, 50, FINGERPRINT_LED_RED, 15);
      return p;
  }

  mqttMessage["mode"] = "learning";
  mqttMessage["id"] = id;
  mqttMessage["state"] = "Remove finger..";
  mqttMessageSize = serializeJson(mqttMessage, mqttBuffer);
  client.publish(STATE_TOPIC, mqttBuffer, mqttMessageSize);
  Serial.println("Remove finger");
  delay(2000);
  
  finger.LEDcontrol(FINGERPRINT_LED_BREATHING, 100, FINGERPRINT_LED_BLUE);
  
  p = 0;
  while (p != FINGERPRINT_NOFINGER) {
    p = finger.getImage();
  }
  Serial.print("ID "); Serial.println(id);
  p = -1;
  mqttMessage["mode"] = "learning";
  mqttMessage["id"] = id;
  mqttMessage["state"] = "Place same finger..";
  mqttMessageSize = serializeJson(mqttMessage, mqttBuffer);
  client.publish(STATE_TOPIC, mqttBuffer, mqttMessageSize);
  Serial.println("Place same finger again");
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
      case FINGERPRINT_OK:
        Serial.println("Image taken");
        break;
      case FINGERPRINT_NOFINGER:
        Serial.print(".");
        break;
      case FINGERPRINT_PACKETRECIEVEERR:
        Serial.println("Communication error");
        finger.LEDcontrol(FINGERPRINT_LED_FLASHING, 50, FINGERPRINT_LED_RED, 5);
        break;
      case FINGERPRINT_IMAGEFAIL:
        Serial.println("Imaging error");
        finger.LEDcontrol(FINGERPRINT_LED_FLASHING, 50, FINGERPRINT_LED_RED, 10);
        break;
      default:
        Serial.println("Unknown error");
        finger.LEDcontrol(FINGERPRINT_LED_FLASHING, 50, FINGERPRINT_LED_RED, 15);
        break;
    }
  }

  // OK success!

  p = finger.image2Tz(2);
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Image too messy");
      finger.LEDcontrol(FINGERPRINT_LED_FLASHING, 18, FINGERPRINT_LED_PURPLE, 5);
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      finger.LEDcontrol(FINGERPRINT_LED_FLASHING, 50, FINGERPRINT_LED_RED, 5);
      return p;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");
      finger.LEDcontrol(FINGERPRINT_LED_FLASHING, 50, FINGERPRINT_LED_PURPLE, 10);
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Could not find fingerprint features");
      finger.LEDcontrol(FINGERPRINT_LED_FLASHING, 50, FINGERPRINT_LED_PURPLE, 15);
      return p;
    default:
      Serial.println("Unknown error");
      finger.LEDcontrol(FINGERPRINT_LED_FLASHING, 50, FINGERPRINT_LED_RED, 15);
      return p;
  }

  // OK converted!
  Serial.print("Creating model for #");  Serial.println(id);

  p = finger.createModel();
  if (p == FINGERPRINT_OK) {
    Serial.println("Prints matched!");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    finger.LEDcontrol(FINGERPRINT_LED_FLASHING, 50, FINGERPRINT_LED_RED, 5);
    return p;
  } else if (p == FINGERPRINT_ENROLLMISMATCH) {
    Serial.println("Fingerprints did not match");
    finger.LEDcontrol(FINGERPRINT_LED_FLASHING, 50, FINGERPRINT_LED_RED, 10);
    return p;
  } else {
    Serial.println("Unknown error");
    finger.LEDcontrol(FINGERPRINT_LED_FLASHING, 50, FINGERPRINT_LED_RED, 15);
    return p;
  }
  
  finger.LEDcontrol(FINGERPRINT_LED_ON, 0, FINGERPRINT_LED_BLUE);
  
  Serial.print("ID "); Serial.println(id);
  p = finger.storeModel(id);
  if (p == FINGERPRINT_OK) {
    mqttMessage["mode"] = "learning";
    mqttMessage["id"] = id;
    mqttMessage["state"] = "Success, stored!";
    mqttMessageSize = serializeJson(mqttMessage, mqttBuffer);
    client.publish(STATE_TOPIC, mqttBuffer, mqttMessageSize);
    finger.LEDcontrol(FINGERPRINT_LED_GRADUAL_OFF, 100, FINGERPRINT_LED_BLUE);
    Serial.println("Stored!");
    return true;
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    finger.LEDcontrol(FINGERPRINT_LED_FLASHING, 50, FINGERPRINT_LED_RED, 5);
    return p;
  } else if (p == FINGERPRINT_BADLOCATION) {
    Serial.println("Could not store in that location");
    finger.LEDcontrol(FINGERPRINT_LED_FLASHING, 50, FINGERPRINT_LED_RED, 10);
    return p;
  } else if (p == FINGERPRINT_FLASHERR) {
    Serial.println("Error writing to flash");
    finger.LEDcontrol(FINGERPRINT_LED_FLASHING, 50, FINGERPRINT_LED_RED, 20);
    return p;
  } else {
    Serial.println("Unknown error");
    finger.LEDcontrol(FINGERPRINT_LED_FLASHING, 50, FINGERPRINT_LED_RED, 15);
    return p;
  }
}

uint8_t deleteFingerprint() {
  uint8_t p = -1;
  finger.LEDcontrol(FINGERPRINT_LED_BREATHING, 100, FINGERPRINT_LED_RED);
  p = finger.deleteModel(id);

  if (p == FINGERPRINT_OK) {
    Serial.println("Deleted!");
    mqttMessage["mode"] = "deleting";
    mqttMessage["id"] = id;
    mqttMessage["state"] = "Deleted!";
    size_t mqttMessageSize = serializeJson(mqttMessage, mqttBuffer);
    client.publish(STATE_TOPIC, mqttBuffer, mqttMessageSize);
    finger.LEDcontrol(FINGERPRINT_LED_GRADUAL_OFF, 200, FINGERPRINT_LED_RED);
    return true;
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    finger.LEDcontrol(FINGERPRINT_LED_FLASHING, 50, FINGERPRINT_LED_RED, 5);
    return p;
  } else if (p == FINGERPRINT_BADLOCATION) {
    Serial.println("Could not delete in that location");
    finger.LEDcontrol(FINGERPRINT_LED_FLASHING, 50, FINGERPRINT_LED_RED, 10);
    return p;
  } else if (p == FINGERPRINT_FLASHERR) {
    Serial.println("Error writing to flash");
    finger.LEDcontrol(FINGERPRINT_LED_FLASHING, 50, FINGERPRINT_LED_RED, 20);
    return p;
  } else {
    Serial.print("Unknown error: 0x"); Serial.println(p, HEX);
    finger.LEDcontrol(FINGERPRINT_LED_FLASHING, 50, FINGERPRINT_LED_RED, 15);
    return p;
  }
}

void reconnect() {
  while (!client.connected()) {       // Loop until connected to MQTT server
    Serial.print("Attempting MQTT connection...");
    if (client.connect(HOSTNAME, mqtt_username, mqtt_password, AVAILABILITY_TOPIC, 1, true, "offline")) {       //Connect to MQTT server
      Serial.println("connected");
      client.publish(AVAILABILITY_TOPIC, "online");         // Once connected, publish online to the availability topic
      client.subscribe(MODE_LEARNING);       //Subscribe to Learning Mode Topic
      client.subscribe(MODE_READING);
      client.subscribe(MODE_DELETE);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);  // Will attempt connection again in 5 seconds
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {                    //The MQTT callback which listens for incoming messages on the subscribed topics
  if (strcmp(topic, MODE_LEARNING) == 0) {
    char charArray[3];
    for (int i = 0; i < length; i++) {
      //Serial.print((char)payload[i]);
      charArray[i] = payload[i];
    }
    id = atoi(charArray);
    if (id > 0 && id < 128) {
      Serial.println("Entering Learning mode");
      mqttMessage["mode"] = "learning";
      mqttMessage["id"] = id;
      size_t mqttMessageSize = serializeJson(mqttMessage, mqttBuffer);
      client.publish(STATE_TOPIC, mqttBuffer, mqttMessageSize);
      while (!getFingerprintEnroll());
      Serial.println("Exiting Learning mode");
      modeLearning = false;
      modeReading = true;
      modeDelete = false;
      id = 0;
    } else {
      Serial.println("No");
    }
    Serial.println();
  }

  if (strcmp(topic, MODE_DELETE) == 0) {
    char charArray[3];
    for (int i = 0; i < length; i++) {
      //Serial.print((char)payload[i]);
      charArray[i] = payload[i];
    }
    id = atoi(charArray);
    if (id > 0 && id < 128) {
      mqttMessage["mode"] = "deleting";
      mqttMessage["id"] = id;
      size_t mqttMessageSize = serializeJson(mqttMessage, mqttBuffer);
      client.publish(STATE_TOPIC, mqttBuffer, mqttMessageSize);
      Serial.println("Entering delete mode");
      while (! deleteFingerprint());
      Serial.println("Exiting delete mode");
      delay(2000); //Make the mqttMessage readable in HA
      modeLearning = false;
      modeReading = true;
      modeDelete = false;
      id = 0;
    }
    Serial.println();
  }
}