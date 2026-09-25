#include "ESP-Functions.h"


///////////////////////////////////////////////////////////////////////////////////////////

void restartESP(bool diags) {
  // Test to see if the ESP will reboot into "flash mode" for programming, 
  // if so it stays there, effectively dead until manually reset
  // as booting with GPIO0 low results in non-functioning device
  // Flash mode is GPIO0=LOW, normal boot is GPIO=HIGH (it's pulled up)
  
  if (diags) { Serial.print ("Waiting for program pin (GPIO0) to go high, for safe reboot."); }

  pinMode (0, INPUT_PULLUP);  // Explicitly define the pin as input, with pullup to high, in case it's used as an output elsewhere
  while (!digitalRead(0)) {
    delay (1000);
    if (diags) { Serial.print ("."); }
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


///////////////////////////////////////////////////////////////////////////////////////////

String macToStr(const uint8_t* mac)
  //Returns the MAC address as a string
{
  String result;
  for (int i = 0; i < 6; ++i) {
    result += String(mac[i], 16);
    if (i < 5)
      result += ':';
  }
  return result;
}

