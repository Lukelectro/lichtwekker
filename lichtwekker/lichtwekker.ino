/*
   lichtwekker. Uses Digitalread etc. even though slower. Excuse is to easily port to other 'duino's or change pinout. (It's not lazyness! PIND&=1<<7 is actually shorter!)
   also re-uses FastLED demo reel and fire2012
   uses DS3231MZ, do note: with battery backup time is kept on power failure, but wake-up time is reset to default 6:30 on bootup!
*/

const unsigned char TIMEOUT = 25 * 5;  // time-out for time display, in 1/5s , max 255 (=51s)
const unsigned char EGGOUT = 8 * 5;    // time-out for showreel/pong entry, in 1/5s, max 255 (=51s)
const unsigned int DEBOUNCE = 150;     // these buttons bounce horribly long

#include <FastLED.h>  // use version 3.2.6
FASTLED_USING_NAMESPACE

#include <MD_DS3231.h>
#include <Wire.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <util/crc16.h>

#include "onedpong.h"
#include "fire.h"
#include "showreel.h"

/* when modifying switch or led data pins, also modify them in onedpong.h */
#define SW_TOP 3
#define SW1 2
#define SW2 6
#define DATA_PIN 7
#define CW_LEDS 9
#define WW_LEDS 8
#define LED_TYPE WS2811
#define COLOR_ORDER GRB
#define NUM_LEDS 60  // time display won' t work unless you are using at least 60 led's
CRGB leds[NUM_LEDS];

#define BRIGHTNESS 128  // set max brightness to limit power consumption.

typedef struct
{
  unsigned int h;
  unsigned int m;
  unsigned int s;
} hms;

hms AlarmTime, SetTime, currenttime;
CRGB indicator = CRGB::Black;

fpointer Show = NULL;  // Set this pointer to what function should be called just before a refresh in tick();

enum RGBSTATE { SHOWTIME,
                SHOWTIME2,
                SETTIME,
                SETAL,
                REST1,
                REST2,
                SHOWREEL,
                SWAKE,
                EASTERPONG } state;
enum WSTATE { OFF,
              WW,
              CW,
              CWW,
              TIME,
              RST,
              LWAKE } light;
#define NEXTLIGHT \
  do { (light < RST) ? light = light + 1 : light = OFF; } while (0); /* Go to next light LSTATE, wrap to first LSTATE*/


unsigned int waking = 0;
volatile unsigned char tick5Hz_8bit;  // because millis() is b0rked

bool alset = true;     // alarm set or not?
bool readRTC = false;  // to indicate RTC needs to get read again

void setup() {
  delay(3000);  // 3 second delay for recovery

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

  // Destroy Arduino's timer config (Standard it is configured for PWM which I don't use, but I need a 5 Hz interrupt for screen refresh and previously for timekeeping)
  cli();
  TCCR1A = 0;  // 0, and not 1 (WGM10/8bit PWM, the standard config that gives trouble here.)
  TCNT1 = 0;   // clear timer (to prevent Arduino restoring its unwanted PWM config on timer overflow)
  TCCR1B = 0;
  TCCR1C = 0;

  OCR1A = 0x0C34;  // 16Mhz / (1024 * 3125) = 5 Hz (0x0C34 = 3124, because it counts from 0-3124 like prescaler counts from 0-1023)
  TCCR1B = 0x0D;   // clk/1024, CTC mode

  TIMSK1 = 0x02;  // enable OC1A interrupt
  sei();
}

ISR(TIMER1_COMPA_vect) {
  // Will be called at 5Hz

  gHue++;  //for various visual effects
  tick5Hz_8bit++;

  readRTC = true; /* set a flag and let main read RTC once */

  if (Show != NULL) {  // so it can be set to NULL to disable auto-refresh
    Show();
    FastLED.show();
  }
}

