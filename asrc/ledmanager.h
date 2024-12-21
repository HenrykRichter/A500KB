/*

  The LEDmanager maintains current (and default) settings for the 7 LEDs
  on the Keyboard. It also communicates with the Keyboard by sending/receiving
  configuration data across the serial link.

  The definitions in ledmanager.h should be kept to match the
  implementation on the other end (see led.c in the AVR sources).

*/
#ifndef _INC_LEDMANAGER_H
#define _INC_LEDMANAGER_H

#include <exec/types.h>
#include "ciacomm.h"

/* 7 RGB LEDs are on the prototype board */
#define N_LED	7
#define N_DIGITAL_LED 1 /* V5/V6: digital LED */

/* virtual 9th LED for save config command */
#define LEDIDX_SAVEEEPROM (N_LED+N_DIGITAL_LED)

/* possible LED states */
#define LED_IDLE      0 /* idle             */
#define LED_ACTIVE    1 /* primary   active */
#define LED_SECONDARY 2 /* secondary active */
#define LED_STATES    3

/* LED sources */
#define LEDB_SRC_POWER  0
#define LEDB_SRC_FLOPPY 1
#define LEDB_SRC_IN3    2
#define LEDB_SRC_IN4    3
#define LEDB_SRC_CAPS   4
#define LEDB_SRC_SWAP     7 /* Keyboard send: swap primary/secondary */
#define LEDB_SRC_INACTIVE 8 /* dummy in software configurator */
#define LEDF_SRC_POWER  (1<<LEDB_SRC_POWER)
#define LEDF_SRC_FLOPPY (1<<LEDB_SRC_FLOPPY)
#define LEDF_SRC_IN3    (1<<LEDB_SRC_IN3)
#define LEDF_SRC_IN4    (1<<LEDB_SRC_IN4)
#define LEDF_SRC_CAPS   (1<<LEDB_SRC_CAPS)
#define LEDF_SRC_SWAP   (1<<LEDB_SRC_SWAP)
#define LEDF_SRC_INACTIVE (1<<LEDB_SRC_INACTIVE) /* dummy in software configurator */

/* Make sure, these are in sync with implementation on KB */
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
/* save config (o argument) */
#define LEDCMD_SAVEEEPROM 0x80
/* get keyboard type/version */
#define LEDCMD_GETVERSION 0xA0
/* set mode (static, cycle etc.), 1 byte argument */
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
#define LEDGV_TYPE_A500MINI 0x03
#define LEDGV_VERSION    0x01 /* software version */


/* PUBLIC PROTO */

/* startup */
LONG ledmanager_init(void);

/* copy loaded config from target keyboard to active variables */
LONG ledmanager_loadconfigentry( LONG led, UBYTE *recvbuf, LONG nbytes, ULONG flags );
/* send one LED's config to keyboard (src,rgb,mode) */
LONG ledmanager_sendConfig(LONG led);
/* store config in Keyboard's eeprom */
LONG ledmanager_saveEEPROM(void);

LONG ledmanager_getColor(LONG led,LONG state,LONG rgb);
LONG ledmanager_getSrc(LONG led,LONG state);
LONG ledmanager_getMode(LONG led);

void ledmanager_setColor(LONG led,LONG state,LONG rgb,LONG col);
void ledmanager_setSrc( LONG led, LONG src, LONG state);
void ledmanager_setMode(LONG led, LONG mode);

/* quit */
LONG ledmanager_exit(void);

/* load/save preset (FILE) */
LONG ledmanager_loadpresets( STRPTR fname );
LONG ledmanager_savepresets( STRPTR fname );

#endif /* _INC_LEDMANAGER_H */
