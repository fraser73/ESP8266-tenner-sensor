
/*
 * £10 MQTT multi-sensor, by Fraser MacIntosh
 * Initially intended to be a cheap temperature sensor using the ESP8266, it is now
 * a configurable multi sensor and controller, for a minimal price point - you should still
 * be able to get a temperature sensor working for approx £10.
 * 
 * Git: https://github.com/fraser73/ESP8266-tenner-sensor
 * 
 * Initial MQTT code based on https://gist.github.com/anonymous/8d5b19fa4cab521b6690
 * By James Bruce, 2015
 */

//Include Libraries
#include <PubSubClient.h>       // Nick O'leary's MQTT client
#include <ArduinoJson.h>        // Arduino JSON ibrary
#include <ESP8266WiFi.h>        // ESP8266 WiFi Library
#include <time.h>               // Time library
#include <OneWire.h>            // 
#include <DallasTemperature.h>  // DallasTemperature Library for the DS18B20
#include <Adafruit_NeoPixel.h>  // 

//Include the Configration files - make sure you update this before compilation!
#include "configuration.h"
#include "ESP-functions.h"

// Further vars
bool gasLogLast = false;     // Used to store that last logged value of the gas pin
bool gasLogCurrent = false;  // Used to compare the current version of the gap pin to the previous

time_t now;                  // this are the seconds since Epoch (1970) - UTC
tm tm;                       // the structure tm holds time information in a more convenient way

bool unHandledMessage = false;  // Used to signal from the callback that there's an unhandled message
String stringifiedTopic = "";
String stringifiedPayload = "";

// If using Neo Pixels, make sure the next non-comment line is correctly setup (it should be!)
// Parameter 1 = number of pixels in strip
// Parameter 2 = Arduino pin number (most are valid)
// Parameter 3 = pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
Adafruit_NeoPixel strip = Adafruit_NeoPixel(numberOfNeoPixels, neoPixelPin, NEO_GRB + NEO_KHZ800);

//Setup a oneWire instance
OneWire oneWire(ONE_WIRE_BUS);

//Pass the oneWire reference to Dallas Temperature
DallasTemperature sensors (&oneWire);

//Define WiFi and MQTT clients
WiFiClient wifiClient;
PubSubClient client(wifiClient);

/*
 * ************************************************* Setup *************************************************
 */
 
void setup() {

  // State what's enabled, if diags is true
  if (diags) {
    Serial.begin(9600);
    if (temperatureLogging) { Serial.println ("Temperature logging enabled"); }
    if (gasLogging) { Serial.println ("Gas Logging enabled"); }
    if (neoPixels) { Serial.println ("Neopixels enabled"); }
    }

  // Setup Temperature Sensor, if temperatureLogging is true
  if (temperatureLogging) {
        // Switch on temperature sensor, if it's on an IO pin
    if (isTempSensorOnPin) {
      if (diags) { Serial.print ("Switching on Temperature Sensor on pin ");
                   Serial.println (tempSensorPowerPin);}
      pinMode (tempSensorPowerPin, OUTPUT);
      digitalWrite(tempSensorPowerPin, HIGH);
    }
    //Start one wire bus
    if (diags) { Serial.println ("Starting OneWire/Dallas Temperature Sensors "); }
    sensors.begin();
  }

  // Setup Gas usage logging if gasLogging true
  if (gasLogging) {
    if (diags) { Serial.print ("Configuring Gas Pulse Sensing on pin ");
                 Serial.println (gasPin); }
    pinMode (gasPin, INPUT_PULLUP);
    gasLogCurrent=digitalRead (gasPin);
    gasLogLast=gasLogCurrent;
  }
  
  // Setup Neopixels
  if (neoPixels) {
    if (diags) { Serial.println ("Configuring NeoPixels: ");
                 Serial.print ("NeoPixels on pin: ");
                 Serial.println (neoPixelPin);
                 Serial.print ("Number of pixels: ");
                 Serial.println (numberOfNeoPixels);  }

    if (diags) { Serial.print ("Starting Strip: "); }
    if (strip.begin()) {
      if (diags) { Serial.println ("Success"); }
    } else {
      if (diags) { Serial.println ("Failed"); }
    }

    // We'll just presume that if the strip started, it will actually work
    strip.show();
  }

  // Define MQTT server and port
  // setup callback routine for arriving messages
  client.setServer(mqttServer, 1883);
  client.setCallback(callback);


  //Start WiFi and connect to network
  if (diags) {
    Serial.print("Connecting to ");
    Serial.println(ssid);  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  int loopCounter=0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    if (diags) {Serial.print(".");}
    loopCounter++;
    if (loopCounter>=maxWiFiTries) {
      if (diags) {Serial.println ("");
                  Serial.print ("WiFi not connected after ");
                  Serial.print (maxWiFiTries);
                  Serial.println (" seconds");
                  }
      restartESP(diags);
    }
  }

  if (diags) {
    Serial.println("");
    Serial.println("WiFi connected");  
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    }


  // Get NTP Server Setup
  // ip_addr_t ntpServerName;
  if (diags) { Serial.println ("NTP server set to "+ntpServerName); }

  // Setup NTP & Initial Sync

//  setupNTP(ntpServerName, 100000, diags);

  configTime(0, 0, ntpServerName);

  // Wait for time sync
  if (diags) { Serial.print("Waiting for NTP Time Sync"); }

  while ((now = time(nullptr)) < 100000) {
    delay(500);
    if (diags) { Serial.print("."); }
  }
  
  if (diags) { Serial.printf("\nTime synced: %s", ctime(&now)); }


////////////////////////////////////////////////////////////////////////////////////////////
  //Setup MQTT and connect to broker
  //Generate client name based on MAC address
  uint8_t mac[6];
  WiFi.macAddress(mac);
  mqttClientName += macToStr(mac);

  //Setup MQTT topic strings
  subscribeTopic += macToStr(mac);
  subscribeTopic += ("/command");
  temperatureTopic += macToStr(mac);
  temperatureTopic += ("/temperature");
  gasTopic += macToStr(mac);
  gasTopic += ("/gas");


  if (diags) {
    Serial.print("Connecting to ");
    Serial.print(mqttServer);
    Serial.print(" as ");
    Serial.println(mqttClientName);
  }
  
  if (client.connect((char*) mqttClientName.c_str())) {
    if (neoPixels) {
      if (diags) {
        Serial.println("Connected to MQTT broker");
        Serial.print("Subscribed to: ");
        Serial.println(subscribeTopic);
      }
      client.subscribe((char*) subscribeTopic.c_str());
    }
  } else {
    if (diags) {
      Serial.println("MQTT connect failed");
      Serial.println("Will reset and try again...");
    }
    restartESP(diags);
  }

  prevTime = 0;
}


