#include <Arduino.h>
#include <FastLED.h>
#include <SoftwareSerial.h>
#include <Adafruit_Fingerprint.h>

#define LED_PIN 12
#define NUM_LEDS 35

#define RXD2 16
#define TXD2 17

CRGB leds[NUM_LEDS];

CRGB colors[256];
int hues[256];

// SoftwareSerial mySerial(34, 35); //rx, tx
#define mySerial Serial2
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

int currentFingerID = 1;

float spotPos = 0;
uint8_t fingerId = 0;

TaskHandle_t ledTask;

void ledLoop(void * parameter) {
  while(true) {
    if(fingerId > 0) {
      for(int i = 0; i < NUM_LEDS; i++) {
        float l = abs(spotPos - 1 - (float) i / NUM_LEDS);
        float m = abs(spotPos - (float) i / NUM_LEDS);
        float r = abs(spotPos + 1 - (float) i / NUM_LEDS);
        float v = max(0.0f, 0.1f - min(m, min(r, l)));
        leds[i] = CRGB((int) (colors[fingerId].r * v), (int) (colors[fingerId].g * v), (int) (colors[fingerId].b * v));
      }
    } else {
      for(int i = 0; i < NUM_LEDS; i++) {
        int offset = (int) (millis() * 0.1f);
        CRGB rgb;
        CHSV hsv(((uint8_t) i * 255 / NUM_LEDS + offset), 255, 100);
        hsv2rgb_rainbow(hsv, rgb);
        leds[i] = rgb;
      }
    }
    spotPos += 0.001;
    if(spotPos >= 1.0) {
      spotPos = 0;
    }
    FastLED.show();
  }
}

void setup() {
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
  Serial.begin(115200);
  delay(100);
  finger.begin(57600);
  delay(5);
  if (finger.verifyPassword()) {
    Serial.println("Found fingerprint sensor!");
  } else {
    Serial.println("Did not find fingerprint sensor :(");
    while (1) { delay(1); }
  }

  srand(micros());
  for(int i = 0; i < 256; i++) {
    CRGB rgb;
    int hue = rand() % 256;
    CHSV hsv(hue, 255, 255);
    hsv2rgb_rainbow(hsv, rgb);
    colors[i] = rgb;
    hues[i] = hue;
  }

  Serial.println(F("Reading sensor parameters"));
  finger.getParameters();
  Serial.print(F("Status: 0x")); Serial.println(finger.status_reg, HEX);
  Serial.print(F("Sys ID: 0x")); Serial.println(finger.system_id, HEX);
  Serial.print(F("Capacity: ")); Serial.println(finger.capacity);
  Serial.print(F("Security level: ")); Serial.println(finger.security_level);
  Serial.print(F("Device address: ")); Serial.println(finger.device_addr, HEX);
  Serial.print(F("Packet len: ")); Serial.println(finger.packet_len);
  Serial.print(F("Baud rate: ")); Serial.println(finger.baud_rate);

  finger.getTemplateCount();

  if (finger.templateCount == 0) {
    Serial.print("Sensor doesn't contain any fingerprint data. Please run the 'enroll' example.");
  }
  else {
    Serial.println("Waiting for valid finger...");
      Serial.print("Sensor contains "); Serial.print(finger.templateCount); Serial.println(" templates");
  }

  xTaskCreatePinnedToCore(
      ledLoop, /* Function to implement the task */
      "Task1", /* Name of the task */
      10000,  /* Stack size in words */
      NULL,  /* Task input parameter */
      0,  /* Priority of the task */
      &ledTask,  /* Task handle. */
      0); /* Core where the task should run */
}

