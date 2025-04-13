/*
 ********************************************************************************
 * main.c                                                                       *
 *                                                                              *
 * Author: Henryk Richter <bax@comlab.uni-rostock.de>                           *
 *                                                                              *
 * Purpose: Amiga keyboard based on ATMega32                                    *
 *                                                                              *
 * DEBUG NOTES: RXD (D2) / TXD (D3) are the same pins used for KBClock (D2) and *
 *              KBData (D3)                                                     *
 * Hence, if DEBUG is defined (see Makefile), then KBClock goes to F2 and       *
 *        KBData to F3, i.e. IN_LED3,IN_LED4                                    *
 *                                                                              *
 *                                                                              *
 *                                                                              *
 *                                                                              *
 ********************************************************************************
*/
#define ENABLE_WATCHDOG WDTO_250MS
#define ENABLE_USB
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/io.h>
#include <util/delay.h> /* might be <avr/delay.h>, depending on toolchain */
#ifdef ENABLE_WATCHDOG
#include <avr/wdt.h>
#endif
//#include "avr/delay.h"
#include "baxtypes.h"
#include "twi.h"
#include "led.h"
#include "spi.h"
#include "led_digital.h"
#ifdef ENABLE_USB
#include "usb.h"
#endif /* ENABLE_USB */

#include "kbdefs.h"

//#define DEBUGONLY
#ifdef DEBUG
#include "uart.h"
#define DBGOUT(a) uart1_putc(a);
#define DEBUG_ON()
#else
#define DBGOUT(a)
#define DEBUG_ON()
#endif

#define NULL (0)

/* set/clear Bit in register or port */
#define BitSet( _port_, _bit_ ) _port_ |=   (1 << (_bit_) )
#define BitClr( _port_, _bit_ ) _port_ &= (~(1 << (_bit_) ) )
/* copy bit from source to destination (2 clocks via T Bit) */
#define BitCpy( _out_, _obit_, _in_, _ibit_ ) \
	asm volatile ( "bst %2,%3" "\n\t" \
	               "bld %0,%1" : "+r" (_out_) : "I" (_obit_) , "r" (_in_) , "I" (_ibit_) );


/* internal proto */
char amiga_kbsend( unsigned char scan_internal, unsigned char updown );
char amiga_kbsync( void );

/* ringbuffer defs and protos */
typedef unsigned char RING_TYPE;
typedef unsigned char RINGPOS_TYPE;
void init_ring( void );
char write_ring( RING_TYPE val );
char read_ring( RING_TYPE *val );

/* put number on UART */
void uart_puthexuchar(unsigned char a);
void uart_puthexuint(uint16_t a);

/* regular caps on/off = 1/0, with force flag 0x40 */
void show_caps( unsigned char state );


unsigned char *recv_commands(unsigned char *nrecv);


/* active key table -> bytes here, could be mapped to bits */
unsigned char kbtable[(OCOUNT+OCOUNT_SPC)*ICOUNT];

/* commands from Amiga, USB LED configuration */
#define RECVBUFSIZE 32 
unsigned char recv_buffer[RECVBUFSIZE];

#define KEYIDLE 0 /* keyidle / keydown should only use one bit (!) */
#define KEYDOWN 1
#define DEBOUNCE_TIME  32  /* 0...127 -> number of iterations to wait for key to settle */
#define DEBOUNCE_SHIFT   1 /* debounce count is shifted by this in kbtable              */
#define DEBOUNCE_MASK  127 /* mask to fetch debounce count                              */

#define GETKEYSTATE(_a_) kbtable[_a_]
#define SETKEYSTATE(_a_,_b_) kbtable[_a_] = _b_



/* port mapping, base ADDRESS is port D */
#define PDOFF 0x0
#define PAOFF _SFR_ADDR(DDRA)-_SFR_ADDR(DDRD)
#define PBOFF _SFR_ADDR(DDRB)-_SFR_ADDR(DDRD)
#define PCOFF _SFR_ADDR(DDRC)-_SFR_ADDR(DDRD)
#define PEOFF _SFR_ADDR(DDRE)-_SFR_ADDR(DDRD)


/* states */
#define STATE_RESYNC	1	/* sync loss */
#define STATE_POWERUP	2	/* after powerup or reset */
#define STATE_RESET	4	/* sent reset warning */
#define STATE_KBWAIT	8	/* wait for KB ACK low */
#define STATE_KBWAIT2	16	/* wait for KB ACK high (after low) */
#define STATE_POWERUP2	32	/* remember that we have to send end of powerup sequence */
#define STATE_OVERFLOW	64	/* buffer overflow */

/* waiting time for sync (in 10 us units) = 143000 us = 143 ms */
#define SYNC_WAIT	14300
/* waiting time for reset (in 10 us units) = 10ms+500ms */
#define RESET_WAIT	60000
/* waiting time before rest is issued */
#define RESET_WAIT1	20

/* delay in loops switching between send/receive modes */
#define KBDSEND_SWITCHDELAY 4
/* delay in loops after a key was acknowledged by remote end */
#define KBDSEND_KEYDELAY    2

/* map row/column to scan code, each start with 1 (labeling on board) */
#define SCANCODE(_row_,_column_) (((_row_)-1)*ICOUNT) + (_column_)-1

/* keyboard input table (kbinputlist is 0 terminated) */
                                 /* 1     2     3     4     5     6     7     8     9     10    11    12    13      */
//unsigned char  kbinputlist[14] =  { 1<<5, 1<<6, 1<<6, 1<<3, 1<<4, 1<<2, 1<<3, 1<<7, 1<<4, 1<<5, 1<<0, 1<<1, 1<<2,  0 };
//unsigned short kbinputports[14] = { PDOFF,PDOFF,PCOFF,PDOFF,PDOFF,PDOFF,PCOFF,PDOFF,PCOFF,PCOFF,PCOFF,PCOFF,PCOFF, 0 };

unsigned char kbinputlist[16]  = { 1<<0, 1<<1, 1<<2, 1<<3, 1<<4, 1<<5, 1<<6, 1<<7, 1<<0, 1<<1, 1<<2, 1<<3, 1<<4, 1<<5, 1<<6 ,0};
unsigned short kbinputports[16]= { PCOFF,PCOFF,PCOFF,PCOFF,PCOFF,PCOFF,PCOFF,PCOFF,PEOFF,PEOFF,PEOFF,PEOFF,PEOFF,PEOFF,PEOFF,0};

//unsigned char kbinputlist[14] =  { 0xFC,   0x7F,  0 };
//unsigned char kbinputports[14] = { PORTDOFF,  PORTCOFF, 0 };

/* mapping from our keyboard layout to Amiga scan codes */
/* 
   The last row are the special keys which are scanned key by key, beginning with
   scancode 7,1. Please note that the order of keys in the last row needs to 
   match the order in "kbinputspecials".

   scan codes 0x47-0x49,0x68-0x7F are unused
*/
const unsigned char kbmap[(OCOUNT+OCOUNT_SPC)*ICOUNT] PROGMEM = {
/*help F10  F9   F8   F7   KP/  F6   KP]  F5   F4   F3   F2   F1   KP[  ESC  */
  0x5f,0x59,0x58,0x57,0x56,0x5c,0x55,0x5b,0x54,0x53,0x52,0x51,0x50,0x5a,0x45,
/*cu   \    +-   -_   0    9    8    7    6    5    4    3    2    1    '    */
  0x4c,0x0d,0x0C,0x0B,0x0A,0x09,0x08,0x07,0x06,0x05,0x04,0x03,0x02,0x01,0x00,
/*cl   Ret  ]}   [{   P    O    I    U    Y    T    R    E    W    Q    Tab  */
  0x4f,0x44,0x1B,0x1A,0x19,0x18,0x17,0x16,0x15,0x14,0x13,0x12,0x11,0x10,0x42,
/*cr   Del  #    .,   ;:   L    K    J    H    G    F    D    S    A    Caps */
  0x4e,0x46,0x2B,0x2A,0x29,0x28,0x27,0x26,0x25,0x24,0x23,0x22,0x21,0x20,0x62,
/*cd   BKSpC SPC N/A  /?   .>   ,<   M    N    B    V    C    X    Z    <>   */
  0x4d,0x41,0x40,0x40,0x3A,0x39,0x38,0x37,0x36,0x35,0x34,0x33,0x32,0x31,0x30,
/*Num- Num0 Num1 Num4 Num7 KPENTNum2 Num5 Num8 Num. Num3 Num6 Num9 Num+ Num* */
  0x4a,0x0F,0x1D,0x2D,0x3D,0x43,0x1E,0x2E,0x3E,0x3C,0x1F,0x2F,0x3F,0x5E,0x5D,
/* LA  LALT LSH  CTRL RA   RALT RSHIFT                                       */
  0x66,0x64,0x60,0x63,0x67,0x65,0x61
};
unsigned char kbinputspecials[8]  = { 1<<SPCB_LAMIGA, 1<<SPCB_LALT,1<<SPCB_LSHIFT,1<<SPCB_CTRL,
                                      1<<SPCB_RAMIGA, 1<<SPCB_RALT,1<<SPCB_RSHIFT,0};

