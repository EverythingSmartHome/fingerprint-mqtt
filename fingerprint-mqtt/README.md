# fingerprint-mqtt.ino

This is the default sketch and is the one used in the guide. You won't need to manually change anything to run this and only need to install the libraries mentioned in the root README file.

# fingerprint-mqtt-led.ino

This version is a slight variation on the default sketch but allows you to enable and disable the onboard fingerprint LED light via an MQTT topic. I have Home Assistant turn the sensor on/off when motion is detected in the same room. 

At time of writing the Adafruit fingerprint library doesn't have LED functionality so we'll need to modify the library files to add this. Luckily it's pretty easy, find the library folder and follow the below steps.

Note I've only tested this with the sensor in the guide.

# fingerprint-mqtt-led-touch.ino

Same as fingerprint-mqtt-led.ino, but in addition implements module's built-in touch sensor to only turn on LED when a finger touches the glass. Module can still be disabled (regardless of finger detection) using the MQTT topic.

**Note**: You must wire pins 4 and 5 of the FPM10A for this to work. Wire T-Out to D3 and T-3v to 3v. 


## Adafruit_Fingerprint.h

Below the initial define section add this

```
#define FINGERPRINT_OPENLED 0x50
#define FINGERPRINT_CLOSELED 0x51
```

Then declare the new methods

```
//The first three are already there, just placed here as a reference
  uint8_t getImage(void);
  uint8_t image2Tz(uint8_t slot = 1);
  uint8_t createModel(void);
  uint8_t OpenLED(void);
  uint8_t CloseLED(void);
```

## Adafruit_Fingerprint.cpp

Add the new methods:

```
uint8_t Adafruit_Fingerprint::image2Tz(uint8_t slot) { // this one is already in your cpp file, I'm just showing it here as a placemente reference
  SEND_CMD_PACKET(FINGERPRINT_IMAGE2TZ,slot);
}

// Custom function
/**************************************************************************/
/*!
    @brief   Ask the sensor to turn on the LED
    @returns <code>FINGERPRINT_OK</code> on success
    @returns anything else for operation failed
*/
/**************************************************************************/
uint8_t Adafruit_Fingerprint::OpenLED(void) {
  SEND_CMD_PACKET(FINGERPRINT_OPENLED);
}

// Custom function
/**************************************************************************/
/*!
    @brief   Ask the sensor to turn off the LED
    @returns <code>FINGERPRINT_OK</code> on success
    @returns anything else for operation failed
*/
/**************************************************************************/
uint8_t Adafruit_Fingerprint::CloseLED(void) {
  SEND_CMD_PACKET(FINGERPRINT_CLOSELED);
}
```

You'll then be able to send `on` or `off` to the `/fingerprint/enabled` topic to enable/disable the sensor. You will however need the LED on in order to take a fingerprint reading. 
