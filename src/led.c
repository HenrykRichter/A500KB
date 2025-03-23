/*
 ********************************************************************************
 * led.c                                                                        *
 *                                                                              *
 * Author: Henryk Richter <bax@comlab.uni-rostock.de>                           *
 *                                                                              *
 * Purpose: LED controller interface                                            *
 *                                                                              *
 *                                                                              *
 *                                                                              *
 *                                                                              *
 *                                                                              *
 ********************************************************************************
*/
/*
  Gamma table
  track LED source changes -> send updated colors
  done: LED source map for LED 0-6
  LED color table idle/active/secondary

  TODO:
   LED mode regular/special (knight rider, rainbow, cycle)
*/

#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h> 
//#include <avr/io.h>
#include <util/delay.h> /* might be <avr/delay.h>, depending on toolchain */
//#include "avr/delay.h"
#include "baxtypes.h"
#include "twi.h"
#include "kbdefs.h"
#include "led.h"
#include "gammatab.h"

#define DEBUGONLY
#ifdef DEBUG
#include "uart.h"
#define DBGOUT(a) uart1_putc(a);
#define DEBUG_ON()
#else
#define DBGOUT(a)
#define DEBUG_ON()
#endif

#ifndef NULL
#define NULL (0)
#endif


/* set/clear Bit in register or port */
#define BitSet( _port_, _bit_ ) _port_ |=   (1 << (_bit_) )
#define BitClr( _port_, _bit_ ) _port_ &= (~(1 << (_bit_) ) )
/* copy bit from source to destination (2 clocks via T Bit) */
#define BitCpy( _out_, _obit_, _in_, _ibit_ ) \
	asm volatile ( "bst %2,%3" "\n\t" \
	               "bld %0,%1" : "+r" (_out_) : "I" (_obit_) , "r" (_in_) , "I" (_ibit_) );

/* put number on UART */
extern void uart_puthexuchar(unsigned char a);
extern void uart_puthexuint(uint16_t a);

/* external variables */
extern unsigned char caps_on; /* CAPSLOCK state 0=off,1=on */

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
 2, 0x6E, 0x60, /* [Global Current Control],[enable full current (0x1-0xFF)]              */
 2, 0x70, 0xBF, /* [Phase Delay and Clock Phase],[PDE=1,PSn=1] */
 /* end of list */
 0
};

#if 1
/* end of command list */
#define TCMD_END 0xff

#if 0
/* current state */
#define LED_IDLE      0 /* idle             */
#define LED_ACTIVE    1 /* primary   active */
#define LED_SECONDARY 2 /* secondary active */
#define LED_STATES    3 
#endif

unsigned char get_secmap( unsigned char srcmap );

unsigned char led_currentstate; /* current input source state    */
unsigned char src_active;       /* source state as sent to LEDs  */

unsigned char LED_SRCMAP[N_LED+N_LED_DIGI_CONF];    /* flags applying to this LED      */
unsigned char LED_SECMAP[N_LED+N_LED_DIGI_CONF];    /* bit combinations (including SWAP flag) for secondary function */
unsigned char LED_RGB[N_LED+N_LED_DIGI_CONF][LED_STATES][3]; /* RGB config for LEDs */
unsigned char LED_MODES[N_LED+N_LED_DIGI_CONF];     /* static,cycle, rainbow, knight rider etc. */
unsigned char LED_MODESTATE[N_LED+N_LED_DIGI_CONF]; /* mode-private storage for current state */

/* 
  TWI command list: 
   1 Byte LED, 1 Byte STATE
  Terminated by 0xFF,0xFF
*/
unsigned char twicmds[(N_LED+1)*2];
uint8_t initseq[MAXLEDINITSEQ];

#endif

void adc_start( unsigned  char channel )
{
  ADMUX  = (ADMUX&0xE0)|(channel & 0x7); /* select ADC channel 0 (channels 0-7 in lower 3 bits) */
  ADCSRA |= (1<<ADSC); /* start ADC (single conversion) */
}

uint16_t adc_get( void )
{ 
  uint8_t  AinLow;
  uint16_t Ain;

  AinLow = (int)ADCL;         /* lower 8 bit */
  Ain = ((uint16_t)ADCH)<<8;  /* upper two bit, shifted */
  Ain = Ain|AinLow;

  return Ain;
}

unsigned char update_bit( unsigned char old_state, unsigned char decision, unsigned char flag )
{
 unsigned char ret;

 ret = (decision) ? old_state|flag : old_state&(~flag);

 return ret;
}



/* get RGB for a given LED and state */
unsigned char *led_getcolor( uint8_t ledidx, uint8_t state )
{
  unsigned char *ret = &LED_RGB[N_LED][LED_IDLE][0]; /* def: first digi LED */

  if( (ledidx < (N_LED+N_LED_DIGI_CONF)) && (state < LED_STATES) )
  	ret = &LED_RGB[ledidx][state][0];

  return ret;
}

unsigned char led_getmode( uint8_t ledidx )
{
  unsigned char ret = LED_MODES[N_LED];

  if( ledidx < (N_LED+N_LED_DIGI_CONF) ) 
  	ret = LED_MODES[ledidx];
 
  return ret;
}


