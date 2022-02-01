/*
  definitions for keyboard and Atmega ports

  requires prior loading of avr headers
*/
#ifndef KBDEFS_H
#define KBDEFS_H

/* LED port, pin as bit index */
#define LEDPORT PORTB
#define LEDDDR  DDRB
#define LEDPIN  7

/* OUTPUT PORT: MAX. 8 BITS */
#define OPORT  PORTA
#define ODDR   DDRA
#define OMASK  0xFC
/* output start bit, output stop bit */ 
#define OSTART (1<<2)
#define OMAX   (1<<7)

/* 6 outputs */
#define OCOUNT 6

/* input ports */
#define ICOUNT 15

/* computer communication port(s) */

#ifdef DEBUG
/* clock on PF2 in DEBUG MODE (IN_LED3) */
#define KBDSEND_CLKP PORTF
#define KBDSEND_CLKD DDRF
#define KBDSEND_CLKB 2
#define KBDSEND_CLKPIN PINF
/* kbdata out on PF3 in DEBUG MODE (IN_LED4) */
#define KBDSEND_SENDP PORTF
#define KBDSEND_SENDD DDRF
#define KBDSEND_SENDB 3
#define KBDSEND_PIN   PINF
#else /* DEBUG */
/* REGULAR */
/* clock on PD2 */
#define KBDSEND_CLKP PORTD
#define KBDSEND_CLKD DDRD
#define KBDSEND_CLKB 2
#define KBDSEND_CLKPIN PIND
/* kbdata out on PD3 */
#define KBDSEND_SENDP PORTD
#define KBDSEND_SENDD DDRD
#define KBDSEND_SENDB 3
#define KBDSEND_PIN  PIND
#endif 

/* special keys (ALT,SHIFT,AMIGA,CTRL) */
#define OCOUNT_SPC 1
#define SPCB_LAMIGA 6
#define SPCB_LALT   5
#define SPCB_LSHIFT 4
#define SPCB_CTRL   3
#define SPCB_RAMIGA 2
#define SPCB_RALT   1
#define SPCB_RSHIFT 0
#define SPCMASK  0x7F /* PB0-PB6 */
#define SPCPORT  PORTB
#define SPCDDR   DDRB
#define SPCPIN   PINB

#if 1
#define KBDSEND_ACKPIN KBDSEND_PIN
#define KBDSEND_ACKB   KBDSEND_SENDB
#else
/* kbd acknowledge input on PB0 (other keyboard) */
#define KBDSEND_ACKP PORTB
#define KBDSEND_ACKD DDRB
#define KBDSEND_ACKPIN PINB
#define KBDSEND_ACKB 0
#endif

/* kbd reset output on PB1 (A500) */
#define KBDSEND_RSTP   PORTD
#define KBDSEND_RSTDDR DDRD
#define KBDSEND_RSTB 4

/* spare pads */
#define KBD_SPARE1PORT PORTD
#define KBD_SPARE1DDR  DDRD
#define KBD_SPARE1PIN  PIND
#define KBD_SPARE1B    6

#define KBD_SPARE2PORT PORTD
#define KBD_SPARE2DDR  DDRD
#define KBD_SPARE2PIN  PIND
#define KBD_SPARE2B    7



/* LED sources: Power (analog), Drive, LED3, LED4 */
#define PLED_DDR  DDRF
#define PLED_PORT PORTF
#define PLED_PIN  PINF
#define PLED_BIT 0
#define DRVLED_DDR  DDRF
#define DRVLED_PORT PORTF
#define DRVLED_PIN  PINF
#define DRVLED_BIT 1

/* IN_LED3, IN_LED4 are inactive in debug mode, just use a fake port in that case */
#ifndef DEBUG
#define IN3LED_DDR  DDRF
#define IN3LED_PIN  PINF
#define IN3LED_PORT PORTF
#define IN3LED_BIT 2

#define IN4LED_DDR  DDRF
#define IN4LED_PIN  PINF
#define IN4LED_PORT PORTF
#define IN4LED_BIT 3

#else /* DEBUG */
/* fake ports in debug mode: 4,5 are unused */
#define IN3LED_DDR  DDRF
#define IN3LED_PIN  PINF
#define IN3LED_PORT PORTF
#define IN3LED_BIT 4
#define IN4LED_DDR  DDRF
#define IN4LED_PIN  PINF
#define IN4LED_PORT PORTF
#define IN4LED_BIT 5
#endif /* DEBUG */


#endif /* KBDEFS_H */
