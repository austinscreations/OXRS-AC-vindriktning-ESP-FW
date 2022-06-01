/**
  Ikea vindriktning (AQS) Firmware for the Open eXtensible Rack System

  Documentation:
    To be added

  GitHub repository:
    To be added

  Copyright 2022 Austins Creations

  Based off the work done by Sören Beye
  https://github.com/Hypfer/esp8266-vindriktning-particle-sensor
*/

/*------------------------ Board Type ---------------------------------*/
//#define MCU32
//#define MCU8266
//#define MCULILY

/*----------------------- Connection Type -----------------------------*/
//#define ETHMODE
//#define WIFIMODE

/*------------------------- I2C pins ----------------------------------*/
//#define I2C_SDA   0
//#define I2C_SCL   1

// rack32   = 21  22
// LilyGO   = 33  32
// room8266 =  4   5
// D1 mini  =  4   0 // non standard pins

/*--------------------------- Macros ----------------------------------*/
#define STRINGIFY(s) STRINGIFY1(s)
#define STRINGIFY1(s) #s

/*--------------------------- Libraries -------------------------------*/
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <OXRS_MQTT.h>
#include <OXRS_API.h>
#include <MqttLogger.h>

// IKEA sensor reading tools from
// https://github.com/Hypfer/esp8266-vindriktning-particle-sensor
#include <serialCom.h>
#include <types.h>

#if defined(LED_RGBW) || defined(LED_RGB)
#include "ledPWMNeopixel.h"
#endif

/*--------------------------- Constants ----------------------------------*/
// AQS Variables - used for auto mode leds
#define LOW_PARTICLE_COUNT          13
#define HIGH_PARTICLE_COUNT         36

// Serial
#define SERIAL_BAUD_RATE            115200

// REST API
#define REST_API_PORT               80

// Supported LED modes
#define LED_MODE_AUTO               0
#define LED_MODE_MANUAL             1

// Supported LED states
#define LED_STATE_OFF               0
#define LED_STATE_ON                1

// Default fade interval (microseconds)
#define DEFAULT_FADE_INTERVAL_US    20000L;

// Update timing for IKEA sensor
#define DEFAULT_IKEA_UPDATE_MS 60000

// Default auto mode led brightness
#define DEFAULT_AUTO_BRIGHTNESS 50

/*--------------------------- Global Variables ---------------------------*/
// stack size counter (for determine used heap size on ESP8266)
char * g_stack_start;

// Fade interval used if no explicit interval defined in command payload
uint32_t g_fade_interval_us = DEFAULT_FADE_INTERVAL_US;
uint32_t g_auto_fade_interval_us = DEFAULT_FADE_INTERVAL_US;

// LED auto mode brightness
uint8_t g_auto_brightness = DEFAULT_AUTO_BRIGHTNESS;

// LED controls
uint8_t ledMode = LED_MODE_AUTO;
uint8_t ledState = LED_STATE_OFF;

/*-------------------------- Internal datatypes --------------------------*/
// led variables
uint8_t ledColour[12] = {0};
uint32_t fadeIntervalUs = DEFAULT_FADE_INTERVAL_US;
unsigned long lastFadeUs;
unsigned long lastAutoFadeUs;

//IKEA variables
uint32_t updateMs = DEFAULT_IKEA_UPDATE_MS;
uint32_t lastUpdate;
uint16_t ledPM = 0;

/*--------------------------- Instantiate Global Objects -----------------*/
// WiFi client
WiFiClient client;

// MQTT
PubSubClient mqttClient(client);
OXRS_MQTT mqtt(mqttClient);

// REST API
WiFiServer server(REST_API_PORT);
OXRS_API api(mqtt);

// Logging
MqttLogger logger(mqttClient, "log", MqttLoggerMode::MqttAndSerial);

// Data structure for the IKEA sensor
particleSensorState_t state;

//add the ability to control LEDs with custom library
#if defined(LED_RGBW) || defined(LED_RGB)
neopixelDriver pixelDriver;
#endif

/*--------------------------- JSON builders -----------------*/
uint32_t getStackSize()
{
  char stack;
  return (uint32_t)g_stack_start - (uint32_t)&stack;  
}

