/* led controller functions */
#ifndef _INC_LED_H
#define _INC_LED_H

#include "kbdefs.h"

/* init LED controller (twi etc) */
void led_init();

/* scan inputs, return current state of all inputs (returns quickly) */
unsigned char led_getinputstate();

/* update controller based on input state */
unsigned char led_updatecontroller( unsigned char state );

/* save current configuration */
void led_saveconfig( char );

/* apply new configuration commands from host (command stream by serial input) */
unsigned char led_putcommands( unsigned char *recvcmd, unsigned char nrecv );

/* force flag for led_updatecontroller() */
#define LED_FORCE_UPDATE 0x80

/* The commands are located in the upper 3 bits */
/* the lower 5 bits denote the LED index        */
#define LEDCMD_MASK   0xE0
#define LEDINDEX_MASK 0x1F
/* source change (1 byte argument) */
#define LEDCMD_SOURCE 0x20
/* color change  (4 byte argument) = state,R,G,B */
#define LEDCMD_COLOR  0x40
/* get config (1 byte argument which is currently 0) */
#define LEDCMD_GETCONFIG 0x60
/* save to EEPROM (no argument) */
#define LEDCMD_SAVECONFIG 0x80


#endif
