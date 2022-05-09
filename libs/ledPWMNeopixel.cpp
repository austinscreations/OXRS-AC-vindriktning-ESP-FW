// cpp file v0.0.0

#include "ledPWMNeopixel.h"

neopixelDriver::neopixelDriver(Adafruit_NeoPixel &neopixel)
{
    _neopixelPixels = &neopixel;
}