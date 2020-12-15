
/*
 * £10 MQTT multi-sensor, by Fraser MacIntosh
 * Initially intended to be a cheap temperature sensor using the ESP8266, it is now
 * a configurable multi sensor and controller, for a minimal price point - you should still
 * be able to get a temperature sensor working for approx £10.
 * 
 * Git: https://github.com/fraser73/ESP8266-tenner-sensor
 * 
 * Initial code based on https://gist.github.com/anonymous/8d5b19fa4cab521b6690
 * By James Bruce, 2015
 */

//Include Libraries
#include <FS.h>                 // ESP8266 Filesystem FS.h library
#include <MQTTClient.h>         // IDE MQTT library
#include <ArduinoJson.h>        // ArduinoJson v6 library (not the Arduino_JSON which seems to have been abandoned)
#include <ESP8266WiFi.h>        // 
#include <OneWire.h>            // 
#include <DallasTemperature.h>  // DallasTemperature Library for the DS18B20
#include <Adafruit_NeoPixel.h>  // 

//Include the Configration files - make sure you update this before compilation!
#include "configuration.h"


// Further vars
bool gasLogLast = false;
bool gasLogCurrent = false;


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

//Define WiFi client and MQTT client
WiFiClient wifiClient;
MQTTClient client;

/*
 * ************************************************* Setup *************************************************
 */
 
void setup() {
  if (diags) {
    Serial.begin(9600);
    if (temperatureLogging) { Serial.println ("Temperature logging enabled"); }
    if (gasLogging) { Serial.println ("Gas Logging enabled"); }
    if (neoPixels) { Serial.println ("Neopixels enabled"); }
    
    }

// ////////////// experimental section will need tidying up...

// mount the SPIFFS filesystem
// bool success = SPIFFS.begin();
 
//  if(SPIFFS.begin()){
//    Serial.println("File system mounted with success");  
//  }else{
 //   Serial.println("Error mounting the file system");  
//  }

//write a file


// read a file


// ///////////////////////////

  if (temperatureLogging) {
    // Switch on temperature sensor, if it's on an IO pin
    if (isTempSensorOnPin) {
      if (diags) { Serial.print ("Switching on Temperature Sensor on pin ");
                   Serial.println (tempSensorPowerPin);}
      pinMode (tempSensorPowerPin, OUTPUT);
      digitalWrite(tempSensorPowerPin, HIGH);
    }
    //Start one wire bus
    sensors.begin();
  }

  if (gasLogging) {
    pinMode (gasPin, INPUT_PULLUP);
    gasLogCurrent=digitalRead (gasPin);
    gasLogLast=gasLogCurrent;
  }
  
  if (neoPixels) {
    strip.begin();
    strip.show(); // initialise all pixels to "off"
  }

  //Start WiFi and connect to network
  client.begin(server,wifiClient);
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
      restartESP();
    }
  }

  if (diags) {
    Serial.println("");
    Serial.println("WiFi connected");  
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());}
    
  //Setup MQTT and connect to broker
  //Generate client name based on MAC address
  uint8_t mac[6];
  WiFi.macAddress(mac);
  clientName += macToStr(mac);

  //Setup MQTT topic strings
  subscribeTopic += macToStr(mac);
  subscribeTopic += ("/command");
  temperatureTopic += macToStr(mac);
  temperatureTopic += ("/temperature");
  gasTopic += macToStr(mac);
  gasTopic += ("/gas");


  if (diags) {
    Serial.print("Connecting to ");
    Serial.print(server);
    Serial.print(" as ");
    Serial.println(clientName);
  }
  
  if (client.connect((char*) clientName.c_str())) {
    if (neoPixels) {
      if (diags) {
        Serial.println("Connected to MQTT broker");
        Serial.print("Subscribed to: ");
        Serial.println(subscribeTopic);
      }
      client.subscribe(subscribeTopic);
    }
  }
  else {
    if (diags) {
      Serial.println("MQTT connect failed");
      Serial.println("Will reset and try again...");
    }
    restartESP();
  }

  prevTime = 0;
}


/*
 * ************************************************* Loop *************************************************
 */
 
void loop() {
  static int counter = 0;

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
    client.publish(temperatureTopic, temperatureString);
  }
  }
  
  //Free up some time for background tasks of the ESP8266
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
        client.publish(gasTopic, "Gas Pulse!");
        if (diags) {Serial.println ("Logging gas pulse.");}
      }
    }
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