void loop() {

  static unsigned int egg = 0;
  static unsigned char timeout, eggout, brightness_wake, pwm_cnt;
  static bool autoreel = true;

  if ((unsigned char)(tick5Hz_8bit - eggout) > EGGOUT) {
    eggout = tick5Hz_8bit;
    egg = 0;
  }

  if (readRTC) { /* flag set in interrupt */
    RTC.readTime();
    currenttime.h = RTC.h;
    currenttime.m = RTC.m;
    currenttime.s = RTC.s;
    if (alset && AlarmTime.h == currenttime.h && AlarmTime.m == currenttime.m && AlarmTime.s == currenttime.s) {
      state = SWAKE;
    }
    readRTC = false;
  }

  switch (state) {
    case REST1:
      Show = NULL;
      fill_solid(leds, NUM_LEDS, CRGB::Black);
      FastLED.show();
      state = REST2;
      break;
    case REST2:
      break;
    case SHOWTIME:
      timeout = tick5Hz_8bit;
      Show = shownow;
      state = SHOWTIME2;
      break;
    case SHOWTIME2:

      if (alset) indicator = CRGB::DarkGoldenrod;
      else indicator = CRGB::Black;

      //automatically go dark after e.g. 25s but only if light is OFF
      //(there is a way to use the time display as a light)
      if ((light == OFF) && ((unsigned char)(tick5Hz_8bit - timeout) >= TIMEOUT)) {
        state = REST1;
      }

      break;
    case SETTIME:
      indicator = CRGB::LightGoldenrodYellow;
      Show = shownow;
      currenttime = AdjustTime(currenttime);
      RTC.h = currenttime.h;
      RTC.m = currenttime.m;
      RTC.s = currenttime.s;
      RTC.writeTime();
      state = SHOWTIME;
      break;
    case SETAL:
      indicator = CRGB::OliveDrab;
      Show = showAl;
      alset = !alset;  // alarm on/off
      AlarmTime = AdjustTime(AlarmTime);
      state = SHOWTIME;
      break;
    case SHOWREEL:

      Show = NULL;                         // disable automatic 5s refresh (Note: the interrupt itself keeps running! it just does not call Show when Show is a NULL pointer)
      gPatterns[gCurrentPatternNumber]();  // call the patern
      delay(40);                           // 25 fps / do not starve timekeeping timer interrupt
      FastLED.show();
      gHue++;  // some effects base their hue on this, and increasing it 5Hz is barely perciptible, 25 Hz is a much more apealing effect



      if (autoreel) {
        EVERY_N_SECONDS(10)
        nextPattern();
      };  // change patterns periodically

      break;
    case SWAKE:
      Show = WakeAnim;  // more sophisticated animation before turning lights on
      break;
    case EASTERPONG:
      Pongloop();  //play pong
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
      /* use the time display on RGB strip as a light, without using the CWWW strip */
      break;
    case RST:
      state = REST1; /* So also turn time display off when resetting lights to off*/
      light = OFF;
      break;
    case LWAKE:
      pwm_cnt++;
      if (pwm_cnt == 255) {
        if (brightness_wake < 255) brightness_wake++;
        pwm_cnt = 0;
      }
      if (brightness_wake >= pwm_cnt) /* >= so when brightness = 255 there is DC and no PWM, so no flicker anymore */
      {
        digitalWrite(WW_LEDS, HIGH);
      } else {
        digitalWrite(WW_LEDS, LOW);
      }
      break;
    default:
      light = OFF;
  }



  if (digitalRead(SW1) == 0) {
    while (digitalRead(SW1) == 0) {
      delay(DEBOUNCE);  // wait for release
    }
    delay(DEBOUNCE);

    switch (state) {
      case SWAKE:
        egg = 0;
        light = OFF;
        brightness_wake = 0;
        waking = 0;  // reset wake animation
        state = SHOWTIME;
        break;
      case SHOWREEL:
      case EASTERPONG:
        egg = 0;
        state = SHOWTIME;
        break;
      default:
        state = SETTIME;
    }
  }
  if (state != EASTERPONG) {  // otherwise Pong cannot read the switches it needs

    if (digitalRead(SW2) == 0) {
      while (digitalRead(SW2) == 0) {
        delay(DEBOUNCE);  // wait for release
      }
      delay(DEBOUNCE);

      switch (state) {
        case SHOWREEL:
          nextPattern();
          autoreel = false;
          break;
        case SWAKE:
          egg = 0;
          light = OFF;
          brightness_wake = 0;
          waking = 0;  // reset wake animation
          state = SHOWTIME;
          break;
        default:
          state = SETAL;
      }
    }


    if (digitalRead(SW_TOP) == 0) {
      uint16_t langingedrukt = 0;
      while (digitalRead(SW_TOP) == 0) {
        delay(DEBOUNCE);  // wait for release
        langingedrukt++;
        if (langingedrukt > (3000 / DEBOUNCE)) {
          fpointer pTemp = Show;
          Show = NULL;
          timesync();
          Show = pTemp;
        }
      }
      delay(DEBOUNCE);

      switch (state) {
        case REST2:
          state = SHOWTIME;
          break;
        case SHOWTIME2:
          NEXTLIGHT; /* switch white light to next state (OFF,WW,CW,WW+CW,OFF)*/
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
            fill_solid(leds, NUM_LEDS, CRGB::Black);
            FastLED.show();
            light = OFF;
            state = EASTERPONG;
          }
          break;
        case SWAKE:
          egg = 0;
          light = OFF;
          brightness_wake = 0;
          waking = 0;  // reset wake animation
          state = SHOWTIME;
          break;
        default:
          break;
      }
    }
  }
}