#define D70 0x00
#define D71 0x01
#define D72 0x02
#define D73 0x03
#define D74 0x04
#define D75 0x05
#define D76 0x06
#define D77 0x07
#define D78 0x08
#define D79 0x09
#define D80 0x0A
#define D81 0x0B
#define D82 0x0C
#define D83 0x0D
#define D84 0x0E
#define D8INVAL 0x0f
/*
 map denoting how far left/right a pressed key is in the layout

 15 LEDs, where D70-D81 are more or less in a row (left-to-right), while 
 D82,D83,D84 are below them and go leftward

 0x00 0x01 0x02 0x03 0x04 0x05 0x06 0x07 0x08 0x09 0x0a 0x0b 0x0c 0x0d 0x0e 
 D70  D71  D72  D73  D74  D75  D76  D77  D78  D79  D80  D81  D82  D83  D84 

 L-R are indices 0-9, i.e ESC-F10

 right now, all keys are marked with something, see D8INVAL if necessary
 to denote an invalid key

*/
const unsigned char kbleftright[(OCOUNT+OCOUNT_SPC)*ICOUNT] PROGMEM = {
/*help F10  F9   F8   F7   KP/  F6   KP]  F5   F4   F3   F2   F1   KP[  ESC  */
   D81, D78, D77, D76, D76, D81, D75, D81, D74, D73, D72, D72, D71, D81, D70,
/*cu   \    +-   -_   0    9    8    7    6    5    4    3    2    1    '    */
   D80, D78, D77, D77, D76, D76, D75, D75, D74, D73, D73, D72, D72, D71, D70,
/*cl   Ret  ]}   [{   P    O    I    U    Y    T    R    E    W    Q    Tab  */
   D84, D79, D78, D77, D77, D76, D75, D75, D74, D74, D73, D72, D72, D71, D70,
/*cr   Del  #    .,   ;:   L    K    J    H    G    F    D    S    A    Caps */
   D82, D79, D78, D77, D77, D76, D76, D75, D74, D74, D73, D73, D72, D71, D70,
/*cd   BKSpC SPC N/A  /?   .>   ,<   M    N    B    V    C    X    Z    <>   */
   D83, D79, D74, D84, D77, D77, D77, D75, D75, D74, D74, D73, D72, D72, D71,
/*Num- Num0 Num1 Num4 Num7 KPENTNum2 Num5 Num8 Num. Num3 Num6 Num9 Num+ Num* */
   D81, D82, D82, D82, D81, D82, D82, D82, D81, D82, D82, D82, D81, D82, D81,
/* LA  LALT LSH  CTRL RA   RALT RSHIFT                                       */
   D70, D71, D70, D70, D78, D78, D84
};


#ifdef ENABLE_USB
/* mapping from our keyboard layout to USB HID scan codes */
/* 
   The last row are the special keys which are scanned key by key, beginning with
   scancode 7,1. Please note that the order of keys in the last row needs to 
   match the order in "kbinputspecials".

   scan codes 0x47-0x49,0x68-0x7F are unused

   KEY_HASHTILDE 0x32 	Keyboard Int' # and ~ 	Intended for key next to vertical Return key. Keyboard Backslash (0x31) is used instead
   \ KEY_BACKSLASH needs update
*/
#include "usb_keys.h"
const unsigned char usbkbmap[(OCOUNT+OCOUNT_SPC)*ICOUNT] PROGMEM = {
/* help      F10       F9      F8      F7      KP/          F6      KP]   F5     F4     F3     F2     F1      KP[   ESC  */
 KEY_HELP, KEY_F10,  KEY_F9, KEY_F8, KEY_F7, KEY_KPSLASH, KEY_F6, KEY_SCROLLLOCK, KEY_F5,KEY_F4,KEY_F3,KEY_F2,KEY_F1, KEY_NUMLOCK,  KEY_ESC, 
/*   KEY_HELP, KEY_F10,  KEY_F9, KEY_F8, KEY_F7, KEY_KPSLASH, KEY_F6, KEY_KPRBRACE,   KEY_F5,KEY_F4,KEY_F3,KEY_F2,KEY_F1, KEY_KPLBRACE, KEY_ESC,*/
/*cu        \             +-         -_         0     9     8     7     6     5     4     3     2     1     '~          */
  KEY_UP,   KEY_BACKSLASH,KEY_EQUAL, KEY_MINUS, KEY_0,KEY_9,KEY_8,KEY_7,KEY_6,KEY_5,KEY_4,KEY_3,KEY_2,KEY_1,KEY_GRAVE,
/*cl        Ret       ]}              [{             P     O     I     U     Y     T     R     E     W     Q     Tab  */
  KEY_LEFT, KEY_ENTER,KEY_RIGHTBRACE, KEY_LEFTBRACE, KEY_P,KEY_O,KEY_I,KEY_U,KEY_Y,KEY_T,KEY_R,KEY_E,KEY_W,KEY_Q,KEY_TAB,
/*cr        Del        #              '"              ;:             L     K     J     H     G     F     D     S     A     Caps */
  KEY_RIGHT,KEY_DELETE,KEY_HASHTILDE, KEY_APOSTROPHE, KEY_SEMICOLON, KEY_L,KEY_K,KEY_J,KEY_H,KEY_G,KEY_F,KEY_D,KEY_S,KEY_A,KEY_CAPSLOCK,
/*cd        BKSpC         SPC        N/A      /?         .>       ,<         M     N     B     V     C     X     Z     <>   */
  KEY_DOWN, KEY_BACKSPACE,KEY_SPACE, KEY_DOT, KEY_SLASH, KEY_DOT, KEY_COMMA, KEY_M,KEY_N,KEY_B,KEY_V,KEY_C,KEY_X,KEY_Z,KEY_102ND, /* 102ND is between Shift and Z */
/*Num-      Num0  Num1  Num4  Num7  KPENT Num2  Num5  Num8  Num.  Num3  Num6  Num9  Num+  Num* */
KEY_KPMINUS,KEY_KP0, KEY_KP1, KEY_KP4, KEY_KP7, KEY_KPENTER, KEY_KP2, KEY_KP5, KEY_KP8, KEY_KPDOT, KEY_KP3, KEY_KP6, KEY_KP9, KEY_KPPLUS, KEY_KPASTERISK,
/* separate row, handled differently (part of the modifiers, not the sent keys */
/* LA  LALT LSH  CTRL RA   RALT RSHIFT                                       */
  KEY_MOD_LMETA,KEY_MOD_LALT,KEY_MOD_LSHIFT,KEY_MOD_LCTRL,KEY_MOD_RMETA,KEY_MOD_RALT,KEY_MOD_RSHIFT
};
#endif /* ENABLE_USB */

#define COMM_ACK   0x73
#define COMM_ACK1  0x7B
#define COMM_NACK  0x77
#define COMM_NACK1 0x7F

#if 0
/* ~  1    ESC  F1   F2   F3   F4   F5   F6   F7   F8   F9   F10    */
 0x00,0x01,0x45,0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,0x58,0x59,
/* 2  3    4    5    6    7    8    9    0    ß    ´    \    BKSPC  */
 0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x41,
/*TAB 'q', 'w', 'e', 'r', 't', 'z', 'u', 'i', 'o', 'p', 'ü', '+',   */
 0x42,0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1a,0x1b,
/*CTRL CAPS, a  s    d    f    g    h    j    k    l    ö    ä      */
 0x63,0x62,0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,0x29,0x2a,
/* Sh  <   y    x    c    v    b    n    m    ,    .    -     Shift */ 
 0x60,0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x3a,0x61,
/*ALT AMIGA  xx   xx   xx   xx SPACE  xx  xx Amiga Alt  #   Return  */
 0x64,0x66,0x5d,0x5d,0x5d,0x5d,0x40,0x5d,0x5d,0x67,0x64,0x2b,0x44,
/* KP. KP0 xx,  KP1  KP2  KP3  KPEnt  cd   cl   cr   cu help del    */
 0x3c,0x0f,0x5d,0x1d,0x1e,0x1f,0x43,0x4d,0x4f,0x4e,0x4c,0x5f,0x46,
/*KP] KP[  KP/  KP4  KP5  KP6  xx   KP+  KP-  KP*  KP9  KP8  KP7   */
 0x5b,0x5a,0x5c,0x2d,0x2e,0x2f,0x5d,0x5e,0x4a,0x5d,0x3f,0x3e,0x3d,
};
#endif

/* Amiga Keycodes */
#define KEYCODE_RESET_WARN		0x78
#define KEYCODE_RETRANSMIT		0xf9
#define KEYCODE_BUFFER_OVERFLOW		0xfa
#define KEYCODE_KEYBOARD_FAIL		0xfc
#define KEYCODE_POWERUPSTREAM_START	0xfd
#define KEYCODE_POWERUPSTREAM_STOP	0xfe

