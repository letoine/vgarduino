/*
 * Output VGA video directly from arduino
 * 
 * -- About --
 * Project location is: http://code.google.com/p/vgarduino/
 * It was inspired by: https://code.google.com/p/arduino-vgaout/
 * Code is intended to be simplified at maximum for educational
 * purposes.
 * 
 * -- License ---
 * Copyright (C) 2014 Antoine Terrienne
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
 * 
 * -- History --
 * (2014/11/27) v1.1 - make output more stable, fix horizontal/vertical inversion in variable naming.
 * (2014/02/09) v1.0 - outputs a simple pattern of 8 color bars
 * 
 * -- VGA connector pinout --
 * pin num - signal - my cable colors
 *  1      -  Red   - pink
 *  2      - Green  - light green
 *  3      - Blue   - light blue
 *  4      -   NC
 *  5      -  GND   - yellow
 *  6-9    -  GND
 *  10     -  GND   - black
 *  11-12  -   NC
 *  13     - H-Sync - dark green
 *  14     - V-Sync - white
 *  15     -   NC   - red
 * 
 * -- VGA signals diagram --
 * 
 * Horizontal
 *                ______________
 *               |              |
 *               |     video    |
 *         ______|              |_____
 *           |<C>|<------ D --->|<E>|
 *     __     ______________________     ____
 *       |   |                      |   |
 *       |   |        synch         |   |
 *       |___|                      |___|
 *       |<B>|                      |
 *       |<------------- A -------->|
 * 
 * Vertical
 *                ______________
 *               |              |
 *               |     video    |
 *         ______|              |_____
 *           |<Q>|<------ R --->|<S>|
 *     __     ______________________     ____
 *       |   |                      |   |
 *       |   |        synch         |   |
 *       |___|                      |___|
 *       |<P>|                      |
 *       |<------------- O -------->|
 * 
 * 
 * -- VGA signal timings --
 * 
 * Timings for a 640x480 60Hz signal. Timing for a pixel is 0.04us.
 * 
 * Timings for the number of clock ticks are calculated from the clock
 * frequencey of the Arduino (16Mhz). Each clock tick is: 0.0625 us
 * 
 * area :           name          :    size    :  timing : clock ticks
 * A    : Full line               : 800 pixels : 32   us : 512
 * B    : Horizontal synch pulse  :  96 pixels :  3.84us : 61.44
 * C    : Horizontal back porch   :  48 pixles :  1.92us : 30.72
 * D    : Horizontal active video : 640 pixels : 25.6 us : 409.6
 * E    : Horizontal front porch  :  16 pixels :  0.64us : 10.24
 * 
 * O    : Full frame              : 524 lines  : 16768us : 
 * P    : Vertical synch pulse    :   2 lines  :    64us : 
 * Q    : Vertical back porch     :  31 lines  :   992us : 
 * R    : Vertical active video   : 480 lines  : 15360us : 
 * S    : Vertical front porch    :  11 lines  :   352us : 
 * 
 * -- Circuit schematic ---
 *
 * Note if you change the pin configuration in the code you will have
 * to adapt the circuit.
 *
 * __________
 *           |
 * Arduino   |                                 VGA pin
 *       GND |
 *        13 |                  R1
 *        12 |     +-----------WWWW---------- 13 - H-Sync
 *        11 |     |            R2
 *        10 |     |   +-------WWWW---------- 14 - V-Sync
 *         9 +-----+   |
 *         8 +---------+        R3
 *         7 +-----------------WWWW----------  1 - Red
 *         6 +-------------+
 *         5 +---------+   |    R4
 *         4 |         |   +---WWWW----------  2 - Green
 *         3 |         |        R5
 *         2 |         +-------WWWW----------  3 - Blue
 *         1 |
 * Digital 0 |
 *  _________|
 *                R1=R2=R3=R4=R5=470ohm
 *
 *
 * references:
 *  http://www.javiervalcarce.eu/wiki/VGA_Video_Signal_Format_and_Timing_Specifications
 *  http://www-mtl.mit.edu/Courses/6.111/labkit/vga.shtml
 *  http://arduino.cc/en/Hacking/PinMapping168
 *  http://www.nongnu.org/avr-libc/user-manual/modules.html
 *  http://www.atmel.com/devices/ATMEGA328P.aspx
 */

