// lichtwekker. Uses Digitalread etc. even though slower. Excuse is to easily port to other 'duino's or change pinout. (It's not lazyness! PIND&=1<<7 is actually shorter!)
const int TIMEOUT = 5;

#include "FastLED.h"
#include <mTime.h>             // use modified time.h lib. (Uses timer1 interrupt instead of milis -
//- this breaks arduino built-in servo/pwm, but standard time.h relies on millis() which gets broken by FastLED as that disables interrupts for well over a millisecond)

FASTLED_USING_NAMESPACE

#if defined(FASTLED_VERSION) && (FASTLED_VERSION < 3001000)
#warning "Requires FastLED 3.1 or later; check github for latest code."
#endif

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

#define BRIGHTNESS          128


time_t AlarmTime(7,30,0), SetTime;
CRGB indicator = CRGB::Black;


void setup() {
  delay(3000); // 3 second delay for recovery

  pinMode(SW1,INPUT_PULLUP);
  pinMode(SW2,INPUT_PULLUP);
  pinMode(SW_TOP,INPUT_PULLUP);
  pinMode(CW_LEDS,OUTPUT);
  pinMode(WW_LEDS,OUTPUT);
  
  TimeStart(tick); // to init timer interrupt in modified time library, and make it call the tick function on interrupt.
  
  // Set timer slower by overwriting settings:
  TCCR0B=4; // prescaler 256 instead of 64. (So millis gets 4 times as slow and spending NLEDS(=60)*30us=1.8ms with interrupts disabled is no longer an issue)
  
  // tell FastLED about the LED strip configuration
  FastLED.addLeds<LED_TYPE,DATA_PIN,COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  // set master brightness control
  FastLED.setBrightness(BRIGHTNESS);
  
}


typedef void (*fpointer)();
fpointer Show = sinelon; // Set this pointer to what function should be called just before a refresh in tick();

uint8_t gHue = 0; // rotating "base color" used by many of the patterns
  
void loop()
{

  enum {SHOWTIME, SHOWTIME2, SETTIME, SETAL, REST1, REST2, SHOWREEL};
  enum LSTATE {OFF, WW, CW, CWW, RST, EASTERPONG};
  static int light = OFF, state=SHOWTIME, egg=0;
  static time_t compare;


  switch (state) {
    case REST1:
    Show=nothing;
    fill_solid( leds, NUM_LEDS, CRGB::Black);
    FastLED.show();
    state=REST2;
    break;
    case REST2:
    break;
    case SHOWTIME:
    compare=now();
    Show = shownow;
    state=SHOWTIME2;
    break;
    case SHOWTIME2:
        if(now()-compare>TIMEOUT){ //na b.v. 5 seconden
        state=REST1;
        egg=0;
        }
   
    break;
    case SETTIME:
    indicator = CRGB::DarkBlue;
    setTime(AdjustTime(now()));
    state=SHOWTIME;
    break;
    case SETAL:
    indicator = CRGB::DarkGreen;
    Show=showAl;
    AlarmTime = AdjustTime(AlarmTime);
    // todo: alarm on/off
    state=SHOWTIME;
    break;
    case SHOWREEL:
    // todo: show fastled showreel / use buttons to choose which effect or auto-rotate
    Show=sinelon;
    break;
    default:
    state=SHOWTIME;
  }

  switch(light){
  case OFF:
  digitalWrite(WW_LEDS,LOW);
  digitalWrite(CW_LEDS,LOW);
  break;
  case WW:
  digitalWrite(WW_LEDS,HIGH);
  digitalWrite(CW_LEDS,LOW);
  break;
  case CW:
  digitalWrite(WW_LEDS,LOW);
  digitalWrite(CW_LEDS,HIGH);
  break;
  case CWW:
  digitalWrite(WW_LEDS,HIGH);
  digitalWrite(CW_LEDS,HIGH);
  break;
  case RST:
  light=OFF;
  //TODO: counter++, maar resetten na 5s?
  break;
  case EASTERPONG:
  // TODO: play pong
  break;
  default:
  light=OFF;
  }
  

/*
  if(digitalRead(SW1)==0){
    while(digitalRead(SW1)==0) delay(20); // wait for release
    }
  
  if(digitalRead(SW2)==0){
    while(digitalRead(SW2)==0) delay(20); // wait for release
    
    }
  */
  
  if(digitalRead(SW_TOP)==0){
    while(digitalRead(SW_TOP)==0) delay(20); // wait for release
    switch (state) {
    case REST2:
    state=SHOWTIME;
    if(light!=OFF) light++;
    break;
    case SHOWTIME2:
    light++;
    egg++;
      if(egg>9){
        light=OFF;
        state=SHOWREEL;
        }// todo: uitbreiden met pong?
    break;
    case SHOWREEL:
    //?
    break;
  }
    }

// TODO: above statemachine should replace most of this...
  //Show= shownow;
  
  if(digitalRead(SW1)==0){
    indicator = CRGB::DarkBlue;
    setTime(AdjustTime(now()));
    }
  
  if(digitalRead(SW2)==0){
    indicator = CRGB::DarkGreen;
    Show=showAl;
    AlarmTime = AdjustTime(AlarmTime);
    }
 /* 
  if(digitalRead(SW_TOP)==0){
    digitalWrite(WW_LEDS,!digitalRead(WW_LEDS)); // whoa. This could be much better by writing to PINx.x so it toggles in 1 asm instruction with no RMW, but OK... 
    while(digitalRead(SW_TOP)==0); // wait untill button release.
    }
  */
}


