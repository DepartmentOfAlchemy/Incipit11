#define DEBUG  // comment out to turn off debug serial output
#include "DebugMacros.h"
#include "Effects.h"
#include "StateMachine.h"

#include <tinyNeoPixel_Static.h>
#include <OneButton.h>
#include <EEPROM.h>

#define PWM_OUTPUT PIN_PA5
#define NEOPIXEL   PIN_PA7
#define TX         PIN_PB2
#define RX         PIN_PB3

#define PIN_BUTTON  PIN_PA4
#define PIN_TRIGGER PIN_PA6

const uint32_t MILLIS_30_MINUTES = 1800000L;

#define NUMLEDS 1
byte pixels[NUMLEDS * 3];
tinyNeoPixel leds = tinyNeoPixel(NUMLEDS, NEOPIXEL, NEO_GRB, pixels);

const uint32_t COLOR_BLACK = leds.Color(0, 0, 0);
const uint32_t COLOR_GREEN_50 = leds.Color(0, 128, 0);
const uint32_t COLOR_YELLOW = leds.Color(231, 216, 77);
const uint32_t COLOR_RED = leds.Color(128, 0, 0);
const uint32_t COLOR_BLUE = leds.Color(0, 0, 128);

OneButton button;
OneButton trigger;

uint8_t outputStatus = LOW;
unsigned long currentMillis = 0;
unsigned long outputPreviousMillis = 0;
uint8_t buttonPressed = false;
uint8_t buttonClicked = false;
uint8_t buttonDoubleClicked = false;
uint8_t buttonLongPressStarted = false;
uint8_t triggerPressed = false;
unsigned long previousMillis = 0;
const long interval = 1000; // 1 second duration for testing
const long stepInterval = 25;

const uint8_t recordingPrepLength = 4;
const long recordingPrepOn = 200; // milliseconds on
const long recordingPrepOff = 1000 - recordingPrepOn; // milliseconds off
uint8_t recordingPrepState = false; // on (true) or off (false)
uint8_t recordingPrepCount = 0;

# seesaw compatibility
#define CONFIG_I3C_PERIPH_ADDR  0x49

void fClicked() {
  DPRINTLN("Click.");
  buttonClicked = true;
}

void fPressed() {
  DPRINTLN("Pressed.");
  buttonPressed = true;
}

void fTriggerPressed() {
  DPRINTLN("Trigger pressed.");
  triggerPressed = true;
}

void fDoubleClick() {
  DPRINTLN("Double click.");
  buttonDoubleClicked = true;
}

void fLongPressStart() {
  DPRINTLN("Long press start.");
  buttonLongPressStarted = true;
}

void clearButtons() {
  buttonPressed = false;
  buttonClicked = false;
  buttonDoubleClicked = false;
  buttonLongPressStarted = false;
  triggerPressed = false;
}

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
const uint8_t EFFECTS_NR_ITEMS = 6;

uint8_t ambientEffect = CONSTANT_0;
uint8_t triggeredEffect = CONSTANT_0;
uint32_t triggeredLengthMillis = 0;

// EEPROM Addresses
const int ADDR_AMBIENT_EFFECT = 0;
const int ADDR_TRIGGERED_EFFECT = ADDR_AMBIENT_EFFECT + sizeof(ambientEffect);
const int ADDR_TRIGGERED_LENGTH = ADDR_TRIGGERED_EFFECT + sizeof(triggeredEffect);

Effect *effects[EFFECTS_NR_ITEMS];
uint8_t currentEffect = EFFECTS_NR_ITEMS;

Dimmer dimmer0 = Dimmer(PWM_OUTPUT);
Dimmer dimmer20 = Dimmer(PWM_OUTPUT);
Dimmer dimmer40 = Dimmer(PWM_OUTPUT);
Dimmer dimmer60 = Dimmer(PWM_OUTPUT);
Dimmer dimmer80 = Dimmer(PWM_OUTPUT);
Dimmer dimmer100 = Dimmer(PWM_OUTPUT);

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
}

void setEffect(uint8_t type) {
  if (currentEffect != type) {
    // exit current effect
    if (currentEffect != EFFECTS_NR_ITEMS) {
      effects[currentEffect]->exit();
    }

    // assign current effect
    currentEffect = type;

    // enter current effect
    if (currentEffect != EFFECTS_NR_ITEMS) {
      effects[currentEffect]->enter();
    }

    logCurrentEffect();
  }
}

void nextEffect() {
  uint8_t nextEffect = currentEffect + 1;
  
  if (nextEffect >= EFFECTS_NR_ITEMS) {
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

  leds.setPixelColor(0, COLOR_GREEN_50); // green
  leds.show();

  setEffect(ambientEffect);
}

void ambientStateUpdate()
{
  // state transition
  if (buttonClicked) {
    clearButtons();

    previousMillis = currentMillis;
    DPRINTLN("Ambient going to next effect.");
    nextEffect();
    ambientEffect = currentEffect;
    EEPROM.put(ADDR_AMBIENT_EFFECT, ambientEffect);
  } else if (buttonDoubleClicked || triggerPressed) {
    stateMachine.goToState(&triggeredState);
    return;
  } else if (buttonLongPressStarted) {
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
    if ((buttonClicked) || ((currentMillis - previousMillis) >= MILLIS_30_MINUTES)) {
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
  if (ambientEffect >= EFFECTS_NR_ITEMS) {
    ambientEffect = CONSTANT_0;
  }
  DPRINTLN("Getting triggered effect from eeprom");
  EEPROM.get(ADDR_TRIGGERED_EFFECT, triggeredEffect);
  DPRINT("Got triggered effect from eeprom: ");
  DPRINTLN(triggeredEffect);
  if (triggeredEffect >= EFFECTS_NR_ITEMS) {
    triggeredEffect = CONSTANT_0;
  }
  DPRINTLN("Getting triggered length millis from eeprom: ");
  DPRINTLN(EEPROM.get(ADDR_TRIGGERED_LENGTH, triggeredLengthMillis));
  DPRINT("Got triggered length millis from eeprom: ");
  DPRINTLN(triggeredLengthMillis);
  if (triggeredLengthMillis > MILLIS_30_MINUTES) {
    triggeredLengthMillis = MILLIS_30_MINUTES;
  }

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
  effects[currentEffect]->update(currentMillis);
}
