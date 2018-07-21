#include <CheapStepper.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <math.h>
#include <EEPROM.h>

/*
homebridge/from/set
{"name" : "Blind 1" ,  "service_name" : "blind_1" , "characteristic" : "TargetPosition" , "value" : 50}


*/

const int stepsPerRev = 200;
CheapStepper stepper;

// Constants
const char *autoconf_ssid = "ESP8266 Blinds"; //AP name for WiFi setup AP which your ESP will open when not able to connect to other WiFi
const char *autoconf_pwd = "12345678";        // AP password so noone else can connect to the ESP in case your router fails
const char *mqtt_server = "192.168.1.15";     //MQTT Server IP, your home MQTT server eg Mosquitto on RPi, or some public MQTT
const int mqtt_port = 1883;                   //MQTT Server PORT, default is 1883 but can be anything.
const bool isInvert = true;
const int btnUp = D1;
const int btnDown = D2;

// MQTT Constants
const char *mqtt_device_value_from_set_topic = "homebridge/from/set";
const char *mqtt_device_value_to_set_topic = "homebridge/to/set";
const char *device_name = "Blind 1";

// Global Variable
WiFiClient espClient;
PubSubClient client(espClient);
bool moveClockwise = true;
bool isMoving = false;
unsigned int previousPositionStep = 0;
unsigned int currentPositionStep = 0;
unsigned int targetPositionStep = 0;
unsigned int currentPositionPercent = 0;
unsigned int lastValue = 0;
unsigned int lastSavedValue = 0;
unsigned int maxPositionStep = 4096 * 4;         //Maximum step for blinds to get down


//Declare prototype functions
void setPosition(unsigned int);
void saveStatus();

void setup_ota()
{

  // Set OTA Password, and change it in platformio.ini
  ArduinoOTA.setPassword("ESP8266_PASSWORD");
  ArduinoOTA.onStart([]() {});
  ArduinoOTA.onEnd([]() {});
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {});
  ArduinoOTA.onError([](ota_error_t error) {
    if (error == OTA_AUTH_ERROR)
      ; // Auth failed
    else if (error == OTA_BEGIN_ERROR)
      ; // Begin failed
    else if (error == OTA_CONNECT_ERROR)
      ; // Connect failed
    else if (error == OTA_RECEIVE_ERROR)
      ; // Receive failed
    else if (error == OTA_END_ERROR)
      ; // End failed
  });
  ArduinoOTA.begin();
}

