#define DEBUG  // comment out to turn off debug serial output
#include "DebugMacros.h"

//
// Adafruit Seesaw compatibility
//
#define PRODUCT_CODE 0001 // no longer stocked at Adafruit
#define CONFIG_I2C_PERIPH_ADDR 0x49

#define FLAG_BUTTON_PRESSED (1UL << 0)
#define FLAG_BUTTON_CLICKED (1UL << 1)
#define FLAG_BUTTON_DOUBLE_CLICKED (1UL << 2)
#define FLAG_BUTTON_LONG_PRESS_STARTED (1UL << 3)
#define FLAG_TRIGGER_PRESSED (1UL << 4)
volatile uint32_t g_bufferedBulkGPIORead = 0;

//#define CONFIG_ADDR_INVERTED
#define CONFIG_ADDR_0_PIN PIN_PA1
#define CONFIG_ADDR_1_PIN PIN_PA2
#define CONFIG_ADDR_2_PIN PIN_PA3

#include "DOA_seesawCompatibility.h"
// end Adafruit Seesaw compatibility

#include "Effect.h"
#include "StateMachine.h"

#include <tinyNeoPixel_Static.h>
#include <OneButton.h>
#include <EEPROM.h>
#include <Wire.h>

#define PWM_OUTPUT PIN_PA5
#define NEOPIXEL   PIN_PA7
#define TX         PIN_PB2
#define RX         PIN_PB3

#define PIN_BUTTON  PIN_PA4
#define PIN_TRIGGER PIN_PA6

const uint32_t MILLIS_30_MINUTES = 1800000L;
const uint32_t MILLIS_10_SECONDS = 10000L;

#define NUMLEDS 1
byte pixels[NUMLEDS * 3];
tinyNeoPixel leds = tinyNeoPixel(NUMLEDS, NEOPIXEL, NEO_GRB, pixels);

const uint32_t COLOR_BLACK = leds.Color(0, 0, 0);
const uint32_t COLOR_GREEN_75 = leds.Color(0, 64, 0);
const uint32_t COLOR_YELLOW = leds.Color(231, 216, 77);
const uint32_t COLOR_RED = leds.Color(128, 0, 0);
const uint32_t COLOR_BLUE = leds.Color(0, 0, 128);
const uint32_t COLOR_PURPLE = leds.Color(187, 118, 245);

OneButton button;
OneButton trigger;

uint32_t buttonFlags = 0;
// set the local buttonFlags
#define BUTTON_FLAG_SET(X) buttonFlags |= X
#define BUTTON_FLAG(X) (buttonFlags & X)

// Defining the "keys". While there are technically 2 keys, the button or the
// trigger. There are multiple features supported in the button so they are
// represented as virtual keys.
//
// An edge rising means: pressed
// An edge falling means: clicked
// The supported events are:
//   - button edge rising: button pressed
//   - button edge falling: button clicked
//   - button double edge falling: button double clicked
//   - button long edge rising: button long press started
//   - trigger edge rising: trigger pressed 
#define KEY_NUM_BUTTON        1
#define KEY_NUM_BUTTON_DOUBLE 2
#define KEY_NUM_BUTTON_LONG   3
#define KEY_NUM_TRIGGER       4

keyEventRaw buttonPressedEvent = { {SEESAW_KEYPAD_EDGE_RISING, KEY_NUM_BUTTON} };
keyEventRaw buttonClickedEvent = { {SEESAW_KEYPAD_EDGE_FALLING, KEY_NUM_BUTTON} };
keyEventRaw buttonDoubleClickedEvent = { {SEESAW_KEYPAD_EDGE_FALLING, KEY_NUM_BUTTON_DOUBLE} };
keyEventRaw buttonLongPressStartedEvent = { {SEESAW_KEYPAD_EDGE_RISING, KEY_NUM_BUTTON_LONG} };
keyEventRaw triggerPressedEvent = { {SEESAW_KEYPAD_EDGE_RISING, KEY_NUM_TRIGGER} };

