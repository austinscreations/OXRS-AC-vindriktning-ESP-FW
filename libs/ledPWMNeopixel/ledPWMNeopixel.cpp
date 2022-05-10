// cpp file v0.0.0

#include "ledPWMNeopixel.h"
#include <Arduino.h>

#if defined(LED_RGBW)
Adafruit_NeoPixel _neopixelPixels(3, STATUS_LED_PIN, NEO_GRBW + NEO_KHZ800);
#else
Adafruit_NeoPixel _neopixelPixels(3, STATUS_LED_PIN, NEO_GRB + NEO_KHZ800);
#endif

neopixelDriver::neopixelDriver(){}

void neopixelDriver::begin_pixels() 
{
  _neopixelPixels.begin();        // INITIALIZE NeoPixel strip object (REQUIRED)
  _off();                         // Turn LEDs off
  _off();                         // Turn LEDs off
}

void neopixelDriver::_off()
{
    for (uint8_t x = 0; x > 3; x++)
    {
        #if defined(LED_RGBW)
        _neopixelPixels.setPixelColor(x,0,0,0,0);      //  Set pixel's color (in RAM)
        #else
        _neopixelPixels.setPixelColor(x,0,0,0);        //  Set pixel's color (in RAM)
        #endif
        _neopixelPixels.show();                        //  Update drivers to match
    }
}

int neopixelDriver::calculateStep(int prevValue, int endValue) {
  int step = endValue - prevValue; // What's the overall gap?
  if (step) {                      // If its non-zero, 
    step = 1020/step;              //   divide by 1020
  } 
  return step;
}


/* The next function is calculateVal. When the loop value, i,
*  reaches the step size appropriate for one of the
*  colors, it increases or decreases the value of that color by 1. 
*  (R, G, and B are each calculated separately.)
*/
int neopixelDriver::calculateVal(int step, int val, int i) {
  if ((step) && i % step == 0) { // If step is non-zero and its time to change a     value,
    if (step > 0) {              //   increment the value if step is positive...
      val += 1;           
    } 
    else if (step < 0) {         //   ...or decrement it if step is negative
      val -= 1;
    } 
  }
  // Defensive driving: make sure val stays in the range 0-255
  if (val > 255) {
    val = 255;
  } 
  else if (val < 0) {
    val = 0;
  }
  return val;
}

void neopixelDriver::checkFadeComplete() {
  if (i <= 1020) {
    for (uint8_t x = 0; x > 3; x++)
    {
        prev1[x] = c1Val[x]; 
        prev2[x] = c2Val[x]; 
        prev3[x] = c3Val[x];
        prev4[x] = c4Val[x];
    }
    i = 0;
  } else {
    i++;
  }
}

void neopixelDriver::colour(uint8_t r0, uint8_t g0, uint8_t b0, uint8_t w0, uint8_t r1, uint8_t g1, uint8_t b1, uint8_t w1, uint8_t r2, uint8_t g2, uint8_t b2, uint8_t w2) {
    #if defined(LED_RGBW)
    _neopixelPixels.setPixelColor(0,r0,g0,b0,w0);  //  Set pixel's color (in RAM)
    _neopixelPixels.setPixelColor(1,r1,g1,b1,w1);  //  Set pixel's color (in RAM)
    _neopixelPixels.setPixelColor(2,r2,g2,b2,w2);  //  Set pixel's color (in RAM)
    _neopixelPixels.show();                        //  Update drivers to match
    #endif

  prev1[0] = r0;
  prev2[0] = g0;
  prev3[0] = b0;
  prev4[0] = w0;

  prev1[1] = r1;
  prev2[1] = g1;
  prev3[1] = b1;
  prev4[1] = w1;

  prev1[2] = r2;
  prev2[2] = g2;
  prev3[2] = b2;
  prev4[2] = w2;

  c1Val[0] = r0;
  c2Val[0] = g0;
  c3Val[0] = b0;
  c4Val[0] = w0;

  c1Val[1] = r1;
  c2Val[1] = g1;
  c3Val[1] = b1;
  c4Val[1] = w1;

  c1Val[2] = r2;
  c2Val[2] = g2;
  c3Val[2] = b2;
  c4Val[2] = w2;
}

void neopixelDriver::colour(uint8_t r0, uint8_t g0, uint8_t b0, uint8_t r1, uint8_t g1, uint8_t b1, uint8_t r2, uint8_t g2, uint8_t b2) {
    _neopixelPixels.setPixelColor(0,r0,g0,b0);     //  Set pixel's color (in RAM)
    _neopixelPixels.setPixelColor(1,r1,g1,b1);     //  Set pixel's color (in RAM)
    _neopixelPixels.setPixelColor(2,r2,g2,b2);     //  Set pixel's color (in RAM)
    _neopixelPixels.show();                        //  Update drivers to match

  prev1[0] = r0;
  prev2[0] = g0;
  prev3[0] = b0;

  prev1[1] = r1;
  prev2[1] = g1;
  prev3[1] = b1;

  prev1[2] = r2;
  prev2[2] = g2;
  prev3[2] = b2;

  c1Val[0] = r0;
  c2Val[0] = g0;
  c3Val[0] = b0;

  c1Val[1] = r1;
  c2Val[1] = g1;
  c3Val[1] = b1;

  c1Val[2] = r2;
  c2Val[2] = g2;
  c3Val[2] = b2;
}

