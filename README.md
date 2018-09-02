
# IKEA motorize blinds
![header](https://github.com/maxmacstn/MQTT-MotorizeBlinds/raw/master/img/motorize_blinds_en.png)

Code for ESP8266 Microcontrollers that control DIY motorized blids made from IKEA TUPPLUR blinds. Code in this repo is intentionaly to use with Homebridge [MQTT plugin](https://www.npmjs.com/package/homebridge-mqtt) on raspberry Pi.
I use 3D printed motor mount from here [instructables - MOTORIZED WIFI IKEA ROLLER BLIND](https://www.instructables.com/id/Motorized-WiFi-IKEA-Roller-Blind/)

## Features
 - Buttons up and down for manual control.
 - Persistence state - Saved current position to EEPROM.
 - Calibrate mode - Calibrate lenght, top and buttom position of the window and store to EEPROM.

## Hardware
 - 28BJY-48 Stepper motor + ULN2003 motor driver
 - Wemos D1 mini (or any ESP8266 modules)
 - two momentary switches
 - IKEA TUPPLUR blinds
 
![schematic](https://github.com/maxmacstn/MQTT-MotorizeBlinds/raw/master/Schematics_bb.png)

## Add this devices to Homebridge-MQTT 
You can use program like MQTTLENS to send this command

Topic
```
homebridge/to/add
```
Payload
```json
{
     "name": "Blind 1",
     "service_name": "blind_1",
     "service": "WindowCovering"
}
```
## Instruction
 1. I developed this program with VSCode. If you want to use Arduino IDE, please copy content in "ESP8266 Dependencies" folder to libraries folder.
 2. Don't forget to change MQTT server ip address to match yours.
 3. For the first time, use Wifi setup to connect to your router.
 3. Press and hold down button while inputting power or reset to enter calibrate mode. Release button, blind will going down, press again when it reached lowese point, it will go up, press again when it reach highest point, it will stop and record the value.

## Dependencies
 - [CheapStepper](https://github.com/tyhenry/CheapStepper) Library - I modified from this library a little bit.
 - PubSubClient 
 
## Known issues
Calibrate mode is hard to perform, motor rotate wrong rotation but it can solve by restart the process again (Maybe caused by switch debouncing). 
Rapidly press up or down button may cause system to crash, maybe it blocked main loop.