void sinelon()
{
  // a colored dot sweeping back and forth, with fading trails
  fadeToBlackBy( leds, NUM_LEDS, 20);
  int pos = beatsin16(13,0,NUM_LEDS);
  leds[pos] += CHSV( gHue, 255, 192);
}


void nothing(){
  };

void showtime(time_t TTS){ // TTS = Time To Show
  fill_solid( leds, NUM_LEDS, CRGB::Black); 
  leds[minute(TTS)] += CRGB::DarkRed;
  leds[NUM_LEDS-hour(TTS)] += CRGB::Green;
  leds[second(TTS)] += CRGB::DarkRed;
  leds[0] += indicator;
  }

void shownow(){ // bit of a wraparound, because Show(); does not take arguments.
  showtime(now());
  }

void showAl(){
  showtime(AlarmTime);
  }  

void showAdj(){
  showtime(SetTime);
  }
 
time_t AdjustTime(time_t startval){ //starts from startval and returns adjusted time, shows it on ledstrip while adjusting 
  // TODO: implement
  TimeElements temp;
 
  /* // when minute wraps around, hour gets rounded up or down. Same with minute when second wraps... So lets use breaktime instead...
  temp.Hour=hour(startval);
  temp.Minute=minute(startval);
  temp.Second=second(startval); // could have used breaktime() fnction but only need these. Seem
  */
  breakTime(startval, temp);
  
  Show=showAdj;
  
  while( digitalRead(SW1)==0 || digitalRead(SW2)==0 ) delay(20);
  
  while(digitalRead(SW2) !=0){
    if(digitalRead(SW1)==0){
      if(temp.Hour<24) temp.Hour++; else temp.Hour=0;
      delay(100); // remember: 4 times as long, because millis is slowed down...
    }
    //setTime(uur,minuut,seconde,1,1,1970); // Oh.. This can only set the current system time... And that WHILE seconds keep counting up... What kind of Bleep is that?
    SetTime = makeTime(temp);
  }

  while(digitalRead(SW2)==0) delay(100);
  
  while(digitalRead(SW2) !=0){
    if(digitalRead(SW1)==0){
      if(temp.Minute<60) temp.Minute++; else temp.Minute=0;
      delay(100);
    }
    SetTime = makeTime(temp);
  }
  
  while(digitalRead(SW2)==0) delay(100);
  
  while(digitalRead(SW2) !=0){
    if(digitalRead(SW1)==0){
      if(temp.Second<60) temp.Second++; else temp.Second=0;
      delay(100);
    }
    SetTime = makeTime(temp);
  }

  while(digitalRead(SW2)==0) delay(100);
  
  indicator = CRGB::Black; // Whoa. Then how to indicate that alarm is set?
  //if(AlarmSet) indicator = CRGB::Red; else indicator=CRGB::Black // something like that?
  //AlarmSet?indicator:CRGB::Red:CRGB::Black; // unreadable... But shorter
  return SetTime; // even though it is a global anyway... (Yeah, should've thought this trough. But it needs to be a global to use it in interrupt).
  }

  

void tick() {
  // Will be called at 5Hz.
 
  if ( hour(AlarmTime) == hour() && minute(AlarmTime) == minute() && second(AlarmTime) == second() ) {
    digitalWrite(WW_LEDS, 1); // TODO: more sofisticated fade-in and something that makes the weker go for longer then just that one second the times match
  }; 
  
  Show(); 
  FastLED.show();
}