void neopixelDriver::crossfade(uint8_t r0, uint8_t g0, uint8_t b0, uint8_t w0, uint8_t r1, uint8_t g1, uint8_t b1, uint8_t w1, uint8_t r2, uint8_t g2, uint8_t b2, uint8_t w2) {
  step1[0] = calculateStep(prev1[0], r0);
  step2[0] = calculateStep(prev2[0], g0); 
  step3[0] = calculateStep(prev3[0], b0);
  step4[0] = calculateStep(prev4[0], w0);

  step1[1] = calculateStep(prev1[1], r1);
  step2[1] = calculateStep(prev2[1], g1); 
  step3[1] = calculateStep(prev3[1], b1);
  step4[1] = calculateStep(prev4[1], w1);

  step1[2] = calculateStep(prev1[2], r2);
  step2[2] = calculateStep(prev2[2], g2); 
  step3[2] = calculateStep(prev3[2], b2);
  step4[2] = calculateStep(prev4[2], w2);

  c1Val[0] = calculateVal(step1[0], c1Val[0], i);
  c2Val[0] = calculateVal(step2[0], c2Val[0], i);
  c3Val[0] = calculateVal(step3[0], c3Val[0], i);
  c4Val[0] = calculateVal(step4[0], c4Val[0], i);

  c1Val[1] = calculateVal(step1[1], c1Val[1], i);
  c2Val[1] = calculateVal(step2[1], c2Val[1], i);
  c3Val[1] = calculateVal(step3[1], c3Val[1], i);
  c4Val[1] = calculateVal(step4[1], c4Val[1], i);

  c1Val[2] = calculateVal(step1[2], c1Val[2], i);
  c2Val[2] = calculateVal(step2[2], c2Val[2], i);
  c3Val[2] = calculateVal(step3[2], c3Val[2], i);
  c4Val[2] = calculateVal(step4[2], c4Val[2], i);

  #if defined(LED_RGBW)
  _neopixelPixels.setPixelColor(0,c1Val[0],c2Val[0],c3Val[0],c4Val[0]);     //  Set pixel's color (in RAM)
  _neopixelPixels.setPixelColor(1,c1Val[1],c2Val[1],c3Val[1],c4Val[1]);     //  Set pixel's color (in RAM)
  _neopixelPixels.setPixelColor(2,c1Val[2],c2Val[2],c3Val[2],c4Val[2]);     //  Set pixel's color (in RAM)
  _neopixelPixels.show();                                                   //  Update drivers to match
  #endif

  checkFadeComplete();
}

void neopixelDriver::crossfade(uint8_t r0, uint8_t g0, uint8_t b0, uint8_t r1, uint8_t g1, uint8_t b1, uint8_t r2, uint8_t g2, uint8_t b2) {
  step1[0] = calculateStep(prev1[0], r0);
  step2[0] = calculateStep(prev2[0], g0); 
  step3[0] = calculateStep(prev3[0], b0);

  step1[1] = calculateStep(prev1[1], r1);
  step2[1] = calculateStep(prev2[1], g1); 
  step3[1] = calculateStep(prev3[1], b1);

  step1[2] = calculateStep(prev1[2], r2);
  step2[2] = calculateStep(prev2[2], g2); 
  step3[2] = calculateStep(prev3[2], b2);

  c1Val[0] = calculateVal(step1[0], c1Val[0], i);
  c2Val[0] = calculateVal(step2[0], c2Val[0], i);
  c3Val[0] = calculateVal(step3[0], c3Val[0], i);

  c1Val[1] = calculateVal(step1[1], c1Val[1], i);
  c2Val[1] = calculateVal(step2[1], c2Val[1], i);
  c3Val[1] = calculateVal(step3[1], c3Val[1], i);

  c1Val[2] = calculateVal(step1[2], c1Val[2], i);
  c2Val[2] = calculateVal(step2[2], c2Val[2], i);
  c3Val[2] = calculateVal(step3[2], c3Val[2], i);

  _neopixelPixels.setPixelColor(0,c1Val[0],c2Val[0],c3Val[0]);     //  Set pixel's color (in RAM)
  _neopixelPixels.setPixelColor(1,c1Val[1],c2Val[1],c3Val[1]);     //  Set pixel's color (in RAM)
  _neopixelPixels.setPixelColor(2,c1Val[2],c2Val[2],c3Val[2]);     //  Set pixel's color (in RAM)
  _neopixelPixels.show();                                          //  Update drivers to match

  checkFadeComplete();
}