/*
 ********************************************************************************
 * led_digital.c                                                                *
 *                                                                              *
 * Author: Henryk Richter <bax@comlab.uni-rostock.de>                           *
 *                                                                              *
 * Purpose: control for digital LED strip (APA102,SK9822)                       *
 *                                                                              *
 *                                                                              *
 *                                                                              *
 *                                                                              *
 *                                                                              *
 ********************************************************************************
*/
/*
 modes:
  - static color
  - static color + moving white dot
  - rainbow slow
  - rainbow fast
  - KITT (knight rider)
  - saturation

  - splash (white in currently pressed column, then fade to static color)

*/
#define LEDD_FX_STATIC     0
#define LEDD_FX_DOT        1
#define LEDD_FX_RAINBOWSLW 2
#define LEDD_FX_RAINBOWFST 3
#define LEDD_FX_SATURATION 4
#define LEDD_FX_KITT       5

#define LEDD_FX_SPLASH     6

#define LEDD_STATE LED_ACTIVE

#include <avr/interrupt.h>
#include <avr/pgmspace.h>
//#include <avr/io.h>
#include <util/delay.h> /* might be <avr/delay.h>, depending on toolchain */
//#include "avr/delay.h"
#include "baxtypes.h"
#include "spi.h"
#include "led_digital.h"
#include "led.h" /* HSV2RGB */
#define GAMMATAB_EXT
#include "gammatab.h"

#ifndef NULL
#define NULL (0)
#endif

void LED_Start_Frame();
void LED_Stop_Frame( unsigned char nled );

unsigned char LED_update_dot( unsigned char ledd_cur );
unsigned char LED_update_rainbow( unsigned char pos, unsigned char spd  );
unsigned char LED_update_saturation( unsigned char pos, unsigned char spd  );
unsigned char LED_update_kitt( unsigned char pos );

/* actual update rate */
#define LED_DELAY 1

unsigned char ledd_cur = 0;
unsigned char ledd_dly = LED_DELAY;
unsigned char ledd_fxstate[N_DIGI_LED]; /* private to active fx */

/* call for a single time instance (~16ms) */
char led_digital_step()
{
 unsigned char fx = LEDD_FX_RAINBOWSLW; //LEDD_FX_SATURATION;//LEDD_FX_KITT; //LEDD_FX_RAINBOWSLW; //LEDD_FX_DOT;

 fx = led_getmode( IDX_LED_DIGI );
// { unsigned char *rgb = led_getcolor( IDX_LED_DIGI, LEDD_STATE );

 /* global update interval ( 15.2ms*(1+LED_DELAY) ) */
 if( ledd_dly > 0 )
 {
 	ledd_dly--;
	return 0;
 }
 ledd_dly = LED_DELAY;

 /* start update cycle */
 LED_Start_Frame();

 /* TODO: more cute effects */
 switch( fx )
 {
	case LEDD_FX_DOT:
		ledd_cur = LED_update_dot( ledd_cur );break;
	case LEDD_FX_RAINBOWSLW:
		ledd_cur = LED_update_rainbow( ledd_cur, 1 );break;
	case LEDD_FX_RAINBOWFST:
		ledd_cur = LED_update_rainbow( ledd_cur, 4 );break;
	case LEDD_FX_SATURATION:
		ledd_cur = LED_update_saturation( ledd_cur, 1 );break;
	case LEDD_FX_KITT:
		ledd_cur = LED_update_kitt( ledd_cur ); break;

 	case LEDD_FX_STATIC:
	default:
		ledd_cur = LED_update_dot( N_DIGI_LED );break;
 }

 
 LED_Stop_Frame(N_DIGI_LED);


 return 0;
}

#define N_KITT_LED 9

unsigned char LED_update_kitt( unsigned char pos )
{
  unsigned char idx,r,g,b;
//  int16_t dst;
  unsigned char cur;//,dir;
//  unsigned char *rgb = led_getcolor( IDX_LED_DIGI, LEDD_STATE );

#define KITT_POSDIV 2

//  dir = 0;
  cur = ( pos>>KITT_POSDIV );
  if( cur >= N_KITT_LED )
  {
//  	dir = 1;
	if( cur >= N_KITT_LED*2 )
	{
	 cur = 0;
	 pos = 0xff;
	}
	else
	 cur = N_KITT_LED*2 - 1 - cur;
  }
  pos++;

  for( idx = 0 ; idx < N_KITT_LED ; idx++ )
  {
#define KITT_SPD 15
   if( idx == cur )
   {
   	ledd_fxstate[idx] = 255-5-(1<<KITT_POSDIV);
   }
   else
   {
    if( ledd_fxstate[idx] >= KITT_SPD )
		ledd_fxstate[idx] -= KITT_SPD; 
    else	ledd_fxstate[idx]  =  0;
   }

   r = ledd_fxstate[idx] + 5; /* some background glow */

   LCD_SPI( 0xE0 + 31 ); /* preamble + brightness (<31 might flicker, see https://cpldcpu.com/2014/11/30/understanding-the-apa102-superled/) */
   g = 0;
   b = 0;

   if( cur==idx )
   {
    if( (idx == 0) || (idx == (N_KITT_LED-1)))
     g = r>>2;
   }

   LCD_SPI( b ); // B
   LCD_SPI( g ); // G
   LCD_SPI( r ); // R

  }

  return pos;
}