/* LAMIGA R5 C1 RAMIGA R5 C9 CTRL R3 C0 */
#define SCANCODE_LAMIGA   SCANCODE(7,1)
#define SCANCODE_RAMIGA   SCANCODE(7,5)
#define SCANCODE_CTRL     SCANCODE(7,4)
#define SCANCODE_CAPSLOCK SCANCODE(4,15)

/* ringbuffer for sending */
#define SENDBBUFFER_SIZE 32 
RING_TYPE sendbuffer[SENDBBUFFER_SIZE];
RINGPOS_TYPE ringw,ringr; /* ringbuffer positions for sending/receiving */

#ifdef DEBUG
//static char debug_on = 0;
/* for debugging only */
const char debuglist[(OCOUNT+OCOUNT_SPC)*ICOUNT] PROGMEM = {
/*help F10  F9   F8   F7   KP/  F6   KP]  F5   F4   F3   F2   F1   KP[  ESC  */
  'H', '0', '9', '8', '7', '/', '6', ']', '5', '4', '3', '2', '1', '[', 'E',
/*cu   \    +-   -_   0    9    8    7    6    5    4    3    2    1    '    */
  'U', '\\', '+', '-', '0','9','8', '7', '6', '5', '4', '3', '2', '1', '`',
/*cl   Ret  ]}   [{   P    O    I    U    Y    T    R    E    W    Q    Tab  */
  'L', 10  , ']','[','P', 'O', 'I', 'U', 'Y', 'T', 'R', 'E', 'W', 'Q',  8,
/*cr   Del  #    .,   ;:   L    K    J    H    G    F    D    S    A    Caps */
  'R', 'D' ,'#', '.', ';','L', 'K', 'J', 'H', 'G', 'F', 'D', 'S', 'A', 'C', 
/*cd   BKSpC SPC N/A  /?   .>   ,<   M    N    B    V    C    X    Z    <>   */
  'D', '<', ' ', '!', '/', '.' ,',','M', 'N', 'B', 'V', 'C', 'X', 'Z', '<',
/*Num- Num0 Num1 Num4 Num7 ENT  Num2 Num5 Num8 Num. Num3 Num6 Num9 Num+ Num* */
  '-', '0', '1', '4', '7', 10, '2', '5', '8', '.', '3', '6', '9', '+', '*',
/* LA  LALT LSH  CTRL RA   RALT RSHIFT                                       */
  'A', 'a', 's', 'c', 'A', 'a', 's'
};
#endif

#if 0
/* LED controller address (twi.c needs address >>1) */
#define I2CADDRESS (0x68>>1)
/* 
  init string: 
   length, sequence
  terminated with length 0
*/
#define MAXLEDINITSEQ 8
const unsigned char ledinitlist[] PROGMEM = {
 2, 0x00, 0x07, /* [control register 0],[16 bit (0x6), normal op (0x1), 16 Bit (%000<<4)] */
 2, 0x6E, 0xFF, /* [Global Current Control],[enable full current (0x1-0xFF)]              */
 2, 0x70, 0xBF, /* [Phase Delay and Clock Phase],[PDE=1,PSn=1] */
 /* end of list */
 0
};

unsigned char twirec;
void twi_callback(uint8_t adr, uint8_t *data)
{
	twirec=*data;
}
#endif

unsigned char caps_on; /* for LED controller */

#ifdef ENABLE_USB
/*
  entering here only makes sense once get_usb_config_status() returns something >0

  TODO: scancode translation, usb_send()

*/
void mainloop_usb(void)
{
  unsigned char i,j,pos; // ledstat
  volatile unsigned char cur;
  unsigned char mods,actct;
  unsigned char mod1,act0,kbled,trig;
  unsigned char *recb;
  uint8_t st;

  kbled   = 0x7F;	/* illegal, so we force an update on CapsLock */
//  caps_on = 0;          /* getting into USB mode: assume caps off */
//  show_caps( caps_on ); /* set caps off once we have USB sync     */

  recb = recv_buffer;
  /* power */
  *recb++ = LEDCMD_COLOR + 3; /* power 0 */
  *recb++ = 0; /* idle */
  *recb++ = 0x20;
  *recb++ = 0x00;
  *recb++ = 0x00;
  *recb++ = LEDCMD_COLOR + 4;
  *recb++ = 0;
  *recb++ = 0x00;
  *recb++ = 0x20;
  *recb++ = 0x00;
  *recb++ = LEDCMD_COLOR + 5;
  *recb++ = 0;
  *recb++ = 0x00;
  *recb++ = 0x00;
  *recb++ = 0x20;
 
  *recb++ = LEDCMD_COLOR + 0; /* floppy 0 */
  *recb++ = 0; /* idle */
  *recb++ = 0x00;
  *recb++ = 0x00;
  *recb++ = 0x00;
  *recb++ = LEDCMD_COLOR + 1; /* floppy 1 */
  *recb++ = 0;
  *recb++ = 0x00;
  *recb++ = 0x00;
  *recb++ = 0x00;
  *recb++ = LEDCMD_COLOR + 2;
  *recb++ = 0;
  *recb++ = 0x00;
  *recb++ = 0x00;
  *recb++ = 0x00;

  led_putcommands( recv_buffer, recb - recv_buffer );
  recb = recv_buffer;

  *recb++ = LEDCMD_COLOR + 0; /* floppy 0 */
  *recb++ = 1; /* active */
  *recb++ = 0x00;
  *recb++ = 0x50;
  *recb++ = 0x20;
  *recb++ = LEDCMD_COLOR + 1; /* floppy 1 */
  *recb++ = 1;
  *recb++ = 0x00;
  *recb++ = 0x45;
  *recb++ = 0x25;
  *recb++ = LEDCMD_COLOR + 2;
  *recb++ = 1;
  *recb++ = 0x00;
  *recb++ = 0x40;
  *recb++ = 0x30;
   
  /* configure sources for Power,Power,Power,Floppy,In3,In4 */
  *recb++ = LEDCMD_SOURCE + 3;
  *recb++ = LEDF_SRC_POWER; 
  *recb++ = LEDCMD_SOURCE + 4;
  *recb++ = LEDF_SRC_POWER; 
  *recb++ = LEDCMD_SOURCE + 5;
  *recb++ = LEDF_SRC_POWER; 
  *recb++ = LEDCMD_SOURCE + 0;
  *recb++ = LEDF_SRC_FLOPPY;
  *recb++ = LEDCMD_SOURCE + 1;
  *recb++ = LEDF_SRC_IN3; 
  *recb++ = LEDCMD_SOURCE + 2;
  *recb++ = LEDF_SRC_IN4;

  led_putcommands( recv_buffer, recb - recv_buffer );
  recb = recv_buffer;

  *recb++ = LEDCMD_SETMODE + 3;
  *recb++ = 0;
  *recb++ = LEDCMD_SETMODE + 4;
  *recb++ = 0;
  *recb++ = LEDCMD_SETMODE + 5;
  *recb++ = 0;

  led_putcommands( recv_buffer, recb - recv_buffer );
  recb = recv_buffer;


  /* default: all off */
  led_setinputstate( LEDF_SRC_POWER,  0 );
  led_setinputstate( LEDF_SRC_FLOPPY, 0 );
  led_setinputstate( LEDF_SRC_IN3,    0 );
  led_setinputstate( LEDF_SRC_CAPS,   0 );
  st = led_setinputstate( LEDF_SRC_IN4,    0 );
  led_updatecontroller(st|LED_FORCE_UPDATE); /* */

  mod1=0;
  act0=0;
  while( get_usb_config_status() != 0 )
  {
	/* loop through kbd output bits and collect input bits */
	pos = 0; /* first key in list */
	actct = 0;
	trig  = 0; /* trigger for USB interrupt to send something */
#ifdef ENABLE_WATCHDOG
	wdt_reset();    /* we're alive (!) */
#endif
	for( j=OSTART; j != 0 ; j <<= 1 )
	{
	  if( !(j & OMASK ) )	/* oport bit inactive ? */
	 	continue;

	  OPORT =  (OPORT|(OMASK)) ^ j; /* make current low, set rest to high */

	  /* this delay could be a bit higher considering impedances/capacitance across the keyboard */
	  /* e.g. 10-20 us delay + check if KBClock is low from the other end */
	  /* also: differentiate between KBWAIT and idle or include a short loop */
	  _delay_us(5); /* wait a little (200 kHz @ 5us) */
	  /*--------------------------------------------------------*/ 

	  /* -------------------------------------------------------*/
	  /* loop through input ports, put new detected keys into   */
	  /* ringbuffer                                             */
	  /*                                                        */
	  for( i=0 ;  (kbinputlist[i] != 0) ; i++ )
	  {
		/* get port state and compare with kbtable */
		cur = *(&PIND + kbinputports[i] ) & kbinputlist[i]; /* */
		cur = (cur) ? KEYIDLE : KEYDOWN; /* low active */

		/* include in sent list, if down (after debounce) */
		if( (cur == KEYDOWN) && (actct < USB_KB_NKEYS ) )
		{
			keyboard_pressed_keys[actct] = pgm_read_byte(&usbkbmap[pos]);
			actct++;
			trig = 1;
			DBGOUT( pgm_read_byte(&debuglist[pos] )  )
		}
		pos++;	/* position in key list */
	  }
	  /*--------------------------------------------------------*/ 
	}

	/* handle extra keys, low active */
	pos = SCANCODE(7,1);  /* LAMIGA is the first special key                   */
	i = SPCPIN & SPCMASK; /* get special port reading (mask is redundant btw.) */
	j = 0;
	mods = 0;
	while( kbinputspecials[j] != 0 )
	{
		if( !(i & kbinputspecials[j] )) /* low ?             */
		{
			/* we have a modifier key */
			mods |= pgm_read_byte(&usbkbmap[pos]);
			trig  = 1;
			DBGOUT( pgm_read_byte(&debuglist[pos] )  )
		}
		pos++;
		j++;
	}
	keyboard_modifier = mods; /* in usb.c */


	if( act0 != actct )
		trig = 1;
	act0=actct;

	if( mods != mod1 )
		trig = 1;
	mod1 = mods;

	/* clear end of table */
	while( actct < USB_KB_NKEYS )
	{
		keyboard_pressed_keys[actct] = 0;
		actct++;
	}

	if( trig ) /* keys are down or went up */
	{
		usb_send();
		_delay_ms(10); /* wait a little */
	}

	/* LED update */
	if( kbled != keyboard_leds )
	{
	
		kbled = keyboard_leds;

		caps_on = (kbled & (1<<SET_LED_CAPS_B)) ? 1 : 0;
		show_caps( caps_on );
		led_setinputstate( LEDF_SRC_FLOPPY, caps_on );
		led_setinputstate( LEDF_SRC_CAPS, caps_on );

		st = (kbled & (1<<SET_LED_NUM_B)) ? 1 : 0;
		led_setinputstate( LEDF_SRC_IN3, st );
		st = (kbled & (1<<SET_LED_SCR_B))? 1 : 0;
		st = led_setinputstate( LEDF_SRC_IN4, st );

                led_updatecontroller(st); /* */

	}
  }
 
}
#endif /* ENABLE_USB */



