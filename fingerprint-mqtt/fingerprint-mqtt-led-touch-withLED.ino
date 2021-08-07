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
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_Fingerprint.h>

// Wifi Settings
#define SSID                          "Your Wifi SSID"
#define PASSWORD                      "Wifi Password"

// MQTT Settings
#define HOSTNAME                      "fingerprint-sensor"
#define MQTT_SERVER                   "MQTT Broker Server"
#define SENSOR_ENABLED_TOPIC          "/fingerprint/enabled"
#define STATE_TOPIC                   "/fingerprint/mode/status"
#define ALARM_TOPIC                   "alarm"
#define MODE_LEARNING                 "/fingerprint/mode/learning"
#define MODE_READING                  "/fingerprint/mode/reading"
#define MODE_DELETE                   "/fingerprint/mode/delete"
#define AVAILABILITY_TOPIC            "/fingerprint/available"
#define mqtt_username                 "MQTT Username"
#define mqtt_password                 "MQTT Password"

#define SENSOR_TX 12                  //GPIO Pin for RX
#define SENSOR_RX 14                  //GPIO Pin for TX

SoftwareSerial mySerial(SENSOR_TX, SENSOR_RX);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

WiFiClient wifiClient;                // Initiate WiFi library
PubSubClient client(wifiClient);      // Initiate PubSubClient library

uint8_t id = 0;                       //Stores the current fingerprint ID
uint8_t lastID = 0;                   //Stores the last matched ID
uint8_t lastConfidenceScore = 0;      //Stores the last matched confidence score
boolean modeLearning = false;
boolean modeReading = true;
boolean modeDelete = false;
boolean sensorOn = true;
boolean ledOn = false;

//Declare JSON variables
DynamicJsonDocument mqttMessage(100);
char mqttBuffer[100];

void setup()
{
  pinMode(D7, OUTPUT); // Connect D7 to resistor of 220 Ohm and the other side to + of the LED, - of the LED to G
  pinMode(D3, INPUT); // Connect D3 to T-Out (pin 5 on reader), T-3v to 3v
  Serial.begin(57600);
  while (!Serial);
  delay(100);
  Serial.println("\n\nWelcome to the MQTT Fingerprint Sensor program!");

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

  client.setServer(MQTT_SERVER, 1883);                // Set MQTT server and port number
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();                //Just incase we get disconnected from MQTT server
  }

  if (ledOn == true) {
    digitalWrite(D7, HIGH);
  } else if (ledOn == false) {
    digitalWrite(D7, LOW);
  }
  
  int fingerState = digitalRead(D3); // Read T-Out, normally HIGH (when no finger)
  
  if (fingerState == HIGH) {
    finger.LEDcontrol(false);
    //Serial.println("No Finger Detected");
    mqttMessage["mode"] = "reading";
    mqttMessage["id"] = id;
    mqttMessage["state"] = "Waiting";
    size_t mqttMessageSize = serializeJson(mqttMessage, mqttBuffer);
    client.publish(STATE_TOPIC, mqttBuffer, mqttMessageSize);
  } else if (sensorOn == false) {        // Disable sensor regardless of finger presence
    finger.LEDcontrol(false);
  } else {
    if (modeReading == true && modeLearning == false) {
      uint8_t result = getFingerprintID();
      if (result == FINGERPRINT_OK) {
        mqttMessage["mode"] = "reading";
        mqttMessage["id"] = lastID;
        mqttMessage["state"] = "Matched";
        mqttMessage["confidence"] = lastConfidenceScore;
        size_t mqttMessageSize = serializeJson(mqttMessage, mqttBuffer);
        client.publish(STATE_TOPIC, mqttBuffer, mqttMessageSize);
        delay(500);
      } else if (result == FINGERPRINT_NOTFOUND) {
        mqttMessage["mode"] = "reading";
        mqttMessage["match"] = false;
        mqttMessage["id"] = id;
        mqttMessage["state"] = "Not matched";
        size_t mqttMessageSize = serializeJson(mqttMessage, mqttBuffer);
        client.publish(STATE_TOPIC, mqttBuffer, mqttMessageSize);
        delay(500);
      } else if (result == FINGERPRINT_NOFINGER) {
        mqttMessage["mode"] = "reading";
        mqttMessage["id"] = id;
        mqttMessage["state"] = "Waiting";
        size_t mqttMessageSize = serializeJson(mqttMessage, mqttBuffer);
        client.publish(STATE_TOPIC, mqttBuffer, mqttMessageSize);
      }
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
      break;
    case FINGERPRINT_NOFINGER:
      //Serial.println("No finger detected");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return p;
    case FINGERPRINT_IMAGEFAIL:
      Serial.println("Imaging error");
      return p;
    default:
      Serial.println("Unknown error");
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
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return p;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Could not find fingerprint features");
      return p;
    default:
      Serial.println("Unknown error");
      return p;
  }

  // OK converted!
  p = finger.fingerFastSearch();
  if (p == FINGERPRINT_OK) {
    Serial.println("Found a print match!");
    lastID = finger.fingerID;
    lastConfidenceScore = finger.confidence;
    return p;
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    return p;
  } else if (p == FINGERPRINT_NOTFOUND) {
    Serial.println("Did not find a match");
    return p;
  } else {
    Serial.println("Unknown error");
    return p;
  }

  // found a match!
  Serial.print("Found ID #"); Serial.print(finger.fingerID);
  Serial.print(" with confidence of "); Serial.println(finger.confidence);

  return finger.fingerID;
}

