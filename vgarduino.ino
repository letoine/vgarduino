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

#define RGB_PORT   D
#define RED_PIN    7
#define GREEN_PIN  6
#define BLUE_PIN   5

#define SYNCH_PORT B
#define VSYNCH_PIN 1
#define HSYNCH_PIN 0

#define CONCAT(a, b)  a##b
#define PORT(p)  CONCAT(PORT,p)
#define DDR(p)  CONCAT(DDR,p)

#define BLACK   0
#define RED     _BV(RED_PIN)
#define GREEN   _BV(GREEN_PIN)
#define BLUE    _BV(BLUE_PIN)
#define CYAN    (_BV(GREEN_PIN)|_BV(BLUE_PIN))
#define YELLOW  (_BV(RED_PIN)|_BV(GREEN_PIN))
#define MAGENTA (_BV(RED_PIN)|_BV(BLUE_PIN))
#define WHITE   (_BV(RED_PIN)|_BV(GREEN_PIN)|_BV(BLUE_PIN))

#define SYNCH_PULSE  0
#define BACK_PORCH   1
#define ACTIVE_VIDEO 2
#define FRONT_PORCH  3

#define H_FRONT_PORCH_LINES  11
#define H_SYNCH_PULSE_LINES  2
#define H_BACK_PORCH_LINES   31
#define H_ACTIVE_VIDEO_LINES 480

int nb_line_modes[] = {
  H_SYNCH_PULSE_LINES,
  H_BACK_PORCH_LINES,
  H_ACTIVE_VIDEO_LINES,
  H_FRONT_PORCH_LINES,
};

#define V_SYNCH_PULSE_TICKS  61
#define V_BACK_PORCH_TICKS   30
#define V_ACTIVE_VIDEO_TICKS 406
#define V_FRONT_PORCH_TICKS  10
#define V_FULL_LINE_NB_TICKS (V_SYNCH_PULSE_TICKS + \
                              V_BACK_PORCH_TICKS +  \
                              V_ACTIVE_VIDEO_TICKS + \
                              V_FRONT_PORCH_TICKS)

#define WAIT_VIDEO_INACTIVE()  do {} while (phase == ACTIVE_VIDEO)
#define WAIT_VIDEO_ACTIVE()  do {} while (phase != ACTIVE_VIDEO)

byte phase;
int line_number;

inline void incrementLineNumber() {
  line_number++;
  if (line_number == nb_line_modes[phase]) {
    line_number = 0;
    phase++;
    phase &= 0x03; // Take adventage that we have 4 phases.
  }
}

inline void waitTimer1(byte tick) {
  do { } while(TCNT1L < tick );
}

// vertical sync pulse start
ISR(TIMER1_CAPT_vect) {
  // wait for the end of the back porch
  waitTimer1(V_SYNCH_PULSE_TICKS+V_BACK_PORCH_TICKS);
  
  switch (phase) {
    case FRONT_PORCH:
      if (line_number == (H_FRONT_PORCH_LINES - 1)) {
        // end of front porch, start of Vsynch pulse
        bitClear(PORT(SYNCH_PORT), HSYNCH_PIN);
      }
      break;
    case SYNCH_PULSE:
      if (line_number == (H_SYNCH_PULSE_LINES - 1)) {
        // end of Vsynch pulse, start of back porch
        bitSet(PORT(SYNCH_PORT), HSYNCH_PIN);
      }
      break;
    case ACTIVE_VIDEO:
      {
        PORT(RGB_PORT) = RED;
        waitTimer1(
          V_SYNCH_PULSE_TICKS+
          V_BACK_PORCH_TICKS+
          (V_ACTIVE_VIDEO_TICKS/8)
        );
        PORT(RGB_PORT) = GREEN;
        waitTimer1(
          V_SYNCH_PULSE_TICKS+
          V_BACK_PORCH_TICKS+
          ((V_ACTIVE_VIDEO_TICKS * 2)/8)
        );
        PORT(RGB_PORT) = BLUE;
        waitTimer1(
          V_SYNCH_PULSE_TICKS+
          V_BACK_PORCH_TICKS+
          ((V_ACTIVE_VIDEO_TICKS * 3)/8)
        );
        PORT(RGB_PORT) = CYAN;
        waitTimer1(
          V_SYNCH_PULSE_TICKS+
          V_BACK_PORCH_TICKS+
          ((V_ACTIVE_VIDEO_TICKS * 4)/8)
        );
        PORT(RGB_PORT) = YELLOW;
        waitTimer1(
          V_SYNCH_PULSE_TICKS+
          V_BACK_PORCH_TICKS+
          ((V_ACTIVE_VIDEO_TICKS * 5)/8)
        );
        PORT(RGB_PORT) = MAGENTA;
        waitTimer1(
          V_SYNCH_PULSE_TICKS+
          V_BACK_PORCH_TICKS+
          ((V_ACTIVE_VIDEO_TICKS * 6)/8)
        );
        PORT(RGB_PORT) = BLACK;
        waitTimer1(
          V_SYNCH_PULSE_TICKS+
          V_BACK_PORCH_TICKS+
          ((V_ACTIVE_VIDEO_TICKS * 7)/8)
        );
        PORT(RGB_PORT) = WHITE;
        waitTimer1(
          V_SYNCH_PULSE_TICKS+
          V_BACK_PORCH_TICKS+
          V_ACTIVE_VIDEO_TICKS
        );
        PORT(RGB_PORT) = (_BV(VSYNCH_PIN)|_BV(HSYNCH_PIN));
      }
      break;
  }
  incrementLineNumber();
}

void init_video() {
  // Set pins for color and synch as output
  DDR(RGB_PORT) = (_BV(RED_PIN)|_BV(GREEN_PIN)|_BV(BLUE_PIN));
  DDR(SYNCH_PORT) = (_BV(VSYNCH_PIN)|_BV(HSYNCH_PIN));
  
  PORT(RGB_PORT) = 0;
  PORT(SYNCH_PORT) = (_BV(VSYNCH_PIN)|_BV(HSYNCH_PIN));
  
  TCCR1A = 0; // disable all first.
  TCCR1B = 0;
  TCCR1C = 0;
  cli();
  // init line/phase status
  line_number = 0;
  phase = FRONT_PORCH;
  // init timing
  TCNT1 = 0;
  OCR1A = V_SYNCH_PULSE_TICKS;
  ICR1 = V_FULL_LINE_NB_TICKS;
  TIMSK1 = _BV(ICIE1);
  TCCR1A = _BV(COM1A1)|_BV(COM1A0)|_BV(WGM11);
  TCCR1B = _BV(WGM13)|_BV(WGM12)|_BV(CS10);
  sei();
}

void setup () {
  init_video();
}

void loop () {
}