/*
  set LED state outside of regular polling in led_getinputstate() 
  - used for USB mode

  pass source as flag ( LEDF_... )
  pass state  as 0 = off , 1 = on


*/
uint8_t led_setinputstate( uint8_t source, uint8_t state )
{
  led_currentstate = update_bit( led_currentstate, state, source );
  return led_currentstate;
}

/* scan inputs, return current state of all inputs, return quickly */
/*
   some inputs may be analog and need to be muxed,
   hence the returned state is not instantaneus. 
   
   This function will initialize ADC and update the input states
   once an ADC cycle is completed. 

*/
unsigned char adc_cycle;
unsigned char led_getinputstate()
{
 uint16_t ad;

	led_currentstate = (caps_on) ? led_currentstate|LEDF_SRC_CAPS : led_currentstate&(~LEDF_SRC_CAPS);

	/* cycle through ADCs */
	switch( adc_cycle )
	{
		case 0:
			adc_start(0); /* channel0: POWER LED */
			adc_cycle++;
			break;
		case 1: /* Power LED: >4.15V = ON (5*850/1024) */
			if( (ADCSRA&(1<<ADIF))==0 ) /* wait for ADC */
				break;
			ad = adc_get( );
			led_currentstate = update_bit( led_currentstate, ad>850, LEDF_SRC_POWER );
			adc_cycle++;
				break;
			//led_currentstate = (ad > 850) ?  led_currentstate|LEDF_SRC_POWER : led_currentstate&(~LEDF_SRC_POWER);

		case 2:
			adc_start(1); /* channel1: FLOPPY LED */
			adc_cycle++;
			break;
		case 3:	/* FLOPPY LED: >2.5V = on, else off */
			if( (ADCSRA&(1<<ADIF))==0 ) /* wait for ADC */
				break;
			ad = adc_get();
			led_currentstate = update_bit( led_currentstate, ad>512, LEDF_SRC_FLOPPY );
			adc_cycle++;
			break;
			//led_currentstate = (ad > 512) ? led_currentstate|LEDF_SRC_FLOPPY : led_currentstate&(~LEDF_SRC_FLOPPY);

		case 4: /* IN3, IN4: digital for now, low active */
			led_currentstate = update_bit( led_currentstate, 0==(IN3LED_PIN&(1<<IN3LED_BIT)), LEDF_SRC_IN3 );
			led_currentstate = update_bit( led_currentstate, 0==(IN4LED_PIN&(1<<IN4LED_BIT)), LEDF_SRC_IN4 );
			adc_cycle++;
			break;

		default:
			adc_cycle = 0;
			break;
	}

	return led_currentstate;
}

/* TODO: vector for rgb */
void RGB2HSV( int16_t *hsv, uint8_t r, uint8_t g, uint8_t b )
{
//	uint8_t r,g,b;
	uint8_t rmax,rmin; /* channel index */
	uint8_t delta;
	int16_t h,s,v;
	uint8_t rgb[3];

	rgb[0] = r;
	rgb[1] = g;
	rgb[2] = b;

	/* RGB 2 HSV conversion */
/*	r = rgb[0];
	g = rgb[1];
	b = rgb[2];*/

	/* sort channel indices by level */
	rmin = 2;
	rmax = 0;
	if( g > r )
	{ /* r < g */
		rmax = 1;
		if( b > g )
		{
			rmax = 2;
			rmin = 0;
		}
		else
		 if( r < b )
		 	rmin = 0;
	}
	else
	{ /* r >= g */
	 if( b > r )
	 {
		rmax = 2;
		rmin = 1;
	 }
	 else
	  if( g < b )
		 rmin = 1;
	}

//	PRINT(("rmin %d rmax %d\n",rmin,rmax));

	delta = rgb[rmax]-rgb[rmin];
	v = rgb[rmax];
	if( delta < 128 )
		s = (v) ? (((uint16_t)delta)*255)/v : 0;
	else	s = (v) ? (((uint16_t)delta)<<7)/((v+1)>>1) : 0;
	if( s > 255 )
		s=255;
	// v is in Levels 0...255
	// s is Saturation * 256
	// H is in degrees * 1023/360 

	if( !delta )
		h=0;
	else
	{
		switch( rmax )
		{
			case 0: /* R is max */
			/* 60° * ( (G-B)/delta mod 6 ) */ 
				h = 171*((int16_t)g-(int16_t)b)/delta;
				break;
			case 1: /* G is max */
			/* 60° * ( (B-R)/delta + 2 )   */
				h = (171*((int16_t)b-(int16_t)r))/delta + 342;
				break;
			default: /* B is max */
			/* 60° * ( (R-G)/delta + 4 )   */
				h = 171*((int16_t)r-(int16_t)g)/delta + 683;
				break;
		}
	}
#if 1
	h &= 0x3ff;
#else
	if( h < 0 )
		h=h+1024;
	if( h > 1023 )
		h=h-1024;
#endif
//	PRINT(("%d %d %d -> hsv %d %d %d \n",r,g,b,h,s,v));

	hsv[0] = h;
	hsv[1] = s;
	hsv[2] = v;
}