uint8_t getFingerprintEnroll() {
  int p = -1;
  mqttMessage["mode"] = "learning";
  mqttMessage["id"] = id;
  mqttMessage["state"] = "Place finger..";
  size_t mqttMessageSize = serializeJson(mqttMessage, mqttBuffer);
  client.publish(STATE_TOPIC, mqttBuffer, mqttMessageSize);
  Serial.print("Waiting for valid finger to enroll as #"); Serial.println(id);
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
        break;
      case FINGERPRINT_IMAGEFAIL:
        Serial.println("Imaging error");
        break;
      default:
        Serial.println("Unknown error");
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
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return p;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Could not find fingerprint features");
      return p;
    default:
      Serial.println("Unknown error");
      return p;
  }

  mqttMessage["mode"] = "learning";
  mqttMessage["id"] = id;
  mqttMessage["state"] = "Remove finger..";
  mqttMessageSize = serializeJson(mqttMessage, mqttBuffer);
  client.publish(STATE_TOPIC, mqttBuffer, mqttMessageSize);
  Serial.println("Remove finger");
  delay(2000);
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
        break;
      case FINGERPRINT_IMAGEFAIL:
        Serial.println("Imaging error");
        break;
      default:
        Serial.println("Unknown error");
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
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return p;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Could not find fingerprint features");
      return p;
    default:
      Serial.println("Unknown error");
      return p;
  }

  // OK converted!
  Serial.print("Creating model for #");  Serial.println(id);

  p = finger.createModel();
  if (p == FINGERPRINT_OK) {
    Serial.println("Prints matched!");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    return p;
  } else if (p == FINGERPRINT_ENROLLMISMATCH) {
    Serial.println("Fingerprints did not match");
    return p;
  } else {
    Serial.println("Unknown error");
    return p;
  }

  Serial.print("ID "); Serial.println(id);
  p = finger.storeModel(id);
  if (p == FINGERPRINT_OK) {
    mqttMessage["mode"] = "learning";
    mqttMessage["id"] = id;
    mqttMessage["state"] = "Success, stored!";
    mqttMessageSize = serializeJson(mqttMessage, mqttBuffer);
    client.publish(STATE_TOPIC, mqttBuffer, mqttMessageSize);
    Serial.println("Stored!");
    return true;
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    return p;
  } else if (p == FINGERPRINT_BADLOCATION) {
    Serial.println("Could not store in that location");
    return p;
  } else if (p == FINGERPRINT_FLASHERR) {
    Serial.println("Error writing to flash");
    return p;
  } else {
    Serial.println("Unknown error");
    return p;
  }
}

uint8_t deleteFingerprint() {
  uint8_t p = -1;

  p = finger.deleteModel(id);

  if (p == FINGERPRINT_OK) {
    Serial.println("Deleted!");
    mqttMessage["mode"] = "deleting";
    mqttMessage["id"] = id;
    mqttMessage["state"] = "Deleted!";
    size_t mqttMessageSize = serializeJson(mqttMessage, mqttBuffer);
    client.publish(STATE_TOPIC, mqttBuffer, mqttMessageSize);
    return true;
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    return p;
  } else if (p == FINGERPRINT_BADLOCATION) {
    Serial.println("Could not delete in that location");
    return p;
  } else if (p == FINGERPRINT_FLASHERR) {
    Serial.println("Error writing to flash");
    return p;
  } else {
    Serial.print("Unknown error: 0x"); Serial.println(p, HEX);
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
      client.subscribe(SENSOR_ENABLED_TOPIC);
      client.subscribe(ALARM_TOPIC);
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

  if (strcmp(topic, SENSOR_ENABLED_TOPIC) == 0) {
    String msg;
    for (int i = 0; i < length; i++) {
      msg += (char)payload[i];
    }

    if (msg == "on"){
      sensorOn = true;
      Serial.println("Turning sensor on");
    } else if (msg == "off") {
      sensorOn = false;
      Serial.println("Turning sensor off");
    }
  }
  
  if (strcmp(topic, ALARM_TOPIC) == 0) {
    String msg;
    for (int i = 0; i < length; i++) {
      msg += (char)payload[i];
    }
    Serial.println(msg);
    if (msg == "disarmed") {
      ledOn = false;
      Serial.println("Turning LED off");
    } else {
      ledOn = true;
      Serial.println("Turning LED on");
    }
  }
}
