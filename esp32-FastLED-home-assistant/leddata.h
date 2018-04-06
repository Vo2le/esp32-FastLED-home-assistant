#pragma once
#include "settings.h"
#pragma once
#include "FastLED.h"


/* this variable hold queue handle */
xQueueHandle xQueue;
TaskHandle_t xTask1;
TaskHandle_t xTask2;

/*
 * Multithreading
 */
typedef struct{
  int led;
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

/*
 * Color and Transition Values
 */
typedef struct {
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
} LedState;