#include <avr/interrupt.h>

#define CONCAT(a, b)  a##b
#define PORT(p)  CONCAT(PORT,p)
#define DDR(p)  CONCAT(DDR,p)

#define RGB_PORT   D
#define RED_PIN    7
#define GREEN_PIN  6
#define BLUE_PIN   5

#define SYNCH_PORT B
#define HSYNCH_PIN 1
#define VSYNCH_PIN 0

#define BLACK_MASK   0
#define RED_MASK     _BV(RED_PIN)
#define GREEN_MASK   _BV(GREEN_PIN)
#define BLUE_MASK    _BV(BLUE_PIN)
#define CYAN_MASK    (_BV(GREEN_PIN)|_BV(BLUE_PIN))
#define YELLOW_MASK  (_BV(RED_PIN)|_BV(GREEN_PIN))
#define MAGENTA_MASK (_BV(RED_PIN)|_BV(BLUE_PIN))
#define WHITE_MASK   (_BV(RED_PIN)|_BV(GREEN_PIN)|_BV(BLUE_PIN))

#define V_FRONT_PORCH_LINES  11
#define V_SYNCH_PULSE_LINES  2
#define V_BACK_PORCH_LINES   31
#define V_ACTIVE_VIDEO_LINES 480

int nb_line_modes[] = {
#define SYNCH_PULSE  0
  V_SYNCH_PULSE_LINES,
#define BACK_PORCH   1
  V_BACK_PORCH_LINES,
#define ACTIVE_VIDEO 2
  V_ACTIVE_VIDEO_LINES,
  V_FRONT_PORCH_LINES,
#define FRONT_PORCH  3
};

#define H_SYNCH_PULSE_TICKS  61
#define H_BACK_PORCH_TICKS   30
#define H_ACTIVE_VIDEO_TICKS 406
#define H_FRONT_PORCH_TICKS  10
#define H_FULL_LINE_NB_TICKS (H_SYNCH_PULSE_TICKS + \
                              H_BACK_PORCH_TICKS +  \
                              H_ACTIVE_VIDEO_TICKS + \
                              H_FRONT_PORCH_TICKS)

#define WAIT_VIDEO_INACTIVE()  do {} while (phase == ACTIVE_VIDEO)
#define WAIT_VIDEO_ACTIVE()  do {} while (phase != ACTIVE_VIDEO)

byte phase;
int lines_left;

inline void incrementLineNumber() {
  lines_left--;
  if (lines_left == 0) {
    phase++;
    phase &= 0x03; // Take adventage that we have 4 phases only.
    lines_left =  nb_line_modes[phase];
  }
}

/*inline void waitTimer1(byte tick) {
  do { } while(TCNT1L < tick);
}*/

/* Wait for TCNT1 to be equal to tick
* this is a way to synchronise with the
* timer, because the interrupt can be triggered with a delay
* we have to compensate to make sure we execute the color changing
* commands at the same offset from the synch.
*
* we first get the difference between the current TCNT1 timer value
* and the expected one, this will give us an exact the number of ticks
* to wait.
* Then we jump threw some hoops to make sure the consume onyl the
* required numbers of ticks to synch the execution flow with TCNT1 counter.
*
* This version only checks TCNT1L, as we won't need to synch with a value
* greater than 255.
*/
inline void waitTimer1(byte tick) {
  asm volatile(
  // we count clock cycles for each instruction to synch-up with TCNT1L
  "sub %[tick], %[tmp]\n\t"// 1   +1=1
  "sub %[tick], 13\n\t"    // 1   +1=2 green path consumes 12 ticks whatever happens
  "rjmp 2f\n\t"            // 2   +2=4
  "1: subi %[tick], 4\n\t" // 1        each loop to reduce the ticks consume 4 clocks
  "2: cpi %[tick], 4\n\t"  // 1   +1=5
  "brcc 1b\n\t"            // 1/2 +1=6
  //     at this point,  tick can be =    0     1     2     3
  "cpi %[tick], 0\n\t"     // 1        +1=1  +1=1  +1=1  +1=1
  "breq 3f\n\t"            // 1/2      +2=3  +1=2  +1=2  +1=2
  "cpi %[tick], 1\n\t"     // 1              +1=3  +1=3  +1=3
  "breq 1f\n\t"            // 1/2            +2=5  +1=4  +1=4
  "cpi %[tick], 2\n\t"     // 1                    +1=5  +1=5
  "breq 2f\n\t"            // 1/2                  +2=7  +1=6
  "0: nop\n\t"             // 1        +1=4              +1=7
  "1: nop\n\t"             // 1        +1=5  +1=6        +1=8
  "2: nop\n\t"             // 1        +1=6  +1=7  +1=8  +1=9
  "3:"                     //          -0=6  -1=6  -2=6  -3=6
  : 
  : [tick] "r" (tick),
    [tmp] "r" (TCNT1L)
  );
}