int main(void)
{
  unsigned char i,j,pos,state; // ledstat
  unsigned char need_confeeprom = 0; /* 1 = config to EEPROM requested */
  unsigned char kbdsend_delay = 0; /* give host some time to switch between send/receive modes (in config tool) */
  unsigned short kbdwait = 0;
  unsigned short rstwait = 0;
  unsigned short keyb_idle = 0;
  unsigned char inputstate; /* track inputs (Power,Floppy,CapsLock,extra inputs) */
  volatile unsigned char cur;
  unsigned char caps;
  unsigned char nrecv=0; /* commands from Host */
  unsigned char *recvcmd;

  /* disable clock prescaler: this is usually intended to be done by "make fuse" (see MakeFile),
     but when that step is omitted, the device would run at 2 MHz instead of 16 */
  CLKPR = 0x80; /* enable prescaler change */
#if (F_CPU == 8000000 )
  CLKPR = 0x01; /* prescaler 2 (0x1) 1 (0x0) */
#else
  CLKPR = 0x00;
#endif
//  clock_prescale_set(clock_div_2);



//  cli();

#ifdef ENABLE_WATCHDOG
  /* watchdog, clocking */
  MCUSR &= ~(1 << WDRF); /* Disable watchdog if enabled by bootloader/fuses */
//      wdt_disable();
  wdt_enable(WDTO_8S);    /* we hang for more than 8s -> reset */
#endif

  /* initialize computer communication ports (D2/D3 regular, F2/F3 in debug) */
  KBDSEND_SENDD &= ~(1<<KBDSEND_SENDB); /* send data port to input */
  KBDSEND_SENDP |=  (1<<KBDSEND_SENDB); /* def: high = pullup on   */
  KBDSEND_CLKD  &= ~(1<<KBDSEND_CLKB);  /* clock port to input     */
  KBDSEND_CLKP  |=  (1<<KBDSEND_CLKB);  /* def: high = pullup on   */ 

  /* initialize regular Caps LED */
  caps     = KEYIDLE; /* caps lock treated separately */
  LEDDDR  |=   1<<LEDPIN;       /* output */
  LEDPORT &= ~(1<<LEDPIN);	/* off */

  caps_on  = 1;         /* for LED controller */
  show_caps( caps_on ); /* light CAPS-lock until sync with Amiga */
//  ledstat  =  (1<<LEDPIN);	/* on  */
//  LEDPORT |=  (1<<LEDPIN);	/* on = NPN transistor switches to GND = LED on */

  /* initialized output ports (def: high) */
  ODDR    |= OMASK;     /* output */
  OPORT   |= OMASK;     /* high (i.e. no active scan in progress) */
//  OPORT   &= ~(OMASK);  /* low */

  /* initialize Input ports */
  for( i=0 ; kbinputlist[i] != 0 ; i++ )
  {
	*(&DDRD + kbinputports[i] ) &= ~(kbinputlist[i]); /* clear DDR bits -> input         */
	*(&PORTD+ kbinputports[i] ) |= (kbinputlist[i]); /* clear PORT bits -> enable pullup */
	//*(&PORTD+ kbinputports[i] ) &= ~(kbinputlist[i]); /* clear PORT bits -> no pullup  */
  }

  /* special keys (ALT,SHIFT,AMIGA,CTRL) */
  SPCDDR  &= ~(SPCMASK); /* input */
  SPCPORT |= SPCMASK;    /* pullup on */

  /* reset line available (A500) ? */
#ifdef KBDSEND_RSTP
  KBDSEND_RSTDDR &= ~(1<<KBDSEND_RSTB); /* input */
  RST_PORTIDLE	//  KBDSEND_RSTP   |=  (1<<KBDSEND_RSTB); /* pull-up RST */
#endif

  /* LED sources */
  DRVLED_DDR  &= ~(1<<DRVLED_BIT);
  DRVLED_PORT |=  (1<<DRVLED_BIT); /* in, with pull-up */

  IN3LED_DDR  &= ~(1<<IN3LED_BIT);
  IN3LED_PORT |=  (1<<IN3LED_BIT);
  IN4LED_DDR  &= ~(1<<IN4LED_BIT);
  IN4LED_PORT |=  (1<<IN4LED_BIT);

  PLED_DDR    &=  ~(1<<PLED_BIT);
  PLED_PORT   &=  ~(1<<PLED_BIT); /* we use this as analog input */
  ADCSRA = 0x87;                  /* Enable ADC, fr/128  */
  ADMUX  = 0x40;                  /* Vref: Avcc, ADC channel: 0 (PortF on AT90USB1287 */

  /* spare inputs (used for options) */
  KBD_SPARE1DDR  &= ~(1<<KBD_SPARE1B);
  KBD_SPARE1PORT |=  (1<<KBD_SPARE1B); /* in, with pull-up */
  KBD_SPARE2DDR  &= ~(1<<KBD_SPARE2B);
  KBD_SPARE2PORT |=  (1<<KBD_SPARE2B); /* in, with pull-up */

  /* init SPI pins (CLK,DAT) for extra LED chain (V4 Keyboards) */
  /* TODO: check KBD_SPARE2PIN & (1<<KBD_SPARE2B) for mounted resistor */
  LCD_SPI_Start(); /* spi.c */

  /* initialize keyboard states */
  for( i=0 ; i < OCOUNT*ICOUNT ; i++ )
  {
  	kbtable[i] = KEYIDLE;
  }

  /* init USB */
#ifdef ENABLE_USB
  usb_init();
#endif /* ENABLE_USB */

  /* init UART for serial communication to PC */
#ifdef DEBUG
  uart1_init(UART_BAUD_SELECT(9600UL,F_CPU));
  uart1_puts("AMIGA 500 KEYBOARD BY BAX\r\n");
#endif

  led_init(); /* start up LED controller (and enable interrupts) */
  TCCR2A = 0; /* normal mode, OC0A disconnected, OC0B disconnected,   */
#if (F_CPU == 8000000 )
  TCCR2B = (1<<CS22) | (1<<CS21); /* prescaler 256 -> 8 MHz/256 = 31250 Hz counter  -> 8 ms before overflow */
#else
  TCCR2B = (1<<CS20) | (1<<CS21) | (1<<CS22); /* prescaler 1024 -> 16 MHz/1024 = 15625 Hz counter -> 16.3 ms before overflow */
#endif
  TCNT2  = 0x00; /* start timer with 0 */
  TIFR2  = 0x01; /* clear TOV0 overflow flag (write 1 to set flag to 0) */
// while( (TIFR2 & 0x01) == 0  ) /* break waiting loop after 4ms */

//  sei(); /* needed for TWI, USB (and UART in debug mode) */

/*
  Gamma table
  track LED source changes -> send updated colors

  LED source map for LED 0-6
  LED color table idle/active
  LED mode regular/special (knight rider, rainbow, cycle)
*/

#if 0
  ADMUX  = (ADMUX&0xE0)|(0x00 & 0x7); /* select ADC channel 0 (channels 0-7 in lower 3 bits) */
  ADCSRA |= (1<<ADSC); /* start ADC (single conversion) */
  while((ADCSRA&(1<<ADIF))==0); /* wait for ADC */
  _delay_us(10);
  { uint8_t  AinLow;
    uint16_t Ain;
    AinLow = (int)ADCL;		/* lower 8 bit */
    Ain = ((uint16_t)ADCH)<<8;	/* upper two bit, shifted */
    Ain = Ain|AinLow;

    uart_puthexuint(Ain);
    DBGOUT(13);
    DBGOUT(10);
  }
#endif

  /* prime LED controller: all on, then all off */
  twi_wait();
  led_updatecontroller(0xff); /* update will only start if TWI is idle */
  inputstate = led_getinputstate();
  twi_wait();
  led_updatecontroller(inputstate);

  /* print HSV2RGB conversion */
#if 0
//#ifdef DEBUG
  if(1)
  {
	uint8_t rgb[3];

	DBGOUT(13);
	DBGOUT(10);
	for( i=0 ; i<255; i++ )
	{
		HSV2RGB( rgb, ((int16_t)i)<<2, 255,255 );
		uart_puthexuchar( rgb[0] );

	DBGOUT(32);
		uart_puthexuchar( rgb[1] );
	DBGOUT(32);
		uart_puthexuchar( rgb[2] );
	    DBGOUT(13);
	    DBGOUT(10);
	}
  }
#endif

#ifdef ENABLE_WATCHDOG
  /* start watchdog (for real, this time) */
  wdt_disable();
  wdt_enable(ENABLE_WATCHDOG);    /* we hang for more than 250ms -> reset */
#endif

  /* */
  init_ring();	/* prepare ringbuffer */
  state = STATE_POWERUP; /* synchronize with Amiga, perform power-up procedure */
  while( 1 ) 
  {
	inputstate = led_getinputstate(); /* get current inputs state */
	keyb_idle++;

#ifdef ENABLE_WATCHDOG
	wdt_reset();    /* we're alive (!) */
#endif

	/* Digital LED strip update -> make sure not to miss KB ACK */
	if( !(state & STATE_KBWAIT) )
	{
	 if( TIFR2 & 0x01 ) /* timer overflow (16.3ms) */
	 {
		TIFR2  = 0x01; /* clear TOV0 overflow flag (write 1 to set flag to 0) */
		led_digital_step();
	 }

#ifdef ENABLE_USB
		/* jump to USB main loop when USB connection was established */
		if( get_usb_config_status() != 0 )
		{
#ifdef DEBUG
  			uart1_puts("Entering USB mode\r\n");
#endif
			mainloop_usb();
#ifdef DEBUG
  			uart1_puts("Left USB mode\r\n");
#endif
			continue;
		}	
#endif /* ENABLE_USB */
	}

	while( state & (STATE_POWERUP|STATE_RESYNC) )
	{
		kbdwait = 0;
		/* synchronize with Amiga, stay here until Amiga answers */
#ifdef ENABLE_WATCHDOG
		wdt_reset();    /* we're alive (!), needed here because we're in a loop for potentially a long time */
#endif

		/* LED control is part of KBSync Mode */
//		led_updatecontroller(inputstate); /* */
#ifndef DEBUGONLY
		if( amiga_kbsync() > 0 )
#endif
			break;

#ifdef ENABLE_USB
		/* jump to USB main loop when USB connection was established */
		if( get_usb_config_status() != 0 )
		{
#ifdef DEBUG
	  		uart1_puts("Entering USB mode\r\n");
#endif
			mainloop_usb();
#ifdef DEBUG
  			uart1_puts("Left USB mode\r\n");
#endif
			continue;
		}	
#endif /* ENABLE_USB */
		/* CTRL-A-A while waiting for sync? */
		if( !(SPCPIN & ( (1<<SPCB_CTRL)+(1<<SPCB_LAMIGA)+(1<<SPCB_RAMIGA) ) ))
		{
#ifdef KBDSEND_RSTP
			KBDSEND_RSTDDR |=  (1<<KBDSEND_RSTB); /* output */
			KBDSEND_RSTP   &= ~(1<<KBDSEND_RSTB); /* /RST */
#endif
			KBDSEND_CLKD |=  (1<<KBDSEND_CLKB);  /* switch to output */
			KBDSEND_CLKP &= ~(1<<KBDSEND_CLKB);  /* clock low */
		}
		else
		{
			KBDSEND_CLKD  &= ~(1<<KBDSEND_CLKB);  /* switch to input */
			KBDSEND_CLKP  |=  (1<<KBDSEND_CLKB);  /* clock high (internal pullup) */
#ifdef KBDSEND_RSTP
			KBDSEND_RSTDDR &= ~(1<<KBDSEND_RSTB); /* input */
			RST_PORTIDLE  // KBDSEND_RSTP   |=  (1<<KBDSEND_RSTB); /* pull-up RST */
#endif
		}

//		inputstate = led_getinputstate(); /* get current inputs state */
		DBGOUT( '.'  )
	}

	/* sync is achieved here AND these flags are present only after a fresh start 
	   so we don't have to wait for active transmissions here
	*/
	if( state & (STATE_POWERUP|STATE_RESYNC) )
	{
		state &= ~(STATE_KBWAIT|STATE_KBWAIT2|STATE_OVERFLOW); /* no waiting (yet) */

		if( state & STATE_POWERUP )
		{
			init_ring();
			DBGOUT('P');
			/* power-up stream code $FD */
			amiga_kbsend( KEYCODE_POWERUPSTREAM_START, 2 );
			state |= STATE_KBWAIT|STATE_POWERUP2; /* powerup is two-phase */
		}
		else 	/* resync */
		{	/* FIXME: we didn't remember what we need to retransmit */
			DBGOUT('S');
			amiga_kbsend( KEYCODE_RETRANSMIT, 2 );
			state |= STATE_KBWAIT;
		}
		state &= ~(STATE_POWERUP|STATE_RESYNC);
	}

	/* loop through kbd output bits and collect input bits */
	pos = 0; /* first key in list */
//	for( j=0x80 ; (j != 0)&&(!(state & STATE_RESYNC)) ; j >>= 1 ) /* requires unsigned char for j */
	for( j=OSTART; (j != 0) && (!(state & STATE_RESYNC)) ; j <<= 1 )
	{
	  if( !(j & OMASK ) )	/* oport bit inactive ? */
	 	continue;

//	  OPORT = (OPORT&(~(OMASK))) | j ; /* make current high, clear rest */
	  OPORT =  (OPORT|(OMASK)) ^ j; /* make current low, set rest to high */

	  /*------------------------------------------------------ */
	  /* check whether we got an acknowledgement               */
	  /*                                                       */
	  if( state & STATE_KBWAIT )
	  {
		keyb_idle = 0;
		/* wait2 = wait for ACK to high again */
		if(  state & STATE_KBWAIT2 )
		{
			if( KBDSEND_ACKPIN & (1<<KBDSEND_ACKB) )
			{
				state &= ~(STATE_KBWAIT|STATE_KBWAIT2);
				DBGOUT('-');
			}
		}
		else
		{
			kbdwait++; /* waiting for ACK in 5 us units */

			/* wait 1 = wait for ACK low */
			if( !(KBDSEND_ACKPIN & (1<<KBDSEND_ACKB)) )
			{
				state |= STATE_KBWAIT2;
				kbdwait = 0;
				DBGOUT('_');
			}

			if( kbdwait > SYNC_WAIT*2 ) /* we wait 5 us but SYNC_WAIT is in 10us units */
			{
				DBGOUT('!');
				state |= STATE_RESYNC;
			}
		}
#ifdef DEBUGONLY
		state &= ~(STATE_KBWAIT|STATE_KBWAIT2);
#endif
	  }
	  else
	  {
		/* this delay could be a bit higher considering impedances/capacitance across the keyboard */
		/* e.g. 10-20 us delay + check if KBClock is low from the other end */
		/* also: differentiate between KBWAIT and idle or include a short loop */
		_delay_us(5); /* wait a little (200 kHz @ 5us) */

		if( keyb_idle > 5 )
		{
			/* check if there is a command from remote end */
			if( !(KBDSEND_ACKPIN & (1<<KBDSEND_ACKB)) )	/* data low ? */
				break;
		}
	  }
	  /*--------------------------------------------------------*/ 


	  /* -------------------------------------------------------*/
	  /* loop through input ports, put new detected keys into   */
	  /* ringbuffer                                             */
	  /*                                                        */
	  for( i=0 ;  (kbinputlist[i] != 0) && (!(state & STATE_RESYNC)) ; i++ )
	  {
		/* get port state and compare with kbtable */
		cur = *(&PIND + kbinputports[i] ) & kbinputlist[i]; /* */
//		cur = (cur) ? KEYDOWN : KEYIDLE;
		cur = (cur) ? KEYIDLE : KEYDOWN; /* low active */

		if( (kbtable[pos]&KEYDOWN) != cur )
		{
			unsigned char deb;

			/* KEY CHANGED -> debounce */
			deb = (((kbtable[pos])>>DEBOUNCE_SHIFT)&DEBOUNCE_MASK);
			deb++;
			if( (deb >= DEBOUNCE_TIME) || (state & STATE_POWERUP2) )
			{
				/* special treatment for CAPS-LOCK */
				if( pos == SCANCODE_CAPSLOCK )
				{
					/* ignore KEYUP on CAPS LOCK */
					if( cur == KEYDOWN ) 
					{
						caps ^= KEYDOWN;
						//ledstat = (caps<<LEDPIN);			/* on/off */
						caps_on = caps;
						//LEDPORT = (LEDPORT&(~(1<<LEDPIN)))|ledstat;	/* on */
						show_caps( caps_on );

						if( !write_ring( pgm_read_byte(&kbmap[pos]) | ((caps^KEYDOWN)<<7) ) )
							state |= STATE_OVERFLOW;
						/* TODO: what do we do with CapsLock and digital LEDs ? */
					}
				}
				else
				{
					/* all other keys */
					unsigned char code = pgm_read_byte(&kbmap[pos]) | ((cur^KEYDOWN)<<7);
					/* write to buffer sent "up" is 1, internal "up" is 0 (updown sent last) */
					if( !write_ring( code ) )
						state |= STATE_OVERFLOW;
					led_digital_updown( code, pgm_read_byte(&kbleftright[pos]) );
				}
				DBGOUT( pgm_read_byte(&debuglist[pos] )  )
				kbtable[pos] = cur; /* store key, no timeout */
			}
			else /* remember debounce count */
				kbtable[pos] = (kbtable[pos]&KEYDOWN) | (deb<<DEBOUNCE_SHIFT);
		}
		else
		{
			/* no change, are we debouncing (debounce counter >0) ? */
			if( kbtable[pos] >= (1<<DEBOUNCE_SHIFT) )
				kbtable[pos] -= (1<<DEBOUNCE_SHIFT);
		}
		pos++;	/* position in key list */
	  }
	  /*--------------------------------------------------------*/ 
	}

	/* handle extra keys, low active */
	pos = SCANCODE(7,1);  /* LAMIGA is the first special key                   */
	i = SPCPIN & SPCMASK; /* get special port reading (mask is redundant btw.) */
	j = 0;
	while( kbinputspecials[j] != 0 )
	{
		if( i & kbinputspecials[j] ) /* high?                */
			cur = KEYIDLE;       /* then idle            */
		else	cur = KEYDOWN;       /* if low, then keydown */
		if( (kbtable[pos]&KEYDOWN) != cur )
		{
			unsigned char code = pgm_read_byte(&kbmap[pos]) | ((cur^KEYDOWN)<<7);
			/* write to buffer sent "up" is 1, internal "up" is 0 (updown sent last) */
			if( !write_ring( code ) )
				state |= STATE_OVERFLOW;
			DBGOUT( pgm_read_byte(&debuglist[pos] )  )
			kbtable[pos] = cur; /* store key, no timeout */

			led_digital_updown( code, pgm_read_byte(&kbleftright[pos]) );
		}
		pos++;
		j++;
	}

	/* --------------------------------------------------------------------- */
	/* END of Powerup stream appended to immediately queued keys after reset */
	if( state & STATE_POWERUP2 )
	{
		write_ring( KEYCODE_POWERUPSTREAM_STOP );
		state &= ~STATE_POWERUP2;

		//ledstat  = 0;	/* LED off after power-on phase */
		//LEDPORT  = (LEDPORT&(~(1<<LEDPIN)))|ledstat;	/* off */

		caps_on  = 0;   /* referenced in LED controller */
		show_caps( caps_on );
	}
	/* --------------------------------------------------------------------- */


	/* --------------------------------------------------------------------- */
	/* send next key if any is in list                                       */
	if( !(state & (STATE_RESET|STATE_KBWAIT|STATE_KBWAIT2) ))
	{
		if( kbdsend_delay == 0 )
		{
			RING_TYPE val;
			if( read_ring( &val ) )
			{
				amiga_kbsend( val, 2 );
				state |= STATE_KBWAIT;
				kbdwait = 0;
				keyb_idle = 0;
				kbdsend_delay = KBDSEND_KEYDELAY; /* wait some time */
			}
		}
		else
			kbdsend_delay--;
	}
	/* --------------------------------------------------------------------- */
	if( !(state & (STATE_KBWAIT|STATE_KBWAIT2) )) /* redundant: keyb_idle is 0 while in wait */
	{
		if( need_confeeprom == 1 ) /* TODO: put this in the flags */
		{
			/* we need to wait for acknowledge from Amiga to "COMM_ACK1" before 
			   we can spend the time (8ms per Byte) to update the EEPROM
			*/
			if( keyb_idle > 1 )
			{
				led_saveconfig( 0x7f );
#ifdef DEBUG
				uart1_puts(" Saved config\r\n");
#endif
				/* DONE here ! */
				write_ring( COMM_ACK | 0x80 ); /* one should be sufficient... */
				write_ring( COMM_ACK | 0x80 );

				show_caps( 0x40 ); /* clear caps lock (if necessary) */
				need_confeeprom = 0;
			}
		}

		if( keyb_idle > 5 )
		{

			/* check if there is a command from remote end */
			if( !(KBDSEND_ACKPIN & (1<<KBDSEND_ACKB)) )	/* data low ? */
			{
				if( !(KBDSEND_CLKPIN & (1<<KBDSEND_CLKB))) /* clock low ? */
				{
					/* AHA! We're being sent data */
					/* leave normal processing and get input data */
					recvcmd = recv_commands(&nrecv);
					if( nrecv & 0x80 )
					{
					 /* TODO: decide whether to send NACK or wait for timeout */
#ifdef DEBUG
						uart_puthexuchar( nrecv );
						uart1_puts(" Command Sync error!\r\n");
#endif
						keyb_idle = 0;
						/* this sometimes leads to SNAFU = endless ping-pong when a command was mis-detected by us */
//						write_ring( COMM_NACK | 0x80 ); 
					}
					else
					if( nrecv > 0 )
					{
						char nsend = led_putcommands( recvcmd, nrecv );
						unsigned char *sendbuf = recvcmd;

						kbdsend_delay = KBDSEND_SWITCHDELAY;
						keyb_idle = 0;
#ifdef DEBUG
						uart_puthexuchar( nrecv );
						uart1_puts(" Bytes received: \r\n");
						while( nrecv-- )
						{
							uart_puthexuchar( *recvcmd++ );	
							uart1_puts(" ");
						}
						uart1_puts("\r\n");
#endif
						/* 
							We need to save the configuration. This may take a while.
							Hence, it's best to acknowledge the command, then take some
							time and re-sync before sending the final ack 
						*/
						if( nsend == -1 )
						{
							show_caps( 0x41 ); /* indicator: we want to save the config */

							write_ring( COMM_ACK1 | 0x80 ); /* write ACK1 to host: i.e. we need some time */
							write_ring( COMM_ACK1 | 0x80 ); /* write ACK1 to host: i.e. we need some time */
							write_ring( 0 ); /* empty, no further bytes */
							need_confeeprom = 1;
						}
						else
						{
							/* do we need to send something back (like config) */
							if( nsend > 0 )
							{
								write_ring( COMM_ACK1 | 0x80 ); /* first might get swallowed by CIA */
								write_ring( COMM_ACK1 | 0x80 );
								write_ring( nsend );
								while( nsend > 0 )
								{
									write_ring( *sendbuf++ );
									nsend--;
								}
							}
							else
								write_ring( COMM_ACK | 0x80 ); /* first might get swallowed */
							write_ring( COMM_ACK | 0x80 ); 
						}
					}
				}
			}
		}

		if( keyb_idle > 1024 )
	 	{
			inputstate |= LED_FORCE_UPDATE;
			keyb_idle = 5;
		}

	}
	led_updatecontroller(inputstate); /* */
	inputstate &= ~(LED_FORCE_UPDATE);

	/* --------------------------------------------------------------------- */
	/* check CTRL-LAMIGA-LAMIGA                                              */
	if( ((kbtable[SCANCODE_LAMIGA] & kbtable[SCANCODE_RAMIGA] & kbtable[SCANCODE_CTRL]) & KEYDOWN) == KEYDOWN )
	{
		/* TODO: send reset warning, do hard reset after a while -> I don't like it, frankly */
		if( !(state & STATE_RESET ))
		{
			rstwait = 0; /* count clock low in 5*8 us units */
			DBGOUT( 'R' )
			state |= STATE_RESET; /* let's reset if keys continue to be pressed */
		}
	}
	else
	{
		/* CTRL-LAMIGA-LAMIGA released ? */
		 KBDSEND_CLKD  &= ~(1<<KBDSEND_CLKB);  /* switch to input */
		 KBDSEND_CLKP  |=  (1<<KBDSEND_CLKB);  /* clock high (internal pullup) */
		 state &= ~STATE_RESET; /* no longer prepare to demand reset */
	}

	if( (state & STATE_RESET) )
	{
		rstwait += OCOUNT/2; /* 10 us units (assume 40us per KB scan loop) */
#if 0
		if( rstwait >= RESET_WAIT0 )
		{
			write_ring(KEYCODE_RESET_WARN);
		}
#endif
		if( rstwait >= RESET_WAIT1 )
		{
#ifdef KBDSEND_RSTP
			KBDSEND_RSTDDR |=  (1<<KBDSEND_RSTB); /* output */
			KBDSEND_RSTP   &= ~(1<<KBDSEND_RSTB); /* /RST */
#endif
			KBDSEND_CLKD |=  (1<<KBDSEND_CLKB);  /* switch to output */
			KBDSEND_CLKP &= ~(1<<KBDSEND_CLKB);  /* clock low */
			state &= ~(STATE_KBWAIT|STATE_KBWAIT2); /* no longer wait for KB ACK */
		}
#if 0
		if( rstwait >= RESET_WAIT ) /* (auto) hold time elapsed ? */
		{
#ifdef KBDSEND_RSTP
			KBDSEND_RSTDDR |=  (1<<KBDSEND_RSTB); /* output */
			KBDSEND_RSTP   &= ~(1<<KBDSEND_RSTB); /* /RST */
#endif
	
		 	KBDSEND_CLKP  |= (1<<KBDSEND_CLKB);  /* clock high  */
			state = STATE_POWERUP; /* repeat power-up procedure */
		}
#endif
	}
	else
	{
#ifdef KBDSEND_RSTP
	  KBDSEND_RSTDDR &= ~(1<<KBDSEND_RSTB); /* input */
	  RST_PORTIDLE  //  KBDSEND_RSTP   |=  (1<<KBDSEND_RSTB); /* pull-up RST */
#endif
	}

	/* --------------------------------------------------------------------- */

#ifdef DEBUG
	//if( uart_have_newline() ) 
	if( uart1_available() )
	{
		cur=uart1_getc();
		uart1_putc(cur);

		DEBUG_ON();

		//ledstat  ^= (1<<LEDPIN);	/* on/off */
		//LEDPORT = (LEDPORT&(~(1<<LEDPIN)))|ledstat;	/* on */

		switch(cur)
		{
			/* red up/down */
			case 'r': break;

			case 't': break;

			/* green up/down */
			case 'g': break;

			case 'h': break;

			/* blue up/down */
			case 'b': break;

			case 'n': break;

			default: break;
		};
	}
#endif
 }

 return 0;
}

