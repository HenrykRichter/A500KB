/*
  ledmanager.c

  Retrieve/Send current LED config

  The LEDmanager maintains current (and default) settings for the 7 LEDs
  on the Keyboard. It also communicates with the Keyboard by sending/receiving
  configuration data across the serial link.

  The definitions in ledmanager.h should be kept to match the
  implementation on the other end (see led.c in the AVR sources).

*/
#include "compiler.h"
#include "ledmanager.h"
//#include "ciacomm.h" // see ledmanager.h
#include <proto/exec.h>
#include <proto/dos.h>
#include <exec/memory.h>
#include <dos/dos.h>


/* current state */
unsigned short LED_SRCMAP[N_LED][LED_STATES]; /* flag bits (not flags) applying to this LED (state 0 is bogus) */
unsigned char  LED_RGB[N_LED][LED_STATES][3]; /* RGB config for LEDs */
unsigned char  LED_MODES[N_LED];  /* static,cycle, rainbow, knight rider etc. */

/* last sent state / default / loaded from Keyboard  */
unsigned short LED_lastSRCMAP[N_LED][LED_STATES];
unsigned char  LED_lastRGB[N_LED][LED_STATES][3]; /* RGB config for LEDs */
unsigned char  LED_lastMODES[N_LED];  /* static,cycle, rainbow, knight rider etc. */

UBYTE cmdstream[48]; /* 2 bytes preamble, 1 CMD SOURCE (2 bytes), 3 CMDs RGB (5 bytes each) */
SHORT lastchange;    /* index of last LED that was changed in config tool */
SHORT lastsent;
SHORT retries;       /* we try to re-send data a couple of times */
SHORT needcfg;       /* we need to save current config in EEPROM */
#define NRETRIES 10

#define LEMCF_CHK 1

/* private proto */
void led_defaults();
LONG ledmanager_copy_last( LONG led, LONG flags ); /* copy LED settings to last sent location */
LONG ledmanager_sendcommands( LONG led ); /* generate command stream and send data */


LONG ledmanager_init(void)
{
	led_defaults();
	lastchange = -1; /* no LED config was changed recently */
	lastsent   = -1; /* last attempted transmission LED index */
	needcfg    =  0; /* we don't need to send save command */

	return CIAKB_Init();
}


LONG ledmanager_exit(void)
{
	return CIAKB_Exit();
}

/* store config in Keyboard's eeprom */
LONG ledmanager_saveEEPROM(void)
{
	return ledmanager_sendConfig( LEDIDX_SAVEEEPROM );
}

/*
  Send configuration changes to keyboard
   - check last changed LED first
     - assemble config directive, write
   - in next run: check return 
     - send again, if failure
     - if ok, copy configuration to "last known good"

  note: tosendled is only adhered when no retries on the previous led are in progress
*/
LONG ledmanager_sendConfig( LONG tosendled )
{
//	LONG tosendled = -1; /* index that needs sending next (or retry) */
	LONG res = KCMD_IDLE;

	if( tosendled == LEDIDX_SAVEEEPROM )
	{
		needcfg	= 1;
	}

	/* check if busy (1/0) = try again later */
	if( CIAKB_IsBusy() )
	{
		return KCMD_BUSY;
	}

	/* did we recently send something ? */
	if( lastsent >= 0 )
	{
		res = CIAKB_Wait();
		if( (res == KCMD_ACK) || (res==KCMD_IDLE) )
		{
			ledmanager_copy_last( lastsent, 0 );
			if( lastsent == LEDIDX_SAVEEEPROM )
				needcfg = 0;
			lastsent = -1; /* ok, done.  */
			retries  =  0; /* no retries */
		}
		else
		{
			retries++;
			tosendled = lastsent;
			if( retries < NRETRIES )
				res |= KCMD_RETRYING;
		}
	}
	
	/* nothing to send yet ? check LED list */
	if( tosendled < 0 )
	{
		if( needcfg )
			tosendled = LEDIDX_SAVEEEPROM;
		else
		{
			for( tosendled=0 ; tosendled < N_LED ; tosendled++ )
			{
				/* any LED that has changes ? */
				if( 0 != ledmanager_copy_last( tosendled, LEMCF_CHK ) )
					break;
			}
			if( tosendled == N_LED )
				tosendled = -1;
		}
	}

	if( tosendled >= 0 )
	{
		/* send command stream to keyboard */
		lastsent = tosendled; /* remember for retries */
		if( ledmanager_sendcommands( tosendled ) != 0 )
			res = KCMD_NACK;
	}
	else
		res |= KCMD_NOWORK;

	return res; /* KCMD_TIMEOUT,KCMD_ACK,KCMD_NACK,KCMD_IDLE, optionally |KCMD_RETRYING */
}


