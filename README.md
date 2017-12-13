# ESP8266 Tenner Sensor

Arduino code for a multi use sensor, which repots in MQTT based on the ESP8266 chip. Intended to cost less than a £10.

Details:
Tested on Arduino IDE 1.6.5 and ESP01+ESP12 ESP8266 modules
See https://thefrinkiac7.wordpress.com/esp8266-modules/ for details of hardware construction

The MQTT client name will be ESP8266-{mac address} for uniqueness

NB: We're using the MQTTClient library, rather than pubsubclient as it seemed to be more stable with the ESP8266 module.

Current features:
1-wire temperature sensors (DSB1820)
Ability to switch off sensor when not in use (not really needed for a DSB1820, but may be for other sensors)
Super-power save for modules where the reset line is broken out
Ability to switch a number of Neopixel RGB LEDs under control of MQTT message (format message as: "RRR GGG BBB ttt" (no "s) where R G and B are 000-255 with leading zeros and ttt is ms between each LED's update)
Ability to read pulses from a gas meter and report as MQTT

To Do:
"mqtt log" flag and code up logging to mqtt for off-serial hook up
See if a DSB1820 can be run from parasite power using the internal pullup IO lines.
Basic API to allow different neo-pixel effects (strobe, rainbow, whoosh, etc)



Gotchas:
If using GPIO0 for Gas Sensor, if the sensor is activated when you start the ESP8266 it will go into program mode and not start the sketch - Use another GPIO line!! or disconnect sensors before boot!