/*
  communication protocol specifics:
  1) sync to amiga (push 1s, wait for ACK)
  2) if sync was lost, go into sync mode, then transmit "sync lost" character

  power up sequence
  1) achieve sync (without sync lost char)
  2) power-up stream code $FD, keys, $FE
  3) shutdown Caps Lock LED
*/


/*
  map internal scan code to Amiga scan code, send data

  two extra cases:
   - Reset Condition, i.e. CTRL,AMIGA,AMIGA down
   - timeout from amiga, no ACK

  input:  internal scan code or actual keyboard code
          up/down flag (0/1), actual KB code flag (2) where no remapping takes place
  output: 1 = OK
          0 = transmission failure
*/
char amiga_kbsend( unsigned char scan_internal, unsigned char updown )
{
 unsigned char i;
 unsigned char code,curbit;

 /* this first code path is from old implementation and currently actually unused 
    Calls to this function are made with "updown == 2"
 */
 if( updown <= 1 )
 {
  code  = pgm_read_byte(&kbmap[scan_internal])<<1; /* keycode is sent first (7 bits) */
  code |= (updown ^ KEYDOWN); /* sent "up" is 1, internal "up" is 0 (updown sent last) */
 }
 else
  code = (scan_internal<<1)|(scan_internal>>7); /* remap to send order */

 KBDSEND_CLKP |= (1<<KBDSEND_CLKB); /* clock high before loop */
 KBDSEND_CLKD |= (1<<KBDSEND_CLKB); /* output                 */
 KBDSEND_SENDP |= (1<<KBDSEND_SENDB); /* def: high = pullup on   */
 KBDSEND_SENDD |= (1<<KBDSEND_SENDB); /* output */

 for( i=0 ; i < 8 ; i++ )
 {
	curbit = (1<<KBDSEND_SENDB); /* high, if next bit == 0 */
	if( code & 0x80 )
	 	curbit = 0;
	KBDSEND_SENDP = (KBDSEND_SENDP&~(1<<KBDSEND_SENDB))|curbit;	/* set data  */

	_delay_us(20);

	KBDSEND_CLKP &= ~(1<<KBDSEND_CLKB); /* clock low */

	_delay_us(20);

	KBDSEND_CLKP |= (1<<KBDSEND_CLKB); /* clock high */

	_delay_us(30);

	code <<= 1;
 }

 /* make sure, DAT is high (pull hard) */
 KBDSEND_SENDP |= (1<<KBDSEND_SENDB);  /* set DAT high (=pullup) */

 /* wait some more after finishing */
 _delay_us(20);

 /* get DAT to input again (it's up and has a pull) */
 KBDSEND_SENDD &= ~(1<<KBDSEND_SENDB); /* set DAT back to input  */

 /* we're done with clock, leave it high but get it back to input */
 KBDSEND_CLKD  &= ~(1<<KBDSEND_CLKB);  /* clock port to input     */

 /* now the data was sent, we'll wait for the Amiga pulling the dat line low as ACK */
 return	1;
}