/*

*/
LONG ledmanager_sendcommands( LONG led )
{
	UBYTE *cmd = cmdstream;
	LONG  ncmd = 0;
	LONG  act,sec,res,i;

	/* preamble */
	*cmd++ = 0x00;
	*cmd++ = 0x03;

	if( led == LEDIDX_SAVEEEPROM )
	{
		*cmd++ = LEDCMD_SAVEEEPROM;
	}
	else
	{
		/* source mapping:
		   if LED_ACTIVE < LED_SECONDARY, then send inverse flag
		   alongside activation mask
		   if both are the same source, then use ACTIVE only
		   if( SECONDARY but not ACTIVE), then send inverse flag,too
		*/
		*cmd++ = LEDCMD_SOURCE | led;
		act    = LED_SRCMAP[led][LED_ACTIVE];
		sec    = LED_SRCMAP[led][LED_SECONDARY];
		if( (sec != LEDB_SRC_INACTIVE) &&               /* if secondary is inactive, we won't need to swap */
		    ( (act < sec) || (act==LEDB_SRC_INACTIVE) ) /* if primary is inactive or the secondary has a higher index, then swap */
		  )
		{
			res = (1<<act) | (1<<sec) | LEDF_SRC_SWAP;
		}
		else
		{	/* secondary is < primary, hence primary will light first */
			res = (1<<act) | (1<<sec);
		}
		*cmd++ = (UBYTE)res;

		/* now send colors = CMD+STATE+RGB */
		for( i=LED_IDLE ; i <= LED_SECONDARY ; i++ )
		{
			*cmd++ = LEDCMD_COLOR | led;
			*cmd++ = i;
			*cmd++ = LED_RGB[led][i][0];
			*cmd++ = LED_RGB[led][i][1];
			*cmd++ = LED_RGB[led][i][2];
		}
	}

	ncmd = cmd - cmdstream;

	/* 0==OK, else FAIL */
	return CIAKB_Send( cmdstream, ncmd );
//	return 0;
}


/* decode packed SRCMAP (including SWAP flag) into activation
   flag numbers 

   returns primary or secondary activation flag number (not mask) or
   LEDB_SRC_INACTIVE for unused activation options (primary and/or
   secondary)
*/
ULONG ledmanager_decodesrcmap( ULONG srcmap, ULONG which )
{
	ULONG ret;
	int j,lobit,hibit;


	lobit = -1; /* lower bit */
	hibit = -1; /* upper bit */

	/* identify the two active bits (lower bit first, then upper bit) */
	for( j = 0 ; j < 7 ; j++ )
	{
		if( srcmap & (1<<j) )
		{
			lobit = j;
			j++;
			break;
		}
	}
	for(  ; j < 7 ; j++ )
	{
		if( srcmap & (1<<j) )
		{
			hibit = j;
			break;
		}
	}
	
	ret = LEDB_SRC_INACTIVE; /* if no bit found: assume inactive */ 

	if( lobit >= 0 ) /* at least one bit found in source map ? */
	{
		/* secondary is upper bit ? */
		if( srcmap & 0x80 )
		{
			/* (sec > pri) or pri inactive */
			if( hibit < 0 ) /* only one bit in mask -> secondary == lobit */
			 ret = (which == LED_ACTIVE) ? LEDB_SRC_INACTIVE : lobit;
			else
			 ret = (which == LED_ACTIVE) ? lobit : hibit;
		}
		else
		{
			/* secondary is the lower bit (if both sources are active) */
			if( hibit < 0 )
			 ret = (which == LED_ACTIVE) ? lobit : LEDB_SRC_INACTIVE;
			else
			 ret = (which == LED_ACTIVE) ? hibit : lobit;
		}
	}

	return ret;
}