/* TODO: vector for hsv 
// v is in Levels 0...255
// s is Saturation 0...255 
// H is in degrees * 1024/360 (0...1023)

 rgb   is a 3 entry array (R,G,B) uint8_t
 h,s,v are in 16 bit
*/
void HSV2RGB( uint8_t *rgb, int16_t h, int16_t s, int16_t v )
{
	uint8_t r,g,b;

	/* transform back */
	r = v;
	g = v;
	b = v;
	if( s )
	{
	 int16_t region,remainder,p,q,t;

	 region = h/171;
	 remainder=((h - (region * 171)) * 6)>>2;

	 /* a little shift-fu to keep numbers in 16 bit range */
	 p = (v * ((255 - s)>>2)) >> 6;
	 q = (v * ((255 - ( (s * (remainder>>1))>>7))>>2) ) >> 6;
	 t = (v * ((255 - ( (s * ((255 - remainder)>>1))>>7))>>2) ) >> 6;
	 if( q < 0 ) q=0;
	 if( t < 0 ) t=0;
	 if( p < 0 ) p=0;

#if 0
	 if(1)// if( (p>255) || (q>255) || (t>255) )
	 {
		PRINT(("hsv %d %d %d -> p %d q %d t%d v%d rem %d\n",h,s,v,p,q,t,v,remainder));
//		printf("%d %d %d -> hsv %d %d %d \n",r,g,b,h,s,v);
	 }
#endif

	 switch(region)
	 {
	  case 0:
	  	r = v; g = t; b = p; break;
	  case 1:
	        r = q; g = v; b = p; break;
	  case 2:
	  	r = p; g = v; b = t; break;
	  case 3:
	        r = p; g = q; b = v; break;
	  case 4:
	        r = t; g = p; b = v; break;
	  default:
	        r = v; g = p; b = q; break;
	 }
	}

	/* store */
	rgb[0] = r;
	rgb[1] = g;
	rgb[2] = b;
}


#define PRINT(a)
void cycle_rainbow( uint8_t *rgb, uint8_t increment )
{
	uint8_t r,g,b;
	uint8_t rmax,rmin; /* channel index */
	uint8_t delta;
	int16_t h,s,v;

	/* RGB 2 HSV conversion */
	r = rgb[0];
	g = rgb[1];
	b = rgb[2];

	/* sort channel indices by level */
	rmin = 2;
	rmax = 0;
	if( g > r )
	{ /* r < g */
		rmax = 1;
		if( b > g )
		{
			rmax = 2;
			rmin = 0;
		}
		else
		 if( r < b )
		 	rmin = 0;
	}
	else
	{ /* r >= g */
	 if( b > r )
	 {
		rmax = 2;
		rmin = 1;
	 }
	 else
	  if( g < b )
		 rmin = 1;
	}

	PRINT(("rmin %d rmax %d\n",rmin,rmax));

	delta = rgb[rmax]-rgb[rmin];
	v = rgb[rmax];
	if( delta < 128 )
		s = (v) ? (((uint16_t)delta)*255)/v : 0;
	else	s = (v) ? (((uint16_t)delta)<<7)/((v+1)>>1) : 0;
	if( s > 255 )
		s=255;
	// v is in Levels 0...255
	// s is Saturation * 256
	// H is in degrees * 255/360 

	if( !delta )
		h=0;
	else
	{
		switch( rmax )
		{
			case 0: /* R is max */
			/* 60° * ( (G-B)/delta mod 6 ) */ 
				h = 171*((int16_t)g-(int16_t)b)/delta;
				break;
			case 1: /* G is max */
			/* 60° * ( (B-R)/delta + 2 )   */
				h = (171*((int16_t)b-(int16_t)r))/delta + 342;
				break;
			default: /* B is max */
			/* 60° * ( (R-G)/delta + 4 )   */
				h = 171*((int16_t)r-(int16_t)g)/delta + 683;
				break;
		}
	}
#if 1
	h &= 0x3ff;
#else
	if( h < 0 )
		h=h+1024;
	if( h > 1023 )
		h=h-1024;
#endif
	PRINT(("%d %d %d -> hsv %d %d %d \n",r,g,b,h,s,v));

#if 1
	/* increase Hue */
	h += ((int)increment)<<2;
	h &= 0x3ff; /* periodicity with 1024 */
#endif

#if 1
	HSV2RGB( rgb, h, s, v );
#else
	/* transform back */
	r = v;
	g = v;
	b = v;
	if( s )
	{
	 int region,remainder,p,q,t;

	 region = h/171;
	 remainder=((h - (region * 171)) * 6)>>2;
	
	 p = (v * (255 - s)) >> 8;
	 q = (v * (255 - ((s * remainder) >> 8))) >> 8;
	 t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;
	 if( q < 0 ) q=0;
	 if( t < 0 ) t=0;
	 if( p < 0 ) p=0;

#if 1
	 if(1)// if( (p>255) || (q>255) || (t>255) )
	 {
		PRINT(("hsv %d %d %d -> p %d q %d t%d v%d rem %d\n",h,s,v,p,q,t,v,remainder));
//		printf("%d %d %d -> hsv %d %d %d \n",r,g,b,h,s,v);
	 }
#endif

	 switch(region)
	 {
	  case 0:
	  	r = v; g = t; b = p; break;
	  case 1:
	        r = q; g = v; b = p; break;
	  case 2:
	  	r = p; g = v; b = t; break;
	  case 3:
	        r = p; g = q; b = v; break;
	  case 4:
	        r = t; g = p; b = v; break;
	  default:
	        r = v; g = p; b = q; break;
	 }
	}

	/* store */
	rgb[0] = r;
	rgb[1] = g;
	rgb[2] = b;
#endif

}


