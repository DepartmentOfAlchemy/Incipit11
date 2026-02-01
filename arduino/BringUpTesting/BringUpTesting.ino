#define DEBUG  // comment out to turn off debug serial output
#include "DebugMacros.h"

#define TEST_BUTTON   // use to test the button
                      // else test the neopixel

#include <tinyNeoPixel_Static.h>
#include <OneButton.h>

#define PWM_OUTPUT PIN_PA5
#define NEOPIXEL   PIN_PA7
#define TX         PIN_PB2
#define RX         PIN_PB3

#define PIN_BUTTON  PIN_PA4
#define PIN_TRIGGER PIN_PA6

#define NUMLEDS 1
byte pixels[NUMLEDS * 3];
tinyNeoPixel leds = tinyNeoPixel(NUMLEDS, NEOPIXEL, NEO_GRB, pixels);

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if(WheelPos < 85) {
    return leds.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if(WheelPos < 170) {
    WheelPos -= 85;
    return leds.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return leds.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}

byte wheelPos = 0;

OneButton button;
OneButton trigger;

enum state_t {
  STATE_AMBIENT = 0,
  STATE_BUTTON,
  STATE_TRIGGER
};

uint8_t outputStatus = LOW;
unsigned long outputPreviousMillis = 0;
enum state_t state = STATE_AMBIENT;
uint8_t button_pressed = false;
uint8_t trigger_pressed = false;
unsigned long previousMillis = 0;
const long interval = 1000; // 1 second duration for testing

void fClicked() {
  DPRINTLN("Click.");
  //button_pressed = true;
}

void fPressed() {
  DPRINTLN("Pressed.");
  button_pressed = true;
}

void fTriggerPressed() {
  DPRINTLN("Trigger pressed.");
  trigger_pressed = true;
}

void setup() {
  // put your setup code here, to run once:
  pinMode(PWM_OUTPUT, OUTPUT);
  pinModeFast(NEOPIXEL, OUTPUT);

  button.setup(PIN_BUTTON);
  button.attachClick(fClicked);
  button.attachPress(fPressed);

  trigger.setup(PIN_TRIGGER);
  trigger.attachPress(fTriggerPressed);

  SERIALPINS(TX, RX);
  SERIALBEGIN(115200);
  DELAY(1000); // wait a second for serial
  DPRINTLN("Incipit11 started up.");
  DPRINTLN("Transition to STATE_AMBIENT");
}

#ifdef TEST_BUTTON   // use to test the button

void loop() {
  // put your main code here, to run repeatedly:
  unsigned long currentMillis = millis();

  // output pulsating
  digitalWrite(PWM_OUTPUT, outputStatus);
  if (currentMillis - outputPreviousMillis >= interval) {
    outputPreviousMillis = currentMillis;
    if (outputStatus == LOW) {
      outputStatus = HIGH;
    } else {
      outputStatus = LOW;
    }
  }

  button.tick();
  trigger.tick();

  switch (state) {
    case STATE_AMBIENT:
      leds.setPixelColor(0, leds.Color(0, 2, 0)); // green
      leds.show();

      // state transition
      if (button_pressed) {
        button_pressed = false;

        previousMillis = currentMillis;
        state = STATE_BUTTON;
        DPRINTLN("Transition to STATE_BUTTON");
      } else if (trigger_pressed) {
        trigger_pressed = false;

        previousMillis = currentMillis;
        state = STATE_TRIGGER;
        DPRINTLN("Transition to STATE_TRIGGER");
      }
      break; 

    case STATE_BUTTON:
      leds.setPixelColor(0, leds.Color(0, 0, 128)); // blue
      leds.show();

      if (currentMillis - previousMillis >= interval) {
        previousMillis = currentMillis;

        // clear button/trigger status
        button_pressed = false;
        trigger_pressed = false;

        state = STATE_AMBIENT;
        DPRINTLN("Transition to STATE_AMBIENT");
      }
      break;

    case STATE_TRIGGER:
    default:
      leds.setPixelColor(0, leds.Color(128, 0, 0)); // red
      leds.show();

      if (currentMillis - previousMillis >= interval) {
        previousMillis = currentMillis;

        // clear button/trigger status
        button_pressed = false;
        trigger_pressed = false;

        state = STATE_AMBIENT;
        DPRINTLN("Transition to STATE_AMBIENT");
      }
      break;
  }
}

#else

// test the neopixel
void loop() {
  // put your main code here, to run repeatedly:
  button.tick();
  digitalWrite(PWM_OUTPUT, HIGH);
  leds.setPixelColor(0, Wheel(wheelPos));
  leds.show();                      // LED turns on
  DPRINT("WheelPos: ");
  DPRINTLN(wheelPos);
  ++wheelPos;
  if (wheelPos > 255) {
    wheelPos = 0;
  };
  delay(1000);
  digitalWrite(PWM_OUTPUT, LOW);
  leds.setPixelColor(0, Wheel(wheelPos));
  leds.show();                      // LED turns on
  DPRINT("WheelPos: ");
  DPRINTLN(wheelPos);
  ++wheelPos;
  if (wheelPos > 255) {
    wheelPos = 0;
  };
  delay(1000);
}

#endif