/*
 * ************************************************* Loop *************************************************
 */
 
void loop() {
  //static int counter = 0;

  if (temperatureLogging) {
    //If we've gone over the mS delay value for a new temperature sensor report...
    if(prevTime + interval < millis() || prevTime == 0){
      prevTime = millis();
      if (diags) {
      Serial.print("Checking temperature at uptime mS: ");
      Serial.println(prevTime);
      }

    // Obtain the temperature from the one wire sensor
    // and convert to a string.
    sensors.requestTemperatures(); // Send the command to get temperatures
    float temperature=(sensors.getTempCByIndex(0)); // by index 0 to get the first sensor on the bus - more can be available
    char buffer[12];
    String temperatureString = dtostrf(temperature, 4, 1, buffer);

    if (diags) {
      Serial.print("Temperature obtained as: ");
      Serial.print(temperatureString);
      Serial.println ("C");
      Serial.println ("Publishing to MQTT broker at "+temperatureTopic);
      Serial.println();
    }

    client.publish((char*) temperatureTopic.c_str(), (char*) temperatureString.c_str());
  }
  }
  
  //Process MQTT messages
  client.loop();

  if (gasLogging) {
    // Read Gas Pin
    gasLogCurrent=digitalRead (gasPin);
    
    //Look to see if the Gas pulse pin has changed.
    if (gasLogCurrent!=gasLogLast) {
      gasLogLast=gasLogCurrent;
      delay (50); // wait 50ms for debouncing

      // Report only HIGH going LOW (ie: sensor becoming activated)
      // If reporting whenever a state change, we'll report twice as 
      // many gas pulses as required.
      if (gasLogCurrent==false){
        client.publish((char*) gasTopic.c_str(), "Gas Pulse!");
        if (diags) {Serial.println ("Logging gas pulse.");}
      }
    }
  }

  //Process MQTT messages
  client.loop();

  // Handle NeoPixel updates if there's been a message callback
  if (unHandledMessage) { 
    messageReceived (stringifiedTopic, stringifiedPayload);
    unHandledMessage=false;
  }



// End of all logging and NeoPixels etc, on to housekeeping

// Go into power save before checking other reboot type functions, so we don't end up
// in a loop burning battery, if we don't need to

  if (superPowerSave && temperatureLogging && !neoPixels && !gasLogging) {

  // If we're only logging temperature and super-power-save is enabled
  // hibernate the ESP8266 and restart when next log needs to be made
  
  if (diags) {
    Serial.println ("Going into hibernation...");
    }
    if (isTempSensorOnPin) {
      if (diags) { Serial.print ("Switching off Temperature Sensor on pin ");
                   Serial.println (tempSensorPowerPin);}
      digitalWrite(tempSensorPowerPin, LOW);
    }
  ESP.deepSleep(superPowerSaveDuration*1000000, WAKE_RF_DEFAULT); // Sleep for superPowerSaveDuration seconds
  }

// reset after a day to avoid memory leaks 
  if(millis()>resetPeriod){
    if (diags) {Serial.println ("Restarting after reset period reached");}
    restartESP(diags);
  }

// If we're not connected to the MQTT broker any more, restart to reconnect networking
  if(!client.connected()){
    if (diags) {Serial.println ("No longer connected to MQTT broker, restarting");}
    restartESP(diags);
  }
  
}

/*
 * ************************************************* Other Routines *************************************************
 */

