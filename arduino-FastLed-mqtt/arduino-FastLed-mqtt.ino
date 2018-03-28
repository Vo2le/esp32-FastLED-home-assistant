/*
 * fastLED: https://github.com/FastLED/FastLED
 * ArduinoJson
 * mqtt: https://github.com/knolleary/pubsubclient
 */

#include <ArduinoJson.h>
#include <WiFi.h>
#include "PubSubClient.h"
#include "FastLED.h"


/*
 * WLAN
 */
const char* ssid     = "";
const char* password = "";
const char* mqtt_server = "";


/*
 * FastLED
 */
#define NUM_LEDS 122
#define DATA_PIN  23
#define LED_TYPE  WS2812B
//#define CLOCK_PIN 8


CRGB leds[NUM_LEDS];


/*
 * Multithreading
 */
typedef struct{
  byte *payload;
}Data;
/* this variable hold queue handle */
xQueueHandle xQueue;
TaskHandle_t xTask1;
TaskHandle_t xTask2;


/*
 * Wifi - MQTT
 */
WiFiClient espClient;
PubSubClient client(espClient);


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


void callback(char* topic, byte* payload, unsigned int length) {

  BaseType_t xStatus;
  const TickType_t xTicksToWait = pdMS_TO_TICKS(10);
  Data data;

  data.payload = (byte *)malloc(256);
  memset(data.payload, 0, 256);
  memcpy(data.payload, payload, length);
  
  /* send data to front of the queue */
  xStatus = xQueueSendToFront( xQueue, &data, xTicksToWait );
  
//  /* check whether sending is ok or not */
//  if( xStatus == pdPASS ) {
//    /* increase counter of sender 1 */
//  }
  
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
      // Once connected, publish an announcement...
      //client.publish("/woodie/ws281x/1/get", "hello world");
      // ... and resubscribe
      client.subscribe("/woodie/ws281x/1/set");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}


void setup() {
  delay(2000);
  Serial.begin(115200);
  
  /* create the queue which size can contains 5 elements of Data */
  xQueue = xQueueCreate(5, sizeof(Data));

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


void communicationTask( void * parameter )
{
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  
  /* keep the status of sending data */
  if (!client.connected()) {
    reconnect();
  }

  while(1) {
    client.loop();
  }
  
  vTaskDelete( NULL );
}




/*
 * Color and Transition Values
 */

bool state = false;

CRGB oldColor = CRGB(0, 0, 0);
CRGB currentColor = CRGB(0, 0, 0);
CRGB newColor = CRGB(0, 0, 0);
int currentBrightness = 0;
int newBrightness = 128;

char * effect = "none";
int colorTransition = 0;

uint8_t gHue = 0;

/*
 * parse received JSON and controll LED-Stripe
 */

void ledTask( void * parameter )
{

  FastLED.addLeds<LED_TYPE, DATA_PIN, GRB>(leds, NUM_LEDS).setCorrection(TypicalSMD5050);

  /* keep the status of receiving data */
  BaseType_t xStatus;
  /* time to block the task until data is available */
  const TickType_t xTicksToWait = pdMS_TO_TICKS(1);
  Data data;
  for(;;){
    /* receive data from the queue */
    xStatus = xQueueReceive( xQueue, &data, xTicksToWait );
      
    /* check whether receiving is ok or not */
    if(xStatus == pdPASS){

      StaticJsonBuffer<300> JSONBuffer;                         //Memory pool
      JsonObject& parsed = JSONBuffer.parseObject(data.payload);
      free(data.payload);

      //state
      if(parsed["state"] == "ON")
        state = true;
      
      //color
      if(parsed.containsKey("color")) {
        oldColor = currentColor;
        newColor = CRGB(parsed["color"]["r"], parsed["color"]["g"], parsed["color"]["b"]);
        colorTransition = 0;
      }
      
      //brightness
      if(parsed.containsKey("brightness")) {
        newBrightness = parsed["brightness"];
      }
      
      //effect
      if(parsed.containsKey("effect")) {
        const char * tmpEffect = parsed["effect"];
        effect = strdup(tmpEffect);
        Serial.print("Effect changed");
        Serial.println(effect);
      }
      
    }

    if(colorTransition < 255)
    {
      currentColor = blend(oldColor, newColor, colorTransition);
      colorTransition = colorTransition + 20;
    } else {
      currentColor = newColor;
    }

    if(currentBrightness < newBrightness - 5) {
      currentBrightness = currentBrightness + 5;
    } else if(currentBrightness > newBrightness + 5) {
      currentBrightness = currentBrightness - 5;
    }

    if(strcmp(effect,"sinelon") == 0) {
      fadeToBlackBy( leds, NUM_LEDS, 20);
      int pos = beatsin16( 13, 0, NUM_LEDS-1 );
      leds[pos] += currentColor;
      leds[pos] %= currentBrightness;
    } else if(strcmp(effect,"confetti") == 0) {
      fadeToBlackBy( leds, NUM_LEDS, 1);
      int pos = random16(NUM_LEDS);
      EVERY_N_MILLISECONDS( 100 ) {gHue++;}
      leds[pos] += CHSV( gHue + random8(64), 200, currentBrightness);
    } else if(strcmp(effect,"rainbow") == 0) {
      fill_rainbow( leds, NUM_LEDS, gHue, 7);
    } else {
      for (int i = 0; i < NUM_LEDS; i++) {
          leds[i] = currentColor;
          leds[i] %= currentBrightness;
      }
    }
    
    FastLED.show();
    delay(1);
    
  }

  vTaskDelete( NULL );
}