unsigned char twi_ledupdate_pos;
unsigned char led_sending;
void twi_ledupdate_callback( uint8_t address, uint8_t *data )
{
	unsigned char i,t;
	unsigned char led_cur[3];

	/* a little delay between consecutive TWI writes */
	_delay_us(4);

	i=twicmds[twi_ledupdate_pos]; /* get target LED */
	if( i == TCMD_END ) /* end of list: commit changes */
	{
		initseq[0] = 0x49;
		initseq[1] = 0x00;
		twi_write(I2CADDRESS, initseq, 2, (0) ); /* No more callback */
		led_sending = 0;
		return;
	}
	t = twicmds[twi_ledupdate_pos+1]; /* get state index */

	initseq[0] = 0x01+i*3*2; /* LED index to hardware register */

	led_cur[0] = LED_RGB[i][t][0];
	led_cur[1] = LED_RGB[i][t][1];
	led_cur[2] = LED_RGB[i][t][2];

	switch( LED_MODES[i] )
	{
	 case LEDM_STATIC:
	 	break;
	 case LEDM_RAINBOW:
		LED_MODESTATE[i]++;
		cycle_rainbow( led_cur, LED_MODESTATE[i] );
		break;
	 case LEDM_PULSE:
		{
		 unsigned char d,c = LED_MODESTATE[i]+1;
		 LED_MODESTATE[i] = c;
		 if( c > 127 ) c = 255-c;

		 for( d=0 ; d < 3 ; d++ )
		 {
			led_cur[d] = ((int)led_cur[d] * (int)c + 32)>>7;
		 }
		}
		break;
	 //case LEDM_SAT:
	 default:
		{
		 unsigned char d,c = LED_MODESTATE[i]+1;
		 int dif,sum;
		 LED_MODESTATE[i] = c;
		 if( c > 127 ) c = 255-c;

		 sum = ((int)led_cur[0] + (int)led_cur[1] + (int)led_cur[1] + (int)led_cur[2])>>2;
		 for( d=0 ; d < 3 ; d++ )
		 {
			dif = (int)led_cur[d] - sum;
			dif = (dif * (int)c + 64)>>7; /* (de)saturate */
			led_cur[d] = sum + dif;
		 }
		}
	 	break;
	}
#if 1
	initseq[1] = pgm_read_byte(&gamma24_tableLH[led_cur[0]][GAMMATAB_L]); /* red L */
	initseq[2] = pgm_read_byte(&gamma24_tableLH[led_cur[0]][GAMMATAB_H]); /* red H */
	initseq[3] = pgm_read_byte(&gamma24_tableLH[led_cur[1]][GAMMATAB_L]); /* grn L */
	initseq[4] = pgm_read_byte(&gamma24_tableLH[led_cur[1]][GAMMATAB_H]); /* grn H */
	initseq[5] = pgm_read_byte(&gamma24_tableLH[led_cur[2]][GAMMATAB_L]); /* blu L */
	initseq[6] = pgm_read_byte(&gamma24_tableLH[led_cur[2]][GAMMATAB_H]); /* blu H */
#else
	/* TODO: Gamma table */
	initseq[1] = (LED_RGB[i][t][0]<<4);  /* red L */
	initseq[2] = (LED_RGB[i][t][0]>>4);  /* red H */
	initseq[3] = (LED_RGB[i][t][1]<<4);  /* grn L */
	initseq[4] = (LED_RGB[i][t][1]>>4);  /* grn H */
	initseq[5] = (LED_RGB[i][t][2]<<4);  /* blu L */
	initseq[6] = (LED_RGB[i][t][2]>>4);  /* blu H */
#endif
	twi_ledupdate_pos+=2;

	twi_write(I2CADDRESS, initseq, 7, twi_ledupdate_callback );
}


