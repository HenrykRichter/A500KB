/*
  Savereq.c: requesters

  (C) 2022 Henryk Richter

*/
#include "savereq.h"

#include <proto/intuition.h>
#include <proto/utility.h>
#include <proto/dos.h>
#include <exec/types.h>
#include <exec/lists.h>
#include <intuition/intuition.h>

#include "ciacomm.h"
#include "ledmanager.h"

#define SR_WAITBUSY 2
#define SR_LOADCONFIG 4

#define LCS_NLEDs    N_LED

LONG keyboard_type;
LONG keyboard_version;

struct EasyStruct SavingES = {
	    sizeof (struct EasyStruct),
	    0,
	    (STRPTR)"A500KB Save EEPROM",
	    (STRPTR)"Please wait while updating EEPROM.",
	    (STRPTR)"Cancel"
	};
struct EasyStruct AboutES = {
	    sizeof (struct EasyStruct),
	    0,
	    (STRPTR)"About A500KB",
	    (STRPTR)"Configurator for LEDs on A500KB custom Keyboard\n(C) 2022 Henryk Richter\n\nPlease note that this tool is not useful\nfor regular Amiga keyboards.",
	    (STRPTR)"OK"
	};
struct EasyStruct LoadConfigES = {
	    sizeof (struct EasyStruct),
	    0,
	    (STRPTR)"A500KB Loading",
	    (STRPTR)"Loading current configuration from A500KB keyboard...\n"
	            "ATTENTION: DON'T TOUCH ANY KEY UNTIL THE REQUESTER DISAPPEARS!\n\n"
	            "This process might take some seconds\n",
	    (STRPTR)"Cancel"
	};
struct EasyStruct ErrLoadES  = {
	    sizeof (struct EasyStruct),
	    0,
	    (STRPTR)"A500KB Loading",
	    (STRPTR)"Failed to load Config from keyboard.\n"
	            "Your keyboard did not respond with it's current\n"
	            "config codes. Keeping Defaults.\n",
	    (STRPTR)"Continue"
	};


LONG LoadConfig_Func( struct myWindow *win, ULONG *state );


/* wait for return */
LONG do_Req( struct myWindow *win, struct EasyStruct *template, ULONG flags )
{
    struct Window *reqwin,*refwin=NULL;
    int retval,cmdres;
    ULONG state = 0;
    
    if( win )
    	refwin = win->window;

    reqwin = BuildEasyRequest( refwin, template, IDCMP_INTUITICKS, NULL );
    if( !reqwin )
    	return -1;

    while( 1 ) 
    {
	retval = SysReqHandler( reqwin, NULL, TRUE );

    	if( flags & SR_WAITBUSY )
	{
    		/* still waiting for ACK ? */
		if( !CIAKB_IsBusy() )
			break;
	}
	if( flags & SR_LOADCONFIG )
	{
		cmdres = LoadConfig_Func( win, &state );
		if( cmdres >= 0 )
		{
			retval = cmdres;
			/* Printf("Config for %ld LEDs retrieved\n",(LONG)cmdres); */
			break;
		}
	}

    	if( retval >= 0 ) /* what? The User clicked Cancel ? */
	{
		if( flags & SR_LOADCONFIG ) /* loadconfig mode: 0..n = N loaded LEDs, hence return negative */
			retval = -1-retval;
		break;
	}
    }
    FreeSysRequest( reqwin );
   
    if( flags & SR_WAITBUSY )
    {
    	cmdres = CIAKB_Wait();
    }

    return retval;
}


void SaveEEPROM_Req( struct myWindow *win )
{
	do_Req( win, &SavingES, SR_WAITBUSY );
}


void About_Req( struct myWindow *win )
{
	do_Req( win, &AboutES, 0 );
}


void LoadConfig_Req( struct myWindow *win )
{
	LONG res = do_Req( win, &LoadConfigES, SR_LOADCONFIG );	

	if( (res != LCS_NLEDs) && (res >= 0) )
	{
		do_Req( win, &ErrLoadES, 0 );
	}
}



UBYTE lc_cmdstream[4];
UBYTE lc_recvbuffer[64];
LONG LoadConfig_Func( struct myWindow *win, ULONG *state )
{
#define LCS_LEDs     15	       /* last possible index reserved for identification string */
#define LCS_SENT     (1<<4)
#define LCS_TOADD    (1<<24)   /* timeout addition */
#define LCS_TOMASK   (255<<24) /* mask for timeouts */
#define LCS_TOTHRESH (20<<24)  /* give up after 20 timeouts */

	LONG cmdres;
	LONG idx = (*state & LCS_LEDs)-1;

	/* Printf("IDX %ld state %lx\n",idx,*state); */
/*
  load config for all 7 LEDs and their activation
  remember number of timeouts and successful transmissions

  state variable: (0..7)&15 = LED index (4 bit)
                  (16..32)  = action code (START=0<<4,SENT=1<<4) (2 bit)
		  (N.b)<<16 = successful calls
		  (M.b)<<24 = timeouts
*/
	/* we are at start condition ? */
	if( !(*state & LCS_SENT ) )
	{
		LONG n;

		/* preamble */
		lc_cmdstream[0] = 0x00;
		lc_cmdstream[1] = 0x03;

		if( idx == -1 )
		{
			lc_cmdstream[2] = LEDCMD_GETVERSION;
			n = 3;
		}
		else
		{
			/* send request */
			lc_cmdstream[2] = LEDCMD_GETCONFIG | idx;
			lc_cmdstream[3] = 0x00;
			n = 4;
		}
		/* TODO: use ledmanager_sendcommands */
		CIAKB_Send( lc_cmdstream, n );
		*state |= LCS_SENT;
		return -1;
	}

	/* we have sent a request, check if still busy */
	if( CIAKB_IsBusy() )
		return -1;

	*state &= ~(LCS_SENT); /* no pending transaction */

	/* get return code */
    	cmdres  = CIAKB_Wait();

	/* if good, copy data to LEDmanager and go to next LED, else retry and remember number of timeouts */
	if( cmdres == KCMD_ACK )
	{
		LONG n = CIAKB_GetData( lc_recvbuffer, 64 );
		if( n > 1 )
		{
			if( idx == -1 )
			{
				/* getconfig -> 0xBA,(LEDGV_TYPE_A500/LEDGV_TYPE_A3000),VERSION */
				if( lc_recvbuffer[0] == 0xBA )
				{
					keyboard_type    = lc_recvbuffer[1];
					keyboard_version = lc_recvbuffer[2];
				}
			}
			else
			{
				ledmanager_loadconfigentry( idx, lc_recvbuffer, n, 1 );
			}

			/* next LED or "done" */
			idx++;
			if( idx >= LCS_NLEDs )
				return idx;		/* done */
			idx++; /* "-1" in the beginning of the file */
			*state = (*state & ~(LCS_LEDs) ) | idx;
			return -1; /* next LED in next run */
		}
		/* fall through: no data received, treat like timeout */
	}

	/* too many timeouts: return number of successfully loaded config blobs */
	if( (*state & LCS_TOMASK) >= LCS_TOTHRESH ) /* masking redundant here */
		return idx;
	*state += LCS_TOADD; /* number of timeouts +1 */

	/* next call in this function will retry by sending another request */

	return -1;
}

