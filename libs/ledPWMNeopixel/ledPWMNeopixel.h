// h file 0.0.0

/*!
 *
 * modified from my ledPWM library but designed to control 3 neopixels
 * https://github.com/austinscreations/ledPWM/
 * 
 */

#ifndef ledPWMNeopixel_H
#define ledPWMNeopixel_H

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

/*!
 *  @brief  Class that stores state and functions for interacting with PCA9685
 * PWM chip
 */
class neopixelDriver {
public:
  neopixelDriver();

  void begin_pixels();

  void colour(uint8_t r0, uint8_t g0, uint8_t b0, uint8_t w0, uint8_t r1, uint8_t g1, uint8_t b1, uint8_t w1, uint8_t r2, uint8_t g2, uint8_t b2, uint8_t w2); // RGBW - for 3 LEDs
  void colour(uint8_t r0, uint8_t g0, uint8_t b0, uint8_t r1, uint8_t g1, uint8_t b1, uint8_t r2, uint8_t g2, uint8_t b2);                                     // RGB  - for 3 LEDs

  void crossfade(uint8_t r0, uint8_t g0, uint8_t b0, uint8_t w0, uint8_t r1, uint8_t g1, uint8_t b1, uint8_t w1, uint8_t r2, uint8_t g2, uint8_t b2, uint8_t w2); // RGBW - for 3 LEDs
  void crossfade(uint8_t r0, uint8_t g0, uint8_t b0, uint8_t r1, uint8_t g1, uint8_t b1, uint8_t r2, uint8_t g2, uint8_t b2);                                     // RGBW - for 3 LEDs
  
private:

void _off();

  int calculateVal(int step, int val, int i);
  int calculateStep(int prevValue, int endValue);

  void checkFadeComplete();

  int i = 0;
  
  int prev1[3] = {};
  int prev2[3] = {};
  int prev3[3] = {};
  int prev4[3] = {};
  
  int c1Val[3] = {};
  int c2Val[3] = {}; 
  int c3Val[3] = {};
  int c4Val[3] = {};
  
  int step1[3] = {};
  int step2[3] = {};
  int step3[3] = {};
  int step4[3] = {};
};

#endif