char led_putcommands( unsigned char *recvcmd, unsigned char nrecv )
{
	unsigned char index,st,r,g,b;
	char confget = -1,needsave = -1;
	unsigned char *sendbuf = recvcmd; /* just re-use the command buffer */

	while( nrecv )
	{
		index = *recvcmd & LEDINDEX_MASK;

		nrecv--;
		switch( *recvcmd++ & LEDCMD_MASK )
		{
			case LEDCMD_SAVECONFIG:
				needsave = 0x7f; /* save all */
				break;
			case LEDCMD_GETVERSION:
				confget = 0x7F; /* trigger version requested */
				break;
			case LEDCMD_GETCONFIG:
				if( !nrecv )
					break;
				nrecv--;
				recvcmd++;	/* skip argument byte (unused for now) */
				if( index >= (N_LED+N_LED_DIGI_CONF) )
					break;
				confget = index; /* trigger: this LED's config is needed */
				/* FIXME: only one config per call, for now -> also: size limitation of command and ring buffers */
				break;
			case LEDCMD_SOURCE:
				if( !nrecv )
					break;
				nrecv--;
				r = *recvcmd++;
				if( index >= (N_LED+N_LED_DIGI_CONF) )
					break;
				LED_SRCMAP[index] = r;
				LED_SECMAP[index] = get_secmap(r); /* bit combinations (including SWAP flag) for secondary function */
				break;
			case LEDCMD_SETMODE:
				if( !nrecv )
					break;
				nrecv--;
				r = *recvcmd++;
				if( index >= (N_LED+N_LED_DIGI_CONF) )
					break;
				LED_MODES[index] = r;
				LED_MODESTATE[index] = 0;
					break;
			case LEDCMD_COLOR:
				if( nrecv < 4 )
				{
					nrecv = 0; /* insufficient data, stop loop */
					break;
				}
				nrecv -= 4;
				st = *recvcmd++;
				r  = *recvcmd++;
				g  = *recvcmd++;
				b  = *recvcmd++;

				if( (st < LED_STATES) && (index < (N_LED+N_LED_DIGI_CONF) ) )
				{
					LED_RGB[index][st][0] = r;
					LED_RGB[index][st][1] = g;
					LED_RGB[index][st][2] = b;
				}
				break;

			default: /* unhandled command: stop loop */
				nrecv = 0;
				break;
		}
	}

	/* if we got the command to upload our config, just dump it into the
	   command receive buffer and return the number of bytes

           That data is inserted into the outgoing stream before ACK/NACK
	*/
	if( confget >= 0 ) /* def: -1 */
	{
		if( confget == 0x7F )
		{
			*sendbuf++ = LEDGV_HEADER;    /* 0xBA */
#ifdef KEYBOARD_TYPE
			*sendbuf++ = KEYBOARD_TYPE;   /* from Makefile */
#else
#ifndef PULL_RST
			/* auto mode: check RESET line, 
			              if up: A500 
				      else:  no connection =  A3000 */
			if( KBDSEND_RSTPIN & (1<<KBDSEND_RSTB) )
				*sendbuf++ = LEDGV_TYPE_A500;
			else
				*sendbuf++ = LEDGV_TYPE_A3000;
#else
			*sendbuf++ = LEDGV_TYPE_A500; /* LEDGV_TYPE_A3000,LEDGV_TYPE_A500 */
#endif
#endif
			if( LED_HAVE_DIGILED )
				*sendbuf++ = LEDGV_VERSION - 1;   /* 5,7,9,... = Digital LED enabled and present   */
			else	*sendbuf++ = LEDGV_VERSION;       /* 6,8,... = Digital LED enabled but not present */

			return 3;
		}

		*sendbuf++ = LED_SRCMAP[(unsigned char)confget];
		for( st = 0 ; st < LED_STATES ; st++ )
		{
			*sendbuf++ = LED_RGB[(unsigned char)confget][st][0];
			*sendbuf++ = LED_RGB[(unsigned char)confget][st][1];
			*sendbuf++ = LED_RGB[(unsigned char)confget][st][2];
		}
#if (LEDGV_VERSION>1)
		*sendbuf++ = LED_MODES[(unsigned char)confget];
		return (LED_STATES*3)+2;
#else
		return (LED_STATES*3)+1;
#endif
	}

	if( needsave >= 0 )
	{
		return -1;
	}

	return 0;
}


/* bit combinations (including SWAP flag) for secondary function 

   returns bogus comparison value (0xff) if no bit was set in "srcmap"
*/
unsigned char get_secmap( unsigned char srcmap )
{
	unsigned char ret = 0;
	int j,pripos,secpos;

	pripos = -1; /* lower bit */
	secpos = -1; /* upper bit */

	/* identify the two active bits (lower bit first, then upper bit) */
	for( j = 0 ; j < 7 ; j++ )
	{
		if( srcmap & (1<<j) )
		{
			pripos = j;
			j++;
			break;
		}
	}
	for(  ; j < 7 ; j++ )
	{
		if( srcmap & (1<<j) )
		{
			secpos = j;
			break;
		}
	}
	
	ret = 0xff; /* if no bit found: return bogus bit pattern 7 source bits and "swap" set */

	if( pripos >= 0 ) /* at least one bit found in source map ? */
	{
		/* secondary is upper bit ? */
		if( srcmap & 0x80 )
		{
			/* (sec > pri) or pri inactive */
			if( secpos >= 0 )
				pripos = secpos;
			/* secondary is either the only source bit (pripos) or 
			   the higher bit (from secpos) */
			ret = 0x80 | (1<<pripos);
		}
		else
		{
			/* secondary is the lower bit (if both sources are active) */
			if( secpos >= 0 )
				ret = (1<<pripos); /* pripos was checked above */
		}
	}

	return ret;
}


/* apply input state to LEDs */
unsigned char led_updatecontroller( unsigned char state )
{
	unsigned char chg,*tcmd,i,t,q;

	chg = src_active^state; /* changed inputs */
	if( !chg )
		return state;
	if( twi_isbusy() )
		return state;
	if( led_sending )
		return state;

	if( state & LED_FORCE_UPDATE )
		chg = LEDF_ALL; /* force all changed */
	state &= LEDF_ALL;
	/* traverse LEDs and issue commands into TWI wait queue
	   concept: write LED index and state into command buffer,
	            which is translated into TWI sequences in it's
		    write callback (plus "confirm" command)
	*/
	tcmd = twicmds; /* generate new command list */

	for( i=0 ; i < N_LED ; i++ )
	{
		/* does the change in sources apply to this LED? */
		if( !( LED_SRCMAP[i] & chg ) && (chg != LEDF_ALL) )
			continue; /* no, next */
		*tcmd++ = i; /* this LED needs new RGB */
#if 1
		t = LED_SRCMAP[i] & (state|0x80); /* get primary/secondary color from state map, decide based on srcmap what's primary, what's secondary */
		if( LED_SECMAP[i] == t ) /* is this bit combo (exactly) the secondary condition ? */
			q = 2;
		else
		{
			q = (t&0x7f) ? 1 : 0; /* either idle or primary */
		}
		*tcmd++ = q; // pgm_read_byte(&prisectab[t]);
#else
		t = LED_SRCMAP[i] & state; /* on/off state vs. assigned bit(s) */
		/* TODO: more states: secondary RGB */
		if( t )
			*tcmd++ = 1;
		else	*tcmd++ = 0;
#endif
	}

	/* TODO: respect LED_MODES */
	led_sending = 1;
	*tcmd++ = TCMD_END;
	*tcmd++ = TCMD_END;

	twi_ledupdate_pos = 0;    /* start of list */
	twi_ledupdate_callback(0,(0)); /* the callback will start to send the list */

	src_active = state; /* we did everything, save this state */

	return state;
}