/* synchronize with Amiga
   - set KBDAT output to 1
   - toggle KBCLOCK 
   - wait for handshake signal from Amiga

   Note: this call might take up to 288 ms. Since we have nothing to do in unsynchronized state,
         the waiting is done in here.

  output: 1 = SYNCHRONIZED
          0 = no response from Amiga (i.e. have to try again)
*/
char amiga_kbsync( void )
{
 unsigned short count; 

 KBDSEND_CLKP |= (1<<KBDSEND_CLKB); /* clock high before loop */
 KBDSEND_CLKD |= (1<<KBDSEND_CLKB); /* output                 */
 KBDSEND_SENDP |= (1<<KBDSEND_SENDB); /* def: high = pullup on   */
 KBDSEND_SENDD |= (1<<KBDSEND_SENDB); /* output */

 KBDSEND_CLKP  |= (1<<KBDSEND_CLKB);  /* clock high */
 KBDSEND_SENDP &= ~(1<<KBDSEND_SENDB); /* set DAT low */
 _delay_us(20);
 KBDSEND_CLKP  &= ~(1<<KBDSEND_CLKB); /* clock low */
 _delay_us(20);
 KBDSEND_CLKP  |= (1<<KBDSEND_CLKB);  /* clock high */
 _delay_us(20);
 KBDSEND_SENDP |= (1<<KBDSEND_SENDB); /* set data 1 */

 /* caution: KBDSEND_PIN == KBDSEND_ACKPIN with new keyboard -> hence back to input */
 KBDSEND_SENDD &= ~(1<<KBDSEND_SENDB); /* back to input (with pullup) */
 KBDSEND_CLKD  &= ~(1<<KBDSEND_CLKB);  /* back to input (with pullup, see above "clock high") */

 /* 14300*10 us = 143 ms */
 for( count = 1 ; count < SYNC_WAIT ; count++ )
 {
 	if( !(count&0x3ff) ) /* every 1024 ticks */
	{
		unsigned char inputstate = led_getinputstate(); /* get current inputs state */
		led_updatecontroller(inputstate); /* */

	}
	else
 		_delay_us(10);

	if( TIFR2 & 0x01 ) /* timer overflow (16.3ms) ? */
	{
		TIFR2  = 0x01; /* clear TOV0 overflow flag (write 1 to set flag to 0) */
		led_digital_step();
	}

 	if( !(KBDSEND_ACKPIN & (1<<KBDSEND_ACKB)) )
 		break;
#ifdef ENABLE_USB
	if( get_usb_config_status() )
	{
		count = SYNC_WAIT;
		break;
	}
#endif
 }

 if( count == SYNC_WAIT )
 	return 0;

 /* wait for ACK to get up again */
 for( count = 1 ; count < SYNC_WAIT ; count++ )
 {
	if( !(count&0x3ff) ) /* every 1024 ticks */
	{
		unsigned char inputstate = led_getinputstate(); /* get current inputs state */
		led_updatecontroller(inputstate); /* */
	}
	else
		_delay_us(10);

	if( TIFR2 & 0x01 ) /* timer overflow (16.3ms) ? */
	{
		TIFR2  = 0x01; /* clear TOV0 overflow flag (write 1 to set flag to 0) */
		led_digital_step();
	}

	if( (KBDSEND_ACKPIN & (1<<KBDSEND_ACKB)) )
		break;
 }
 
 if( count == SYNC_WAIT )
 	return 0;

 return 1;
}

