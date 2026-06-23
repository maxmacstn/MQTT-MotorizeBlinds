
# IKEA Motorized Blinds
![header](https://github.com/maxmacstn/MQTT-MotorizeBlinds/raw/master/img/motorize_blinds_en.png)

Code for ESP8266 microcontrollers that control DIY motorized blinds made from IKEA TUPPLUR blinds. The code in this repo is intended for use with the Homebridge [MQTT plugin](https://www.npmjs.com/package/homebridge-mqtt) on a Raspberry Pi.
I use the 3D-printed motor mount from here: [Instructables - MOTORIZED WIFI IKEA ROLLER BLIND](https://www.instructables.com/id/Motorized-WiFi-IKEA-Roller-Blind/)

## Features
 - Up and down buttons for manual control.
 - Persistent state - the current position is saved to EEPROM.
 - Calibrate mode - calibrates the length and the top and bottom positions of the window and stores them in EEPROM. Enter it at boot or by holding the down button for 5 seconds during normal operation.
 - Home Assistant support via MQTT auto-discovery (cover entity with open/close/stop).
 - Over-the-air (OTA) firmware updates.
 - Stop the motor at any time from MQTT / Home Assistant / Homebridge while it is moving.

## Hardware
 - 28BYJ-48 stepper motor + ULN2003 motor driver
 - Wemos D1 mini (or any ESP8266 module)
 - Two momentary switches
 - IKEA TUPPLUR blinds
 
![schematic](https://github.com/maxmacstn/MQTT-MotorizeBlinds/raw/master/Schematics_bb.png)

## Add these devices to Homebridge-MQTT
You can use a program like MQTTLENS to send this command.

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
 1. I developed this program with VSCode. If you want to use the Arduino IDE, please copy the contents of the "ESP8266 Dependencies" folder into your libraries folder.
 2. Don't forget to change the MQTT server IP address to match yours.
 3. The first time, use the WiFi setup to connect to your router.
 4. Enter calibrate mode in either of two ways: hold the down button while powering on or resetting, **or** hold the down button for 5 seconds during normal operation. Release the button and the blind goes down; press again when it reaches the lowest point and it goes up; press again when it reaches the highest point and it stops and records the value.

### OTA updates
The firmware exposes OTA updates (hostname = the device `service_name`, e.g. `blind_1.local`). To flash over WiFi, uncomment the `espota` upload lines in `platformio.ini`, set `upload_port` to the device IP/hostname, and keep `--auth` matching `ArduinoOTA.setPassword(...)` in the firmware.

## Dependencies
 - [CheapStepper](https://github.com/tyhenry/CheapStepper) library - I modified this library a little bit.
 - PubSubClient 