//Free up some time for background tasks of the ESP8266
  client.loop();

// reset after a day to avoid memory leaks 
  if(millis()>resetPeriod){
    if (diags) {Serial.println ("Restarting after reset period reached");}
    restartESP();
  }

// If we're not connected to the MQTT broker any more, restart to reconnect networking
  if(!client.connected()){
    if (diags) {Serial.println ("No longer connected to MQTT broker, restarting");}
    restartESP();
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
  }
  else{
    iCount++;                           // advance the counter
  }
  return dtostrf(f, 0, p, pBuff);       // call the library function
}

void messageReceived(String topic, String payload, char * bytes, unsigned int length) {
  //Only process recieved messages if we're controlling Neo Pixels
  if (neoPixels) {
  int neoPixelRed = 0; // Store the value for the Red channel
  int neoPixelGreen = 0; // Store the value for the Green channel
  int neoPixelBlue = 0; // Store the value for the Blue channel.
  int neoPixelDelay = 0; // The time in ms for swipe in.
  
  if (diags) {
    Serial.print("incoming: ");
    Serial.print(topic);
    Serial.print(" - ");
    Serial.print(payload);
    Serial.println();
    Serial.print ("MQTT Payload: ");
    Serial.println(payload);
  }

  // Handle message processing here - format:
  // "000 000 000 000" to "255 255 255 999" for Red, Green, Blue, Delay between each pixel
  // Break up the message payload and convert to integers
  neoPixelRed = payload.substring(0,3).toInt();
  neoPixelGreen = payload.substring(4,7).toInt();
  neoPixelBlue = payload.substring(8,11).toInt();
  neoPixelDelay = payload.substring(12,15).toInt();

  // Make sure the payload isn't greater than 255 or less than 0
  if (neoPixelRed > 255) {neoPixelRed=255;};
  if (neoPixelRed < 0) {neoPixelRed=0;};
  if (neoPixelGreen > 255) {neoPixelGreen=255;};
  if (neoPixelGreen < 0) {neoPixelGreen=0;};
  if (neoPixelBlue > 255) {neoPixelBlue=255;};
  if (neoPixelBlue < 0) {neoPixelBlue=0;};

  // Make sure the speed isn't greater than 999 or less than 0
  if (neoPixelDelay > 999) {neoPixelDelay=999;};
  if (neoPixelDelay < 0) {neoPixelDelay=0;};

  if (diags) {
    Serial.print("Red : ");
    Serial.println (neoPixelRed);
    Serial.print("Green : ");
    Serial.println (neoPixelGreen);
    Serial.print("Blue : ");
    Serial.println (neoPixelBlue);
    Serial.print("Delay : ");
    Serial.println (neoPixelDelay);
  }

  //Set the neo Pixels
  colourWipe(strip.Color(neoPixelRed, neoPixelGreen, neoPixelBlue), neoPixelDelay);
  
  }
}


// Fill the dots one after the other with a color
void colourWipe(uint32_t c, uint8_t wait) {
  for(uint16_t i=0; i<strip.numPixels(); i++) {
    strip.setPixelColor(i, c);
    strip.show();
    delay(wait);
  }
}

//Returns the MAC address as a string
String macToStr(const uint8_t* mac)
{
  String result;
  for (int i = 0; i < 6; ++i) {
    result += String(mac[i], 16);
    if (i < 5)
      result += ':';
  }
  return result;
}


// Fill the dots one after the other with a color
void restartESP() {
  // Test to see if the ESP will reboot into "flash mode" for programming, if so it stays there, effectively dead until manually reset
  // Flash mode is hardware GPIO0=LOW
  // Loop round this until GPIO0=HIGH, as booting low results in non-functioning device

  if (diags) {Serial.print ("Waiting for program pin (GPIO0) to go high, for safe reboot.");}

  pinMode (0, INPUT_PULLUP);  // Explicitly define the pin as input, with pullup to high, in case it's used as an output elsewhere
  while (!digitalRead(0)) {
    delay (1000);
    if (diags) {Serial.print (".");}
  }
  
  if (diags) {
    Serial.println("");
    Serial.println("");
    Serial.println("Restarting...");
    Serial.println("");
    Serial.println("");
  }
  
  ESP.restart(); // Call the platform specific restart function for the ESP8266
}