void led_saveconfig( char neededleds )
{
	unsigned char start = 0;
	unsigned char last  = N_LED+N_LED_DIGI_CONF-1;
	unsigned char i,k;
	unsigned char *obuf;
	unsigned char *adr = (unsigned char *)0x100;

	/* 
		header:
		 0xBA, 0x58 = 0xBA 'X' (old config)
		 0xBA, 0x59 = 0xBA 'Y' (V5/V6 config)

		record per LED (2+11*LED_IDX):
		 1 Byte SRCMAP
		 3*3 Bytes RGB
		 1 Byte Mode

	*/
	obuf = adr;
	eeprom_write_byte( obuf, 0xBA );
	obuf++;
	eeprom_write_byte( obuf, 0x59 );

	for( i=start ; i <= last ; i++ )
	{
		obuf = adr + 2 + i*11;
		eeprom_update_byte( obuf++, LED_SRCMAP[i] );
		for( k=0 ; k < LED_STATES; k++ )
		{
			eeprom_update_byte( obuf++, LED_RGB[i][k][0] );
			eeprom_update_byte( obuf++, LED_RGB[i][k][1] );
			eeprom_update_byte( obuf++, LED_RGB[i][k][2] );
		}
		eeprom_update_byte( obuf++, LED_MODES[i] );
	}

	obuf = adr;
	eeprom_update_byte( obuf, 0xBA );
	obuf++;
	eeprom_update_byte( obuf, 0x59 );

#ifdef DEBUG
	uart1_puts("Config Saved ");
	uart_puthexuint( (uint16_t)obuf );
	uart1_puts("\n");
#endif
}

#if 0
uint8_t eeprom_read_byte (const uint8_t *__p);
void eeprom_read_block (void *__dst, const void *__src, size_t __n);

void eeprom_write_byte (uint8_t *__p, uint8_t __value);

void eeprom_update_byte (uint8_t *__p, uint8_t __value);
void eeprom_update_word (uint16_t *__p, uint16_t __value);
void eeprom_update_block (const void *__src, void *__dst, size_t __n);

Suppose we want to write a 55 value to address 64 in EEPROM, then we can write it as,
uint8_t ByteOfData = 0 x55 ;
eeprom_update_byte (( uint8_t *) 64, ByteOfData );
#endif

void led_loadconfig( char neededleds )
{
        unsigned char start = 0;
        unsigned char last  = N_LED-1; /* TODO: verify neededleds */
        unsigned char i,k,nf;
        unsigned char *obuf;
        unsigned char *adr = (unsigned char *)0x100;

	/* check header 
		 0xBA, 0x58 = 0xBA 'X'
	*/
	nf = 0;
	obuf = adr;
	if( eeprom_read_byte( obuf++ ) != 0xBA )
		nf = 1;
	i = eeprom_read_byte( obuf++ );
	if( i != 0x58 )
	{
		if( i != 0x59 )
			nf = 1;
		else	
			last = N_LED+N_LED_DIGI_CONF-1;
	}
	if( nf == 1 )
	{
#ifdef DEBUG
		uart1_puts("Config not found in EEPROM\n");
#endif
		return;
	}
#ifdef DEBUG
	uart1_puts("Loading Config from EEPROM\n");
#endif
	/* 
		record per LED (2+11*LED_IDX):
		 1 Byte SRCMAP
		 3*3 Bytes RGB
		 1 Byte Mode

	*/
	for( i=start ; i <= last ; i++ )
	{
		obuf = adr + 2 + i*11;

		LED_SRCMAP[i] = eeprom_read_byte( obuf++ );
		LED_SECMAP[i] = get_secmap(LED_SRCMAP[i]); /* bit combinations (including SWAP flag) for secondary function */

		for( k=0 ; k < LED_STATES; k++ )
		{
			LED_RGB[i][k][0] = eeprom_read_byte( obuf++ );
			LED_RGB[i][k][1] = eeprom_read_byte( obuf++ );
			LED_RGB[i][k][2] = eeprom_read_byte( obuf++ );
		}

		LED_MODES[i]  = eeprom_read_byte( obuf++ );
	}
}


/* TODO: better TWI receive function (but not needed in this code) */
unsigned char twirec;
void twi_callback(uint8_t adr, uint8_t *data)
{
	twirec=*data;
}