/* float to string
 * f is the float to turn into a string
 * p is the precision (number of decimals)
 * return a string representation of the float.
 */
char *f2s(float f, int p){
  char * pBuff;                         // use to remember which part of the buffer to use for dtostrf
  const int iSize = 10;                 // number of buffers, one for each float before wrapping around
  static char sBuff[iSize][20];         // space for 20 characters including NULL terminator for each float
  static int iCount = 0;                // keep a tab of next place in sBuff to use
  pBuff = sBuff[iCount];                // use this buffer
  if(iCount >= iSize -1){               // check for wrap
    iCount = 0;                         // if wrapping start again and reset
  } else {
    iCount++;                           // advance the counter
  }
  return dtostrf(f, 0, p, pBuff);       // call the library function
}

// Process the received MQTT JSON message, deserialise and set the pixel index as per message
void messageReceived(String topic, String payload) {
  //Only process recieved messages if we're controlling Neo Pixels
  if (neoPixels) {
    int neoPixelIndex = 0; // Index of the pixel to update
    int neoPixelRed = 0;   // Store the value for the Red channel
    int neoPixelGreen = 0; // Store the value for the Green channel
    int neoPixelBlue = 0;  // Store the value for the Blue channel

    if (diags) {
      Serial.print("MQTT Incoming: ");
      Serial.print(topic);
      Serial.println();
      Serial.print ("MQTT Payload: ");
      Serial.println(payload);
      }

    // Handle message processing
    // JSON "{\"index\":255,\"red\":255,\"green\":255,\"blue\":255}"
    // Parse the JSON message payload and convert to integers
    JsonDocument doc;
    if (deserializeJson(doc, payload)) {
      neoPixelIndex = doc["index"]; // 0..numberOfNeoPixels
      neoPixelRed = doc["red"];     // 0..254
      neoPixelGreen = doc["green"]; // 0..254
      neoPixelBlue = doc["blue"];   // 0..254

      // Sanitise payload brightness is max 254 min 0
      // Index is min 0 max numberOfNeoPixels
      if (neoPixelIndex > numberOfNeoPixels) { neoPixelIndex=numberOfNeoPixels; }
      if (neoPixelIndex < 0 )  { neoPixelIndex=0; }
      if (neoPixelRed > 254)   { neoPixelRed=254; }
      if (neoPixelRed < 0)     { neoPixelRed=0; }
      if (neoPixelGreen > 254) { neoPixelGreen=254; }
      if (neoPixelGreen < 0)   { neoPixelGreen=0; }
      if (neoPixelBlue > 254)  { neoPixelBlue=254; }
      if (neoPixelBlue < 0)    { neoPixelBlue=0; }

      if (diags) {
        Serial.print("Index : ");
        Serial.println (neoPixelIndex);
        Serial.print("Red : ");
        Serial.println (neoPixelRed);
        Serial.print("Green : ");
        Serial.println (neoPixelGreen);
        Serial.print("Blue : ");
        Serial.println (neoPixelBlue);
        }

      //Set the neo Pixel
      strip.setPixelColor(neoPixelIndex, neoPixelRed, neoPixelGreen, neoPixelBlue);
  
    } else {
    
    //If the JSON didn't deserialise log the error in diags
      if (diags) { Serial.println ("JSON Failed to deserialize."); }
    
    }
  }
}

// Show the time from NTP
void showTime() {
  time(&now);                       // read the current time
  localtime_r(&now, &tm);           // update the structure tm with the current time
  Serial.print("year:");
  Serial.print(tm.tm_year + 1900);  // years since 1900
  Serial.print("\tmonth:");
  Serial.print(tm.tm_mon + 1);      // January = 0 (!)
  Serial.print("\tday:");
  Serial.print(tm.tm_mday);         // day of month
  Serial.print("\thour:");
  Serial.print(tm.tm_hour);         // hours since midnight  0-23
  Serial.print("\tmin:");
  Serial.print(tm.tm_min);          // minutes after the hour  0-59
  Serial.print("\tsec:");
  Serial.print(tm.tm_sec);          // seconds after the minute  0-61*
  Serial.print("\twday");
  Serial.print(tm.tm_wday);         // days since Sunday 0-6
  if (tm.tm_isdst == 1)             // Daylight Saving Time flag
    Serial.print("\tDST");
  else
    Serial.print("\tstandard");
  Serial.println();
}

// Handle an MQTT message arrival
// The callback should be as fast as possible, in case messages turn up
// while the callback is being processed.
void callback(char* topic, uint8_t* payload, size_t plength) {
    if (diags) {
      Serial.println ("Message arrived");
      Serial.print ("Unprocessed data topic [");
      Serial.print (topic);
      Serial.println ("] ");
    
      Serial.print ("Unprocessed payload [");
      for (size_t i = 0; i < plength; i++) {
        Serial.print((char)payload[i]);
      }
      Serial.println();
    }

    // We want to spend the smallest amount of time possible in the callback
    // So set a flag and strings to be dealt with in the loop()
    stringifiedTopic = String(topic);
    stringifiedPayload = String((char*)payload).substring(0, plength);
    unHandledMessage = true;

}
