#ifndef Effects_h
#define Effects_h

#include "Arduino.h"

// Gamma brightness lookup table <https://victornpb.github.io/gamma-table-generator>
// gamma = 2.20 steps = 256 range = 0-255
const uint8_t gamma_lut[256] = {
     0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   1,
     1,   1,   1,   1,   1,   1,   1,   1,   1,   2,   2,   2,   2,   2,   2,   2,
     3,   3,   3,   3,   3,   4,   4,   4,   4,   5,   5,   5,   5,   6,   6,   6,
     6,   7,   7,   7,   8,   8,   8,   9,   9,   9,  10,  10,  11,  11,  11,  12,
    12,  13,  13,  13,  14,  14,  15,  15,  16,  16,  17,  17,  18,  18,  19,  19,
    20,  20,  21,  22,  22,  23,  23,  24,  25,  25,  26,  26,  27,  28,  28,  29,
    30,  30,  31,  32,  33,  33,  34,  35,  35,  36,  37,  38,  39,  39,  40,  41,
    42,  43,  43,  44,  45,  46,  47,  48,  49,  49,  50,  51,  52,  53,  54,  55,
    56,  57,  58,  59,  60,  61,  62,  63,  64,  65,  66,  67,  68,  69,  70,  71,
    73,  74,  75,  76,  77,  78,  79,  81,  82,  83,  84,  85,  87,  88,  89,  90,
    91,  93,  94,  95,  97,  98,  99, 100, 102, 103, 105, 106, 107, 109, 110, 111,
   113, 114, 116, 117, 119, 120, 121, 123, 124, 126, 127, 129, 130, 132, 133, 135,
   137, 138, 140, 141, 143, 145, 146, 148, 149, 151, 153, 154, 156, 158, 159, 161,
   163, 165, 166, 168, 170, 172, 173, 175, 177, 179, 181, 182, 184, 186, 188, 190,
   192, 194, 196, 197, 199, 201, 203, 205, 207, 209, 211, 213, 215, 217, 219, 221,
   223, 225, 227, 229, 231, 234, 236, 238, 240, 242, 244, 246, 248, 251, 253, 255,
  };

const unsigned long MILLISECONDS_PER_SECOND = 1000;

class Effect {
public:
  Effect(uint8_t pin) : _pin(pin) {
    _brightness = 0;
  }

  uint8_t getPin() {
    return _pin;
  }

  virtual uint8_t getBrightness() {
    return _brightness;
  }

  virtual void setBrightness(uint8_t brightness) {
    _brightness = brightness;
  }

  virtual void enter() {
    // no-op
  }

  virtual void exit() {
    // no-op
  }

  virtual void update(unsigned long now = 0) = 0;

  virtual ~Effect() {}

protected:
  uint8_t _pin;
  uint8_t _brightness;
};

class Dimmer : public Effect {
public:
  Dimmer(uint8_t pin) : Effect(pin) {
    _brightness = 0;
    _strobe = 0;
    transitionPeriod = 0;
    lastTransitionTime = 0;
    nextTransition = true;

    // initialize to off
    analogWrite(_pin, _brightness);
    _lastBrightness = _brightness;
  }

  uint16_t getStrobe() {
    return _strobe;
  }

  void setStrobe(uint16_t frequency) {
    if (frequency > 1400) {
      frequency = 1400;
    }
    _strobe = frequency;

    if (frequency > 0) {
      // convert frequency into transition period
      // (period is equally divided to half off/half on)
      transitionPeriod = (MILLISECONDS_PER_SECOND / frequency) / 2;
    }
  }

  void enter() override {
    // set to turn on if strobing on next update()
    lastTransitionTime = 0;
    nextTransition = true;

    _lastBrightness = 0;
    // edge condition
    // in update, since _brightness == _lastBrightness it would never set the output off
    if (_brightness == 0) {
      analogWrite(_pin, _brightness);
    }
  }

  void update(unsigned long now = 0) override {
    if (_strobe == 0) {
      // constrant
      if (_lastBrightness != _brightness) {
        analogWrite(_pin, gamma_lut[_brightness]);
        _lastBrightness = _brightness;
      }
    } else {
      // strobing
      if (now == 0) {
        now = millis();
      }

      if (now >= (lastTransitionTime + transitionPeriod)) {
        lastTransitionTime = now;
        // transition between on/off
        if (nextTransition) {
          analogWrite(_pin, gamma_lut[_brightness]);
          _lastBrightness = _brightness;
        } else {
          analogWrite(_pin, 0);
          _lastBrightness = 0;
        }

        nextTransition = !nextTransition;
      }
    }
  }

  ~Dimmer() override {}

protected:
  uint8_t _lastBrightness;
  uint16_t _strobe;
  unsigned long transitionPeriod;
  unsigned long lastTransitionTime;
  bool nextTransition;
};

#endif