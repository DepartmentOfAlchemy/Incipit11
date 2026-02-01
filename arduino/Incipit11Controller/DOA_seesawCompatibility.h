/*
 * This is a compatibility library implementation to support Adafruit Seesaw control.
 *
 * This functionality is based on the Adafruit Adafruit_seesawPeripheral project.
 * https://github.com/adafruit/Adafruit_seesawPeripheral/tree/main
 * 
 * Based on Adafruit_seesawPeripheral.h
*/

#ifndef _DOA_SEESAWCOMPATIBILITY_H
#define _DOA_SEESAWCOMPATIBILITY_H

#include "Adafruit_seesaw.h"
#include "DebugMacros.h"
#include <Wire.h>

#define SEESAW_HW_ID 0x88 // assigned to ATtiny1616 value

// Define in controller.
extern volatile uint32_t g_bufferedBulkGPIORead;

typedef void (*PWMCallbackFP)(uint8_t, uint16_t);

extern volatile PWMCallbackFP pwmCallbackPtr;

typedef void (*SeesawResetFP)();

extern volatile SeesawResetFP seesawResetPtr;

// Will be set to the date that the executable was compiled.
uint16_t DATE_CODE = 0;

// Define 'PRODUCT_CODE' to a 4 digit number. Adafruit uses a 'pid' of 4 digits
//assigned to each product as a unique id for the product.
#define CONFIG_VERSION                            \
  (uint32_t)(((uint32_t)PRODUCT_CODE << 16) |     \
             ((uint16_t)DATE_CODE & 0x0000FFFF))

// Define 'CONFIG_ADDR_INVERTED' if the address pin is 'active' and increments
// the address when the address pin is 'high' a.k.a. 1
// Leave undefined if the address is 'active'and increments the address when
// the address pin is 'low' a.k.a. 0
#ifdef CONFIG_ADDR_INVERTED
  #undef CONFIG_ADDR_INVERTED
  #define CONFIG_ADDR_INVERTED 1
#else
  #define CONFIG_ADDR_INVERTED 0
#endif

#ifdef CONFIG_ADDR_0_PIN
  #define CONFIG_ADDR_0 1
#else
  #define CONFIG_ADDR_0 0
  #define CONFIG_ADDR_0_PIN 0
#endif
#ifdef CONFIG_ADDR_1_PIN
  #define CONFIG_ADDR_1 1
#else
  #define CONFIG_ADDR_1 0
  #define CONFIG_ADDR_1_PIN 0
#endif
#ifdef CONFIG_ADDR_2_PIN
  #define CONFIG_ADDR_2 1
#else
  #define CONFIG_ADDR_2 0
  #define CONFIG_ADDR_2_PIN 0
#endif
#ifdef CONFIG_ADDR_3_PIN
  #define CONFIG_ADDR_3 1
#else
  #define CONFIG_ADDR_3 0
  #define CONFIG_ADDR_3_PIN 0
#endif

volatile uint8_t i2c_buffer[32];

// key event ring buffer
#define KEY_BUFFER_CAPACITY 5

keyEventRaw keyBuffer[KEY_BUFFER_CAPACITY];
uint8_t keyHead = 0;
uint8_t keyTail = 0;
uint8_t keyCount = 0;

// key pad event logic
// rising edge: press
// falling edge: clicked
keyEventRaw emptyKeyEvent = { {SEESAW_KEYPAD_EDGE_LOW, 0} };

// Add a key press to the queue (Push)
bool enqueueKeyEvent(keyEventRaw evt) {
  if (keyCount == KEY_BUFFER_CAPACITY) {
    DPRINTLN(F("Error: Queue is full"));
    return false;
  }
  keyBuffer[keyTail] = evt;
  keyTail = (keyTail + 1) % KEY_BUFFER_CAPACITY; // wrap around if reaching end of array
  keyCount++;
  return true;
}

// Remove a key press from the queue (Pop)
keyEventRaw dequeueKeyEvent() {
  if (keyCount == 0) {
    DPRINTLN(F("Error: Queue is empty"));
    return emptyKeyEvent;
  }
  keyEventRaw evt = keyBuffer[keyHead];
  keyHead = (keyHead + 1) % KEY_BUFFER_CAPACITY; // wrap around if reaching end of array
  keyCount--;
  return evt;
}

void DOA_seesawCompatibility_write32(uint32_t value) {
  Wire.write(value >> 24);
  Wire.write(value >> 16);
  Wire.write(value >> 8);
  Wire.write(value);
  return;
}

void DOA_seesawCompatibility_write8(uint8_t value) {
  Wire.write(value);
  return;
}

void DOA_seesawCompatibility_setDatecode(void);
void DOA_seesawCompatibility_reset(void);
void receiveData(int numBytes);
void requestData(void);
void DOA_seesawCompatibility_run(void);

void DOA_seesawCompatibility_setDatecode(void) {
  char buf[12];
  char *bufp = buf;
  int month = 0, day = 0, year = 2000;
  static const char month_names[] = "JanFebMarAprMayJunJulAugSepOctNovDec";

  strncpy(buf, __DATE__, 11);
  buf[11] = 0;

  bufp[3] = 0;
  month = (strstr(month_names, bufp) - month_names) / 3 + 1;

  bufp += 4;
  bufp[2] = 0;
  day = atoi(bufp);

  bufp += 3;
  year = atoi(bufp);

  DATE_CODE = day & 0x1F; // top 5 bits are day of month

  DATE_CODE <<= 4;
  DATE_CODE |= month & 0xF; // middle 4 bits are month

  DATE_CODE <<= 7;
  DATE_CODE |= (year - 2000) & 0x3F; // bottom 7 bits are year
}

