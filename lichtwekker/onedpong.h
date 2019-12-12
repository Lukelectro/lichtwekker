/*
 * Arduino 1D Pong Game with (60) WS2812B LEDs
 *
 * Copyright (C) 2015  B.Stultiens
 * Modified by electroluke for lichtwekker: fastled instead of adafruit, 
 * no sound (need the timer for clock/timekeeping), 2 butons only.
 * and different pinout. 
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef onedpongH
#define onedpongH 
#include <FastLED.h>
FASTLED_USING_NAMESPACE
#define NELEM(x)		(sizeof(x) / sizeof((x)[0]))

#define DATA_PIN		7     // LED data
#define PIN_BUT_RS		3		// Right start/hit button (SW_TOP)
#define PIN_BUT_LS		6	  // Left start/hit button (SW2)
#define PIN_BUT_LP -1
#define PIN_BUT_RP -1

//#define NPIXELS			60		// Number of pixels to handle
#define LED_TYPE    WS2811
#define COLOR_ORDER GRB
#define NUM_LEDS    60
#define NPIXELS NUM_LEDS


#define ZONE_SIZE		7		// Bounce-back zone size
#define SHOW_LO			12		// Score dots intensity background
#define SHOW_HI			48		// Score dots intensity foreground
#define WIN_POINTS		10		// Points needed to win
#define TONE_INTERVAL		5		// Not every ball move should give a sound

// Events from buttons and timers
#define EV_BUT_LS_PRESS		0x01
#define EV_BUT_RS_PRESS		0x02
#define EV_BUT_LP_PRESS		0x04
#define EV_BUT_RP_PRESS		0x08
#define EV_TIMER		0x10
#define EV_TIMEOUT		0x20
#define EV_TONETIMER		0x40

#define TIME_DEBOUNCE		8
#define TIME_IDLE		40
#define TIME_START_TIMEOUT	20000		// Go idle if nothing happens
#define TIME_RESUME_TIMEOUT	7500		// Auto-fire after timeout
#define TIME_BALL_BLINK		150
#define TIME_SPEED_MIN		10
#define TIME_SPEED_INTERVAL	3
#define TIME_POINT_BLINK	233
#define TIME_WIN_BLINK		85
#define TIME_LOCKOUT		250		// Prevent fast button-press to max. 4 times/s

#define TIME_TONE_SERVE		50		// Sound durations
#define TIME_TONE_BOUNCE	50
#define TIME_TONE_MOVE		25
#define TIME_TONE_SCORE		50

enum {
	ST_IDLE = 0,
	ST_START_L,
	ST_START_R,
	ST_MOVE_LR,
	ST_MOVE_RL,
	ST_ZONE_L,
	ST_ZONE_R,
	ST_POINT_L,
	ST_POINT_R,
	ST_RESUME_L,
	ST_RESUME_R,
	ST_WIN_L,
	ST_WIN_R,
};

extern CRGB leds[NUM_LEDS]; // should be _defined_ in sketch, only _declared_ here

/*
 * Return the current state of a button.
 * Returns non-zero on button pressed.
 */
static inline uint8_t button_is_down(uint8_t pin);

/*
 * Debounce a button and return an event at the rising edge of the detection.
 * The rising edge ensures that there is no delay from pressing the button and
 * the event propagating. It is a prerequisite that the input line is not
 * glitchy.
 * A release event may be generated if the routine is slightly modified.
 */
static inline uint8_t do_debounce(uint8_t tdiff, uint8_t *bstate, uint8_t *debtmr, uint8_t pin, uint8_t ev);
/*
 * Timer countdown and return an event on timer reaching zero.
 */
static inline uint8_t do_timer(uint8_t tdiff, uint16_t *tmr, uint8_t ev);

/*
 * Draw the left and right zones where the ball may be hit back.
 */
static void draw_sides();

/*
 * Draw the ball with a tail of five pixels in diminishing intensity.
 */
static void draw_ball(int8_t dir, uint8_t pos);

/*
 * Draw the playing field consisting of the zones and the points scored so far.
 */
static void draw_course(uint8_t v);

/*
 * Animate the game idle situation with following content:
 * - A rainbow pattern
 * - Ball bouncing left-right-left-right
 * - Score animation
 */
static uint16_t ai_h;
static uint8_t ai_state;
static uint8_t ai_pos;

static void animate_idle_init(void);

#define H_STEPS	1542

static void animate_idle(void);

/*
 * Animate a winner. Flash the winning side's points.
 */
static void animate_win_init();

static uint8_t animate_win(uint8_t side);
/*
 * Active game states suppress fast button pushes
 */
static uint8_t is_game_state(uint8_t s);

/*
 * Set the timer to the speed of the ball and the current boost-state
 */
static inline void speed_to_timer();

/*
 * State transition routine. Setup prerequisites for the new state to function
 * properly.
 * - Handle a state's exit actions
 * - Handle a state's entry actions
 */
static void set_state(uint8_t newstate);

/*
 *  setup
 */
void Pongsetup();

/*
 * Main program, called constantly and forever.
 *
 * - Handle timing and generate events
 * - Run the game's state machine
 */
#define chk_ev(ev)	(events & (ev))

void Pongloop();
#endif