void led_defaults()
{
	uint8_t i;

	/* 0,1,2 are the floppy LED (left,mid,right) */
	LED_SRCMAP[0] = LEDF_SRC_FLOPPY | LEDF_SRC_IN4;
	LED_SRCMAP[1] = LEDF_SRC_FLOPPY | LEDF_SRC_IN3 | LEDF_MAP_SWAP;
	LED_SRCMAP[2] = LEDF_SRC_FLOPPY | LEDF_SRC_IN3;

	LED_SRCMAP[3] = LEDF_SRC_POWER;
	LED_SRCMAP[4] = LEDF_SRC_POWER;
	LED_SRCMAP[5] = LEDF_SRC_POWER;

	LED_SRCMAP[6] = LEDF_SRC_CAPS;

	for( i=0 ; i < 7 ; i++ )
		LED_SECMAP[i] = get_secmap(LED_SRCMAP[i]); /* bit combinations (including SWAP flag) for secondary function */
	for( i=0 ; i < N_LED ; i++ )
	{
		LED_MODES[i] = 0;
		LED_MODESTATE[i] = 0;
	}

	/* RGB defaults */
	for( i=0 ; i < 3 ; i++ )
	{ /* floppy */
		LED_RGB[i][LED_IDLE][0] = 0x00;
		LED_RGB[i][LED_IDLE][1] = 0x00;
		LED_RGB[i][LED_IDLE][2] = 0x00;
	}
	i=0;
	LED_RGB[i][LED_ACTIVE][0] = 0x00; /* green-ish */
	LED_RGB[i][LED_ACTIVE][1] = 0xFF;
	LED_RGB[i][LED_ACTIVE][2] = 0xEA;
	LED_RGB[i][LED_SECONDARY][0] = 0xFF; /* orange */
	LED_RGB[i][LED_SECONDARY][1] = 0x90;
	LED_RGB[i][LED_SECONDARY][2] = 0x00;

	i=1;
	LED_RGB[i][LED_ACTIVE][0] = 0xFF; /* orange */
	LED_RGB[i][LED_ACTIVE][1] = 0x90;
	LED_RGB[i][LED_ACTIVE][2] = 0x00;
	LED_RGB[i][LED_SECONDARY][0] = 0x00; /* cyan */
	LED_RGB[i][LED_SECONDARY][1] = 0xEA;
	LED_RGB[i][LED_SECONDARY][2] = 0xFF;

	i=2;
	LED_RGB[i][LED_ACTIVE][0] = 0x00; /* cyan */
	LED_RGB[i][LED_ACTIVE][1] = 0xEA;
	LED_RGB[i][LED_ACTIVE][2] = 0xFF;
	LED_RGB[i][LED_SECONDARY][0] = 0xFF; /* orange */
	LED_RGB[i][LED_SECONDARY][1] = 0x90;
	LED_RGB[i][LED_SECONDARY][2] = 0x00;

	/* Power */
	for( i=3 ; i < 6 ; i++ )
	{ 
		LED_RGB[i][LED_IDLE][0] = 0x20;
		LED_RGB[i][LED_IDLE][1] = 0x01;
		LED_RGB[i][LED_IDLE][2] = 0x04;
//		LED_RGB[i][LED_IDLE][1] = 0x01+((i-3)<<2);
//		LED_RGB[i][LED_IDLE][2] = 0x04+((i-3)<<1);
		LED_RGB[i][LED_ACTIVE][0] = 0xFF; /* red-ish */
		LED_RGB[i][LED_ACTIVE][1] = 0x03;
		LED_RGB[i][LED_ACTIVE][2] = 0x46;
//		LED_RGB[i][LED_ACTIVE][1] = 0x03+((i-3)<<5);
//		LED_RGB[i][LED_ACTIVE][2] = 0x46+((i-3)<<4);

	}

	/* Caps */
	i=6;
	LED_RGB[i][LED_IDLE][0] = 0x01;
	LED_RGB[i][LED_IDLE][1] = (0x32>>1);
	LED_RGB[i][LED_IDLE][2] = (0x26>>1);
	LED_RGB[i][LED_ACTIVE][0] = 0xA1; /* violet */
	LED_RGB[i][LED_ACTIVE][1] = 0x00;
	LED_RGB[i][LED_ACTIVE][2] = 0xC9;

	/* digital LEDs (V5/V6) */
	i=IDX_LED_DIGI; /* N_LED with V5 */
	LED_RGB[i][LED_IDLE][0] = 0x01; /* R */
	LED_RGB[i][LED_IDLE][1] = 0x50; /* G */
	LED_RGB[i][LED_IDLE][2] = 0xA0; /* B */
	LED_RGB[i][LED_ACTIVE][0] = 0x10; /* dark white */
	LED_RGB[i][LED_ACTIVE][1] = 0x10;
	LED_RGB[i][LED_ACTIVE][2] = 0x10;
	LED_MODES[i] = 0; /* 0=LEDD_FX_STATIC, 6==LEDD_FX_SPLASH */
}