/* load single configuration entry */
LONG ledmanager_loadconfigentry( LONG led, UBYTE *recvbuf, LONG nbytes, ULONG flags )
{
	SHORT i;

	if( (ULONG)led >= N_LED )
		return -1;
 /* format:
     LED_SRCMAP (1 byte)
     RGB idle   (3 bytes)
     RGB active (3 bytes)
     RGB secondary (3 bytes)
 */
	/* TODO: decode sourcemap */
	LED_SRCMAP[led][LED_ACTIVE]   = ledmanager_decodesrcmap( *recvbuf, LED_ACTIVE );
	LED_SRCMAP[led][LED_SECONDARY]= ledmanager_decodesrcmap( *recvbuf, LED_SECONDARY );
	recvbuf++;

	/* copy RGB */
	for( i=LED_IDLE ; i <= LED_SECONDARY ; i++ )
	{
		LED_RGB[led][i][0] = *recvbuf++;
		LED_RGB[led][i][1] = *recvbuf++;
		LED_RGB[led][i][2] = *recvbuf++;
	}

	if( flags & 1 )
	{
		/* this is from keyboard, hence we are currently in sync and there is no
		   need to send these parameters back
		*/
		ledmanager_copy_last( led, 0 );
	}

	return 0;
}

LONG ledmanager_getColor(LONG led,LONG state,LONG rgb)
{
	LONG ret = 0;

	if( (ULONG)led >= N_LED )
		return ret;
	if( (ULONG)state >= LED_STATES )
		return ret;
	if( (ULONG)rgb >= 3 )
		return ret;

	ret = LED_RGB[led][state][rgb];

	return ret;
}


LONG ledmanager_getSrc(LONG led, LONG state)
{
	LONG ret = 0;

	if( (ULONG)led >= N_LED )
		return LEDB_SRC_INACTIVE;
	if( state == LED_IDLE )
		return LEDB_SRC_INACTIVE;

	ret = LED_SRCMAP[led][state];

	return ret;
}


LONG ledmanager_getMode(LONG led)
{
	long ret = 0;

	if( (ULONG)led >= N_LED )
		return ret;

	ret = LED_MODES[led];

	return ret;
}


void ledmanager_setColor(LONG led,LONG state,LONG rgb,LONG col)
{
	if( (ULONG)led >= N_LED )
		return;
	if( (ULONG)state >= LED_STATES )
		return;
	if( (ULONG)rgb >= 3 )
		return;

	lastchange = led;

	LED_RGB[led][state][rgb] = (unsigned char)col;
}


void ledmanager_setSrc( LONG led, LONG state, LONG src)
{
	if( (ULONG)led >= N_LED )
		return;
	if( (ULONG)state >= LED_STATES )
		return;

	lastchange = led;

	LED_SRCMAP[led][state] = src;
}


void ledmanager_setMode(LONG led, LONG mode)
{
	if( (ULONG)led >= N_LED )
		return;

	lastchange = led;

	LED_MODES[led] = (unsigned char)mode;
}



