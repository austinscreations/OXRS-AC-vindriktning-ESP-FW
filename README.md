# OXRS-AC-vindriktning-ESP-FW

Firmware for a D1-Mini to an Ikea vindriktning (AQS)

This firmware gives OXRS compatiablity for control and updating made possible VIA the adminUI
also adds builtin WiFiManager support for handling credentials

Just connect these Wires to GND, VIN (5V) and D4 on a wemos D1 mini

if you want extra functions you can attach I2C sensors to D1 and D2 - the code uses this sensor library to automatically find and add certain sensors
https://github.com/austinscreations/OXRS-AC-I2CSensors-ESP-LIB

you can also add ws2812 (neoipxels) to pin D3 you can use RGBW or RGB variants - just use the correct bin file