uint8_t getFingerprintEnroll() {
  int id = currentFingerID + 1;
  int p = -1;
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
    case FINGERPRINT_OK:
      // Serial.println("Image taken");
      break;
    case FINGERPRINT_NOFINGER:
      // Serial.print(".");
      break;
    case FINGERPRINT_PACKETRECIEVEERR:
      // Serial.println("Communication error");
      break;
    case FINGERPRINT_IMAGEFAIL:
      // Serial.println("Imaging error");
      break;
    default:
      // Serial.println("Unknown error");
      break;
    }
  }

  // OK success!

  p = finger.image2Tz(2);
  switch (p) {
    case FINGERPRINT_OK:
      // Serial.println("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      // Serial.println("Image too messy");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      // Serial.println("Communication error");
      return p;
    case FINGERPRINT_FEATUREFAIL:
      // Serial.println("Could not find fingerprint features");
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      // Serial.println("Could not find fingerprint features");
      return p;
    default:
      // Serial.println("Unknown error");
      return p;
  }

  // OK converted!
  // Serial.print("Creating model for #");  Serial.println(id);

  p = finger.createModel();
  if (p == FINGERPRINT_OK) {
    // Serial.println("Prints matched!");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    // Serial.println("Communication error");
    return p;
  } else if (p == FINGERPRINT_ENROLLMISMATCH) {
    // Serial.println("Fingerprints did not match");
    return p;
  } else {
    // Serial.println("Unknown error");
    return p;
  }

  // Serial.print("ID "); Serial.println(id);
  p = finger.storeModel(id);
  if (p == FINGERPRINT_OK) {
    // Serial.println("Stored!");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    // Serial.println("Communication error");
    return p;
  } else if (p == FINGERPRINT_BADLOCATION) {
    // Serial.println("Could not store in that location");
    return p;
  } else if (p == FINGERPRINT_FLASHERR) {
    // Serial.println("Error writing to flash");
    return p;
  } else {
    // Serial.println("Unknown error");
    return p;
  }

  CRGB rgb;
  CHSV hsv(rand() % 256, 255, 255);
  hsv2rgb_rainbow(hsv, rgb);

  colors[currentFingerID] = rgb;

  currentFingerID++;

  return true;
}

uint8_t getFingerprintID() {
  uint8_t p = finger.getImage();
  switch (p) {
    case FINGERPRINT_OK:
      // Serial.println("Image taken");
      break;
    case FINGERPRINT_NOFINGER:
      // Serial.println("No finger detected");
      return 0;
    case FINGERPRINT_PACKETRECIEVEERR:
      // Serial.println("Communication error");
      return 0;
    case FINGERPRINT_IMAGEFAIL:
      // Serial.println("Imaging error");
      return 0;
    default:
      // Serial.println("Unknown error");
      return 0;
  }

  // OK success!

  p = finger.image2Tz();
  switch (p) {
    case FINGERPRINT_OK:
      // Serial.println("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      // Serial.println("Image too messy");
      return 0;
    case FINGERPRINT_PACKETRECIEVEERR:
      // Serial.println("Communication error");
      return 0;
    case FINGERPRINT_FEATUREFAIL:
      // Serial.println("Could not find fingerprint features");
      return 0;
    case FINGERPRINT_INVALIDIMAGE:
      // Serial.println("Could not find fingerprint features");
      return 0;
    default:
      // Serial.println("Unknown error");
      return 0;
  }

  // OK converted!
  p = finger.fingerSearch();
  if (p == FINGERPRINT_OK) {
    // Serial.println("Found a print match!");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    // Serial.println("Communication error");
    return 0;
  } else if (p == FINGERPRINT_NOTFOUND) {
    // Serial.println("Did not find a match");
    getFingerprintEnroll();
    uint8_t p = finger.getImage();
    while(p != FINGERPRINT_NOFINGER) {
      // Serial.println("waiting for finger removal");
      p = finger.getImage();
      delay(100);
    }
    return 0;
  } else {
    // Serial.println("Unknown error");
    return 0;
  }

  // found a match!
  Serial.print("Found ID #"); Serial.print(finger.fingerID);
  Serial.print(" with confidence of "); Serial.println(finger.confidence);

  return finger.fingerID;
}

void loop() {
  fingerId = getFingerprintID();
}