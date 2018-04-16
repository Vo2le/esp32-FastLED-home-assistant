#include <ArduinoJson.h>
#include <WiFi.h>
#include "PubSubClient.h"
#pragma once
#include "leddata.h"
#include <ArduinoOTA.h>

/*
 * Wifi - MQTT
 */
WiFiClient espClient;
PubSubClient client(espClient);

void callback(char* topic, byte* payload, unsigned int length) {

  Serial.println("[Comm-Thread] Parsing package...");
  Serial.print("[Comm-Thread] ");
  Serial.print(topic);
  Serial.print(": ");

  BaseType_t xStatus;
  const TickType_t xTicksToWait = pdMS_TO_TICKS(10);
  Data data;

  StaticJsonBuffer<300> JSONBuffer;                         //Memory pool
  JsonObject& parsed = JSONBuffer.parseObject(payload);
  parsed.printTo(Serial);
  Serial.println("");

  if (!parsed.success()) {
    Serial.println("[Comm-Thread] Parsing failed!!!");
    return;
  }

  //char string[strlen(topic)];
  //strcpy(string, topic);
  char delimiter[] = "/";
  char *ptr;

  data.hasNumber = 1;
  strtok(topic, delimiter);
  strtok(NULL, delimiter);
  ptr = strtok(NULL, delimiter);
  Serial.print("LED found: ");
  Serial.println(ptr);
  data.number = atoi(ptr);
  Serial.print("As int: ");
  Serial.println(data.number);

  //state
  if(parsed.containsKey("state")){
    data.hasState = 1;
    if(parsed["state"] == "ON"){
      data.state = 1;
    } else {
      // OFF
      data.state = 0;
    }
  } else {
    data.hasState = 0;
  }
  
  //color
  if(parsed.containsKey("color")) {
    data.color_r = parsed["color"]["r"];
    data.color_g = parsed["color"]["g"];
    data.color_b = parsed["color"]["b"];
    
    data.hasColor = 1;
  } else {
    data.hasColor = 0;
  }
  
  //brightness
  if(parsed.containsKey("brightness")) {
    Serial.print("[Comm-Thread] New brightness: ");
    data.brightness = parsed["brightness"];
    Serial.println(data.brightness);
    data.hasBrightness = 1;
  } else {
    data.hasBrightness = 0;
  }

  //beat
  if(parsed.containsKey("beat")) {
    Serial.println("[Comm-Thread] Beat detected");
    data.hasBeat = 1;
  } else {
    data.hasBeat = 0;
  }
  
  //effect
  if(parsed.containsKey("effect")) {
    const char * tmpEffect = parsed["effect"];
    char * effect = strdup(tmpEffect);
    data.effect = (char *)malloc(32);
    memset(data.effect, 0, 32);
    memcpy(data.effect, effect, strlen(effect));    
    data.hasEffect = 1;
  } else {
    data.hasEffect = 0;
  }
  
  /* send data to front of the queue */
  xStatus = xQueueSendToFront( xQueue, &data, xTicksToWait );
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      // ... and resubscribe
      for(int i=0; i < STRIP_COUNT + 1; i++){
        client.subscribe(mqtt_topics[i]);
        Serial.print("Subscribed to mqtt topic: ");
        Serial.println(mqtt_topics[i]);
      }
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void loopSendStates(){
  BaseType_t xStatus;
  const TickType_t xTicksToWait = pdMS_TO_TICKS(10);
  
  Data data;
  
  // receive data from the queue
  xStatus = xQueueReceive(xQueueSend, &data, xTicksToWait);
    
  // check whether receiving is ok or not 
  if(xStatus == pdPASS){
    StaticJsonBuffer<300> jsonBuffer;

    JsonObject& root = jsonBuffer.createObject();
    if(data.hasState == 1){
      if(data.state == 1){
        root["state"] = "ON";
      } else {
        // data.state == 0
        root["state"] = "OFF";
      }
    }
    if(data.hasColor == 1){
      JsonObject& colorObject = root.createNestedObject("color");
      colorObject["r"] = data.color_r;
      colorObject["g"] = data.color_g;
      colorObject["b"] = data.color_b;
    }
    if(data.hasBrightness == 1){
      root["brightness"] = data.brightness;
    }
    if(data.hasEffect == 1){
      root["effect"] = data.effect;
    }

    char mqtt_topic[200] = "";
    strcat(mqtt_topic,"/");
    strcat(mqtt_topic, hostname);
    strcat(mqtt_topic, "/");

    if(data.type == 1){
      strcat(mqtt_topic, "ws281x/"); 
    } else if(data.type == 2){
      strcat(mqtt_topic, "switch/");
    } else if(data.type == 2){
      strcat(mqtt_topic, "binary_sensor/");
    }
    if(data.hasNumber == 1){
      char str[16];
      itoa(data.number, str, 10);
      strcat(mqtt_topic, str);
      strcat(mqtt_topic, "/");
    }
    strcat(mqtt_topic, "get");
    
    char buffer[200];
    root.printTo(buffer);
    Serial.print("[Comm-Thread] Sending to ");
    Serial.print(mqtt_topic);
    Serial.print(": ");
    Serial.println(buffer);
    client.publish(mqtt_topic, buffer);
  }
}

void communicationTask(void * parameter)
{
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

 // OTA Update Stuff
  ArduinoOTA.setHostname(hostname);
  ArduinoOTA.setPassword(ota_password);
 
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";
 
    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  
  /* keep the status of sending data */
  if (!client.connected()) {
    reconnect();
  }

  while(1) {
    client.loop();
    ArduinoOTA.handle();
    loopSendStates();
  }
  
  vTaskDelete( NULL );
}
