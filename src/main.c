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
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/io.h>
#include <util/delay.h> /* might be <avr/delay.h>, depending on toolchain */
//#include "avr/delay.h"
#include "baxtypes.h"
#include "twi.h"
#include "led.h"

#include "kbdefs.h"

#define DEBUGONLY
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

unsigned char *recv_commands(unsigned char *nrecv);


/* active key table -> bytes here, could be mapped to bits */
unsigned char kbtable[(OCOUNT+OCOUNT_SPC)*ICOUNT];

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
/* waiting time before rest is issued (in 10 us units) = 10ms */
#define RESET_WAIT1	100

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
#if 0
/*       ESC F1  F2  F3  F4  F5  F6  F7  F8  F9  F10     */
 '~','1','E','1','2','3','4','5','6','7','8','9','0',
/*                                                BKSPC  */
 '2','3','4','5','6','7','8','9','0','?','`','\\','B',
/*TAB                                         ü          */
 'T','q','w','e','r','t','z','u','i','o','p','U','+',
/*CTRL CAPSLOCK                               Ö   Ä      */
 'C','L','a','s','d','f','g','h','j','k','l','O','A',
/*SHIFT                                            SHIFT */
 'S','<','y','x','c','v','b','n','m',',','.','-','S',
/*ALT AMIGA             SPACE        Amiga Alt   Return  */
 'A','M','!','!','!','!',' ','!','!','M','A','#',10,
/* KP,                       cd  cl  cr  cu  help del    */
 '.','0','!','1','2','3',10 ,'D','L','R','U','H','D',
/* KP */
 ']','[','/','4','5','6','!','+','-','*','9','8','7',
#endif
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

