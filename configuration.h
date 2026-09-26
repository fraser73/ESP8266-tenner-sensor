//Configuration variables for ESP8266 Tenner Sensor Project

// Variables to be set for each different device
// NB: You can't use Gas logging and neo Pixels on an ESP01 module as there aren't enough IO lines

//WiFi variables - currently hard coded!
//const char* ssid = "SSID";
//const char* password = "PASSWORD";
const char* ssid = "Frinkium Nitrate";
const char* password = "tEe3#cjF%dQvV2GD";

//Enable/disable features
const bool temperatureLogging = true; // Set true/false for tempearture logging / no logging
const bool gasLogging = false; // Set true/false for gas pulse logging
const bool neoPixels = true; //  Set true/false for neoPixel lights
const bool superPowerSave = false; // Enable to put into deep sleep mode

//Enable/disable logging
const bool diags = true; // Energy saving in the battery version of the temperature sensor is critical, so serial port operations can be turned off

//Other variables
const int superPowerSaveDuration = 600; // Seconds to remain in power save mode before restarting
const int numberOfNeoPixels = 5; // The number of neoPixels we're controlling
const bool isTempSensorOnPin = false; // true if the temperature sensor isn't hard wired to Vcc
const int tempSensorPowerPin = 4; // The pin the power supply of the temperature sensor is connected to

//WiFi / MQTT / NTP Settings
const int maxWiFiTries = 30;                         // Maximum wait (in seconds) for WiFi to connect, before resetting to try again
String subscribeTopic = "devices/";                  // Subscribe to this topic; anything sent here will be passed into the messageReceived function (will have MAC address, pixelStrip and "command" appended)
String pixelStripTopic = "devices/";                 // Topic to publish details of the pixelstrip anything sent here will be passed into the messageReceived function (will have MAC address & pixelStrip appended)
String temperatureTopic = "devices/";                // Topic to publish temperatures readings to, will have MAC address and "temperature" appended
String gasTopic = "devices/";                        // Topic to publish gas readings to, will have MAC address and "gas" appended
String statusTopic = "devices/";                     // Topic to publish status of the ESP, such as "Awake!" etc. will have the MAC address and status appended
const char* mqttServer = "10.1.11.83";   // IP or URL of MQTT broker
const int mqttPort = 1880;                            // Port that the MQTT broker listens on
String mqttClientName = "ESP8266-";                   // just a name used to talk to MQTT broker, MAC will be appended for uniqueness
String ntpServerName = "10.1.11.2";    // A local network windows NTP Server

//Configure GPIO Connection Pins
//NB: On an ESP-01 module, the 2nd pin from the left on the top row is 2, the 3rd is 0
//ALSO NB: IF YOU WANT TO USE SERIAL MONITOR ON AN ESP8266 ESP-01 BOARD YOU CAN'T USE THE FOLLOWING:
// GPIO1 / Package pin 5 / UART0 TX
// GPIO3 / Package Pin 4 / UART0 RX
const int neoPixelPin = 4; // (originally 2) Device pin the nexPixel strip is connected to
const int gasPin = 2; // The pin the gas pulse is detected on (it'll be the same as the neoPixel PIN, if you're using a esp-01)
const int ONE_WIRE_BUS = 0; // The pin to which the temperature sensor is connected

//General Variables/constants
long interval = 60000; //(ms) - 60 seconds between reports
unsigned long resetPeriod = 86400000; // 1 day - this is the period after which we restart the CPU, to deal with odd memory leak errors
unsigned long prevTime;