void showtime(hms TTS, bool mix = false) {  // TTS = Time To Show

  if (!mix) { /* unless mixing time with another effect, clear ledstrip (to dark) firtst, and mix Time display leds with only each other */
    fill_solid(leds, NUM_LEDS, CRGB::Black);

    leds[TTS.m] += CRGB::DarkRed;
    leds[NUM_LEDS - TTS.h] += CRGB::Green;
    leds[TTS.s] += CRGB::DarkRed;
    leds[0] += indicator;
    //leds[NUM_LEDS - 1] += indicator; /* only 1 indicator*/

    for (uint8_t i = 5; i < NUM_LEDS - 1; i += 5) {  // scale / graticule, small ticks (5m).
      leds[i] += CRGB(0, 0, 8);
    };

    for (uint8_t i = 15; i < NUM_LEDS - 1; i += 15) {  // scale / graticule, big ticks (15m)
      leds[i] += CRGB(4, 0, 0);
    };

    if (light == TIME) {          // when not automatically turning off:
      leds[30] += CRGB(4, 4, 4);  // indicate on 30 minute tick
    }
  } else { /* When mixing with other effects, force time-display led's, so they are more distinctly visible (Do NOT mix/add their collors into existing) */

    leds[TTS.m] = CRGB::DarkRed;
    leds[NUM_LEDS - TTS.h] = CRGB::Green;
    leds[TTS.s] = CRGB::DarkRed;
    leds[0] = indicator;

    for (uint8_t i = 5; i < NUM_LEDS - 1; i += 5) {  // scale / graticule, small ticks (5m).
      leds[i] = CRGB(0, 0, 8);
    };

    for (uint8_t i = 15; i < NUM_LEDS - 1; i += 15) {  // scale / graticule, big ticks (15m)
      leds[i] = CRGB(4, 0, 8);
    };

    if (light == TIME) {         // when not automatically turning off:
      leds[30] = CRGB(8, 4, 8);  // indicate on 30 minute tick
    }
  }
}

void shownow() {  // bit of a wraparound, because Show(); does not take arguments.
  showtime(currenttime, false);
}

void showAl() {
  showtime(AlarmTime);
}

void showAdj() {
  showtime(SetTime);
}

hms AdjustTime(hms startval) {  //starts from startval and returns adjusted time, shows it on ledstrip while adjusting

  Show = showAdj;

  while (digitalRead(SW1) == 0 || digitalRead(SW2) == 0) delay(DEBOUNCE);
  delay(DEBOUNCE);

  while (digitalRead(SW2) != 0) {
    if (digitalRead(SW1) == 0) {
      if (startval.h < 24) startval.h++;
      else startval.h = 0;
      delay(400);
    }
    if (digitalRead(SW_TOP) == 0) {
      if (startval.h > 0) startval.h--;
      else startval.h = 23;
      delay(400);
    }

    SetTime = startval; /* copy so it can be displayed */
  }

  while (digitalRead(SW2) == 0) delay(DEBOUNCE);
  delay(DEBOUNCE);

  while (digitalRead(SW2) != 0) {
    if (digitalRead(SW1) == 0) {
      if (startval.m < 60) startval.m++;
      else startval.m = 0;
      delay(400);
    }
    if (digitalRead(SW_TOP) == 0) {
      if (startval.m > 0) startval.m--;
      else startval.m = 59;
      delay(400);
    }

    SetTime = startval; /* copy so it can be displayed */
  }

  while (digitalRead(SW2) == 0) delay(DEBOUNCE);
  delay(DEBOUNCE);

  while (digitalRead(SW2) != 0) {
    if (digitalRead(SW1) == 0) {
      if (startval.s < 60) startval.s++;
      else startval.s = 0;
      delay(400);
    }
    if (digitalRead(SW_TOP) == 0) {
      if (startval.s > 0) startval.s--;
      else startval.s = 59;
      delay(400);
    }

    SetTime = startval; /* copy so it can be displayed */
  }

  while (digitalRead(SW2) == 0) delay(DEBOUNCE);
  delay(DEBOUNCE);

  indicator = CRGB::Black;  // Whoa. Then how to indicate that alarm is set?
  //if(AlarmSet) indicator = CRGB::Red; else indicator=CRGB::Black // something like that?
  //AlarmSet?indicator:CRGB::Red:CRGB::Black; // unreadable... But shorter
  return startval;  //after modification
}

