/* ********************************************************************************
  file:
   spi.c
   
  purpose:
   ATMega Soft/Hard SPI


  author, copyright:
   Henryk Richter <bax@comlab.uni-rostock.de>

  version:
   0.1

  date:
   10-Apr-2016

  devices:

  acknowledgements:

  license:
   GPL v2

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

  notes:
   - consult header-file for PIN assignments
   - you are free to choose any 3-5 digital output pins in software SPI mode
   - modify LCD_PORT_*, LCD_DDR_*, LCD_PIN_* to your liking
   - in hardware SPI mode, observe clock settings and connect CLK/DATA to SCK/MOSI
     pins of your device

  stats:

*********************************************************************************** */
#include <avr/pgmspace.h>
#include <util/delay.h> /* might be <avr/delay.h>, depending on toolchain */
#include "spi.h"

void LCD_SPI_Start()
{
	BitSet( LCD_DDR_CLK,  LCD_PIN_CLK  );
	BitSet( LCD_DDR_DATA, LCD_PIN_DATA );
#ifndef _LCD_SOFT_SPI
	BitClr(LCD_PORT_CLK,  LCD_PIN_CLK   ); /* clean start of HW SPI */
	BitClr(LCD_PORT_DATA, LCD_PIN_DATA  );
	/* */
	/* double speed (SPSR|1), clock divider in SPR Bits 8 (01 -> clock*2/16=2 MHz @ clock=16MHz, 00=f*2/4 -> 8 MHz ) */
	/* */
	SPSR |= 1;  /* enable double speed  */ // 
	//SPSR &= ~1; /* disable double speed */
	SPCR  = (0<<SPIE)|(1<<SPE)|(0<<DORD)|(1<<MSTR)|(0<<CPOL)|(0<<CPHA)|(1<<SPR1)|(0<<SPR0);	
#endif
}

#if 0
void LCD_SPI_Stop()
{
#ifndef _LCD_SOFT_SPI
	BitClr(LCD_PORT_CLK,  LCD_PIN_CLK   ); /* clean start of HW SPI */
	BitClr(LCD_PORT_DATA, LCD_PIN_DATA  );
#endif
}
#endif

/* 
  one byte output via SPI

  note: 3 implementations in here
        - Software Slow ( #define _LCD_SOFT_SPI, #undef LCD_PIN_CLKDATA  )
	- Software Fast ( #define _LCD_SOFT_SPI, #define LCD_PIN_CLKDATA )
	- Hardware      ( #undef _LCD_SOFT_SPI)
        -> sorry for the whole lot of preprocessor macros, I wanted everything in one active
	   source code path, regardless of fast or slow SPI mode
	-> speed over size here, the loop is always unrolled
*/
#ifdef _LCD_SOFT_SPI
void LCD_SPI( unsigned char dta )
{
#ifndef LCD_PIN_CLKDATA 

#define SPI_BIT_START( _val_, _bit_, _portd_,_pind_,_portc_,_pinc_,_nbit_)
	/* this code supports different ports for CLK, DATA but is slower (~2.6 MHz @ 16 MHz MCU) */
#define SPI_BIT( _val_, _bit_, _portd_,_pind_,_portc_,_pinc_,_nbit_) \
		BitClr( _portd_, _pind_ ); \
		if( (_val_ & (1<<_bit_)) ) BitSet( _portd_, _pind_ ); \
		BitSet( _portc_, _pinc_);\
		BitClr( _portc_, _pinc_);

#else /* LCD_PIN_CLKDATA */
	/* this code is way faster (fastest SW SPI known to date) but requires CLK,DATA on the same port */

/* this lower code reqires clk/data on the same port and is a bit faster (test bench 11s) */
	register unsigned char regclr = LCD_PORT_DATA; /* CLK may be high, Data bit is copied anyway */
	register unsigned char regclk;
#define SPI_BIT_START( _val_, _bit_, _portd_,_pind_,_portc_,_pinc_,_nbit_) \
	asm volatile ( "bst %0,%1": : "r" (_val_), "I" (_bit_)   ); \
	asm volatile ( "ldi %0,%1": "=r" (regclk) : "I" (1<<_pinc_) ); \
	asm volatile ( "cbr %0,%1": "+r" (regclr) : "i" (1<<_pinc_) );

/* this one requires _portd_ == _portc_, at 16 MHz, the "nop" is required, though  */
#define SPI_BIT( _val_, _bit_, _portd_,_pind_,_portc_,_pinc_,_nbit_) \
	asm volatile ( "bld %0,%1": "+r" (regclr) : "I" (_pind_)  );\
	asm volatile ( "out %0,%1": : "I" (_SFR_IO_ADDR(_portd_)), "r" (regclr)  );\
	asm volatile ( "bst %0,%1": : "r" (_val_), "I" (_nbit_)   );\
	asm volatile ( "out %0,%1": : "I" (_SFR_IO_ADDR(_portd_)-2), "r" (regclk) );

#endif /* LCD_PIN_CLKDATA */

	SPI_BIT_START(dta,7,LCD_PORT_DATA,LCD_PIN_DATA,LCD_PORT_CLK,LCD_PIN_CLK,6);
	SPI_BIT(dta,7,LCD_PORT_DATA,LCD_PIN_DATA,LCD_PORT_CLK,LCD_PIN_CLK,6);
	SPI_BIT(dta,6,LCD_PORT_DATA,LCD_PIN_DATA,LCD_PORT_CLK,LCD_PIN_CLK,5);
	SPI_BIT(dta,5,LCD_PORT_DATA,LCD_PIN_DATA,LCD_PORT_CLK,LCD_PIN_CLK,4);
	SPI_BIT(dta,4,LCD_PORT_DATA,LCD_PIN_DATA,LCD_PORT_CLK,LCD_PIN_CLK,3);
	SPI_BIT(dta,3,LCD_PORT_DATA,LCD_PIN_DATA,LCD_PORT_CLK,LCD_PIN_CLK,2);
	SPI_BIT(dta,2,LCD_PORT_DATA,LCD_PIN_DATA,LCD_PORT_CLK,LCD_PIN_CLK,1);
	SPI_BIT(dta,1,LCD_PORT_DATA,LCD_PIN_DATA,LCD_PORT_CLK,LCD_PIN_CLK,0);
	SPI_BIT(dta,0,LCD_PORT_DATA,LCD_PIN_DATA,LCD_PORT_CLK,LCD_PIN_CLK,0);
	/* clock stays high */
}
#else /* _LCD_SOFT_SPI */
inline void LCD_SPI(unsigned char data)
{
	SPDR  = data;
	while(!(SPSR & (1<<SPIF)));
}
#endif /* _LCD_SOFT_SPI */



