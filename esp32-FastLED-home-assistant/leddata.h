#pragma once
#include "settings.h"
#pragma once
#include "FastLED.h"


/* this variable hold queue handle */
xQueueHandle xQueue;
xQueueHandle xQueueSend;
TaskHandle_t xTask1;
TaskHandle_t xTask2;

/*
 * Multithreading
 */
struct Data {
  // type may be:
  //  1: ws281x LEDs
  //  2: relay
  //  3: binary sensor
  //  4: beat
  int type;
  int hasNumber;
  int number;
  int hasState;
  int state;
  int hasColor;
  int color_r;
  int color_g;
  int color_b;
  int hasBrightness;
  int brightness;
  int hasEffect;
  char * effect;
};

/*
 * Color and Transition Values
 */
struct LedState {
  CRGB oldColor;
  CRGB currentColor;
  CRGB newColor;
  CRGBPalette16 gPal;
  int currentBrightness;
  int newBrightness;

  char * effect;
  int colorTransition;

  int count;
  
  byte heat[STRIP_MAX_SIZE];
};
