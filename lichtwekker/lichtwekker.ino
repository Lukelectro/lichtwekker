// lichtwekker. Uses Digitalread etc. even though slower. Excuse is to easily port to other 'duino's or change pinout. (It's not lazyness! PIND&=1<<7 is actually shorter!)
// also re-uses FastLED demo reel.
const unsigned int TIMEOUT = 30000; // time-out for time display, now in millis
const unsigned int EGGOUT = 7000; // time-out for showreel/pong entry, now in millis

#include <FastLED.h>
FASTLED_USING_NAMESPACE

#include <MD_DS3231.h>
#include <Wire.h>

#include "onedpong.h"
#include "fire.h"
#include "showreel.h"


#define SW_TOP 3
#define SW1 2
#define SW2 6

#define CW_LEDS 9
#define WW_LEDS 8
#define DATA_PIN    7
#define LED_TYPE    WS2811
#define COLOR_ORDER GRB
#define NUM_LEDS    60 /* time display won' t work unless you are using at least 60 led's */
CRGB leds[NUM_LEDS];

#define BRIGHTNESS 128 // set max brightness to limit power consumption.

typedef struct
{
  unsigned int h;
  unsigned int m;
  unsigned int s;
} hms;

hms AlarmTime, SetTime, currenttime;
CRGB indicator = CRGB::Black;

fpointer Show = sinelon; // Set this pointer to what function should be called just before a refresh in tick();

enum {SHOWTIME, SHOWTIME2, SETTIME, SETAL, REST1, REST2, SHOWREEL, SWAKE, EASTERPONG};
enum LSTATE {OFF, WW, CW, CWW, TIME, RST, LWAKE};

uint8_t light = OFF, state = SHOWTIME;

unsigned int waking = 0;

bool alset = true; // alarm set or not?

void setup() {
  delay(3000); // 3 second delay for recovery

  AlarmTime.h = 6;
  AlarmTime.m = 30;
  AlarmTime.s = 1;

  pinMode(SW1, INPUT_PULLUP);
  pinMode(SW2, INPUT_PULLUP);
  pinMode(SW_TOP, INPUT_PULLUP);
  pinMode(SDA, INPUT_PULLUP);
  pinMode(SCL, INPUT_PULLUP);
  pinMode(CW_LEDS, OUTPUT);
  pinMode(WW_LEDS, OUTPUT);

  // init RTC to 24 hour clock. Do not set time here, it is batery backed
  RTC.control(DS3231_12H, DS3231_OFF);  // 24 hour clock

  Pongsetup();

  // tell FastLED about the LED strip configuration
  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  // set master brightness control
  FastLED.setBrightness(BRIGHTNESS);

// Destroy Arduino's hidden timer config (Standard it is configured for PWM which I don't use, but I need a 5 Hz interrupt for screen refresh and previously for timekeeping)
cli();
TCCR1A=0; // 0, and not 1 (WGM10/8bit PWM, the standard config that gives trouble here.)
TCNT1=0; // clear timer (to prevent Arduino restoring its unwanted PWM config on timer overflow)
TCCR1B=0;
TCCR1C=0;

OCR1A = 0x0C34; // 16Mhz / (1024 * 3125) = 5 Hz (0x0C34 = 3124, because it counts from 0-3124 like prescaler counts from 0-1023)
TCCR1B = 0x0D; // clk/1024, CTC mode
TIMSK1 = 0x02; // enable OC1A interrupt
sei();
}

