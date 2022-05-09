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
// D1 mini  =  4   0

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

#include <Adafruit_NeoPixel.h>        // For status LED

/*--------------------------- Constants ----------------------------------*/
// Serial
#define SERIAL_BAUD_RATE            115200

// REST API
#define REST_API_PORT               80

// Supported LED states
#define LED_STATE_AUTO               0
#define LED_STATE_MANUAL             1

/*--------------------------- Global Variables ---------------------------*/
// stack size counter (for determine used heap size on ESP8266)
char * g_stack_start;

// LED controls
uint8_t ledState = LED_STATE_AUTO;
uint8_t ledColour[3];

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

Adafruit_NeoPixel pixel(1, STATUS_LED_PIN, NEO_GRB + NEO_KHZ800);

/*--------------------------- JSON builders -----------------*/
uint32_t getStackSize()
{
  char stack;
  return (uint32_t)g_stack_start - (uint32_t)&stack;  
}

void getFirmwareJson(JsonVariant json)
{
  JsonObject firmware = json.createNestedObject("firmware");

  firmware["name"] = STRINGIFY(FW_NAME);
  firmware["shortName"] = STRINGIFY(FW_SHORT_NAME);
  firmware["maker"] = STRINGIFY(FW_MAKER);
  firmware["version"] = STRINGIFY(FW_VERSION);
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
  configSchema["title"] = STRINGIFY(FW_SHORT_NAME);
  configSchema["type"] = "object";
}

void getCommandSchemaJson(JsonVariant json)
{
  JsonObject commandSchema = json.createNestedObject("commandSchema");
  
  // Command schema metadata
  commandSchema["$schema"] = JSON_SCHEMA_VERSION;
  commandSchema["title"] = STRINGIFY(FW_SHORT_NAME);
  commandSchema["type"] = "object";

  JsonObject properties = commandSchema.createNestedObject("properties");

  JsonObject LED = properties.createNestedObject("LED");
  LED["type"] = "array";
  LED["description"] = "Set the mode for the onboard LED, in auto mode the led will blink when parsed data is availble, in manual mode the led will only change based on a payload";
  
  JsonObject LEDItems = LED.createNestedObject("items");
  LEDItems["type"] = "object";

  JsonObject LEDProperties = LEDItems.createNestedObject("properties");

  JsonObject state = LEDProperties.createNestedObject("state");
  state["type"] = "string";
  JsonArray stateEnum = state.createNestedArray("enum");
  stateEnum.add("Auto");
  stateEnum.add("Manual");

  JsonObject colour = LEDProperties.createNestedObject("colour");
  colour["type"] = "array";
  colour["minItems"] = 1;
  colour["maxItems"] = 3;
  JsonObject colourItems = colour.createNestedObject("items");
  colourItems["type"] = "integer";
  colourItems["minimum"] = 0;
  colourItems["maximum"] = 255;

  JsonArray required = LEDItems.createNestedArray("required");
  required.add("state");

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
void pixel_off() {
    pixel.setPixelColor(0,0,0,0);        //  Set pixel's color (in RAM)
    pixel.show();                        //  Update drivers to match
}

void pixel_color(uint8_t R, uint8_t G, uint8_t B){
    pixel.setPixelColor(0,R,G,B);        //  Set pixel's color (in RAM)
    pixel.show();
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
  // turn LED dim green to show mqtt connected
  pixel_color(0,5,0);
}

void mqttDisconnected(int state) 
{
  // turn pixel orange for disconencted mqtt
  pixel_color(15,5,0);
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

}

void jsonLedCommand(JsonVariant json)
{

  if (json.containsKey("state"))
  {
    if (strcmp(json["state"], "Auto") == 0)
    {
      ledState = LED_STATE_AUTO;
    }
    else if (strcmp(json["state"], "Manual") == 0)
    {
      ledState = LED_STATE_MANUAL;
    }
    else 
    {
      logger.println(F("[ledc] invalid state"));
    }
  }

  if (ledState == LED_STATE_MANUAL)
  {
    if (json.containsKey("colour"))
    {
      JsonArray array = json["colour"].as<JsonArray>();
      uint8_t colour = 0;

      for (JsonVariant v : array)
      {
        ledColour[colour++] = v.as<uint8_t>();
      }
    }
    pixel_color(ledColour[0],ledColour[1],ledColour[2]);
  }
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
  // turn pixel blue to show wifi connection
  pixel_color(0,0,20);
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

  // Set up LED indicator
  pixel.begin();        // INITIALIZE NeoPixel strip object (REQUIRED)
  pixel_off();          // Turn indicator off
  pixel_color(20,0,0);  // Turn indicator red

  // Set up serial
  initialiseSerial();  

  // Set up network and obtain an IP address
  byte mac[6];
  initialiseWifi(mac);

  // initialise MQTT
  initialiseMqtt(mac);

  // Set up the REST API
  initialiseRestApi();

}

void loop()
{
  // Check our MQTT broker connection is still ok
  mqtt.loop();

  // Handle any API requests
  WiFiClient client = server.available();
  api.loop(&client);

}