void led_init()
{
  unsigned char i,j;

  led_currentstate = 0; /* all off/idle */
  adc_cycle = 0;
  src_active = 0;       /* source state as sent to LEDs  */

  /* LED sources */
  DRVLED_DDR  &= ~(1<<DRVLED_BIT);
  DRVLED_PORT &= ~(1<<DRVLED_BIT); /* no pull-up, used as analog input */
//  DRVLED_PORT |=  (1<<DRVLED_BIT); /* in, with pull-up */

  IN3LED_DDR  &= ~(1<<IN3LED_BIT);
  IN3LED_PORT &= ~(1<<IN3LED_BIT); // |=  (1<<IN3LED_BIT);
  IN4LED_DDR  &= ~(1<<IN4LED_BIT);
  IN4LED_PORT &= ~(1<<IN4LED_BIT); // |=  (1<<IN4LED_BIT);

  PLED_DDR    &=  ~(1<<PLED_BIT);
  PLED_PORT   &=  ~(1<<PLED_BIT); /* we use this as analog input */

  ADCSRA = 0x87;                  /* Enable ADC, fr/128  */
  ADMUX  = 0x40;                  /* Vref: Avcc, ADC channel: 0 (PortF on AT90USB1287 */

#if 0
  /* ADC test */
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


  /* set up I2C communication with LED controller */
  twi_init();
  DDRD |= (1<<5); /* Pin5 = SDB = Output */
  PORTD|= (1<<5); /* Pin5 = SDB = High   -> LED controller on */

  /* configure ports 1-21, RGB ordered */
  {
    uint8_t  *is;
    uint16_t idx;

	/* from table */
	idx=0;
	while( (i=pgm_read_byte(&ledinitlist[idx++])) != 0 )
	{
		is=initseq;
		j=i;
		while(j--)
		{
			*is++ = pgm_read_byte(&ledinitlist[idx++]);
		}
		twi_write(I2CADDRESS, initseq, i, NULL );
	};

	/* RGB balance (white balance) */
  	for( i=0 ; i < 21 ; i+=3 ) /* 21 outputs for 7 LEDs used */
  	{
		initseq[0] = 0x4A + i; /* target register */ 
		initseq[1] = 0xFF;     /* R */ 
		initseq[2] = 0xFF;     /* G */ 
		initseq[3] = 0x5F;     /* B */
		twi_write(I2CADDRESS, initseq, 4, NULL );
	}

	/* set RGB defaults and input port assignments */
	led_defaults();

	/* load config from EEPROM, if present */
	led_loadconfig( 0x7f );

	/* write some RGB values */
#if 0
	i=0;
	initseq[0] = 0x01+i*3*2;
	initseq[1] = 0xFF; /* red L */
	initseq[2] = 0x1F; /* red H */
	initseq[3] = 0xFF; /* green L */
	initseq[4] = 0x1F; /* green H */
	initseq[5] = 0xFF; /* blue L */
	initseq[6] = 0x1F; /* blue H */
	twi_write(I2CADDRESS, initseq, 7,NULL );

	i=3;
	initseq[0] = 0x01+i*3*2;
	initseq[1] = 0xFF; /* red L */
	initseq[2] = 0x00; /* red H */
	initseq[3] = 0x00; /* green L */
	initseq[4] = 0x00; /* green H */
	initseq[5] = 0x00; /* blue L */
	initseq[6] = 0x00; /* blue H */
	twi_write(I2CADDRESS, initseq, 7,NULL );
	i=4;
	initseq[0] = 0x01+i*3*2;
	initseq[1] = 0x00; /* red L */
	initseq[2] = 0x00; /* red H */
	initseq[3] = 0xff; /* green L */
	initseq[4] = 0x00; /* green H */
	initseq[5] = 0x00; /* blue L */
	initseq[6] = 0x00; /* blue H */
	twi_write(I2CADDRESS, initseq, 7,NULL );
	i=5;
	initseq[0] = 0x01+i*3*2;
	initseq[1] = 0x00; /* red L */
	initseq[2] = 0x00; /* red H */
	initseq[3] = 0x00; /* green L */
	initseq[4] = 0x00; /* green H */
	initseq[5] = 0xff; /* blue L */
	initseq[6] = 0x00; /* blue H */
	twi_write(I2CADDRESS, initseq, 7,NULL );
	i=6;
	initseq[0] = 0x01+i*3*2;
	initseq[1] = 0xff; /* red L */
	initseq[2] = 0x00; /* red H */
	initseq[3] = 0xff; /* green L */
	initseq[4] = 0x00; /* green H */
	initseq[5] = 0xff; /* blue L */
	initseq[6] = 0x00; /* blue H */
	twi_write(I2CADDRESS, initseq, 7,NULL );
#else
#if 0
	/* debug: light some LEDs */
	j=0xF0;
	for( i=0 ; i < 21 ; i++ ) 
	{
		initseq[0] = 0x01+i*2; /* PWM Low register (0x2 would be high) */
		initseq[1] = j;
		j ^= 0xFF;
		twi_write(I2CADDRESS, initseq, 2, NULL );
	}
#endif
#endif
#if 0
	/* confirm changes */
	initseq[0] = 0x49;
	initseq[1] = 0x00;
	twi_write(I2CADDRESS, initseq, 2, NULL );
#endif
#if 0
	/* DEBUG: READ settings */
	initseq[0] = 0x00;
	twi_write(I2CADDRESS, initseq, 1, NULL );
	twi_read(I2CADDRESS,1,twi_callback);
	uart_puthexuchar(twirec);
	DBGOUT(13);DBGOUT(10);

	initseq[0] = 0x02;
	twi_write(I2CADDRESS, initseq, 1, NULL );
	twi_read(I2CADDRESS,1,twi_callback);
	uart_puthexuchar(twirec);
	DBGOUT(13);DBGOUT(10);
#endif


  }


}