SIGNAL(TIMER1_COMPA_vect){
  // Will be called at 5Hz
  
  gHue++;    //for various visual effects
  
  RTC.readTime();
  currenttime.h = RTC.h;
  currenttime.m = RTC.m;
  currenttime.s = RTC.s;

  if ( alset && AlarmTime.h == currenttime.h && AlarmTime.m == currenttime.m && AlarmTime.s == currenttime.s ) {
    waking = 0; // reset wake animation
    state = SWAKE;
  };

  if (Show != NULL) { // so it can be set to NULL to disable auto-refresh
    Show();
    FastLED.show();
  }

}
void loop()
{

  static unsigned int egg = 0, timeoutmills, eggoutmills;
  static bool autoreel = true;

  if ( (millis() - eggoutmills) > EGGOUT) {
    eggoutmills = millis();
    egg = 0;
  }

  switch (state) {
    case REST1:
      Show = NULL;
      fill_solid( leds, NUM_LEDS, CRGB::Black);
      FastLED.show();
      state = REST2;
      break;
    case REST2:
      break;
    case SHOWTIME:
      timeoutmills = millis();
      Show = shownow;
      state = SHOWTIME2;
      break;
    case SHOWTIME2:

      if (alset) indicator = CRGB::DarkGoldenrod; else indicator = CRGB::Black;

      //automatically go dark after e.g. 30s but only if light is OFF
      //(there is a way to use the time display as a light)
      if (((millis() - timeoutmills) > TIMEOUT ) && light == OFF ) {
        state = REST1;
      }

      break;
    case SETTIME:
      indicator = CRGB::LightGoldenrodYellow;
      Show = shownow;
      currenttime = AdjustTime(currenttime);
      RTC.h=currenttime.h;
      RTC.m=currenttime.m;
      RTC.s=currenttime.s;
      RTC.writeTime();
      state = SHOWTIME;
      break;
    case SETAL:
      indicator = CRGB::OliveDrab;
      Show = showAl;
      alset = !alset; // alarm on/off
      AlarmTime = AdjustTime(AlarmTime);
      state = SHOWTIME;
      break;
    case SHOWREEL:
      // todo: show fastled showreel / use buttons to choose which effect or auto-rotate
      //Show=sinelon;
      Show = gPatterns[gCurrentPatternNumber];

      // might want to call fastled.show() more often for smoother display... but below is not the right way
      /*
        Show = nothing;// diable automatic 5s refresh
        gPatterns[gCurrentPatternNumber]; // call the patern
        delay(1000/30);// 30 fps / do not starve timekeeping timer interrupt
        FastLED.show();
      */


      if (autoreel) {
        EVERY_N_SECONDS( 10 ) nextPattern();
      }; // change patterns periodically

      break;
    case SWAKE:
      //light=LWAKE; // otherwise it turns off right again.
      Show = WakeAnim; // more sophisticated animation before turning lights on
      break;
    case EASTERPONG:
      Pongloop();//  play pong
      break;
    default:
      state = SHOWTIME;
  }

  switch (light) {
    case OFF:
      digitalWrite(WW_LEDS, LOW);
      digitalWrite(CW_LEDS, LOW);
      break;
    case WW:
      digitalWrite(WW_LEDS, HIGH);
      digitalWrite(CW_LEDS, LOW);
      break;
    case CW:
      digitalWrite(WW_LEDS, LOW);
      digitalWrite(CW_LEDS, HIGH);
      break;
    case CWW:
      digitalWrite(WW_LEDS, HIGH);
      digitalWrite(CW_LEDS, HIGH);
      break;
    case TIME:
      digitalWrite(WW_LEDS, LOW);
      digitalWrite(CW_LEDS, LOW);
      break;
    case RST:
      state = REST1;
      light = OFF;
      break;
    case LWAKE:
      digitalWrite(WW_LEDS, HIGH);
      break;
    default:
      light = OFF;
  }



  if (digitalRead(SW1) == 0) {
    while (digitalRead(SW1) == 0) delay(100); // wait for release
    switch (state) {
      case SWAKE:
        light = OFF;
      case SHOWREEL:
      case EASTERPONG:
        egg = 0;
        state = SHOWTIME;
        break;
      default:
        state = SETTIME;
    }
  }
  if (state != EASTERPONG) { // otherwise Pong cannot read the switches it needs

    if (digitalRead(SW2) == 0) {
      while (digitalRead(SW2) == 0) delay(100); // wait for release
      switch (state) {
        case SHOWREEL:
          nextPattern();
          autoreel = false;
          break;
        case SWAKE:
          egg = 0;
          light = OFF;
          state = SHOWTIME;
          break;
        default:
          state = SETAL;
      }
    }


    if (digitalRead(SW_TOP) == 0) {
      while (digitalRead(SW_TOP) == 0) { // wait for release
        delay(100); //debounce
      }
      switch (state) {
        case REST2:
          state = SHOWTIME;
          if (light != OFF) light++;
          break;
        case SHOWTIME2:
          light++;
          egg++;
          if (egg > 9) {
            light = OFF;
            autoreel = true;
            state = SHOWREEL;
          }
          break;
        case SHOWREEL:
          egg++;
          if (egg > 13) {
            Show = NULL;
            fill_solid( leds, NUM_LEDS, CRGB::Black);
            FastLED.show();
            light = OFF;
            state = EASTERPONG;
          }
          break;
        case SWAKE:
          egg = 0;
          light = OFF;
          state = SHOWTIME;
          break;
        default:
          break;
      }
    }
  }
}