void init_ring( void )
{
  ringw = 0;                  /* write position = next buffered */
  ringr = SENDBBUFFER_SIZE-1; /* read position  = next sent to Amiga -1 */
}

char write_ring( RING_TYPE val )
{
 RINGPOS_TYPE wp = (ringw+1) & (SENDBBUFFER_SIZE-1);

   /* check for overflow */
   if( wp == ringr ) /* next write pos is read pos ? */
   {
	return 0; /* overflow (actually, the max. buffer usage is SENDBBUFFER_SIZE-1 here */
   }

   sendbuffer[ringw] = val;
   ringw = wp;

 return 1;
}

/* read from ringbuffer
   - returns 0 if no new value is available (1 = value read from buffer)
   - store contents at val 

   if( val == 0 )
    don't advance read pointer and just return whether something was in buffer
*/
char read_ring( RING_TYPE *val )
{
 RINGPOS_TYPE rp = (ringr+1) & (SENDBBUFFER_SIZE-1);

 if( rp == ringw )
 	return 0;
 
 /* shall we return something ? */
 if( val )
 {
   *val  = sendbuffer[rp];
   ringr = rp; /* advance read pointer */
 }

 return 1;
}

/* ----------------------------------------------------------------------- */
/* caps lock LED (regular LED, not the RGB one) 
   state&0x1 == 1 -> on
   state&0x1 == 0 -> off
   state&040 -> force, regardless of SPARE1PIN

   Normally, the regular CAPS lock will honor the SPARE1PIN. If that
   pin is pulled down by R10 (2k), then the normal CAPS lock LED will
   be used. Otherwise, the RGB CAPS lock LED is active only.

   The override (0x40) applies to the Save Config mode.
*/
void show_caps( unsigned char state )
{
	unsigned char ledstat;

	/* not pulled down ? */
	if(   ( KBD_SPARE1PIN & (1<<KBD_SPARE1B) )
	   || ( state & 0x40 )
	  ) 
	{
	  ledstat  = (state & 1 ) ? (1<<LEDPIN) : 0;
	  LEDPORT  = (LEDPORT&(~(1<<LEDPIN)))|ledstat;     /* on/off */
	}
}

