
/*
 * £10 Temperature sensor, by Fraser MacIntosh 
 * Initially based on https://gist.github.com/anonymous/8d5b19fa4cab521b6690
 * By James Bruce, 2015
 */


#include <MQTTClient.h>
#include <ESP8266WiFi.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Adafruit_NeoPixel.h>

// Variables to be set for each different device
// NB: You can't use Gas loggin and neo Pixels on an ESP01 module as there aren't enough IO lines
const char* ssid = "SSID GOES HERE";
const char* password = "PASSWORD GOES HERE!"; 
const bool diags = TRUE; // Energy saving in the battery version of the temperature sensor is critical, so serial port operations can be turned off
const bool superPowerSave = TRUE; // Enable to put into deep sleep mode
const int superPowerSaveDuration = 600; // Seconds to remain in power save mode before restarting
const bool temperatureLogging = TRUE; // Set TRUE/FALSE for tempearture logging / no logging
const bool gasLogging = FALSE; // Set TRUE/FALSE for gas pulse logging
const bool neoPixels = FALSE; //  Set TRUE/FALSE for neoPixel lights
const int numberOfNeoPixels = 5; // The number of neoPixels we're controlling
// On an ESP-01 module, the 2nd pin from the left on the top row is 2, the 3rd is 0
const int neoPixelPin = 2; // Device pin the nexPixel strip is connected to
const int gasPin = 2; // The pin the gas pulse is detected on (it'll be the same as the neoPixel PIN, if you're using a esp-01)
const int ONE_WIRE_BUS = 0; // The pin to which the temperature sensor is connected
const int tempSensorPowerPin = 4; // The pin the power supply of the temperature sensor is connected to
const bool isTempSensorOnPin = TRUE; // true if the temperature sensor isn't hard wired to Vcc
String subscribeTopic = "/devices/"; // subscribe to this topic; anything sent here will be passed into the messageReceived function (will have MAC address and "command" appended
String temperatureTopic = "/devices/"; //topic to publish temperatures readings to, will have MAC address and "temperature" appended
String gasTopic = "/devices/"; //topic to publish gas readings to, will have MAC address and "gas" appended
const char* server = "10.1.10.23"; // server or URL of MQTT broker
String clientName = "ESP8266-"; // just a name used to talk to MQTT broker, MAC will be appended for uniqueness
long interval = 60000; //(ms) - 60 seconds between reports
unsigned long resetPeriod = 86400000; // 1 day - this is the period after which we restart the CPU, to deal with odd memory leak errors
unsigned long prevTime;


// Further vars
bool gasLogLast = FALSE;
bool gasLogCurrent = FALSE;


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
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    if (diags) {Serial.print(".");}
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
    abort();
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
      if (gasLogCurrent==FALSE){      
        client.publish(gasTopic, "Gas Pulse!");
        if (diags) {Serial.println ("Logging gas pulse.");}
      }
    }
  }

  //Free up some time for background tasks of the ESP8266
  client.loop();

  // reset after a day to avoid memory leaks 
  if(millis()>resetPeriod){
    ESP.restart();
  }
  
  if (superPowerSave && temperatureLogging && !neoPixels && !gasLogging) {
  // If we're only logging temperature and super-power-save is enabled
  // hibernate the ESP8266 and restart when next log needs to be made
  
  // Insert code here to go into deep sleep, and restart   

  if (diags) {
    Serial.println ("Going into hibernation...");
    }
    if (isTempSensorOnPin) {
      if (diags) { Serial.print ("Switching off Temperature Sensor on pin ");
                   Serial.println (tempSensorPowerPin);}
      digitalWrite(tempSensorPowerPin, LOW);
    }
  ESP.deepSleep(superPowerSaveDuration*1000000, WAKE_RF_DEFAULT); // Sleep for 60 seconds
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