void WakeAnim() {
  // wake- up animation...
  // todo: improve
  // idea: fade in red leds from bottom to top slowly, and as last step, turn on WW ledstrip.
  const uint8_t STEPS = 7;            // number of fade-in steps per LED. 5 steps per second (refresh at 5 Hz), so between 3 and 15 are reasonable values.
  const uint8_t BRADD = 255 / STEPS;  // how much brightness is added per step?

  if (waking >= NUM_LEDS * STEPS * 2) {  // once the ws28 strip is lit, no longer increment "waking" and no longer play the animation
    light = LWAKE;                       // turn on the lights

    //rainbow();            /* Optionally show rainbow */
    //showtime(currenttime, true); /* optionally mix in time display as well (Might not combine well with all effects...)*/

    showtime(currenttime, false);  // or just show time withouth mixing with animation/effect.

  } else {
    if (waking == 0) fill_solid(leds, NUM_LEDS, CRGB::Black);

    // first dim up RED one by one, then green one by one (resulting in yellow-ish), then turn on the WW ledstrip.
    if (waking < (NUM_LEDS * STEPS)) {
      leds[(waking / STEPS)] += CHSV(HUE_RED, 255, BRADD);
    } else {
      leds[(waking / STEPS) - NUM_LEDS] += CHSV(HUE_GREEN, 255, BRADD);
    }
    waking++;
  }
}

void timesyncblink(uint8_t thesebits) {
  for (uint8_t i = 0; i < 8; i++) {
    if (thesebits & (1 << i)) {
      leds[NUM_LEDS - 1] = CRGB::Black;
      FastLED.show();
      delay(40);
      leds[NUM_LEDS - 1] = CRGB::White;
      FastLED.show();
      delay(80);
    } else {
      leds[NUM_LEDS - 1] = CRGB::Black;
      FastLED.show();
      delay(80);
      leds[NUM_LEDS - 1] = CRGB::White;
      FastLED.show();
      delay(40);
    }
  }
  leds[NUM_LEDS - 1] = CRGB::Black;
  FastLED.show();
}

void timesync() {  // TODO: a way to trigger this with the buttons on Lichtwekker, so it can actually be tested and used to sync the watch...
  uint8_t crc, blinkh, blinkm, blinks;
  //crc uitrekenen over uren, minuten, seconden, NA de transmissietijd bij de tijd te hebben geteld!
  RTC.readTime();
  blinkh = RTC.h;
  blinkm = RTC.m;
  blinks = RTC.s + 4;  // transmission takes 3.84 s: 32 bits at 120 ms per bit. rounded up.
  if (blinks >= 60) {
    blinks -= 60;
    blinkm += 1;
  }
  if (blinkm >= 60) {
    blinkm -= 60;
    blinkh += 1;
  }
  if (blinkh >= 24) {
    blinkh -= 24;
  }

  crc = _crc8_ccitt_update(crc, blinkh);
  crc = _crc8_ccitt_update(crc, blinkm);
  crc = _crc8_ccitt_update(crc, blinks);

  //start blink with LED ON for a long enough while to reset watch to bit 0;
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.setBrightness(255);  //set brightness to max so there is no PWM dimming disturbing data transfer
  leds[NUM_LEDS - 1] = CRGB::Blue;
  FastLED.show();
  delay(5000);

  timesyncblink(blinkh);
  timesyncblink(blinkm);
  timesyncblink(blinks);
  timesyncblink(crc);
  FastLED.setBrightness(BRIGHTNESS);  // restore brightness to limits
}
