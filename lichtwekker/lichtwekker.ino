// lichtwekker. Uses Digitalread etc. even though slower. Excuse is to easily port to other 'duino's or change pinout. (It's not lazyness! PIND&=1<<7 is actually shorter!)
// also re-uses FastLED demo reel.
const int TIMEOUT = 5;

#include <FastLED.h>
#include <mTime.h>             // use modified time.h lib. (Uses timer1 interrupt instead of milis -
//- this breaks arduino built-in servo/pwm, but standard time.h relies on millis() which gets broken by FastLED as that disables interrupts for well over a millisecond)
FASTLED_USING_NAMESPACE
//#include "showreel.h" -- decided it was easyer and clearer to just keep this in one file

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


time_t AlarmTime, SetTime;
CRGB indicator = CRGB::Black;


void setup() {
  delay(3000); // 3 second delay for recovery
  //AlarmTime = (minutesToTime_t(30) + hoursToTime_t(7);
  AlarmTime = 7*3600+30*60;


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

typedef void (*SimplePatternList[])();
SimplePatternList gPatterns = { rainbow, rainbowWithGlitter, confetti, sinelon, juggle, bpm, Fire2012};

#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

uint8_t gHue = 0; // rotating "base color" used by many of the patterns
uint8_t gCurrentPatternNumber = 0; // Index number of which pattern is current

enum {SHOWTIME, SHOWTIME2, SETTIME, SETAL, REST1, REST2, SHOWREEL, SWAKE};
enum LSTATE {OFF, WW, CW, CWW, RST, LWAKE, EASTERPONG};
 
int light = OFF, state=SHOWTIME;

bool alset = true, alring = false; // alarm set / ringing or not?
  
void loop()
{

  static int egg=0;
  static time_t compare;
  static bool autoreel = true;


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

        if(alset) indicator = CRGB::DarkGoldenrod; else indicator = CRGB::Black;
        
        if(now()-compare>TIMEOUT){ //na b.v. 5 seconden
        state=REST1;
        egg=0;
        }
   
    break;
    case SETTIME:
    indicator = CRGB::LightGoldenrodYellow;
    Show=shownow;
    setTime(AdjustTime(now()));
    state=SHOWTIME;
    break;
    case SETAL:
    indicator = CRGB::OliveDrab;
    Show=showAl;
    alset = !alset; // alarm on/off
    AlarmTime = AdjustTime(AlarmTime);
    state=SHOWTIME;
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
      
    
    //if(autoreel) EVERY_N_SECONDS( 10 ) { nextPattern(); }; // change patterns periodically (might be slower because millis is slower??)
    // hah. the above should work but throws compiler errors unless expressed as:
    if(autoreel){ EVERY_N_SECONDS( 10 ) { nextPattern(); };} // change patterns periodically (might be slower because millis is slower??)
    
    
    break;
    case SWAKE:
    //...
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
  break;
  case EASTERPONG:
  // TODO: play pong
  break;
  case LWAKE:
  digitalWrite(WW_LEDS,HIGH);
  // todo: more sophisticated wake-up animation before turning light on.
  break;
  default:
  light=OFF;
  }
  


  if(digitalRead(SW1)==0){
    while(digitalRead(SW1)==0) delay(20); // wait for release
      switch(state){
        case SHOWREEL:
        nextPattern();
        autoreel = false;
        break;
        default:
        state=SETTIME;
        }
    }
  
  if(digitalRead(SW2)==0){
    while(digitalRead(SW2)==0) delay(20); // wait for release
    switch(state){
        case SHOWREEL:
        state=SHOWTIME;
        break;
        default:
        state=SETAL;
        }
    }
  
  
  if(digitalRead(SW_TOP)==0){
    while(digitalRead(SW_TOP)==0) { // wait for release
      delay(20); //debounce
      gHue++;    //for various visual effects
    }
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
        autoreel=true;
        state=SHOWREEL;
        }// todo: uitbreiden met pong?
    break;
    case SHOWREEL:
      //if(egg>13){ light=OFF; state=EASTERPONG;}
    break;
    case SWAKE:
    light=OFF;
    state=SHOWTIME;
    break;
    default:
    break;
  }
    }
}

