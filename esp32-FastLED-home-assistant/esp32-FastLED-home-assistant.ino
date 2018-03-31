/*
 * fastLED: https://github.com/FastLED/FastLED
 * ArduinoJson
 * mqtt: https://github.com/knolleary/pubsubclient
 */

#include <ArduinoJson.h>
#include <WiFi.h>
#include "PubSubClient.h"
#define FASTLED_ALLOW_INTERRUPTS 0
#include "FastLED.h"


/*
 * WLAN
 */
const char* ssid     = "";
const char* password = "";
const char* mqtt_server = "";

/*
 * MQTT
 */
const char* mqtt_server = "";
const char* mqtt_topic = "/esp32/ws281x/0/set";

/*
 * FastLED
 */
#define NUM_LEDS 122
#define DATA_PIN  25
#define LED_TYPE  WS2812B
//#define CLOCK_PIN 8

/*
 * Performance
 */
#define FRAMES_PER_SECOND 60

CRGB leds[NUM_LEDS];

/*
 * Multithreading
 */
typedef struct{
  int state;
  int hasColor;
  int color_r;
  int color_g;
  int color_b;
  int hasBrightness;
  int brightness;
  int hasEffect;
  char * effect;
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

  StaticJsonBuffer<300> JSONBuffer;                         //Memory pool
  JsonObject& parsed = JSONBuffer.parseObject(payload);

  data.state = 0;
  //state
  if(parsed["state"] == "ON"){
    data.state = 1;
  } else if(parsed["state"] == "OFF"){
    data.state = 2;
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
    data.brightness = parsed["brightness"];
    data.hasBrightness = 1;
  } else {
    data.hasBrightness = 0;
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
      client.subscribe(mqtt_topic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}


bool gReverseDirection = false;

// Fire2012 with programmable Color Palette
//
// This code is the same fire simulation as the original "Fire2012",
// but each heat cell's temperature is translated to color through a FastLED
// programmable color palette, instead of through the "HeatColor(...)" function.
//
// Four different static color palettes are provided here, plus one dynamic one.
//
// The three static ones are:
//   1. the FastLED built-in HeatColors_p -- this is the default, and it looks
//      pretty much exactly like the original Fire2012.
//
//  To use any of the other palettes below, just "uncomment" the corresponding code.
//
//   2. a gradient from black to red to yellow to white, which is
//      visually similar to the HeatColors_p, and helps to illustrate
//      what the 'heat colors' palette is actually doing,
//   3. a similar gradient, but in blue colors rather than red ones,
//      i.e. from black to blue to aqua to white, which results in
//      an "icy blue" fire effect,
//   4. a simplified three-step gradient, from black to red to white, just to show
//      that these gradients need not have four components; two or
//      three are possible, too, even if they don't look quite as nice for fire.
//
// The dynamic palette shows how you can change the basic 'hue' of the
// color palette every time through the loop, producing "rainbow fire".

CRGBPalette16 gPal;

void setup_fire2012(CRGB basecolor) {

  // This first palette is the basic 'black body radiation' colors,
  // which run from black to red to bright yellow to white.
  //gPal = HeatColors_p;

  // These are other ways to set up the color palette for the 'fire'.
  // First, a gradient from black to red to yellow to white -- similar to HeatColors_p
  //   gPal = CRGBPalette16( CRGB::Black, CRGB::Red, CRGB::Yellow, CRGB::White);

  // Second, this palette is like the heat colors, but blue/aqua instead of red/yellow
  //   gPal = CRGBPalette16( CRGB::Black, CRGB::Blue, CRGB::Aqua,  CRGB::White);

  // Third, here's a simpler, three-step gradient, from black to red to white
  gPal = CRGBPalette16( CRGB::Black, basecolor, CRGB::White);
}

void loop_fire2012()
{
  // Add entropy to random number generator; we use a lot of it.
  random16_add_entropy(esp_random());

  // Fourth, the most sophisticated: this one sets up a new palette every
  // time through the loop, based on a hue that changes every time.
  // The palette is a gradient from black, to a dark color based on the hue,
  // to a light color based on the hue, to white.
  //
  //   static uint8_t hue = 0;
  //   hue++;
  //   CRGB darkcolor  = CHSV(hue,255,192); // pure hue, three-quarters brightness
  //   CRGB lightcolor = CHSV(hue,128,255); // half 'whitened', full brightness
  //   gPal = CRGBPalette16( CRGB::Black, darkcolor, lightcolor, CRGB::White);


  Fire2012WithPalette(); // run simulation frame, using palette colors
}


// Fire2012 by Mark Kriegsman, July 2012
// as part of "Five Elements" shown here: http://youtu.be/knWiGsmgycY
////
// This basic one-dimensional 'fire' simulation works roughly as follows:
// There's a underlying array of 'heat' cells, that model the temperature
// at each point along the line.  Every cycle through the simulation,
// four steps are performed:
//  1) All cells cool down a little bit, losing heat to the air
//  2) The heat from each cell drifts 'up' and diffuses a little
//  3) Sometimes randomly new 'sparks' of heat are added at the bottom
//  4) The heat from each cell is rendered as a color into the leds array
//     The heat-to-color mapping uses a black-body radiation approximation.
//
// Temperature is in arbitrary units from 0 (cold black) to 255 (white hot).
//
// This simulation scales it self a bit depending on NUM_LEDS; it should look
// "OK" on anywhere from 20 to 100 LEDs without too much tweaking.
//
// I recommend running this simulation at anywhere from 30-100 frames per second,
// meaning an interframe delay of about 10-35 milliseconds.
//
// Looks best on a high-density LED setup (60+ pixels/meter).
//
//
// There are two main parameters you can play with to control the look and
// feel of your fire: COOLING (used in step 1 above), and SPARKING (used
// in step 3 above).
//
// COOLING: How much does the air cool as it rises?
// Less cooling = taller flames.  More cooling = shorter flames.
// Default 55, suggested range 20-100
#define COOLING  70

// SPARKING: What chance (out of 255) is there that a new spark will be lit?
// Higher chance = more roaring fire.  Lower chance = more flickery fire.
// Default 120, suggested range 50-200.
#define SPARKING 140


void Fire2012WithPalette()
{
// Array of temperature readings at each simulation cell
  static byte heat[NUM_LEDS];

  // Step 1.  Cool down every cell a little
    for( int i = 0; i < NUM_LEDS; i++) {
      heat[i] = qsub8( heat[i],  random8(0, ((COOLING * 10) / NUM_LEDS) + 2));
    }

    // Step 2.  Heat from each cell drifts 'up' and diffuses a little
    for( int k= NUM_LEDS - 1; k >= 2; k--) {
      heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2] ) / 3;
    }

    // Step 3.  Randomly ignite new 'sparks' of heat near the bottom
    if( random8() < SPARKING ) {
      int y = random8(7);
      heat[y] = qadd8( heat[y], random8(160,255) );
    }

    // Step 4.  Map from heat cells to LED colors
    for( int j = 0; j < NUM_LEDS; j++) {
      // Scale the heat value from 0-255 down to 0-240
      // for best results with color palettes.
      byte colorindex = scale8( heat[j], 240);
      CRGB color = ColorFromPalette( gPal, colorindex);
      int pixelnumber;
      if( gReverseDirection ) {
        pixelnumber = (NUM_LEDS-1) - j;
      } else {
        pixelnumber = j;
      }
      leds[pixelnumber] = color;
    }
}


