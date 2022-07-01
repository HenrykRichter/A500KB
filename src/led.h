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

/* apply new configuration commands from host (command stream by serial input) 

   returns: 0 = no special action for caller
           >0 = number of bytes to send back to Amiga (typically config bytes)
	   -1 = save configuration requested
*/
char led_putcommands( unsigned char *recvcmd, unsigned char nrecv );

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
/* get keyboard type/version */
#define LEDCMD_GETVERSION 0xA0
/* set LED mode, 1 byte argument */
#define LEDCMD_SETMODE    0xC0

/* Please note that the protocol is designed for short packets to avoid
   overflows in send/receive buffers. As a consequence, only one command
   with return values (from Keyboard to Amiga) may be issued at a time.
   This limitation concerns LEDCMD_GETVERSÌON, LEDCMD_GETCONFIG and
   LEDCMD_SAVECONFIG (asynchronous EEPROM write, where the command is
   acknowledged first and some seconds take place for the writes itself). 
   Use only one of these commands at a time.
*/

/* Arguments for GETVERSION */
#define LEDGV_HEADER     0xBA /* */
#define LEDGV_TYPE_A500  0x01 /* 7 LEDs */
#define LEDGV_TYPE_A3000 0x02 /* 1 LED only */
#define LEDGV_VERSION    0x02 /* software version (1=initial, 2=with mode support) */

/* LED MODES */
#define LEDM_STATIC  0
#define LEDM_RAINBOW 1	/* HSV rainbow */
#define LEDM_PULSE   2  /* Pulsation   */
#define LEDM_SAT     3  /* Saturation up/down */

#endif