void showtime(hms TTS) { // TTS = Time To Show
  fill_solid( leds, NUM_LEDS, CRGB::Black);
  leds[TTS.m] += CRGB::DarkRed;
  leds[NUM_LEDS - TTS.h] += CRGB::Green;
       leds[TTS.s] += CRGB::DarkRed;
       leds[0] += indicator;
       leds[NUM_LEDS - 1] += indicator;

  for (uint8_t i = 5; i < NUM_LEDS - 1; i += 5) { // scale / graticule, also indicator for "display auto-off"
    if (light == TIME) leds[i] += CRGB(0, 0, 10); else leds[i] += CRGB(0, 7, 8);
  };

}

void shownow() { // bit of a wraparound, because Show(); does not take arguments.
  showtime(currenttime);
}

void showAl() {
  showtime(AlarmTime);
}

void showAdj() {
  showtime(SetTime);
}

hms AdjustTime(hms startval) { //starts from startval and returns adjusted time, shows it on ledstrip while adjusting

  Show = showAdj;

  while ( digitalRead(SW1) == 0 || digitalRead(SW2) == 0 ) delay(150);

  while (digitalRead(SW2) != 0) {
    if (digitalRead(SW1) == 0) {
      if (startval.h < 24) startval.h++; else startval.h = 0;
      delay(400);
    }
    if (digitalRead(SW_TOP) == 0) {
      if (startval.h > 0) startval.h--; else startval.h = 23;
      delay(400);
    }
    
  SetTime = startval; /* copy so it can be displayed */
  }

  while (digitalRead(SW2) == 0) delay(150);

  while (digitalRead(SW2) != 0) {
    if (digitalRead(SW1) == 0) {
      if (startval.m < 60) startval.m++; else startval.m = 0;
      delay(400);
    }
    if (digitalRead(SW_TOP) == 0) {
      if (startval.m > 0) startval.m--; else startval.m = 59;
      delay(400);
    }
    
  SetTime = startval; /* copy so it can be displayed */
  }

  while (digitalRead(SW2) == 0) delay(150);

  while (digitalRead(SW2) != 0) {
    if (digitalRead(SW1) == 0) {
      if (startval.s < 60) startval.s++; else startval.s = 0;
      delay(400);
    }
    if (digitalRead(SW_TOP) == 0) {
      if (startval.s > 0) startval.s--; else startval.s = 59;
      delay(400);
    }
  
  SetTime = startval; /* copy so it can be displayed */
  }

  while (digitalRead(SW2) == 0) delay(150);

  indicator = CRGB::Black; // Whoa. Then how to indicate that alarm is set?
  //if(AlarmSet) indicator = CRGB::Red; else indicator=CRGB::Black // something like that?
  //AlarmSet?indicator:CRGB::Red:CRGB::Black; // unreadable... But shorter
  return startval; /* after modification */
}

void WakeAnim() {
  // wake- up animation...
  // todo: improve
  // idea: fade in red leds from bottom to top slowly, and as last step, turn on WW ledstrip.
  const uint8_t STEPS = 7; // number of fade-in steps per LED. 5 steps per second (refresh at 5 Hz), so between 3 and 15 are reasonable values? default 5.
  const uint8_t BRADD = 255 / STEPS; // how much brightness is added per step?

  if (waking == 0) fill_solid( leds, NUM_LEDS, CRGB::Black);
  if (waking <= (NUM_LEDS * STEPS * 2)) waking++;

  // first dim up RED one by one, then green one by one (resulting in yellow-ish), then turn on the WW ledstrip.
  if (waking < NUM_LEDS * STEPS) {
    leds[(waking / STEPS)] += CHSV(HUE_RED, 255, BRADD); // todo: nicer lineair dimming/brightening?
  }
  else
  {
    leds[(waking / STEPS) - NUM_LEDS] += CHSV(HUE_GREEN, 255, BRADD); // todo: nicer lineair dimming/brightening?
  }


  if (waking >= NUM_LEDS * STEPS * 2) { // once the ws28 strip is lit
    light = LWAKE;
    //Show=rainbowWithGlitter; // after wake-up animation, go rainbow. why not?
    // because that won't work
  }
}