uint8_t outputStatus = LOW;
unsigned long currentMillis = 0;
unsigned long outputPreviousMillis = 0;
//uint8_t buttonPressed = false;
//uint8_t buttonClicked = false;
//uint8_t buttonDoubleClicked = false;
//uint8_t buttonLongPressStarted = false;
//uint8_t triggerPressed = false;
unsigned long previousMillis = 0;
const long interval = 1000; // 1 second duration for testing
const long stepInterval = 25;

const uint8_t recordingPrepLength = 4;
const long recordingPrepOn = 200; // milliseconds on
const long recordingPrepOff = 1000 - recordingPrepOn; // milliseconds off
uint8_t recordingPrepState = false; // on (true) or off (false)
uint8_t recordingPrepCount = 0;

void fClicked() {
  DPRINTLN("Click.");
//  buttonClicked = true;
  BUTTON_FLAG_SET(FLAG_BUTTON_CLICKED);
  enqueueKeyEvent(buttonClickedEvent);
}

void fPressed() {
  DPRINTLN("Pressed.");
//  buttonPressed = true;
  BUTTON_FLAG_SET(FLAG_BUTTON_PRESSED);
  enqueueKeyEvent(buttonPressedEvent);
}

void fTriggerPressed() {
  DPRINTLN("Trigger pressed.");
//  triggerPressed = true;
  BUTTON_FLAG_SET(FLAG_TRIGGER_PRESSED);
  enqueueKeyEvent(triggerPressedEvent);
}

void fDoubleClick() {
  DPRINTLN("Double click.");
//  buttonDoubleClicked = true;
  BUTTON_FLAG_SET(FLAG_BUTTON_DOUBLE_CLICKED);
  enqueueKeyEvent(buttonDoubleClickedEvent);
}

void fLongPressStart() {
  DPRINTLN("Long press start.");
//  buttonLongPressStarted = true;
  BUTTON_FLAG_SET(FLAG_BUTTON_LONG_PRESS_STARTED);
  enqueueKeyEvent(triggerPressedEvent);
}

void clearButtons() {
//  buttonPressed = false;
//  buttonClicked = false;
//  buttonDoubleClicked = false;
//  buttonLongPressStarted = false;
//  triggerPressed = false;
  buttonFlags = 0;
}

//
// DOA_seesawCompatibility callbacks
//
// Peripheral mode (set by seesaw controller settin an output value)

bool peripheralMode = false;

// Special peripheral effect for when in peripheral mode.
// This effect is not added to the effects array as it is not part of the
// standard set of effects.
Dimmer peripheralDimmer = Dimmer(PWM_OUTPUT);

// Called by seesaw to "overide" the built in controller logic.
// Transition to peripheral state when an external controller is setting the
// output value until soft reset.
void PWMCallback(uint8_t pin, uint16_t value) {
  DPRINT("PWMCallback Pin: ");
  DPRINT(pin);
  DPRINT(", Value: ");
  DPRINT(value);
  DPRINT(", 8 bit value: ");
  DPRINTLN(value & 0xFF);

  if (pin == 0) {
    peripheralMode = true;
    peripheralDimmer.setBrightness(value & 0xFF); // convert from 16 bit to 8 bit
  }
}

volatile PWMCallbackFP pwmCallbackPtr = PWMCallback;

// Called by seesaw when reset. Return to controller logic.
void SeesawReset() {
  DPRINTLN("Seesaw reset called.");
  peripheralMode = false;
}

volatile SeesawResetFP seesawResetPtr = SeesawReset;

// End DOA_seesawCompatibility callbacks

/*
 **********
 * Effects
 **********
*/