void led_defaults()
{
	int i;

	for( i=0 ; i < N_LED ; i++ )
	{
		LED_MODES[i] = 0;
	}

        /* 0,1,2 are the floppy LED (left,mid,right) */
        LED_SRCMAP[0][LED_ACTIVE] = LEDB_SRC_FLOPPY;
        LED_SRCMAP[1][LED_ACTIVE] = LEDB_SRC_FLOPPY;
        LED_SRCMAP[2][LED_ACTIVE] = LEDB_SRC_IN3;

        LED_SRCMAP[3][LED_ACTIVE] = LEDB_SRC_POWER;
        LED_SRCMAP[4][LED_ACTIVE] = LEDB_SRC_POWER;
        LED_SRCMAP[5][LED_ACTIVE] = LEDB_SRC_POWER;

        LED_SRCMAP[6][LED_ACTIVE] = LEDB_SRC_CAPS;

	for( i=0 ; i < N_LED ; i++ )
	{
		LED_SRCMAP[i][LED_SECONDARY] = LEDB_SRC_INACTIVE;
	}
	/* In3,In4 are secondary on Floppy LED */
	LED_SRCMAP[0][LED_SECONDARY] = LEDB_SRC_IN3;
	LED_SRCMAP[1][LED_SECONDARY] = LEDB_SRC_IN4;
	LED_SRCMAP[2][LED_SECONDARY] = LEDB_SRC_IN3;

        /* RGB defaults */
        for( i=0 ; i < 2 ; i++ )
        { /* floppy (first two LEDs in second row */
                LED_RGB[i][LED_IDLE][0] = 0x00;
                LED_RGB[i][LED_IDLE][1] = 0x00;
                LED_RGB[i][LED_IDLE][2] = 0x00;
                LED_RGB[i][LED_ACTIVE][0] = 0xFF; /* orange */
                LED_RGB[i][LED_ACTIVE][1] = 0x90;
                LED_RGB[i][LED_ACTIVE][2] = 0x00;
                LED_RGB[i][LED_SECONDARY][0] = 0x83; /* magenta */
                LED_RGB[i][LED_SECONDARY][1] = 0x00; //+((i-3)<<5);
                LED_RGB[i][LED_SECONDARY][2] = 0x83; //+((i-3)<<4);
        }
        /* IN3 */
        i=2;
        LED_RGB[i][LED_IDLE][0] = 0x00;
        LED_RGB[i][LED_IDLE][1] = 0x00;
        LED_RGB[i][LED_IDLE][2] = 0x00;
        LED_RGB[i][LED_ACTIVE][0] = 0x00; /* cyan */
        LED_RGB[i][LED_ACTIVE][1] = 0xEA;
        LED_RGB[i][LED_ACTIVE][2] = 0xFF;
        LED_RGB[i][LED_SECONDARY][0] = 0x33; /* dark white */
        LED_RGB[i][LED_SECONDARY][1] = 0x33; //+((i-3)<<5);
        LED_RGB[i][LED_SECONDARY][2] = 0x33; //+((i-3)<<4);
 

        /* Power */
        for( i=3 ; i < 6 ; i++ )
        {
                LED_RGB[i][LED_IDLE][0] = 0x04; /* dark green */
                LED_RGB[i][LED_IDLE][1] = 0x11; //+((i-3)<<2);
                LED_RGB[i][LED_IDLE][2] = 0x01; //+((i-3)<<1);
                LED_RGB[i][LED_ACTIVE][0] = 0x10; /* green-ish */
                LED_RGB[i][LED_ACTIVE][1] = 0x83; //+((i-3)<<5);
                LED_RGB[i][LED_ACTIVE][2] = 0x03; //+((i-3)<<4);
                LED_RGB[i][LED_SECONDARY][0] = 0x10; /* cyan */
                LED_RGB[i][LED_SECONDARY][1] = 0x83; //+((i-3)<<5);
                LED_RGB[i][LED_SECONDARY][2] = 0x83; //+((i-3)<<4);
        }

        /* Caps */
        i=6;
        LED_RGB[i][LED_IDLE][0] = 0x01;	  /* cyan, faint */
        LED_RGB[i][LED_IDLE][1] = 0x04;
        LED_RGB[i][LED_IDLE][2] = 0x04;
        LED_RGB[i][LED_ACTIVE][0] = 0x00;
        LED_RGB[i][LED_ACTIVE][1] = 0x60; /* green-ish */
        LED_RGB[i][LED_ACTIVE][2] = 0x10;

	/* don't assign "changed" status that would cause to send
	   the default configuration to the keyboard at startup:
	   bring both arrays in sync */
	for( i=0 ; i < N_LED ; i++ )
		ledmanager_copy_last( i, 0 );

}

/*
  Copy currently edited state of a single LED to 
  verification area (typically, after sending to keyboard)

  Flags:
   LEMCF_CHK - check if there were changes on this LED,
               returns 1 if yes, 0 if no
*/
LONG ledmanager_copy_last( LONG led, LONG flags )
{
	LONG i;

	if( (ULONG)led >= N_LED )
		return 0;

	/* check for changes */
	if( flags & LEMCF_CHK )
	{
		LONG chk = 0;
		for( i=LED_IDLE ; i <= LED_SECONDARY ; i++ )
		{
			if( LED_lastSRCMAP[led][i] != LED_SRCMAP[led][i] ) chk++;
			if( LED_lastRGB[led][i][0] != LED_RGB[led][i][0] ) chk++;
			if( LED_lastRGB[led][i][1] != LED_RGB[led][i][1] ) chk++;
			if( LED_lastRGB[led][i][2] != LED_RGB[led][i][2] ) chk++;
		}
		if( LED_lastMODES[led] != LED_MODES[led] )
			chk++;

		if( chk )
			return 1;
		else	return 0;
	}

	for( i=LED_IDLE ; i <= LED_SECONDARY ; i++ )
	{
		LED_lastSRCMAP[led][i] = LED_SRCMAP[led][i];
		LED_lastRGB[led][i][0] = LED_RGB[led][i][0];
		LED_lastRGB[led][i][1] = LED_RGB[led][i][1];
		LED_lastRGB[led][i][2] = LED_RGB[led][i][2];
	}

	LED_lastMODES[led] = LED_MODES[led];

	return 0;
}

