/*
 * fastLED: https://github.com/FastLED/FastLED
 * ArduinoJson
 * mqtt: https://github.com/knolleary/pubsubclient
 */

#include "thread-communication.h"
#include "thread-leds.h"
#pragma once
#include "leddata.h"

void setup() {
  delay(100);
  Serial.begin(115200);
  
  // create the queue which size can contains 5 elements of Data
  xQueue = xQueueCreate(5, sizeof(Data));
  // create the sending queue
  xQueueSend = xQueueCreate(5, sizeof(Data));

  xTaskCreatePinnedToCore(
      communicationTask,           /* Task function. */
      "communicationTask",        /* name of task. */
      10000,                    /* Stack size of task */
      NULL,                     /* parameter of the task */
      1,                        /* priority of the task */
      &xTask1,                /* Task handle to keep track of created task */
      1);                    /* pin task to core 0 */
      
  xTaskCreatePinnedToCore(
      ledTask,           /* Task function. */
      "ledTask",        /* name of task. */
      10000,                    /* Stack size of task */
      NULL,                     /* parameter of the task */
      2,                        /* priority of the task */
      &xTask2,            /* Task handle to keep track of created task */
      0);                 /* pin task to core 1 */   
}

void loop() {
}