const uint8_t CONSTANT_0 = 0;
const uint8_t CONSTANT_20 = 1;
const uint8_t CONSTANT_40 = 2;
const uint8_t CONSTANT_60 = 3;
const uint8_t CONSTANT_80 = 4;
const uint8_t CONSTANT_100 = 5;
const uint8_t STROBE_1 = 6;
const uint8_t STROBE_3 = 7;
const uint8_t STROBE_7 = 8;
const uint8_t STROBE_12 = 9;
const uint8_t STROBE_20 = 10;
const uint8_t SPARKLE_1 = 11;
const uint8_t SPARKLE_2 = 12;
const uint8_t SPARKLE_3 = 13;
const uint8_t FLICKER_1 = 14;
const uint8_t EFFECTS_COUNT = 15; // keep this at the end

uint8_t ambientEffect = CONSTANT_0;
uint8_t triggeredEffect = CONSTANT_0;
uint32_t triggeredLengthMillis = 0;

// EEPROM Addresses
const int ADDR_AMBIENT_EFFECT = 0;
const int ADDR_TRIGGERED_EFFECT = ADDR_AMBIENT_EFFECT + sizeof(ambientEffect);
const int ADDR_TRIGGERED_LENGTH = ADDR_TRIGGERED_EFFECT + sizeof(triggeredEffect);

Effect *effects[EFFECTS_COUNT];
uint8_t currentEffect = EFFECTS_COUNT;

Dimmer dimmer0 = Dimmer(PWM_OUTPUT);
Dimmer dimmer20 = Dimmer(PWM_OUTPUT);
Dimmer dimmer40 = Dimmer(PWM_OUTPUT);
Dimmer dimmer60 = Dimmer(PWM_OUTPUT);
Dimmer dimmer80 = Dimmer(PWM_OUTPUT);
Dimmer dimmer100 = Dimmer(PWM_OUTPUT);
Dimmer strobe1 = Dimmer(PWM_OUTPUT);
Dimmer strobe3 = Dimmer(PWM_OUTPUT);
Dimmer strobe7 = Dimmer(PWM_OUTPUT);
Dimmer strobe12 = Dimmer(PWM_OUTPUT);
Dimmer strobe20 = Dimmer(PWM_OUTPUT);
Sparkle sparkle1 = Sparkle(PWM_OUTPUT);
Sparkle sparkle2 = Sparkle(PWM_OUTPUT);
Sparkle sparkle3 = Sparkle(PWM_OUTPUT);
Flicker flicker1 = Flicker(PWM_OUTPUT);

void logCurrentEffect() {
  switch (currentEffect) {
    case CONSTANT_0:
      DPRINTLN("Current effect: CONSTANT_0");
      break;

    case CONSTANT_20:
      DPRINTLN("Current effect: CONSTANT_20");
      break;

    case CONSTANT_40:
      DPRINTLN("Current effect: CONSTANT_40");
      break;

    case CONSTANT_60:
      DPRINTLN("Current effect: CONSTANT_60");
      break;

    case CONSTANT_80:
      DPRINTLN("Current effect: CONSTANT_80");
      break;

    case CONSTANT_100:
      DPRINTLN("Current effect: CONSTANT_100");
      break;

    case STROBE_1:
      DPRINTLN("Current effect: STROBE_1");
      break;

    case STROBE_3:
      DPRINTLN("Current effect: STROBE_3");
      break;

    case STROBE_7:
      DPRINTLN("Current effect: STROBE_7");
      break;

    case STROBE_12:
      DPRINTLN("Current effect: STROBE_12");
      break;

    case STROBE_20:
      DPRINTLN("Current effect: STROBE_20");
      break;

    case SPARKLE_1:
      DPRINTLN("Current effect: SPARKLE_1");
      break;

    case SPARKLE_2:
      DPRINTLN("Current effect: SPARKLE_2");
      break;

    case SPARKLE_3:
      DPRINTLN("Current effect: SPARKLE_3");
      break;

    case FLICKER_1:
      DPRINTLN("Current effect: FLICKER_1");
      break;

    default:
      DPRINT("Current effect (unknown): ");
      DPRINTLN(currentEffect);
      break;
  }
}