// horizontal line start
ISR(TIMER1_CAPT_vect) {
  if (phase == ACTIVE_VIDEO) {
        // wait for the end of the back porch
        waitTimer1(H_SYNCH_PULSE_TICKS+H_BACK_PORCH_TICKS);
        PORT(RGB_PORT) = RED_MASK;
        delayMicroseconds(3);
        PORT(RGB_PORT) = GREEN_MASK;
        delayMicroseconds(3);
        PORT(RGB_PORT) = BLUE_MASK;
        delayMicroseconds(3);
        PORT(RGB_PORT) = CYAN_MASK;
        delayMicroseconds(3);
        PORT(RGB_PORT) = YELLOW_MASK;
        delayMicroseconds(3);
        PORT(RGB_PORT) = MAGENTA_MASK;
        delayMicroseconds(3);
        PORT(RGB_PORT) = BLACK_MASK;
        delayMicroseconds(3);
        PORT(RGB_PORT) = WHITE_MASK;
        delayMicroseconds(3);
        PORT(RGB_PORT) = 0;
  } else if (phase == FRONT_PORCH || phase == SYNCH_PULSE) {
      if (lines_left == 1) {
        // wait for the end of the back porch
        waitTimer1(H_SYNCH_PULSE_TICKS+H_BACK_PORCH_TICKS);
        // if end of front porch, set Vsynch to 0 to start synch pulse
        // if enf of synch pulse, set Vsynch to 1 to start back porch
        PORT(SYNCH_PORT) ^= _BV(VSYNCH_PIN);
      }
  }
  incrementLineNumber();
}


inline void init_io() {
  // Set pins for color and synch as output
  DDR(RGB_PORT) = (_BV(RED_PIN)|_BV(GREEN_PIN)|_BV(BLUE_PIN));
  DDR(SYNCH_PORT) = (_BV(VSYNCH_PIN)|_BV(HSYNCH_PIN));
  
  PORT(RGB_PORT) = 0;
  PORT(SYNCH_PORT) = (_BV(VSYNCH_PIN)|_BV(HSYNCH_PIN));
}

/*
* we are using the TIMER1 to generate the horizontal lines
*/
inline void init_video_line_timer() {
  TCCR0A = 0; // disable timer0, it causes issues to trigger interrupt
  TCCR0B = 0;
  TCCR1A = 0; // disable all first.
  TCCR1B = 0;
  TCCR1C = 0;
  // init line/phase status
  phase = FRONT_PORCH;
  lines_left =  nb_line_modes[phase];
  // init timing
  // reset counter to 0
  TCNT1 = 0;
  // set first match
  OCR1A = H_SYNCH_PULSE_TICKS;
  // set second match to the end of the video line
  // counter 
  ICR1 = H_FULL_LINE_NB_TICKS;
  // Enable the interrupt
  TIMSK1 = _BV(ICIE1);
  // set mode to FastPWN mode
  // 
  TCCR1A = _BV(COM1A1)|_BV(COM1A0)|_BV(WGM11);
  TCCR1B = _BV(WGM13)|_BV(WGM12)|_BV(CS10);
}

void setup () {
  init_io();
  cli();
  init_video_line_timer();
  sei();
}

void loop () {
}
