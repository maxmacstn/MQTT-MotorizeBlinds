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
Add to mqtt API
Topic : homebridge/from/set
Payload: {"name" : "Blind 1" ,  "service_name" : "blind_1" , "characteristic" : "TargetPosition" , "value" : 50}

*/

const int stepsPerRev = 200;
CheapStepper stepper;

// Constants
const char *autoconf_ssid = "ESP8266 Blinds"; //AP name for WiFi setup AP which your ESP will open when not able to connect to other WiFi
const char *autoconf_pwd = "12345678";        //AP password so noone else can connect to the ESP in case your router fails
const char *mqtt_server = "192.168.1.15";     //MQTT Server IP, your home MQTT server eg Mosquitto on RPi, or some public MQTT
const int mqtt_port = 1883;                   //MQTT Server PORT, default is 1883 but can be anything.
const bool isInvert = true;
const int btnUp = D6;
const int btnDown = 13;
const int upRPM = 18;                  //Default
// const int upRPM = 13;                     //Big blinds
const int downRPM = 25;
const int stepper_1 = D1;
const int stepper_2 = D2;
const int stepper_3 = D3;
const int stepper_4 = D4;
const int onboard_led = 1;


// MQTT Constants
const char *mqtt_device_value_from_set_topic = "homebridge/from/set";
const char *mqtt_device_value_to_set_topic = "homebridge/to/set";
String device_name = "Blind 3";
String service_name = "blind_3";


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
  digitalWrite(onboard_led, LOW);
  delay(20);
  digitalWrite(onboard_led, LOW);
}



void stopMoving()
{
  targetPositionStep = currentPositionStep;

  stepper.stop();
  String value;
  String message;
  char data[100];
  message = "{\"name\" : \""+ device_name +"\", \"service_name\" : \""+ service_name+"\", \"characteristic\" : \"TargetPosition\", \"value\" : " + String(currentPositionPercent) + "}";
  message.toCharArray(data, (message.length() + 1));
  client.publish(mqtt_device_value_to_set_topic, data);
  saveStatus();
}

void btnUpPressed()
{
  while (digitalRead(btnUp) == LOW){
    delay(0);
  }
  if (stepper.getStepsLeft() != 0)
  {
    stopMoving();
    return;
  }

  setPosition(100);
  String value;
  String message;
  char data[100];
  message = "{\"name\" : \"" +device_name+ "\", \"service_name\" : \""+service_name+"\", \"characteristic\" : \"TargetPosition\", \"value\" : " + String(100) + "}";
  message.toCharArray(data, (message.length() + 1));
  client.publish(mqtt_device_value_to_set_topic, data);
}

void btnDownPressed()
{
  while (digitalRead(btnDown) == LOW){
    delay(0);
  }
  if (stepper.getStepsLeft() != 0)
  {
    stopMoving();
    return;
  }

  setPosition(0);
  String value;
  String message;
  char data[100];
  message = "{\"name\" : \"" +device_name+ "\", \"service_name\" : \""+service_name+"\", \"characteristic\" : \"TargetPosition\", \"value\" : " + String(0) + "}";
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
  if (strcmp(name, device_name.c_str()) != 0)
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
  message = "{\"name\" : \"" +device_name+ "\", \"service_name\" : \""+service_name+"\", \"characteristic\" : \"CurrentPosition\", \"value\" : " + String(currentPositionPercent) + "}";
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

  // Move Blinds down, it can go faster than up.
  if (isInvert && !moveClockwise){
    stepper.setRpm(downRPM);
    Serial.println("DN");
  }else if (!isInvert && moveClockwise){
    stepper.setRpm(downRPM);
    Serial.println("DN");
  }
  else{
    Serial.println("UP");
    stepper.setRpm(upRPM);
  }
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
  while(digitalRead(btnDown) == LOW){
    delay(0);
  }
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
  // Serial.begin(9600);
  stepper = CheapStepper(stepper_1, stepper_2, stepper_3, stepper_4);
  stepper.begin();
  stepper.setRpm(upRPM);

  // Setup buttons
  pinMode(btnUp, INPUT_PULLUP);
  pinMode(btnDown, INPUT_PULLUP);

  EEPROM.begin(512);

  // //Calibration Mode
  if (digitalRead(btnDown) == LOW){
    callibrateMode();

  }
  
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



  // Setup networking
  WiFiManager wifiManager;
  wifiManager.autoConnect(autoconf_ssid, autoconf_pwd);

  // setup_ota();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  //Attach interrupt for manual button controls
  attachInterrupt(digitalPinToInterrupt(btnUp), btnUpPressed, FALLING);
  attachInterrupt(digitalPinToInterrupt(btnDown), btnDownPressed, FALLING);

  //Turn off led
  pinMode(onboard_led, OUTPUT);
  digitalWrite(onboard_led, HIGH);
  
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

  currentPositionPercent = (int)round(((float)currentPositionStep / (float)maxPositionStep) * 100.0);
  if (isInvert)
    currentPositionPercent = 100 - currentPositionPercent;


    saveStatus();
  }