void intializeEffects() {
  dimmer0.setBrightness(0); // off
  effects[CONSTANT_0] = &dimmer0;

  dimmer20.setBrightness(51); // ~20%
  effects[CONSTANT_20] = &dimmer20;

  dimmer40.setBrightness(102); // ~40%
  effects[CONSTANT_40] = &dimmer40;

  dimmer60.setBrightness(153); // ~60%
  effects[CONSTANT_60] = &dimmer60;

  dimmer80.setBrightness(204); // ~80%
  effects[CONSTANT_80] = &dimmer80;

  dimmer100.setBrightness(255); // 100%
  effects[CONSTANT_100] = &dimmer100;

  strobe1.setBrightness(255); // 100%
  strobe1.setStrobe(1); // 1 Hz
  effects[STROBE_1] = &strobe1;

  strobe3.setBrightness(255); // 100%
  strobe3.setStrobe(3); // 3 Hz
  effects[STROBE_3] = &strobe3;

  strobe7.setBrightness(255); // 100%
  strobe7.setStrobe(7); // 7 Hz
  effects[STROBE_7] = &strobe7;

  strobe12.setBrightness(255); // 100%
  strobe12.setStrobe(12); // 12 Hz
  effects[STROBE_12] = &strobe12;

  strobe20.setBrightness(255); // 100%
  strobe20.setStrobe(20); // 20 Hz
  effects[STROBE_20] = &strobe20;

  sparkle1.setBrightness(255); // 100%
  sparkle1.setIntensity(1);
  effects[SPARKLE_1] = &sparkle1;

  sparkle2.setBrightness(255); // 100%
  sparkle2.setIntensity(2);
  effects[SPARKLE_2] = &sparkle2;

  sparkle3.setBrightness(255); // 100%
  sparkle3.setIntensity(3);
  effects[SPARKLE_3] = &sparkle3;

  effects[FLICKER_1] = &flicker1;
}

void setEffect(uint8_t type) {
  if (currentEffect != type) {
    // exit current effect
    if (currentEffect != EFFECTS_COUNT) {
      effects[currentEffect]->exit();
    }

    // assign current effect
    currentEffect = type;

    // enter current effect
    if (currentEffect != EFFECTS_COUNT) {
      effects[currentEffect]->enter();
    }

    logCurrentEffect();
  }
}

void nextEffect() {
  uint8_t nextEffect = currentEffect + 1;
  
  if (nextEffect >= EFFECTS_COUNT) {
    nextEffect =  CONSTANT_0;
  }

  setEffect(nextEffect);
}

/*
 *
*/

/*
 * States
*/
StateMachine stateMachine;
State startupState(&startupStateEnter, NULL, &startupStateExit);
State ambientState(&ambientStateEnter, &ambientStateUpdate, &ambientStateExit);
State triggeredState(&triggeredStateEnter, &triggeredStateUpdate, &triggeredStateExit);
State prepareRecordingState(&prepareRecordingEnter, &prepareRecordingUpdate, &prepareRecordingExit);
State recordTriggerState(&recordTriggerEnter, &recordTriggerUpdate, &recordTriggerExit);
State peripheralState(&peripheralStateEnter, &peripheralStateUpdate, &peripheralStateExit);

void startupStateEnter()
{
  DPRINTLN("Startup enter");

  // add start up processes here
  leds.setPixelColor(0, COLOR_BLACK); // off
  leds.show();

  stateMachine.goToState(&ambientState);
}

void startupStateExit()
{
  DPRINTLN("Startup exit");
}

void ambientStateEnter()
{
  DPRINTLN("Ambient enter");
  clearButtons();

  leds.setPixelColor(0, COLOR_GREEN_75); // green
  leds.show();

  setEffect(ambientEffect);
}

