/* led controller functions */
#ifndef _INC_LED_DIGITAL_H
#define _INC_LED_DIGITAL_H

#include "kbdefs.h"

/* number of LEDs */
#define N_DIGI_LED         15

char led_digital_step();
void led_digital_updown(unsigned char code, unsigned char leftright);

#endif