void setup() {
  delay(100);
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
CRGB newColor = CRGB(255, 255, 255);
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
  const TickType_t xTicksToWait = pdMS_TO_TICKS(0);
  Data data;
  for(;;){
    /* receive data from the queue */
    xStatus = xQueueReceive( xQueue, &data, xTicksToWait );
      
    /* check whether receiving is ok or not */
    if(xStatus == pdPASS){

      //effect
      if(data.hasEffect == 1) {
        effect = data.effect;
        Serial.print("Effect changed");
        Serial.println(effect);

        if(strcmp(effect, "fire") == 0){
          setup_fire2012(CRGB::Red);
        }
      }
      
      //color
      if(data.hasColor == 1) {
        oldColor = currentColor;
        newColor = CRGB(data.color_r, data.color_g, data.color_b);
        colorTransition = 0;

        if(strcmp(effect, "fire") == 0){
          setup_fire2012(newColor);
        }
      }
      
      //brightness
      if(data.hasBrightness == 1) {
        newBrightness = data.brightness;
      }
    }

    if(colorTransition < 255)
    {
      currentColor = blend(oldColor, newColor, colorTransition);
      colorTransition = colorTransition + 2;
    } else {
      currentColor = newColor;
    }

    if(currentBrightness < newBrightness - 5) {
      currentBrightness = currentBrightness + 5;
    } else if(currentBrightness > newBrightness + 5) {
      currentBrightness = currentBrightness - 5;
    }

    if(data.state == 2) {
      // turn it out
      fadeToBlackBy(leds, NUM_LEDS, 20);
    } else if(strcmp(effect,"sinelon") == 0){
      fadeToBlackBy( leds, NUM_LEDS, 20);
      int pos = beatsin16( 13, 0, NUM_LEDS-1 );
      leds[pos] += currentColor;
      leds[pos] %= currentBrightness;
    } else if(strcmp(effect,"confetti") == 0) {
      fadeToBlackBy( leds, NUM_LEDS, 1);
      int pos = random16(NUM_LEDS);
      leds[pos] += CHSV( gHue + random8(64), 200, currentBrightness);
    } else if(strcmp(effect,"rainbow") == 0) {
      fill_rainbow( leds, NUM_LEDS, gHue, 7);  
    } else if(strcmp(effect,"fire") == 0) {
      random16_add_entropy(esp_random());
      loop_fire2012();
    } else {
      for (int i = 0; i < NUM_LEDS; i++) {
          leds[i] = currentColor;
          leds[i] %= currentBrightness;
      }
    }
    
    EVERY_N_MILLISECONDS( 100 ) {gHue++;}
    FastLED.show();
    FastLED.delay(1000 / FRAMES_PER_SECOND);
  }

  vTaskDelete( NULL );
}