int main(void)
{
  unsigned char ledstat,i,j,pos,state;
  unsigned short kbdwait = 0;
  unsigned short rstwait = 0;
  unsigned short keyb_idle = 0;
  unsigned char inputstate; /* track inputs (Power,Floppy,CapsLock,extra inputs) */
  volatile unsigned char cur;
  unsigned char caps;
  unsigned char nrecv=0; /* commands from Host */
  unsigned char *recvcmd;

  /* initialize computer communication ports (D2/D3 regular, F2/F3 in debug) */
  KBDSEND_SENDD &= ~(1<<KBDSEND_SENDB); /* send data port to input */
  KBDSEND_SENDP |=  (1<<KBDSEND_SENDB); /* def: high = pullup on   */
  KBDSEND_CLKD  &= ~(1<<KBDSEND_CLKB);  /* clock port to input     */
  KBDSEND_CLKP  |=  (1<<KBDSEND_CLKB);  /* def: high = pullup on   */ 

  /* initialize regular Caps LED */
  caps     = KEYIDLE; /* caps lock treated separately */
  caps_on  = 1;
  LEDDDR  |=   1<<LEDPIN;       /* output */
  LEDPORT &= ~(1<<LEDPIN);	/* off */
  ledstat  =  (1<<LEDPIN);	/* on  */
  LEDPORT |=  (1<<LEDPIN);	/* on = NPN transistor switches to GND = LED on */

  /* initialized output ports (def: low) */
  ODDR    |= OMASK;     /* output */
  OPORT   &= ~(OMASK);  /* low */

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
  KBDSEND_RSTP   |=  (1<<KBDSEND_RSTB); /* pull-up RST */
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

  /* initialize keyboard states */
  for( i=0 ; i < OCOUNT*ICOUNT ; i++ )
  {
  	kbtable[i] = KEYIDLE;
  }

  /* init UART for serial communication to PC */
#ifdef DEBUG
  uart1_init(UART_BAUD_SELECT(9600UL,F_CPU));
  sei();
  uart1_puts("AMIGA 500 KEYBOARD BY BAX\r\n");
#endif

  led_init(); /* start up LED controller */

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

  /* */
  init_ring();	/* prepare ringbuffer */
  state = STATE_POWERUP; /* synchronize with Amiga, perform power-up procedure */
  while(1)
  {
	inputstate = led_getinputstate(); /* get current inputs state */
	keyb_idle++;

	while( state & (STATE_POWERUP|STATE_RESYNC) )
	{
		kbdwait = 0;
		/* synchronize with Amiga, stay here until Amiga answers */

		/* LED control is part of KBSync Mode */
//		led_updatecontroller(inputstate); /* */
#ifndef DEBUGONLY
		if( amiga_kbsync() > 0 )
#endif
			break;

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

	  /* this delay could be a bit higher considering impedances/capacitance across the keyboard */
	  /* e.g. 10-20 us delay + check if KBClock is low from the other end */
	  /* also: differentiate between KBWAIT and idle or include a short loop */
	  _delay_us(5); /* wait a little (200 kHz @ 5us) */

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
			if( deb >= DEBOUNCE_TIME )
			{
				/* special treatment for CAPS-LOCK */
				if( pos == SCANCODE_CAPSLOCK )
				{
					/* ignore KEYUP on CAPS LOCK */
					if( cur == KEYDOWN ) 
					{
						caps ^= KEYDOWN;
						ledstat = (caps<<LEDPIN);			/* on/off */
						caps_on = caps;
						LEDPORT = (LEDPORT&(~(1<<LEDPIN)))|ledstat;	/* on */

						if( !write_ring( pgm_read_byte(&kbmap[pos]) | ((caps^KEYDOWN)<<7) ) )
							state |= STATE_OVERFLOW;
					}
				}
				else
				{	/* all other keys */
					/* write to buffer sent "up" is 1, internal "up" is 0 (updown sent last) */
					if( !write_ring( pgm_read_byte(&kbmap[pos]) | ((cur^KEYDOWN)<<7) ) )
						state |= STATE_OVERFLOW;
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
			/* write to buffer sent "up" is 1, internal "up" is 0 (updown sent last) */
			if( !write_ring( pgm_read_byte(&kbmap[pos]) | ((cur^KEYDOWN)<<7) ) )
				state |= STATE_OVERFLOW;
			DBGOUT( pgm_read_byte(&debuglist[pos] )  )
			kbtable[pos] = cur; /* store key, no timeout */
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

		ledstat  = 0;	/* LED off after power-on phase */
		LEDPORT  = (LEDPORT&(~(1<<LEDPIN)))|ledstat;	/* off */
		caps_on  = 0;
	}
	/* --------------------------------------------------------------------- */


	/* --------------------------------------------------------------------- */
	/* send next key if any is in list                                       */
	if( !(state & (STATE_RESET|STATE_KBWAIT|STATE_KBWAIT2) ))
	{
		RING_TYPE val;
		if( read_ring( &val ) )
		{
			amiga_kbsend( val, 2 );
			state |= STATE_KBWAIT;
			kbdwait = 0;
			keyb_idle = 0;
		}
	}
	/* --------------------------------------------------------------------- */
	if( !(state & (STATE_KBWAIT|STATE_KBWAIT2) )) /* redundant: keyb_idle is 0 while in wait */
	{
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
						write_ring( COMM_NACK | 0x80 ); 
					}
					else
					if( nrecv > 0 )
					{
						unsigned char nsend = led_putcommands( recvcmd, nrecv );
						unsigned char *sendbuf = recvcmd;
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
						/* do we need to send something back (like config) */
						if( nsend > 0 )
						{
							write_ring( COMM_ACK1 | 0x80 ); /* first might get swallowed by CIA */
							write_ring( COMM_ACK1 | 0x80 );
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
//		if( (state & STATE_RESET) && (rstwait < RESET_WAIT1)  )
//		{
		 KBDSEND_CLKP  |= (1<<KBDSEND_CLKB);  /* clock high */
		 state &= ~STATE_RESET; /* no longer prepare to demand reset */
//		}
	}

	if( (state & STATE_RESET) )
	{
		rstwait += OCOUNT/2; /* 10 us units (assume 40us per KB scan loop) */
		if( rstwait >= RESET_WAIT1 )
		{
#ifdef KBDSEND_RSTP
			KBDSEND_RSTDDR |=  (1<<KBDSEND_RSTB); /* output */
			KBDSEND_RSTP   &= ~(1<<KBDSEND_RSTB); /* /RST */
#endif
			KBDSEND_CLKP &= ~(1<<KBDSEND_CLKB);  /* clock low */
			state &= ~(STATE_KBWAIT|STATE_KBWAIT2); /* no longer wait for KB ACK */
		}
		if( rstwait >= RESET_WAIT ) /* (auto) hold time elapsed ? */
		{
#ifdef KBDSEND_RSTP
			KBDSEND_RSTDDR |=  (1<<KBDSEND_RSTB); /* output */
			KBDSEND_RSTP   &= ~(1<<KBDSEND_RSTB); /* /RST */
#endif
	
		 	KBDSEND_CLKP  |= (1<<KBDSEND_CLKB);  /* clock high  */
			state = STATE_POWERUP; /* repeat power-up procedure */
		}
	}
	else
	{
#ifdef KBDSEND_RSTP
	  KBDSEND_RSTDDR &= ~(1<<KBDSEND_RSTB); /* input */
	  KBDSEND_RSTP   |=  (1<<KBDSEND_RSTB); /* pull-up RST */
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

		ledstat  ^= (1<<LEDPIN);	/* on/off */
		LEDPORT = (LEDPORT&(~(1<<LEDPIN)))|ledstat;	/* on */

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

	_delay_us(20);

	code <<= 1;
 }

 /* wait some more after finishing */
 _delay_us(20);

 /* get DAT up again */
 KBDSEND_SENDP |= (1<<KBDSEND_SENDB);  /* set DAT high (=pullup) */
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
 KBDSEND_CLKP  &= ~(1<<KBDSEND_CLKB);  /* back to input (with pullup) */

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
 	if( !(KBDSEND_ACKPIN & (1<<KBDSEND_ACKB)) )
 		break;
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
#define RECVBUFSIZE 32 
unsigned char recv_buffer[RECVBUFSIZE];
unsigned char *recv_commands(unsigned char *nrecv)
{
 unsigned char bitcount,recbits,cur,*p;

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
	if( (TIFR0 & 0x01) == 0  ) /* did we have a timeout ? */
	{
	 /* no timeout, wait for end of input stream */
	 TCNT0  = 0x80; /* start timer at half (2ms@16M,divider 256) */
	 TIFR0  = 0x01; /* clear TOV0 overflow flag (write 1 to set flag to 0) */

	 while( (TIFR0 & 0x01) == 0  ) /* break waiting loop after 4ms */
	 {
	 	/* while clock gets to 0, continue waiting */
		if( !(KBDSEND_CLKPIN & (1<<KBDSEND_CLKB)))
			TCNT0  = 0x80;
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
 TCNT0  = 0x00; /* start timer with 0 */
 TIFR0  = 0x01; /* clear TOV0 overflow flag (write 1 to set flag to 0) */

 while( (TIFR0 & 0x01) == 0  ) /* break waiting loop after 4ms */
 {
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