#define HDR 0xBAFFEEDD
#define PRE_NLED 7
struct ledm_Preset {
	ULONG  Header;
	USHORT Version;
	struct {
	 USHORT srcA;
	 USHORT srcS;
	 UBYTE  mode;
	 UBYTE  RGB[9];
	} LED[PRE_NLED];
};

/* load/save preset (FILE) */
LONG ledmanager_loadpresets( STRPTR fname )
{
	struct ledm_Preset *pre;
	SHORT i;
	BPTR  ifile;

	pre = (struct ledm_Preset *)AllocVec( sizeof( struct ledm_Preset ), MEMF_PUBLIC );
	if( !pre )
		return -1;

	ifile = Open( fname, MODE_OLDFILE );
	if( !ifile )
	{
		FreeVec( pre );
		return -3;
	}

	Read( ifile, pre, sizeof( struct ledm_Preset ) );
	Close( ifile );

	if( pre->Header != HDR )
		return -2;
	if( pre->Version != 1  )
		return -2;

	for( i = 0 ; i < PRE_NLED ; i++ )
	{
		LED_SRCMAP[i][LED_ACTIVE]   	= pre->LED[i].srcA;  
		LED_SRCMAP[i][LED_SECONDARY]	= pre->LED[i].srcS;  
		LED_MODES[i]                	= pre->LED[i].mode;
		LED_RGB[i][LED_IDLE][0]     	= pre->LED[i].RGB[0];
		LED_RGB[i][LED_IDLE][1]     	= pre->LED[i].RGB[1];
		LED_RGB[i][LED_IDLE][2]     	= pre->LED[i].RGB[2];
		LED_RGB[i][LED_ACTIVE][0]   	= pre->LED[i].RGB[3];
		LED_RGB[i][LED_ACTIVE][1]   	= pre->LED[i].RGB[4];
		LED_RGB[i][LED_ACTIVE][2]   	= pre->LED[i].RGB[5];
		LED_RGB[i][LED_SECONDARY][0]	= pre->LED[i].RGB[6];
		LED_RGB[i][LED_SECONDARY][1]	= pre->LED[i].RGB[7];
		LED_RGB[i][LED_SECONDARY][2]	= pre->LED[i].RGB[8];
	}

	FreeVec( pre );

	return 0;
}

LONG ledmanager_savepresets( STRPTR fname )
{
	struct ledm_Preset *pre;
	SHORT  i;
	BPTR   ofile;
	LONG   ret = 0;

	Printf( (STRPTR)"save %s\n",(ULONG)fname);

	pre = (struct ledm_Preset *)AllocVec( sizeof( struct ledm_Preset ), MEMF_PUBLIC );
	if( !pre )
		return -1;

	Printf( (STRPTR)"have mem\n");

	pre->Header  = HDR;
	pre->Version = 1;
	for( i = 0 ; i < PRE_NLED ; i++ )
	{
		pre->LED[i].srcA   = LED_SRCMAP[i][LED_ACTIVE];
		pre->LED[i].srcS   = LED_SRCMAP[i][LED_SECONDARY];
		pre->LED[i].mode   = LED_MODES[i];
		pre->LED[i].RGB[0] = LED_RGB[i][LED_IDLE][0];
		pre->LED[i].RGB[1] = LED_RGB[i][LED_IDLE][1];
		pre->LED[i].RGB[2] = LED_RGB[i][LED_IDLE][2];
		pre->LED[i].RGB[3] = LED_RGB[i][LED_ACTIVE][0];
		pre->LED[i].RGB[4] = LED_RGB[i][LED_ACTIVE][1];
		pre->LED[i].RGB[5] = LED_RGB[i][LED_ACTIVE][2];
		pre->LED[i].RGB[6] = LED_RGB[i][LED_SECONDARY][0];
		pre->LED[i].RGB[7] = LED_RGB[i][LED_SECONDARY][1];
		pre->LED[i].RGB[8] = LED_RGB[i][LED_SECONDARY][2];
	}

	ofile = Open( fname, MODE_NEWFILE );
	if( ofile )
	{
		Printf( (STRPTR)"have file\n");
		Write( ofile, pre, sizeof( struct ledm_Preset ) );
		Close( ofile );
	}
	else
		ret = -2;

	FreeVec( pre );

	return ret;
}