void ambientStateUpdate()
{
  // state transition
  if (peripheralMode) {
    clearButtons();
    stateMachine.goToState(&peripheralState);
    return;
//  } else if (buttonClicked) {
  } else if (BUTTON_FLAG(FLAG_BUTTON_CLICKED)) {
    clearButtons();

    previousMillis = currentMillis;
    DPRINTLN("Ambient going to next effect.");
    nextEffect();
    ambientEffect = currentEffect;
    EEPROM.put(ADDR_AMBIENT_EFFECT, ambientEffect);
//  } else if (buttonDoubleClicked || triggerPressed) {
  } else if (BUTTON_FLAG(FLAG_BUTTON_DOUBLE_CLICKED) || BUTTON_FLAG(FLAG_TRIGGER_PRESSED)) {
    stateMachine.goToState(&triggeredState);
    return;
//  } else if (buttonLongPressStarted) {
  } else if (BUTTON_FLAG(FLAG_BUTTON_LONG_PRESS_STARTED)) {
    clearButtons();

    DPRINTLN("Ambient going to prepareRecording");
    stateMachine.goToState(&prepareRecordingState);
    return;
  }
}

void ambientStateExit()
{
  DPRINTLN("Ambient exit");
}

void triggeredStateEnter()
{
  DPRINTLN("Triggered enter");
  clearButtons();

  previousMillis = currentMillis;

  leds.setPixelColor(0, COLOR_BLUE); // blue
  leds.show();

  setEffect(triggeredEffect);
}

void triggeredStateUpdate()
{
  if (currentMillis - previousMillis >= triggeredLengthMillis) {
    stateMachine.goToState(&ambientState);
    return;
  }
}

void triggeredStateExit()
{
  DPRINTLN("Triggered exit");
}

void prepareRecordingEnter() {
  DPRINTLN("prepareRecording enter");
  previousMillis = 0;
  recordingPrepCount = 0;
  recordingPrepState = false; // so that it will immediately turn on
}

void prepareRecordingUpdate() {
  if (recordingPrepState) {
    // on
    if (currentMillis - previousMillis >= recordingPrepOn) {
      // go to off
      leds.setPixelColor(0, COLOR_BLACK); // off
      leds.show();

      recordingPrepState = !recordingPrepState;
      previousMillis = currentMillis;
    }
  } else {
    // off
    if (currentMillis - previousMillis >= recordingPrepOff) {
      if (recordingPrepCount >= recordingPrepLength) {
        // done with recording prep. state transition
        stateMachine.goToState(&recordTriggerState);
        return;
      }

      // go to on
      leds.setPixelColor(0, COLOR_YELLOW); // yellow
      leds.show();

      recordingPrepState = !recordingPrepState;
      previousMillis = currentMillis;

      recordingPrepCount += 1;
    }
  }
}

void prepareRecordingExit() {
  DPRINTLN("prepareRecording exit");
}

void recordTriggerEnter() {
  DPRINTLN("recordTrigger enter");
  clearButtons();
  previousMillis = 0;
  triggeredLengthMillis = 0;
}

void recordTriggerUpdate() {
  if (previousMillis == 0) {
    // start record
    previousMillis = currentMillis;
    leds.setPixelColor(0, COLOR_RED); // red
    leds.show();
  } else {
//    if ((buttonClicked) || ((currentMillis - previousMillis) >= MILLIS_30_MINUTES)) {
    if (BUTTON_FLAG(FLAG_BUTTON_CLICKED) || ((currentMillis - previousMillis) >= MILLIS_30_MINUTES)) {
      // stop recording trigger
      triggeredLengthMillis = currentMillis - previousMillis;
      triggeredEffect = currentEffect;
      EEPROM.put(ADDR_TRIGGERED_EFFECT, triggeredEffect);
      DPRINT("Wrote triggered effect to eeprom: ");
      DPRINTLN(triggeredEffect);
      EEPROM.put(ADDR_TRIGGERED_LENGTH, triggeredLengthMillis);
      DPRINT("Wrote triggered length millis to eeprom: ");
      DPRINTLN(triggeredLengthMillis);

      stateMachine.goToState(&ambientState);
      return;
    }
  }
}

