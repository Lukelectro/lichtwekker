// lichtwekker. Uses Digitalread etc. even though slower. Excuse is to easily port to other 'duino's or change pinout. (It's not lazyness! PIND&=1<<7 is actually shorter!)
// also re-uses FastLED demo reel.
const int TIMEOUT = 30;

#include <FastLED.h>
#include <mTime.h>             // use modified time.h lib. (Uses timer1 interrupt instead of milis -
//- this breaks arduino built-in servo/pwm, but standard time.h relies on millis() which gets broken by FastLED as that disables interrupts for well over a millisecond)
FASTLED_USING_NAMESPACE

#include "onedpong.h"
#include "fire.h"
#include "showreel.h"


#define SW_TOP 5
#define SW1 4
#define SW2 6

#define CW_LEDS 9
#define WW_LEDS 8
#define DATA_PIN    7
#define LED_TYPE    WS2811
#define COLOR_ORDER GRB
#define NUM_LEDS    60
CRGB leds[NUM_LEDS];

#define BRIGHTNESS 128 // set max brightness to limit power consumption.


time_t AlarmTime, SetTime;
CRGB indicator = CRGB::Black;

fpointer Show = sinelon; // Set this pointer to what function should be called just before a refresh in tick();

enum {SHOWTIME, SHOWTIME2, SETTIME, SETAL, REST1, REST2, SHOWREEL, SWAKE, EASTERPONG};
enum LSTATE {OFF, WW, CW, CWW, RST, LWAKE};

uint8_t light = OFF, state = SHOWTIME;

unsigned int waking = 0;

bool alset = true; // alarm set or not?

void setup() {
  delay(3000); // 3 second delay for recovery

  //AlarmTime = (minutesToTime_t(30) + hoursToTime_t(7);
  AlarmTime = 7 * 3600 + 30 * 60;

  setTime(7, 29, 56, 1, 1, 1970); // for testing alarm

  pinMode(SW1, INPUT_PULLUP);
  pinMode(SW2, INPUT_PULLUP);
  pinMode(SW_TOP, INPUT_PULLUP);
  pinMode(CW_LEDS, OUTPUT);
  pinMode(WW_LEDS, OUTPUT);

  Pongsetup();

  TimeStart(tick); // to init timer interrupt in modified time library, and make it call the tick function on interrupt.

  // Set timer slower by overwriting settings:
  //TCCR0B = 4; // prescaler 256 instead of 64. (So millis gets 4 times as slow and spending NLEDS(=60)*30us=1.8ms with interrupts disabled is no longer an issue)
  // test if this is actually needed?

  // tell FastLED about the LED strip configuration
  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  // set master brightness control
  FastLED.setBrightness(BRIGHTNESS);

}

void loop()
{

  static int egg = 0;
  static time_t compare;
  static bool autoreel = true;


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
      compare = now();
      Show = shownow;
      state = SHOWTIME2;
      break;
    case SHOWTIME2:

      if (alset) indicator = CRGB::DarkGoldenrod; else indicator = CRGB::Black;

      if (now() - compare > TIMEOUT) { //na b.v. 5 seconden
        state = REST1;
        egg = 0;
      }

      break;
    case SETTIME:
      indicator = CRGB::LightGoldenrodYellow;
      Show = shownow;
      setTime(AdjustTime(now()));
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


      //if(autoreel) EVERY_N_SECONDS( 10 ) { nextPattern(); }; // change patterns periodically (might be slower because/if millis is slower??)
      // hah. the above should work but throws compiler errors unless expressed as:
      if (autoreel) {
        EVERY_N_SECONDS( 10 ) {
          nextPattern();  // change patterns periodically (might be slower because/if millis is slower??)
        };
      }
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
    case RST:
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
      case SHOWREEL:
        nextPattern();
        autoreel = false;
        break;
      case SWAKE:
        egg = 0;
        light = OFF;
        state = SHOWTIME;
        break;
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
          egg = 0;
          state = SHOWTIME;
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


void showtime(time_t TTS) { // TTS = Time To Show
  fill_solid( leds, NUM_LEDS, CRGB::Black);
  leds[minute(TTS)] += CRGB::DarkRed;
  leds[NUM_LEDS - hour(TTS)] += CRGB::Green;
  leds[second(TTS)] += CRGB::DarkRed;
  leds[0] += indicator;
  leds[NUM_LEDS-1] += indicator;

  for(uint8_t i=4;i<NUM_LEDS,i+=5){ leds[i] += CRGB(0,0,25)}; // scale / graticule
  
}

void shownow() { // bit of a wraparound, because Show(); does not take arguments.
  showtime(now());
}

void showAl() {
  showtime(AlarmTime);
}

void showAdj() {
  showtime(SetTime);
}

time_t AdjustTime(time_t startval) { //starts from startval and returns adjusted time, shows it on ledstrip while adjusting
  TimeElements temp;

  breakTime(startval, temp);

  Show = showAdj;

  while ( digitalRead(SW1) == 0 || digitalRead(SW2) == 0 ) delay(100);

  while (digitalRead(SW2) != 0) {
    if (digitalRead(SW1) == 0) {
      if (temp.Hour < 24) temp.Hour++; else temp.Hour = 0;
      delay(400); 
    }
    SetTime = makeTime(temp);
  }

  while (digitalRead(SW2) == 0) delay(100);

  while (digitalRead(SW2) != 0) {
    if (digitalRead(SW1) == 0) {
      if (temp.Minute < 60) temp.Minute++; else temp.Minute = 0;
      delay(400);
    }
    SetTime = makeTime(temp);
  }

  while (digitalRead(SW2) == 0) delay(100);

  while (digitalRead(SW2) != 0) {
    if (digitalRead(SW1) == 0) {
      if (temp.Second < 60) temp.Second++; else temp.Second = 0;
      delay(400);
    }
    SetTime = makeTime(temp);
  }

  while (digitalRead(SW2) == 0) delay(100);

  indicator = CRGB::Black; // Whoa. Then how to indicate that alarm is set?
  //if(AlarmSet) indicator = CRGB::Red; else indicator=CRGB::Black // something like that?
  //AlarmSet?indicator:CRGB::Red:CRGB::Black; // unreadable... But shorter
  return SetTime; // even though it is a global anyway... (Yeah, should've thought this trough. But it needs to be a global to use it in interrupt).
}

void WakeAnim() {
  // wake- up animation...
  // todo: improve
  // idea: fade in red leds from bottom to top slowly, and as last step, turn on WW ledstrip.
  const uint8_t STEPS = 7; // number of fade-in steps per LED. 5 steps per second (refresh at 5 Hz), so between 3 and 15 are reasonable values? default 5.
  const uint8_t BRADD = 255 / STEPS; // how much brightness is added per step?

  if (waking == 0) fill_solid( leds, NUM_LEDS, CRGB::Black);
  if (waking <= (NUM_LEDS * STEPS)) waking++;

  leds[(waking / STEPS)] += CHSV(HUE_RED, 255, BRADD); // todo: nicer lineair dimming/brightening?

  if (waking >= NUM_LEDS * STEPS) { // once the ws28 strip is lit
    light = LWAKE;
  }
}

void tick() {
  // Will be called at 5Hz.

  gHue++;    //for various visual effects

  if ( alset && hour(AlarmTime) == hour() && minute(AlarmTime) == minute() && second(AlarmTime) == second() ) {
    waking = 0; // reset wake animation
    state = SWAKE;
  };

  if (Show != NULL) { // so it can be set to NULL to disable auto-refresh
    Show();
    FastLED.show();
  }

}