unsigned char LED_update_saturation( unsigned char pos, unsigned char spd  )
{
  uint8_t rgb[3];
  int16_t hsv[3];
  int16_t s;
  unsigned char idx;
  unsigned char *rgb_src = led_getcolor( IDX_LED_DIGI, LEDD_STATE );

  RGB2HSV( hsv, rgb_src[0], rgb_src[1], rgb_src[2] );

  if( pos <= 127 )
  	s = 127-pos;
  else	s = pos-128; /* this hits zero twice but avoids s=128 -> s+s=256 */
  s += s; /* 0...127 -> 0..254 */

  HSV2RGB( rgb, hsv[0], s , hsv[2] );

  for( idx = 0 ; idx < N_DIGI_LED ; idx++ )
  {
	LCD_SPI( 0xE0 + 31 ); /* preamble + brightness */
	LCD_SPI( pgm_read_byte( &gamma24_tableLH[rgb[2]][GAMMATAB_H] )); // B
	LCD_SPI( pgm_read_byte( &gamma24_tableLH[rgb[1]][GAMMATAB_H] )); // G
	LCD_SPI( pgm_read_byte( &gamma24_tableLH[rgb[0]][GAMMATAB_H] )); // R
  }

  return pos+spd;
}

unsigned char LED_update_rainbow( unsigned char pos, unsigned char spd  )
{
  /* pos: "H" in HSV model */
  uint16_t h = ((uint16_t)pos);
  uint8_t rgb[3];
  unsigned char idx;

  if( spd > 1 )
  {
	ledd_fxstate[0] = 0; 
	pos += spd;
  }
  else
  {
   ledd_fxstate[0] += spd;
   if( ledd_fxstate[0] > 3 )
   {
  	pos++;
	h++;
	ledd_fxstate[0] = 0;
   }
  }

  HSV2RGB( (uint8_t*)rgb, (int16_t)( (h<<2)+ledd_fxstate[0] ), (int16_t)255, (int16_t)255 );

  for( idx = 0 ; idx < N_DIGI_LED ; idx++ )
  {
	LCD_SPI( 0xE0 + 31 ); /* preamble + brightness (<31 might flicker, see https://cpldcpu.com/2014/11/30/understanding-the-apa102-superled/) */
#if 1
	LCD_SPI( rgb[2] ); // B
	LCD_SPI( rgb[1] ); // G
	LCD_SPI( rgb[0] ); // R
#else
	LCD_SPI( pgm_read_byte( &gamma24_tableLH[rgb[2]][GAMMATAB_H] )); // B
	LCD_SPI( pgm_read_byte( &gamma24_tableLH[rgb[1]][GAMMATAB_H] )); // G
	LCD_SPI( pgm_read_byte( &gamma24_tableLH[rgb[0]][GAMMATAB_H] )); // R
#endif
  }

  return pos; /* will wrap around automatically */
}



unsigned char LED_update_dot( unsigned char ledd_cur )
{
 unsigned char idx;
 unsigned char *rgb = led_getcolor( IDX_LED_DIGI, LEDD_STATE );

 for( idx = 0 ; idx < N_DIGI_LED ; idx++ )
 {
  LCD_SPI( 0xE0 + 31 ); /* preamble + brightness (<31 might flicker, see https://cpldcpu.com/2014/11/30/understanding-the-apa102-superled/) */
  if( idx == ledd_cur )
  {
	LCD_SPI( 0xf1 ); // B
	LCD_SPI( 0xef ); // G
	LCD_SPI( 0xff ); // R
  }
  else
  {	/* SK9822: GRB */
  	/* APA102: BGR */
	LCD_SPI( rgb[2] ); // B
	LCD_SPI( rgb[1] ); // G
	LCD_SPI( rgb[0] ); // R
  }
 }

 /* 1 time step further */
 ledd_cur++;
 if( ledd_cur >= N_DIGI_LED )
  	ledd_cur = 0;

 return ledd_cur;
}

/* reset (start frame) string to LCD row */
void LED_Start_Frame()
{
     LCD_SPI( 0 );
     LCD_SPI( 0 );
     LCD_SPI( 0 );
     LCD_SPI( 0 );
}

void LED_Stop_Frame( unsigned char nled )
{

     /* SK9822 end frame (=reset) */
     LCD_SPI( 0 );
     LCD_SPI( 0 );
     LCD_SPI( 0 );
     LCD_SPI( 0 );

     /* APA102 end frame */
     /* TODO: we should only need nled/2 zeroes */
     while( 1 )
     {
	 LCD_SPI( 0 );

	 if( nled < 8 )
	 	break;
	 nled -= 8;
     }

}