/* ----------------------------------------------------------------------- */
/*

    Command stream from host 

    commands are read into private buffer and then passed down to calling
    instance

    Protocol: (assure idle state of keyboard output)
     - preamble (0x3)
        - CLK = DAT = LOW (remote end) -> trigger for receive start
	- swallow "0" data at each falling edge of CLK, prepare for first
	  relevant data byte after two "1" pulses at falling edges of CLK
     - get data bytes as long as clock pulses are sent by the remote
     - TODO: CRC8 in last byte

     - expected clock rate is 4800-14400 kBPs or 200us...69us per Bit
     - time per byte 1.6ms...552us

*/
/* ----------------------------------------------------------------------- */
unsigned char *recv_commands(unsigned char *nrecv)
{
 unsigned char bitcount,recbits,cur,*p,loops;

 if( !nrecv )
 	return NULL;
 *nrecv = 0;

 /* sync phase: wait for two times 1 after a bunch of zeros 

    This program can't guarantee that all 8 bits of the preamble
    are detected in time. Hence, we hope for 5 out of 8 bits in the
    sync loop. These 5 bits are 3x zero, followed by 2x one
 
 */
 TCCR0A = 0; /* normal mode, OC0A disconnected, OC0B disconnected,   */
 TCCR0B = (1<<CS02); /* prescaler 256 -> 16 MHz/256 = 62500 Hz counter -> 4 ms before overflow @16 MHz */
 //TCCR0B = (1<<CS00) | (1<<CS02); /* prescaler 1024 -> 16 MHz/1024 = 15625 Hz counter -> 16.3 ms before overflow */

 //------------------------------------------------------
 TCNT0  = 0x00; /* start timer with 0 */
 TIFR0  = 0x01; /* clear TOV0 overflow flag (write 1 to set flag to 0) */
 bitcount = 0;
 recbits = 0;
 cur   = 1; /* wait for next falling edge */
 while( (TIFR0 & 0x01) == 0  ) /* break waiting loop after 4ms */
 {
	_delay_us(10);
#if 1
	if( (KBDSEND_CLKPIN & (1<<KBDSEND_CLKB)))
	{
		/* rising edge */
		if( cur == 0 )
		{
			cur++;
			bitcount++;
			recbits <<= 1;
			if( (KBDSEND_ACKPIN & (1<<KBDSEND_ACKB) ) )
				recbits |= 1;
		}
	}
	else
	{
		cur = 0;
	}
#endif
#if 0
	if( (KBDSEND_CLKPIN & (1<<KBDSEND_CLKB)))
		cur = 1;
	else
	{
		if( cur == 1 ) /* was high, is low -> falling edge */
		{
			cur++;
			bitcount++;
			recbits <<= 1;
			if( (KBDSEND_ACKPIN & (1<<KBDSEND_ACKB) ) )
				recbits |= 1;
		}
		else
		{
		 cur = 0;
		 if( cur > 4 )
		 {
			if( (KBDSEND_ACKPIN & (1<<KBDSEND_ACKB) ) )
				recbits |= 1;
			cur++;
		 }
		}
	}
#endif
	if( recbits == 0x03 )
		break;
	if( bitcount >= 8 )
		if( recbits >= 0x4 )
			break;
 }

 /* SYNC ERROR? less than 5 bits before timeout, result is not 0x3 */
 /* Wait until clock gets quiet, then return                       */
 if( (recbits != 0x03) || (bitcount < 5) || (TIFR0 & 0x01) )
 {
	if( (TIFR0 & 0x01) == 0  ) /* did we have a timeout ? (no further waiting) */
	{
	 /* no timeout, wait for end of input stream */
	 TCNT0  = 0x00; /* start timer at zero for 4ms (0x80 would be 2ms@16M,divider 256) */
	 TIFR0  = 0x01; /* clear TOV0 overflow flag (write 1 to set flag to 0) */

	 while( (TIFR0 & 0x01) == 0  ) /* break waiting loop after 4ms */
	 {
	 	/* while clock gets to 0, continue waiting */
		if( !(KBDSEND_CLKPIN & (1<<KBDSEND_CLKB)))
			TCNT0  = 0x80; /* reduce waiting time for possible next bit to 2ms */
	 }
	}

	*nrecv = recbits|0x80;
	return NULL;
 }


 //---------------------------------------------------------
 /* main loop: restore received bytes (minus preamble) into
               receive buffer
 */
 p = recv_buffer;
 bitcount = 0;
 recbits = 0;
 cur = 0;
 loops=0;
 TCNT0  = 0x00; /* start timer with 0 */
 TIFR0  = 0x01; /* clear TOV0 overflow flag (write 1 to set flag to 0) */

 while( loops < 10 ) /* 4ms * 10 */
 {
	if( ( TIFR0 & 0x01) == 1  ) /* break waiting loop after 4ms * "loops" */
	{
		loops++;
		TIFR0  = 0x01;	/* clear TOV0 flag */
	}

	_delay_us(10);
	if( (KBDSEND_CLKPIN & (1<<KBDSEND_CLKB)))
		cur = 1;
	else
	{
		if( cur == 1 )
		{
			cur++;

			if( bitcount == 8 )
			{
				*p++ = recbits;
				/* avoid buffer overflow */
				if( (p - recv_buffer) >= RECVBUFSIZE )
					break;
				bitcount = 0;
				recbits  = 0;
				TCNT0 = 0x00; /* restart timer */
			}

			bitcount++;
			recbits <<= 1;
			if( (KBDSEND_ACKPIN & (1<<KBDSEND_ACKB) ) )
				recbits |= 1;
		}
		else
		 if( cur < 3 )
		 {
			cur++;
			if( (KBDSEND_ACKPIN & (1<<KBDSEND_ACKB) ) )
				recbits |= 1;
		 }
		 else
		 	cur = 3;
	}
 }

 if( bitcount == 8 )
 {
	*p++ = recbits;
	/* avoid buffer overflow */
//	if( (p - recv_buffer) >= RECVBUFSIZE )
//		break;
	bitcount = 0;
	recbits  = 0;
	TCNT0 = 0x00; /* restart timer */
 }

 *nrecv = (unsigned char)(p - recv_buffer);

 return recv_buffer;
}

//unsigned char *recv=NULL,nrecv=0; /* commands from Host */
//	if( !(KBDSEND_ACKPIN & (1<<KBDSEND_ACKB)) )	/* data low ? */
//      if( !(KBDSEND_CLKPIN & (1<<KBDSEND_CLKB))) /* clock low ? */


void uart_puthexuchar(unsigned char a)
{
 unsigned char t,i;

 DBGOUT('0');
 DBGOUT('x');

 for( i=0 ; i < 2 ; i++ )
 {
	 t = ((a>>4)&0x0F) + '0';
	 if( t > '9' )
 		t = ((a>>4)&0x0F) - 10 + 'A';
	 DBGOUT( t );
	 a = a<<4;
 }
}

void uart_puthexuint(uint16_t a)
{
 unsigned char i;
 unsigned char t;

 DBGOUT('0');
 DBGOUT('x');

 for( i=0 ; i < 4 ; i++ )
 {
	 t = ((a>>12)&0x0F) + '0';
	 if( t > '9' )
 		t = ((a>>12)&0x0F) -10 + 'A';
	 DBGOUT( t );
	 a = a<<4;
 }
}