void reconnect()
{

  // Loop until we're reconnected
  while (!client.connected())
  {

    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect

    if (client.connect(clientId.c_str()))
    {
      // Once connected, resubscribe.
      client.subscribe(mqtt_device_value_from_set_topic);
    }
    else
    {

      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void blink()
{

  //Blink on received MQTT message
  digitalWrite(LED_BUILTIN, LOW);
  delay(20);
  digitalWrite(LED_BUILTIN, HIGH);
}

// //This function will write a 4 byte (32bit) long to the eeprom at
// //the specified address to address + 3.
// void EEPROMWritelong(int address, long value)
// {
//   //Decomposition from a long to 4 bytes by using bitshift.
//   //One = Most significant -> Four = Least significant byte
//   byte four = (value & 0xFF);
//   byte three = ((value >> 8) & 0xFF);
//   byte two = ((value >> 16) & 0xFF);
//   byte one = ((value >> 24) & 0xFF);

//   //Write the 4 bytes into the eeprom memory.
//   EEPROM.put(address, four);
//   EEPROM.put(address + 1, three);
//   EEPROM.put(address + 2, two);
//   EEPROM.put(address + 3, one);
//   EEPROM.commit();
// }

// long EEPROMReadlong(long address)
// {
//   //Read the 4 bytes from the eeprom memory.
//   long four = 0;
//   long three = 0;
//   long two = 0;
//   long one = 0;

//   EEPROM.get(address ,four);
//   EEPROM.get(address + 1, three);
//   EEPROM.get(address + 2, two);
//   EEPROM.get(address + 3, one);


//   //Return the recomposed long by using bitshift.
//   return ((four << 0) & 0xFF) + ((three << 8) & 0xFFFF) + ((two << 16) & 0xFFFFFF) + ((one << 24) & 0xFFFFFFFF);
// }

void stopMoving()
{
  targetPositionStep = currentPositionStep;

  stepper.stop();
  String value;
  String message;
  char data[100];
  message = "{\"name\" : \"Blind 1\", \"service_name\" : \"blind_1\", \"characteristic\" : \"TargetPosition\", \"value\" : " + String(currentPositionPercent) + "}";
  message.toCharArray(data, (message.length() + 1));
  client.publish(mqtt_device_value_to_set_topic, data);
  saveStatus();
}

void btnUpPressed()
{
  while (digitalRead(btnUp) == LOW)
    ;
  if (stepper.getStepsLeft() != 0)
  {
    stopMoving();
    return;
  }

  setPosition(100);
  String value;
  String message;
  char data[100];
  message = "{\"name\" : \"Blind 1\", \"service_name\" : \"blind_1\", \"characteristic\" : \"TargetPosition\", \"value\" : " + String(100) + "}";
  message.toCharArray(data, (message.length() + 1));
  client.publish(mqtt_device_value_to_set_topic, data);
}

void btnDownPressed()
{
  while (digitalRead(btnDown) == LOW);
  if (stepper.getStepsLeft() != 0)
  {
    stopMoving();
    return;
  }

  setPosition(0);
  String value;
  String message;
  char data[100];
  message = "{\"name\" : \"Blind 1\", \"service_name\" : \"blind_1\", \"characteristic\" : \"TargetPosition\", \"value\" : " + String(0) + "}";
  message.toCharArray(data, (message.length() + 1));
  client.publish(mqtt_device_value_to_set_topic, data);
}

void callback(char *topic, byte *payload, unsigned int length)
{

  char c_payload[length];
  memcpy(c_payload, payload, length);
  c_payload[length] = '\0';

  String s_topic = String(topic);
  String s_payload = String(c_payload);

  Serial.println(s_payload + "\0");

  StaticJsonBuffer<200> jsonBuffer;

  JsonObject &root = jsonBuffer.parseObject(s_payload);

  const char *name = root["name"];

  Serial.println(name);
  if (strcmp(name, device_name) != 0)
  {
    return;
  }

  blink();
  const char *characteristic = root["characteristic"];

  if (strcmp(characteristic, "TargetPosition") == 0)
  {
    int value = root["value"];
    Serial.print("Brightness = ");
    Serial.println(value, DEC);
    setPosition(value);
  }
}

void updateServerValue()
{
  String value;
  String message;
  char data[100];
  message = "{\"name\" : \"Blind 1\", \"service_name\" : \"blind_1\", \"characteristic\" : \"CurrentPosition\", \"value\" : " + String(currentPositionPercent) + "}";
  message.toCharArray(data, (message.length() + 1));
  client.publish(mqtt_device_value_to_set_topic, data);
}

void setPosition(unsigned int positionPercent)
{
  if (isInvert)
  {
    positionPercent = 100 - positionPercent;
  }
  Serial.println("set position");

  if (positionPercent != 0 || positionPercent != 100)
    targetPositionStep = (int)round((float)maxPositionStep * ((float)positionPercent / 100.0));
  else{
    if(positionPercent == 100)
      targetPositionStep = maxPositionStep;
    else
      targetPositionStep = 0;
  }
  
  Serial.print("targetPositionStep = ");
  Serial.println(targetPositionStep);
  Serial.print("positionPercent = ");
  Serial.println(positionPercent);

  stepper.stop();

  previousPositionStep = currentPositionStep;

  int stepsToGo = targetPositionStep - currentPositionStep;
  if (stepsToGo < 0)
    moveClockwise = true;
  else
    moveClockwise = false;

  Serial.print("steps to go = ");
  Serial.println(stepsToGo);
  stepper.newMove(moveClockwise, stepsToGo);
}

void callibrateMode()
{
  Serial.println("Calibrate mode");
  int distanceStep = 0;

  //Wait until button release
  while (digitalRead(btnDown) == LOW)
  {
    delay(0);
  }

  if (isInvert)
    stepper.newMove(false, 1000000);
  else
    stepper.newMove(true, 1000000);

  //Do until button pressed - Move to lowest position
  while (digitalRead(btnDown) != LOW)
  {
    stepper.run();
    delay(0);
  }
  stepper.stop();
  Serial.println("Lowest point");

  while (digitalRead(btnDown) == LOW)
  {
    delay(5);
  }

  if (isInvert)
    stepper.newMove(true, 1000000);
  else
    stepper.newMove(false, 1000000);
  // Do until button pressed - Move to highest position + record distance
  while (digitalRead(btnDown) != LOW)
  {
    stepper.run();
    delay(0);
  }
  while(digitalRead(btnDown) == LOW);
  distanceStep = 1000000 - abs(stepper.getStepsLeft());
  stepper.stop();
  Serial.print("Calibrated distance: ");
  Serial.println(distanceStep);
  EEPROM.put(300,distanceStep);
  EEPROM.put(100,0);  
  EEPROM.commit();
  delay(300);
}

void setup()
{
  Serial.begin(9600);
  stepper = CheapStepper(D5, D6, D7, D8);
  stepper.begin();
  stepper.setRpm(18);

  // Setup buttons
  pinMode(btnUp, INPUT_PULLUP);
  pinMode(btnDown, INPUT_PULLUP);

  EEPROM.begin(512);
  // //Calibration Mode
  if (digitalRead(btnDown) == LOW){
    callibrateMode();

  }
  
  // currentPositionStep = EEPROMReadlong(0);
  int savedMaxPositionStep;
  EEPROM.get(300,savedMaxPositionStep);
  EEPROM.get(100,currentPositionStep);
  delay(500);

  Serial.println("Loaded data");
  Serial.println(currentPositionStep);
  Serial.println(savedMaxPositionStep);

  maxPositionStep = (savedMaxPositionStep != 0)? savedMaxPositionStep:maxPositionStep;
  if(currentPositionStep > maxPositionStep){
    currentPositionStep = 0;
    EEPROM.put(300,0);
    EEPROM.commit();
    delay(300);
  }
  // EEPROM.end();



  // Setup networking
  WiFiManager wifiManager;
  wifiManager.autoConnect(autoconf_ssid, autoconf_pwd);
  setup_ota();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  //Attach interrupt for manual button controls
  attachInterrupt(digitalPinToInterrupt(btnUp), btnUpPressed, FALLING);
  attachInterrupt(digitalPinToInterrupt(btnDown), btnDownPressed, FALLING);
  
}

void saveStatus(){

    if (currentPositionPercent != lastValue){

    Serial.println("- Update Data - ");
    Serial.print("steps left = ");
    Serial.println(stepper.getStepsLeft());
    Serial.print("Current position = ");
    Serial.println(currentPositionStep);
    Serial.print("Current percent = ");
    Serial.println(currentPositionPercent);
    updateServerValue();
    lastValue = currentPositionPercent;

    }

    if (stepper.getStepsLeft() == 0 && lastSavedValue != currentPositionStep){
      Serial.println("saved status");
      EEPROM.put(100,currentPositionStep);
      EEPROM.commit();
      // EEPROM.end();
      delay(500);
      lastSavedValue = currentPositionStep;
    }
}

void loop()
{

  if (!client.connected())
  {
    reconnect();
  }
  client.loop();
  ArduinoOTA.handle();
  stepper.run();

  //update value
  if (stepper.getStepsLeft() != 0)
  {
    if (moveClockwise)
      currentPositionStep = abs(targetPositionStep + abs(stepper.getStepsLeft()));
    else
      currentPositionStep = abs(targetPositionStep - abs(stepper.getStepsLeft()));
  }
  // else if(abs(stepper.getStepsLeft()) <= 145){
  //   stepper.stop();
  // }

  currentPositionPercent = (int)round(((float)currentPositionStep / (float)maxPositionStep) * 100.0);
  if (isInvert)
    currentPositionPercent = 100 - currentPositionPercent;


    saveStatus();
  }