void getFirmwareJson(JsonVariant json)
{
  JsonObject firmware = json.createNestedObject("firmware");

  firmware["name"] = FW_NAME;
  firmware["shortName"] = FW_SHORT_NAME;
  firmware["maker"] = FW_MAKER;
  firmware["version"] = STRINGIFY(FW_VERSION);
  
  #if defined(FW_GITHUB_URL)
    firmware["githubUrl"] = FW_GITHUB_URL;
  #endif
}

void getSystemJson(JsonVariant json)
{
  JsonObject system = json.createNestedObject("system");

  system["heapUsedBytes"] = getStackSize();
  system["heapFreeBytes"] = ESP.getFreeHeap();
  system["flashChipSizeBytes"] = ESP.getFlashChipSize();

  system["sketchSpaceUsedBytes"] = ESP.getSketchSize();
  system["sketchSpaceTotalBytes"] = ESP.getFreeSketchSpace();

  FSInfo fsInfo;
  SPIFFS.info(fsInfo);  

  system["fileSystemUsedBytes"] = fsInfo.usedBytes;
  system["fileSystemTotalBytes"] = fsInfo.totalBytes;
}

void getNetworkJson(JsonVariant json)
{
  byte mac[6];
  WiFi.macAddress(mac);
  
  char mac_display[18];
  sprintf_P(mac_display, PSTR("%02X:%02X:%02X:%02X:%02X:%02X"), mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  JsonObject network = json.createNestedObject("network");

  network["mode"] = "wifi";
  network["ip"] = WiFi.localIP();
  network["mac"] = mac_display;
}

void getConfigSchemaJson(JsonVariant json)
{
  JsonObject configSchema = json.createNestedObject("configSchema");
  
  // Config schema metadata
  configSchema["$schema"] = JSON_SCHEMA_VERSION;
  configSchema["title"] = FW_SHORT_NAME;
  configSchema["type"] = "object";

  JsonObject properties = configSchema.createNestedObject("properties");

  JsonObject updateSeconds = properties.createNestedObject("ikeaSensorUpdateSeconds");
  updateSeconds["title"] = "IKEA Sensor Update Interval (seconds)";
  updateSeconds["description"] = "How often to read and report the values from the IKEA sensor  (defaults to 60 seconds, setting to 0 disables sensor reports). Must be a number between 0 and 86400 (i.e. 1 day).";
  updateSeconds["type"] = "integer";
  updateSeconds["minimum"] = 0;
  updateSeconds["maximum"] = 86400;

  #if defined(LED_RGBW) || defined(LED_RGB)
  JsonObject ledMode = properties.createNestedObject("ledMode");
  ledMode["type"] = "string";
  ledMode["description"] = "What mode led's should function in on startup (defaults to auto)";
  JsonArray ledModeEnum = ledMode.createNestedArray("enum");
  ledModeEnum.add("auto");
  ledModeEnum.add("manual");

  JsonObject autoFadeIntervalUs = properties.createNestedObject("autoFadeIntervalUs");
  autoFadeIntervalUs["type"] = "integer";
  autoFadeIntervalUs["minimum"] = 0;
  autoFadeIntervalUs["description"] = "Controls color fading in Auto mode, in microseconds (defaults to 20000us)";

  JsonObject autoBrightness = properties.createNestedObject("autoBrightness");
  autoBrightness["type"] = "integer";
  autoFadeIntervalUs["minimum"] = 0;
  autoBrightness["maximum"] = 255;
  autoBrightness["description"] = "Controls overall brightness of leds in auto mode (0-255 possible) (defaults to 50)";
  
  JsonObject fadeIntervalUs = properties.createNestedObject("fadeIntervalUs");
  fadeIntervalUs["type"] = "integer";
  fadeIntervalUs["minimum"] = 0;
  fadeIntervalUs["description"] = "Default time to fade from off -> on (and vice versa), in microseconds (defaults to 20000us)";
  #endif

}

void getCommandSchemaJson(JsonVariant json)
{
  JsonObject commandSchema = json.createNestedObject("commandSchema");
  
  // Command schema metadata
  commandSchema["$schema"] = JSON_SCHEMA_VERSION;
  commandSchema["title"] = FW_SHORT_NAME;
  commandSchema["type"] = "object";

  JsonObject properties = commandSchema.createNestedObject("properties");

  #if defined(LED_RGBW) || defined(LED_RGB)
  JsonObject LED = properties.createNestedObject("LED");
  LED["type"] = "array";
  LED["description"] = "Set the operation of Neopixels - auto will have the leds act like the one ikea had built in show green, yellow, red. Manaul gives you full control over each led along with fade speed and state of on / off";
  
  JsonObject LEDItems = LED.createNestedObject("items");
  LEDItems["type"] = "object";

  JsonObject LEDProperties = LEDItems.createNestedObject("properties");

  JsonObject mode = LEDProperties.createNestedObject("mode");
  mode["type"] = "string";
  JsonArray modeEnum = mode.createNestedArray("enum");
  modeEnum.add("auto");
  modeEnum.add("manual");

  JsonObject state = LEDProperties.createNestedObject("state");
  state["type"] = "string";
  JsonArray stateEnum = state.createNestedArray("enum");
  stateEnum.add("on");
  stateEnum.add("off");

  JsonObject pixel1 = LEDProperties.createNestedObject("pixel1");
  pixel1["type"] = "array";
  #if defined(LED_RGBW)
  pixel1["maxItems"] = 4;
  #elif defined(RGB)
  pixel1["maxItems"] = 3;
  #endif
  
  JsonObject pixel1Items = pixel1.createNestedObject("items");
  pixel1Items["type"] = "integer";
  pixel1Items["minimum"] = 0;
  pixel1Items["maximum"] = 255;

  JsonObject pixel2 = LEDProperties.createNestedObject("pixel2");
  pixel2["type"] = "array";
  #if defined(LED_RGBW)
  pixel2["maxItems"] = 4;
  #elif defined(RGB)
  pixel2["maxItems"] = 3;
  #endif
  
  JsonObject pixel2Items = pixel2.createNestedObject("items");
  pixel2Items["type"] = "integer";
  pixel2Items["minimum"] = 0;
  pixel2Items["maximum"] = 255;

  JsonObject pixel3 = LEDProperties.createNestedObject("pixel3");
  pixel3["type"] = "array";
  #if defined(LED_RGBW)
  pixel3["maxItems"] = 4;
  #elif defined(RGB)
  pixel3["maxItems"] = 3;
  #endif
  
  JsonObject pixel3Items = pixel3.createNestedObject("items");
  pixel3Items["type"] = "integer";
  pixel3Items["minimum"] = 0;
  pixel3Items["maximum"] = 255;

  JsonObject fadeIntervalUs = LEDProperties.createNestedObject("fadeIntervalUs");
  fadeIntervalUs["type"] = "integer";
  fadeIntervalUs["minimum"] = 0;

  JsonArray required = LEDItems.createNestedArray("required");
  required.add("mode");
  #endif

  JsonObject restart = properties.createNestedObject("restart");
  restart["type"] = "boolean";
}

void apiAdopt(JsonVariant json)
{
  // Build device adoption info
  getFirmwareJson(json);
  getSystemJson(json);
  getNetworkJson(json);
  getConfigSchemaJson(json);
  getCommandSchemaJson(json);
}

/*--------------------------- LED -----------------*/
void ledFade(uint8_t colour[])
{
  if ((micros() - lastFadeUs) > fadeIntervalUs)
  {
    #if defined(LED_RGBW) || defined(LED_RGB)
      #if defined(LED_RGBW)
        pixelDriver.crossfade(colour[0], colour[1], colour[2], colour[3], colour[4], colour[5], colour[6], colour[7], colour[8], colour[9], colour[10], colour[11]);
      #elif defined(LED_RGB)
        pixelDriver.crossfade(colour[0], colour[1], colour[2], colour[3], colour[4], colour[5], colour[6], colour[7], colour[8]);
      #endif
    #endif

    lastFadeUs = micros();
  }
}

void ledGreen()
{
  #if defined(LED_RGBW)
    pixelDriver.crossfade(0, g_auto_brightness, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  #elif defined(LED_RGB)
    pixelDriver.crossfade(0, g_auto_brightness, 0, 0, 0, 0, 0, 0, 0);
  #endif
}

void ledYellow()
{
  #if defined(LED_RGBW)
    pixelDriver.crossfade(0, 0, 0, 0, g_auto_brightness, g_auto_brightness, 0, 0, 0, 0, 0, 0);
  #elif defined(LED_RGB)
    pixelDriver.crossfade(0, 0, 0, g_auto_brightness, g_auto_brightness, 0, 0, 0, 0);
  #endif
}

void ledRed()
{
  #if defined(LED_RGBW)
    pixelDriver.crossfade(0, 0, 0, 0, 0, 0, 0, 0, g_auto_brightness, 0, 0, 0);
  #elif defined(LED_RGB)
    pixelDriver.crossfade(0, 0, 0, 0, 0, 0, g_auto_brightness, 0, 0);
  #endif
}

void autoPixels()
{
  if ((micros() - lastAutoFadeUs) > g_auto_fade_interval_us)
  {
    if (state.valid)
    {
      DynamicJsonDocument json(10);
      json["pm25"] = state.avgPM25;
      if (!json.isNull())
      {
        ledPM = state.avgPM25;
      }
    }
    if (ledPM < LOW_PARTICLE_COUNT)
    {
      ledGreen();
    }
    else if (ledPM > HIGH_PARTICLE_COUNT)
    {
      ledRed();
    }
    else
    {
      ledYellow();
    }
    lastAutoFadeUs = micros();
  }
}

void processPixels()
{
  #if defined(LED_RGBW) 
  uint8_t OFF[12];
  #elif defined(LED_RGB)
  uint8_t OFF[9];
  #endif

  #if defined(LED_RGBW) || defined(LED_RGB)
  memset(OFF, 0, sizeof(OFF));
  
  if (ledState == LED_STATE_OFF)
  {
    ledFade(OFF);
  }
  else if (ledState == LED_STATE_ON)
  {
    // fade
    ledFade(ledColour);
  }
  #endif
}

/*--------------------------- MQTT/API -----------------*/
void mqttConnected() 
{
  // MqttLogger doesn't copy the logging topic to an internal
  // buffer so we have to use a static array here
  static char logTopic[64];
  logger.setTopic(mqtt.getLogTopic(logTopic));

  // Publish device adoption info
  DynamicJsonDocument json(JSON_ADOPT_MAX_SIZE);
  mqtt.publishAdopt(api.getAdopt(json.as<JsonVariant>()));

  // Log the fact we are now connected
  logger.println("[AQS] mqtt connected");
  // turn first LED green to show mqtt connected and device ready
  #if defined(LED_RGBW) 
  pixelDriver.colour(0, 20, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  #elif defined(LED_RGB)
  pixelDriver.colour(0, 20, 0, 0, 0, 0, 0, 0, 0);
  #endif
}

void mqttDisconnected(int state) 
{
  // turn first LED orange for disconnected mqtt
  #if defined(LED_RGBW)
  pixelDriver.colour(15, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  #elif defined(LED_RGB)
  pixelDriver.colour(15, 5, 0, 0, 0, 0, 0, 0, 0);
  #endif
  // Log the disconnect reason
  // See https://github.com/knolleary/pubsubclient/blob/2d228f2f862a95846c65a8518c79f48dfc8f188c/src/PubSubClient.h#L44
  switch (state)
  {
    case MQTT_CONNECTION_TIMEOUT:
      logger.println(F("[AQS] mqtt connection timeout"));
      break;
    case MQTT_CONNECTION_LOST:
      logger.println(F("[AQS] mqtt connection lost"));
      break;
    case MQTT_CONNECT_FAILED:
      logger.println(F("[AQS] mqtt connect failed"));
      break;
    case MQTT_DISCONNECTED:
      logger.println(F("[AQS] mqtt disconnected"));
      break;
    case MQTT_CONNECT_BAD_PROTOCOL:
      logger.println(F("[AQS] mqtt bad protocol"));
      break;
    case MQTT_CONNECT_BAD_CLIENT_ID:
      logger.println(F("[AQS] mqtt bad client id"));
      break;
    case MQTT_CONNECT_UNAVAILABLE:
      logger.println(F("[AQS] mqtt unavailable"));
      break;
    case MQTT_CONNECT_BAD_CREDENTIALS:
      logger.println(F("[AQS] mqtt bad credentials"));
      break;      
    case MQTT_CONNECT_UNAUTHORIZED:
      logger.println(F("[AQS] mqtt unauthorised"));
      break;      
  }
}

void mqttCallback(char * topic, uint8_t * payload, unsigned int length) 
{
  // Pass this message down to our MQTT handler
  mqtt.receive(topic, payload, length);
}

void mqttConfig(JsonVariant json)
{
  if (json.containsKey("ikeaSensorUpdateSeconds"))
  {
    updateMs = json["ikeaSensorUpdateSeconds"].as<uint32_t>() * 1000L;
  }

  #if defined(LED_RGBW) || defined(LED_RGB)
  if (json.containsKey("ledMode"))
  {
    if (strcmp(json["mode"], "auto") == 0)
    {
      ledMode = LED_MODE_AUTO;
    }
    else if (strcmp(json["mode"], "manual") == 0)
    {
      ledMode = LED_MODE_MANUAL;
    }
    else 
    {
      logger.println(F("[AQS] invalid configured ledMode"));
    }
  }
  
  if (json.containsKey("autoFadeIntervalUs"))
  {
    g_auto_fade_interval_us = json["autoFadeIntervalUs"].as<uint32_t>();
  }

  if (json.containsKey("autoBrightness"))
  {
    g_auto_brightness = json["autoBrightness"].as<uint32_t>();
  }

  if (json.containsKey("fadeIntervalUs"))
  {
    g_fade_interval_us = json["fadeIntervalUs"].as<uint32_t>();
    fadeIntervalUs = g_fade_interval_us;
  }
  #endif
}

void jsonLedCommand(JsonVariant json)
{
  #if defined(LED_RGBW) || defined(LED_RGB)
  if (json.containsKey("mode"))
  {
    if (strcmp(json["mode"], "auto") == 0)
    {
      ledMode = LED_MODE_AUTO;
    }
    else if (strcmp(json["mode"], "manual") == 0)
    {
      ledMode = LED_MODE_MANUAL;
    }
    else 
    {
      logger.println(F("[AQS] invalid mode"));
    }
  }

  if (json.containsKey("state"))
  {
    if (strcmp(json["state"], "off") == 0)
    {
      ledState = LED_STATE_OFF;
    }
    else if (strcmp(json["state"], "on") == 0)
    {
      ledState = LED_STATE_ON;
    }
    else 
    {
      logger.println(F("[AQS] invalid state"));
    }
  }

    if (json.containsKey("pixel1"))
    {
      JsonArray array = json["pixel1"].as<JsonArray>();
      uint8_t colour = 0;

      for (JsonVariant v : array)
      {
        ledColour[colour++] = v.as<uint8_t>();
      }
    }

    if (json.containsKey("pixel2"))
    {
      JsonArray array = json["pixel2"].as<JsonArray>();
      #if defined(LED_RGBW)
      uint8_t colour = 4;
      #elif defined(LED_RGB)
      uint8_t colour = 3;
      #endif

      for (JsonVariant v : array)
      {
        ledColour[colour++] = v.as<uint8_t>();
      }
    }

    if (json.containsKey("pixel3"))
    {
      JsonArray array = json["pixel3"].as<JsonArray>();
      #if defined(LED_RGBW)
      uint8_t colour = 8;
      #elif defined(LED_RGB)
      uint8_t colour = 6;
      #endif

      for (JsonVariant v : array)
      {
        ledColour[colour++] = v.as<uint8_t>();
      }
    }

  if (json.containsKey("fadeIntervalUs"))
  {
    fadeIntervalUs = json["fadeIntervalUs"].as<uint32_t>();
  }
  else
  {
    fadeIntervalUs = g_fade_interval_us;
  }
  #endif
}

void mqttCommand(JsonVariant json)
{
  if (json.containsKey("LED"))
  {
    for (JsonVariant led : json["LED"].as<JsonArray>())
    {
      jsonLedCommand(led);
    }
  }

  if (json.containsKey("restart") && json["restart"].as<bool>())
  {
    ESP.restart();
  }
}

/*--------------------------- Initialisation -------------------------------*/
void initialiseSerial()
{
  Serial.begin(SERIAL_BAUD_RATE);
  delay(1000);
  
  logger.println(F("\n[AQS] starting up..."));

  DynamicJsonDocument json(128);
  getFirmwareJson(json.as<JsonVariant>());

  logger.print(F("[AQS] "));
  serializeJson(json, logger);
  logger.println();
}

void initialiseWifi(byte * mac)
{
  // Ensure we are in the correct WiFi mode
  WiFi.mode(WIFI_STA);

  // Connect using saved creds, or start captive portal if none found
  // Blocks until connected or the portal is closed
  WiFiManager wm;
  if (!wm.autoConnect("OXRS_WiFi", "superhouse"))
  {
    // If we are unable to connect then restart
    ESP.restart();
  }

  // Get WiFi base MAC address
  WiFi.macAddress(mac);

  // Format the MAC address for display
  char mac_display[18];
  sprintf_P(mac_display, PSTR("%02X:%02X:%02X:%02X:%02X:%02X"), mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  // Display MAC/IP addresses on serial
  logger.print(F("[AQS] mac address: "));
  logger.println(mac_display);
  logger.print(F("[AQS] ip address: "));
  logger.println(WiFi.localIP());
  // turn first led blue to show wifi connection
  #if defined(LED_RGBW)
  pixelDriver.colour(0, 0, 20, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  #elif defined(LED_RGB)
  pixelDriver.colour(0, 0, 20, 0, 0, 0, 0, 0, 0);
  #endif
}

void initialiseMqtt(byte * mac)
{
  // Set the default client id to the last 3 bytes of the MAC address
  char clientId[32];
  sprintf_P(clientId, PSTR("%02x%02x%02x"), mac[3], mac[4], mac[5]);  
  mqtt.setClientId(clientId);
  
  // Register our callbacks
  mqtt.onConnected(mqttConnected);
  mqtt.onDisconnected(mqttDisconnected);
  mqtt.onConfig(mqttConfig);
  mqtt.onCommand(mqttCommand);  

  // Start listening for MQTT messages
  mqttClient.setCallback(mqttCallback);  
}

void initialiseRestApi(void)
{
  // NOTE: this must be called *after* initialising MQTT since that sets
  //       the default client id, which has lower precendence than MQTT
  //       settings stored in file and loaded by the API

  // Set up the REST API
  api.begin();

  // Register our callbacks
  api.onAdopt(apiAdopt);

  server.begin();
}


/*--------------------------- Program -------------------------------*/
void setup()
{
  // Store the address of the stack at startup so we can determine
  // the stack size at runtime (see getStackSize())
  char stack;
  g_stack_start = &stack;

  // Set up LEDs
  #if defined(LED_RGBW) 
  pixelDriver.begin();
  pixelDriver.colour(20, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  #elif defined(LED_RGB)
  pixelDriver.begin();
  pixelDriver.colour(20, 0, 0, 0, 0, 0, 0, 0, 0);
  #endif

  // Set up serial
  initialiseSerial();  

  // Set up network and obtain an IP address
  byte mac[6];
  initialiseWifi(mac);

  // initialise MQTT
  initialiseMqtt(mac);

  // Set up the REST API
  initialiseRestApi();

  // Setup Ikea sensor software serial connection
  serialCom::setup();

}

void loop()
{
  // Check our MQTT broker connection is still ok
  mqtt.loop();

  // Handle any API requests
  WiFiClient client = server.available();
  api.loop(&client);

  #if defined(LED_RGBW) || defined(LED_RGB)
  if (ledMode == LED_MODE_MANUAL)
  {
    processPixels();
  }
  if (ledMode == LED_MODE_AUTO)
  {
    autoPixels();
  }
  #endif

  serialCom::handleUart(state);

  if ((millis() - lastUpdate) > updateMs)
  {
    lastUpdate = millis();
    logger.println(F("[AQS] tele update ready"));

    if (state.valid)
    {
      if (updateMs == 0)
      {
        return;
      }
      logger.println(F("[AQS] tele state valid"));
      DynamicJsonDocument json(500);
      json["pm25"] = state.avgPM25;
      if (!json.isNull())
      {
        mqtt.publishTelemetry(json.as<JsonVariant>());
        logger.println(F("[AQS] tele data sent"));
      }
    }
  }
}