//#include <Wire.h>
#include "Adafruit_seesaw.h"

#define DEFAULT_I2C_ADDR 0x49

Adafruit_seesaw ss;

#define KEY_EVENTS_CAPACITY 5
keyEventRaw keyEvents[KEY_EVENTS_CAPACITY];

#define KEY_NUM_BUTTON        1
#define KEY_NUM_BUTTON_DOUBLE 2
#define KEY_NUM_BUTTON_LONG   3
#define KEY_NUM_TRIGGER       4

#define KEY_PRESSED SEESAW_KEYPAD_EDGE_RISING
#define KEY_CLICKED SEESAW_KEYPAD_EDGE_FALLING

// Set I2C bus to use: Wire, Wire1, etc.
//#define WIRE Wire

void setup() {
  // put your setup code here, to run once:
  //WIRE.begin();

  Serial.begin(115200);
  // Wait for Serial port to open
  while (!Serial) {
    delay(10);
  }
  delay(500);
  //Serial.println("I2C Scanner");
  Serial.println(F("DOA PID 1 I2C Incipit11 test!"));

  if (!ss.begin(DEFAULT_I2C_ADDR)) {
    Serial.println(F("seesaw not found!"));
    while(1) delay(10);
  }

  uint16_t pid;
  uint8_t year, mon, day;

  ss.getProdDatecode(&pid, &year, &mon, &day);
  Serial.print("seesaw found PID: ");
  Serial.print(pid);
  Serial.print(" datecode: ");
  Serial.print(2000 + year); Serial.print("/");
  Serial.print(mon); Serial.print("/");
  Serial.println(day);

  if (pid != 1) {
    Serial.println(F("Wrong seesaw PID"));
    while (1) delay(10);
  }

  Serial.println(F("seesaw started OK!"));
}

void decodeKeyEvent(keyEventRaw evt) {
  switch (evt.bit.NUM) {
    case KEY_NUM_BUTTON:
      Serial.print(F("Key: Button "));
      break;

    case KEY_NUM_BUTTON_DOUBLE:
      Serial.print(F("Key: Button Double "));
      break;

    case KEY_NUM_BUTTON_LONG:
      Serial.print(F("Key: Button Long "));
      break;

    case KEY_NUM_TRIGGER:
      Serial.print(F("Key: Trigger "));
      break;

    default:
      Serial.print(F("Key num: "));
      Serial.print(evt.bit.NUM);
      Serial.print(F(" "));
      break;
  }

  switch (evt.bit.EDGE) {
    case SEESAW_KEYPAD_EDGE_HIGH:
      Serial.println(F("high"));
      break;

    case SEESAW_KEYPAD_EDGE_LOW:
      Serial.println(F("low"));
      break;

    case SEESAW_KEYPAD_EDGE_RISING:
      Serial.println(F("pressed"));
      break;

    case SEESAW_KEYPAD_EDGE_FALLING:
      Serial.println(F("clicked"));
      break;

    default:
      Serial.print(F("unknown edge: "));
      Serial.println(evt.bit.EDGE);
      break;
  }
}

bool outputToggle = false;

const uint8_t maxLoopCount = 10;
uint8_t loopCount = 0;

void loop() {
  // put your main code here, to run repeatedly:
  /*
  byte error, address;
  int nDevices;

  Serial.println("Scanning...");

  nDevices = 0;
  for (address = 1; address < 127; address++) {
    // The i2c_scanner use the return value of
    // the Write.endTransmission to see if
    // a device did acknowledge to the address.
    WIRE.beginTransmission(address);
    error = WIRE.endTransmission();

    if (error == 0) {
      Serial.print("I2C device found at address 0x");
      if (address < 16) {
        Serial.print("0");
      }
      Serial.print(address, HEX);
      Serial.println("  !");

      nDevices++;
    } else if (error == 4) {
      Serial.print("Unknown error at address 0x");
      if (address < 16) {
        Serial.print("0");
      }
      Serial.println(address, HEX);
    }
  }
  if (nDevices == 0) {
    Serial.println("No I2C devices found\n");
  } else {
    Serial.println("done\n");
  }

  delay(5000);  // wait 5 seconds for next scan
  */

  uint8_t count = ss.getKeypadCount();
  while (count > 0) {
    Serial.print(F("Keypad count: "));
    Serial.println(count);
    uint8_t s = (count > KEY_EVENTS_CAPACITY ? KEY_EVENTS_CAPACITY : count);
    ss.readKeypad(keyEvents, s);
    for (uint8_t i = 0; i < s; i++) {
      decodeKeyEvent(keyEvents[i]);

      if ((keyEvents[i].bit.NUM == KEY_NUM_BUTTON) && (keyEvents[i].bit.EDGE == KEY_CLICKED)) {
        if (loopCount < maxLoopCount) {
          // enter peripheral mode
          if (outputToggle) {
            Serial.println("Sending PWM: Pin: 0, Value: 255");
            ss.analogWrite(0, 255);
          } else {
            Serial.println("Sending PWM: Pin: 0, Value: 50");
            ss.analogWrite(0, 50);
          }
          outputToggle = !outputToggle;
          loopCount++;
        } else {
          loopCount = 0;
          // exit peripheral mode
          Serial.println("Exiting peripheral mode");
          ss.SWReset();
        }
      }
    }
    if (count > s) {
      count = count - s;
    } else {
      count = 0;
    }
  }

  delay(50);
}