void recordTriggerExit() {
  DPRINTLN("recordTrigger exit");
}

void peripheralStateEnter() {
  DPRINTLN("Peripheral enter");
  clearButtons();

  leds.setPixelColor(0, COLOR_PURPLE); // purple
  leds.show();
}

void peripheralStateUpdate() {
  if (!peripheralMode) {
    stateMachine.goToState(&ambientState);
  }
}

void peripheralStateExit() {
  DPRINTLN("Peripheral exit");
}

void setup() {
  // put your setup code here, to run once:
  // put your setup code here, to run once:
  pinMode(PWM_OUTPUT, OUTPUT);
  pinModeFast(NEOPIXEL, OUTPUT);

  intializeEffects();

  button.setup(PIN_BUTTON);
  button.attachClick(fClicked);
  button.attachPress(fPressed);
  button.attachDoubleClick(fDoubleClick);
  button.attachLongPressStart(fLongPressStart);

  trigger.setup(PIN_TRIGGER);
  trigger.attachPress(fTriggerPressed);

  SERIALPINS(TX, RX);
  SERIALBEGIN(115200);
  DELAY(1000); // wait a second for serial
  DPRINTLN("Incipit11 started up.");

  // load data from EEPROM
  DPRINT("ADDR_AMBIENT_EFFECT: ");
  DPRINTLN(ADDR_AMBIENT_EFFECT);
  DPRINT("ADDR_TRIGGERED_EFFECT: ");
  DPRINTLN(ADDR_TRIGGERED_EFFECT);
  DPRINT("ADDR_TRIGGERED_LENGTH: ");
  DPRINTLN(ADDR_TRIGGERED_LENGTH);
  DPRINTLN("Getting ambient effect from eeprom");
  EEPROM.get(ADDR_AMBIENT_EFFECT, ambientEffect);
  DPRINT("Got ambient effect from eeprom: ");
  DPRINTLN(ambientEffect);
  if (ambientEffect >= EFFECTS_COUNT) {
    ambientEffect = CONSTANT_0;
  }
  DPRINTLN("Getting triggered effect from eeprom");
  EEPROM.get(ADDR_TRIGGERED_EFFECT, triggeredEffect);
  DPRINT("Got triggered effect from eeprom: ");
  DPRINTLN(triggeredEffect);
  if (triggeredEffect >= EFFECTS_COUNT) {
    triggeredEffect = CONSTANT_0;
  }
  DPRINTLN("Getting triggered length millis from eeprom: ");
  DPRINTLN(EEPROM.get(ADDR_TRIGGERED_LENGTH, triggeredLengthMillis));
  DPRINT("Got triggered length millis from eeprom: ");
  DPRINTLN(triggeredLengthMillis);
  if (triggeredLengthMillis > MILLIS_30_MINUTES) {
    triggeredLengthMillis = MILLIS_10_SECONDS;
  }

  // Adafruit seesaw peripheral compatibility support
  DPRINTLN(F("Begin seesaw compatibility."));
  DOA_seesawCompatibility_begin();

  setEffect(CONSTANT_0);
  stateMachine.goToState(&startupState);
}

uint8_t brightness = 0;
const long dimInterval = 20;
uint8_t step = 1;
int8_t direction = 1;
int16_t nextStemp = step;

void loop() {
  // put your main code here, to run repeatedly:
  currentMillis = millis();

  button.tick();
  trigger.tick();
  stateMachine.update();
  if (stateMachine.isCurrentState(&peripheralState)) {
    // special peripheral mode effect
    peripheralDimmer.update(currentMillis);
  } else {
    // standard controller effects
    effects[currentEffect]->update(currentMillis);
  }

  DOA_seesawCompatibility_run();
}