void nextPattern()
{
  // add one to the current pattern number, and wrap around at the end
  gCurrentPatternNumber = (gCurrentPatternNumber + 1) % ARRAY_SIZE( gPatterns);
}

void nothing(){ // or could I use NULL?
  };

void showtime(time_t TTS){ // TTS = Time To Show
  fill_solid( leds, NUM_LEDS, CRGB::Black); 
  leds[minute(TTS)] += CRGB::DarkRed;
  leds[NUM_LEDS-hour(TTS)] += CRGB::Green;
  leds[second(TTS)] += CRGB::DarkRed;
  leds[0] += indicator;
  leds[59] += indicator;
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

  void rainbow() 
{
  // FastLED's built-in rainbow generator
  fill_rainbow( leds, NUM_LEDS, gHue, 7);
}

void rainbowWithGlitter() 
{
  // built-in FastLED rainbow, plus some random sparkly glitter
  rainbow();
  addGlitter(80);
}

void addGlitter( fract8 chanceOfGlitter) 
{
  if( random8() < chanceOfGlitter) {
    leds[ random16(NUM_LEDS) ] += CRGB::White;
  }
}

void confetti() 
{
  // random colored speckles that blink in and fade smoothly
  fadeToBlackBy( leds, NUM_LEDS, 10);
  int pos = random16(NUM_LEDS);
  leds[pos] += CHSV( gHue + random8(64), 200, 255);
}

void sinelon()
{
  // a colored dot sweeping back and forth, with fading trails
  fadeToBlackBy( leds, NUM_LEDS, 20);
  int pos = beatsin16(13,0,NUM_LEDS);
  leds[pos] += CHSV( gHue, 255, 192);
}

void bpm()
{
  // colored stripes pulsing at a defined Beats-Per-Minute (BPM)
  uint8_t BeatsPerMinute = 62;
  CRGBPalette16 palette = PartyColors_p;
  uint8_t beat = beatsin8( BeatsPerMinute, 64, 255);
  for( int i = 0; i < NUM_LEDS; i++) { //9948
    leds[i] = ColorFromPalette(palette, gHue+(i*2), beat-gHue+(i*10));
  }
}

void juggle() {
  // eight colored dots, weaving in and out of sync with each other
  fadeToBlackBy( leds, NUM_LEDS, 20);
  byte dothue = 0;
  for( int i = 0; i < 8; i++) {
    leds[beatsin16(i+7,0,NUM_LEDS)] |= CHSV(dothue, 200, 255);
    dothue += 32;
  }
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
// Default 50, suggested range 20-100 
#define COOLING  55

// SPARKING: What chance (out of 255) is there that a new spark will be lit?
// Higher chance = more roaring fire.  Lower chance = more flickery fire.
// Default 120, suggested range 50-200.
#define SPARKING 120

void Fire2012()
{
// Array of temperature readings at each simulation cell
  static byte heat[NUM_LEDS];
  static bool gReverseDirection = false;

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
      CRGB color = HeatColor( heat[j]);
      int pixelnumber;
      if( gReverseDirection ) {
        pixelnumber = (NUM_LEDS-1) - j;
      } else {
        pixelnumber = j;
      }
      leds[pixelnumber] = color;
    }
}


void tick() {
  // Will be called at 5Hz.
 
  if ( alset && hour(AlarmTime) == hour() && minute(AlarmTime) == minute() && second(AlarmTime) == second() ) {
    // TODO: more sofisticated fade-in and something that makes the weker go for longer then just that one second the times match: alring=true etc.
    light=LWAKE; // otherwise it turns off right again.
    state=SWAKE;
  }; 
  
  Show(); 
  FastLED.show();
}