bool DOA_seesawCompatibility_begin(void) {

  DOA_seesawCompatibility_reset();

  Wire.onReceive(receiveData);
  Wire.onRequest(requestData);
  return true;
}

void DOA_seesawCompatibility_reset(void) {
  DOA_seesawCompatibility_setDatecode();

  seesawResetPtr();

  DPRINTLN(F("Wire end"));
  Wire.end();

  uint8_t _i2c_addr = CONFIG_I2C_PERIPH_ADDR;

  #if CONFIG_ADDR_0
    pinMode(CONFIG_ADDR_0_PIN, INPUT_PULLUP);
    if (digitalRead(CONFIG_ADDR_0_PIN) == CONFIG_ADDR_INVERTED) {
      _i2c_addr += 1;
    }
  #endif
  #if CONFIG_ADDR_1
    pinMode(CONFIG_ADDR_1_PIN, INPUT_PULLUP);
    if (digitalRead(CONFIG_ADDR_1_PIN) == CONFIG_ADDR_INVERTED) {
      _i2c_addr += 2;
    }
  #endif
  #if CONFIG_ADDR_2
    pinMode(CONFIG_ADDR_2_PIN, INPUT_PULLUP);
    if (digitalRead(CONFIG_ADDR_2_PIN) == CONFIG_ADDR_INVERTED) {
      _i2c_addr += 4;
    }
  #endif
  #if CONFIG_ADDR_3
    pinMode(CONFIG_ADDR_3_PIN, INPUT_PULLUP);
    if (digitalRead(CONFIG_ADDR_3_PIN) == CONFIG_ADDR_INVERTED) {
      _i2c_addr += 8;
    }
  #endif

  DPRINT(F("Wire begin. Address: "));
  if (_i2c_addr < 16) {
    DPRINT(F("0"));
  }
  DPRINTLN(_i2c_addr, HEX);
  Wire.begin(_i2c_addr);
}

void DOA_seesawCompatibility_run(void) {
  // nothing done in this function yet. available for future enhancement.
}

// --- I2C support ---
void receiveData(int numBytes) {
//  DPRINT(F("Received "));
//  DPRINT(numBytes);
//  DPRINT(F(" bytes: "));
  for (uint8_t i = numBytes; i < sizeof(i2c_buffer); i++) {
    i2c_buffer[i] = 0;
  }

  // check to see if number of bytes received is more than allocated in the buffer
  if ((uint32_t)numBytes > sizeof(i2c_buffer)) {
    DPRINTLN();
    return;
  }

  // read the bytes into the buffer
  for (uint8_t i = 0; i < numBytes; i++) {
    i2c_buffer[i] = Wire.read();
  }

  uint8_t base_cmd = i2c_buffer[0];
  uint8_t module_cmd = i2c_buffer[1];
//  DPRINT(F("Base command: "));
//  DPRINT(base_cmd, HEX);
//  DPRINT(F(", module command: "));
//  DPRINTLN(module_cmd, HEX);

  if (base_cmd == SEESAW_STATUS_BASE) {
    if (module_cmd == SEESAW_STATUS_SWRST) {
      DOA_seesawCompatibility_reset();
      DPRINTLN(F("Resetting"));
    }
  } else if (base_cmd == SEESAW_TIMER_BASE) {
    if (module_cmd == SEESAW_TIMER_PWM) {
      uint8_t pin = i2c_buffer[2];
      uint16_t value = (i2c_buffer[3] << 8) + i2c_buffer[4];
      DPRINT("Set PWM. Pin: ");
      DPRINT(pin);
      DPRINT(", Value: ");
      DPRINT(value);
      DPRINT(", 8 bit value: ");
      DPRINT(value & 0xFF);
      DPRINT(", Value bits: ");
      DPRINTLN(value, BIN);
      if (pwmCallbackPtr != NULL) {
        pwmCallbackPtr(pin, value);
      }
    }
  }
}

void requestData(void) {
  uint8_t base_cmd = i2c_buffer[0];
  uint8_t module_cmd = i2c_buffer[1];

  if (base_cmd == SEESAW_STATUS_BASE) {
    if (module_cmd == SEESAW_STATUS_HW_ID) {
      Wire.write(SEESAW_HW_ID);
    }
    if (module_cmd == SEESAW_STATUS_VERSION) {
      DOA_seesawCompatibility_write32(CONFIG_VERSION | DATE_CODE);
    }
  } else if (base_cmd == SEESAW_GPIO_BASE) {
    if (module_cmd == SEESAW_GPIO_BULK) {
      // Adafruit seesaw library for Arduino doesn't appear to be able to call
      // SEESAW_GPIO_INTFLAG. So using this to act like the INTFLAG
      // DOA_seesawCompatibility_write32(g_bufferedBulkGPIORead);
      DOA_seesawCompatibility_write32(g_bufferedBulkGPIORead);
    }
  } else if (base_cmd == SEESAW_KEYPAD_BASE) {
    if (module_cmd == SEESAW_KEYPAD_COUNT) {
      DOA_seesawCompatibility_write8(keyCount);
      if (keyCount > 0) {
        DPRINT(F("Keypad count: "));
        DPRINTLN(keyCount);
      }
    } else if (module_cmd == SEESAW_KEYPAD_FIFO) {
      while (keyCount > 0) {
        DOA_seesawCompatibility_write8(dequeueKeyEvent().reg);
      }
    }
  }
}

#endif