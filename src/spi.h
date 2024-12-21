/* ********************************************************************************
  file:
   spi.h
   
  purpose:


  author, copyright:
   Henryk Richter <bax@comlab.uni-rostock.de>

  version:
   0.1

  date:
   10-Apr-2016

  devices:
   cheap 1.44" LCDs

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
*********************************************************************************** */
#ifndef	SPI_H
#define	SPI_H

#ifdef __cplusplus
#define PROTOHEADER "C"
#else
#define PROTOHEADER
#endif

/* ******************************************************************************** */
/* functional options of library: 
    Hardware / Software SPI
*/
#define _LCD_SOFT_SPI
                        /* software SPI implementation (slower but more flexible), 
                           else hardware SPI (faster, requires to set the correct 
                           SCK/MOSI pins -> requires MPU with hardware SPI, disable
			   for MCUs like ATTINY85                                   */
/* ******************************************************************************** */


/* ******************************************************************************** */
#if 1
/*

*/
#define LCD_PORT_CLK  PORTA
#define LCD_DDR_CLK   DDRA
#define LCD_PIN_CLK   1
#define LCD_PORT_DATA PORTA
#define LCD_DDR_DATA  DDRA
#define LCD_PIN_DATA  0 
#endif

/* this #define, if not commented out enables the fast software SPI write routine 
   and means that CLK and DATA are on PORT B (change to PINA,PINC,PIND if appropriate) */
#define LCD_PIN_CLKDATA PINA

/* fast software SPI (when enabled with LCD_PIN_CLKDATA defined) runs at clock/4. If your
   display does not initialize, try disabling the LCD_PIN_CLKDATA define. That way, the
   SPI runs approximately at f/6 (2.6 MHz @ 16 MHz MCU). If that helps, check your 
   cabling afterwards (not too long, proper voltage dividers if any, etc.) You might also
   consider hardware SPI at reduced clock. */

/* ******************************************************************************** */

void LCD_SPI_Start();
void LCD_SPI( unsigned char dta );

/* ******************************************************************************** */
/*
   Functions
*/
/* ******************************************************************************** */

/* set/clear Bit in register or port */
#define BitSet( _port_, _bit_ ) _port_ |=   (1 << (_bit_) )
#define BitClr( _port_, _bit_ ) _port_ &= (~(1 << (_bit_) ) )
/* copy bit from source to destination (2 clocks via T Bit) */
#define BitCpy( _out_, _obit_, _in_, _ibit_ ) \
	asm volatile ( "bst %2,%3" "\n\t" \
	               "bld %0,%1" : "+r" (_out_) : "I" (_obit_) , "r" (_in_) , "I" (_ibit_) );


#endif	/* SPI_H */
