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
  neopixelDriver(Adafruit_NeoPixel &neopixel);

//   void begin_pixels(const uint8_t addr);

//   void colour(uint8_t chans, uint8_t offsets, uint8_t c1, uint8_t c2, uint8_t c3, uint8_t c4, uint8_t c5);

//   void crossfade(uint8_t chans, uint8_t offsets, uint8_t c1, uint8_t c2, uint8_t c3, uint8_t c4, uint8_t c5);
  
//   uint8_t complete[17] = {false};

// private:
  
//   // TODO: should these be extended to support more than 5 GPIO channels?
//   const int channelArray[5] = {0,2,4,6,8};
//   int ledArray[5] = {0,0,0,0,0};

//   void clear();

//   int calculateVal(int step, int val, int i);
//   int calculateStep(int prevValue, int endValue);

//   void checkFadeComplete(uint8_t chans);

//   int i = 0;
  
//   int prev1[17] = {};
//   int prev2[17] = {};
//   int prev3[17] = {};
//   int prev4[17] = {};
//   int prev5[17] = {};
  
//   int c1Val[17] = {};
//   int c2Val[17] = {}; 
//   int c3Val[17] = {};
//   int c4Val[17] = {};
//   int c5Val[17] = {};
  
//   int step1;
//   int step2; 
//   int step3;
//   int step4;
//   int step5;
};

#endif