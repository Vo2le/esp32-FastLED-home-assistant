#pragma once
#include "FastLED.h"
#include "fire2012.h"
#pragma once
#include "leddata.h"

CRGB leds_0[NUM_LEDS_0];
CRGB leds_1[NUM_LEDS_1];

uint8_t gHue = 0;

/*
 * parse received JSON and controll LED-Stripe
 */

void ledTask( void * parameter )
{
  
  LedState led_status[STRIP_COUNT];
  for(int i=0; i<STRIP_COUNT; i++){
    led_status[i].oldColor = CRGB(0, 0, 0);
    led_status[i].currentColor = CRGB(0, 0, 0);
    led_status[i].newColor = CRGB(255, 255, 255);
    led_status[i].currentBrightness = 0;
    led_status[i].newBrightness = 128;
    led_status[i].effect = "none";
    led_status[i].colorTransition = 0;
    led_status[i].count = 0;
  }

  gHue = 0;
  
  FastLED.addLeds<LED_TYPE_0, DATA_PIN_0, GRB>(leds_0, NUM_LEDS_0).setCorrection(TypicalSMD5050);
  led_status[0].count = NUM_LEDS_0;
  if(STRIP_COUNT > 1){
    FastLED.addLeds<LED_TYPE_1, DATA_PIN_1, GRB>(leds_1, NUM_LEDS_1).setCorrection(TypicalSMD5050);
    led_status[1].count = NUM_LEDS_1;
  }

  

  // keep the status of receiving data
  BaseType_t xStatus;
  // time to block the task until data is available
  const TickType_t xTicksToWait = pdMS_TO_TICKS(0);
  Data data;
  // determinate the current led
  LedState *strip_state;
  CRGB *leds;

  
  for(;;){
    // receive data from the queue
    xStatus = xQueueReceive( xQueue, &data, xTicksToWait );
      
    // check whether receiving is ok or not 
    if(xStatus == pdPASS){
      // check if led is in range
      if(data.led >= STRIP_COUNT){
        // led strip not defined
        Serial.print("Strip ");
        Serial.print(data.led);
        Serial.print(" addressed, but only ");
        Serial.print(STRIP_COUNT);
        Serial.print(" strips are defined!");
        continue;
      } else {
        strip_state = &led_status[data.led];
        if(data.led == 0)
          leds = leds_0;
        else if(data.led == 1)
          leds = leds_1;
      }


      //effect
      if(data.hasEffect == 1) {
        strip_state->effect = data.effect;
        Serial.print("Effect changed to: ");
        Serial.println(strip_state->effect);

        if(strcmp(strip_state->effect, "fire") == 0){
          strip_state->gPal = CRGBPalette16(CRGB::Black, CRGB::Red, CRGB::White);
        }
      }
      
      //color
      if(data.hasColor == 1) {
        strip_state->oldColor = strip_state->currentColor;
        strip_state->newColor = CRGB(data.color_r, data.color_g, data.color_b);
        strip_state->colorTransition = 0;
        Serial.print("Got new color: (R=");
        Serial.print(data.color_r);
        Serial.print("; G=");
        Serial.print(data.color_g);
        Serial.print("; B=");
        Serial.print(data.color_b);
        Serial.println(")");

        if(strcmp(strip_state->effect, "fire") == 0){
          strip_state->gPal = CRGBPalette16(CRGB::Black, strip_state->newColor, CRGB::White);
        }
      }
      
      //brightness
      if(data.hasBrightness == 1) {
        strip_state->newBrightness = data.brightness;
      }
    }

    for(int s=0; s<STRIP_COUNT; s++){
      strip_state = &led_status[s];
      if(s == 0)
        leds = leds_0;
      else if(s == 1)
        leds = leds_1;
          
      if(strip_state->colorTransition < 255){
        strip_state->currentColor = blend(strip_state->oldColor, strip_state->newColor, strip_state->colorTransition);
        strip_state->colorTransition = strip_state->colorTransition + 2;
      } else {
        strip_state->currentColor = strip_state->newColor;
      }   
      if(strip_state->currentBrightness < strip_state->newBrightness - 5) {
        strip_state->currentBrightness += 5;
      } else if(strip_state->currentBrightness > strip_state->newBrightness + 5) {
        strip_state->currentBrightness -= 5;
      }
      if(data.state == 2) {
        // turn it out
        fadeToBlackBy(leds, strip_state->count, 20);
      } else if(strcmp(strip_state->effect,"sinelon") == 0){
        fadeToBlackBy(leds, strip_state->count, 20);
        int pos = beatsin16(13, 0, strip_state->count-1 );
        *(leds+pos) += strip_state->currentColor;
        *(leds+pos) %= strip_state->currentBrightness;
      } else if(strcmp(strip_state->effect,"confetti") == 0) {
        fadeToBlackBy(leds, strip_state->count, 1);
        int pos = random16(strip_state->count);
        *(leds+pos) += CHSV(gHue + random8(64), 200, strip_state->currentBrightness);
      } else if(strcmp(strip_state->effect,"rainbow") == 0) {
        fill_rainbow(leds, strip_state->count, gHue, 7);
      } else if(strcmp(strip_state->effect, "fire") == 0) {
        random16_add_entropy(esp_random());
        Fire2012WithPalette(strip_state, leds);
      } else {
        for (int i = 0; i < strip_state->count; i++) {
            *(leds+i) = strip_state->currentColor;
            *(leds+i) %= strip_state->currentBrightness;
        }
      }
    }
  
    EVERY_N_MILLISECONDS(100){gHue++;}
    
    FastLED.show();
    FastLED.delay(1000 / FRAMES_PER_SECOND);
  }

  vTaskDelete( NULL );
}
