#ifndef SHOWREELH
#define SHOWREELH
#include <Arduino.h>
#include <FastLED.h>
#include "fire.h"

#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

void sinelon(), rainbow(), addGlitter(fract8), rainbowWithGlitter(), confetti(), sinelon(), juggle(), bpm(), nextPattern(); // prototypes
typedef void (*fpointer)();
typedef void (*SimplePatternList[])();

extern SimplePatternList gPatterns;
extern uint8_t gCurrentPatternNumber, gHue;

#endif
