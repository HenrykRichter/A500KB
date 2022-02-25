/*
  cx_main.c

  (C)2020 Henryk Richter <henryk.richter@gmx.net>

  purpose:
   This is the commodity code. It sets up a broker
   alongside an intuition filter and checks for wheel
   movement periodically.


*/
#include <exec/types.h>
#include <exec/execbase.h>
#include <exec/memory.h>
#include <exec/ports.h>
#include <string.h>
#include <intuition/intuitionbase.h>
#include <libraries/commodities.h>
#include <devices/input.h>

#define __NOLIBBASE__
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/utility.h>
#include <proto/commodities.h>

#include "cx_main.h"
#include "window.h"
#include "macros.h"
#include "pledimage.h"
#include "pledbutton.h"
#include "ledmanager.h"
#include "capsimage.h"
#include "savereq.h"

const STRPTR cx_Name = (STRPTR)"A500KBConfig";
STRPTR cx_Desc = (STRPTR)"Keyboard Configurator," \
                         "H. Richter (C) 2022";

struct cx_Custom {
	struct MsgPort  *eventport;
	struct IOStdReq *eventreq;
	struct InputEvent evt;
};

/* interval in seconds (20s here) */
#define DEF_INTERVAL 20

#if 0
/* enable this to activate the intuition ticks */
#define GET_TICKS_FILTER
#endif

/* Quit when started a second time ? */
//#define UNIQUE_QUIT
#undef UNIQUE_QUIT

struct IntuitionBase *IntuitionBase;
struct Library       *CxBase;
struct GfxBase       *GfxBase;
struct Library       *UtilityBase;
struct Library       *TimerBase;
struct Library       *GadToolsBase;
struct Library       *AslBase;
struct Library       *ColorWheelBase;
struct Library       *GradientSliderBase;
Class  *pledimage_class;
Class  *capsimage_class;
Class  *pledbutton_class;

LONG   openLibs( void );
LONG   closeLibs( void );
CxObj *cx_Setup( struct MsgPort *cx_port, struct configvars *conf, struct cx_Custom *cust );
LONG   cx_Dispose( CxObj *cx_Broker, struct cx_Custom *cust ); 
LONG   cx_Handle_Msg( CxObj *cx_Broker, CxMsg *msg, struct configvars *conf, struct myWindow **mywin );

#ifdef GET_TICKS_FILTER
/* filter function that pings us by signal with every intuition tick */
LONG cx_AddEventFilter( CxObj *cx_Broker, struct Task *self, ULONG signal );
LONG cx_Handle_Signal( struct cx_Custom *cust );

void oneEvent( struct cx_Custom *cust, ULONG code, ULONG class );
void sendEvents( struct cx_Custom *cust, ULONG code, ULONG count );
#endif


LONG cx_main( struct configvars *conf )
{
  struct MsgPort *cx_Port = NULL;
  struct myWindow *mywin = NULL;
#ifdef GET_TICKS_FILTER
  ULONG  cx_Signal = -1;
#endif
  CxObj *cx_Broker = NULL;
  ULONG keeprunning;
  struct cx_Custom cust;
//  ULONG interval;
  LONG  flg;

  struct MsgPort *TimerPort = NULL;
  struct timerequest *timerio = NULL;

  cust.eventport = 0;
  cust.eventreq  = 0;

  //Printf("In Main\n");
 
  do
  {
	if( openLibs() )
	{
		//Printf("OpenLibs Fail\n");
		break;
	}
	
	/* initialize LED manager (and allocate resources = CIA,CIA-A Timer A) */
	if( ledmanager_init() != 0 )
	{
	        ULONG iflags = 0;
   		const struct EasyStruct libnotfoundES = {
	           sizeof (struct EasyStruct),
        	   0,
	           (STRPTR)"Error",
		   (STRPTR)"Cannot initialize CIA Communication.\n Timer-A allocation fail. Quitting.",
		   (STRPTR)"OK",
		  };
		EasyRequest( NULL, (struct EasyStruct *)&libnotfoundES, &iflags );
		break;
	}

#ifdef GET_TICKS_FILTER
	cx_Signal = AllocSignal( -1 );
	if( cx_Signal < 0 )
		break;
#endif
	cx_Port = CreateMsgPort();
	if( !cx_Port )
		break;

	TimerPort = CreateMsgPort();
	if( !TimerPort )
		break;


	timerio = (struct timerequest *)CreateIORequest(TimerPort, sizeof(struct timerequest));
	if( !timerio )
		break;
	if( OpenDevice( (STRPTR)TIMERNAME, UNIT_VBLANK, (struct IORequest *) timerio, 0) != 0 )
		break;
	TimerBase = (struct Library*)timerio->tr_node.io_Device;
#if 0
	timerio->tr_node.io_Command = TR_ADDREQUEST;
	timerio->tr_time.tv_secs = 1;
	timerio->tr_time.tv_micro = 1;
	SendIO((struct IORequest *) timerio);
#endif
//	interval = (conf->interval) ? *conf->interval : DEF_INTERVAL;

	if( !(cx_Broker = cx_Setup( cx_Port, conf, &cust )) )
	{
		//Printf("CX Broker fail\n");
		break;
	}
#ifdef GET_TICKS_FILTER
	/* extra: we want intuition ticks in this program (by signal) */
	cx_AddEventFilter( cx_Broker, FindTask(NULL) , cx_Signal );
#endif

	/* Load Config from Keyboard (with or without main window) */
	LoadConfig_Req( mywin );

	{
	 UBYTE flg = 1;
	 if( conf->cx_popup )
	 	if( 0 == Strnicmp( conf->cx_popup, (STRPTR)"NO", 2 ) )
		 flg = 0;
	 if( flg )
	 {
		mywin = Window_Open(conf);
		if( !mywin ) /* Screen setup failure or out of memory: Quit */
		{
			break;
		}
	 }
	}


	//Printf("Main loop\n");

	/* main loop */
	keeprunning = 1;
	do
	{
		struct Message *msg;

		/* SIGBREAKF_CTRL_F with window */
		ULONG signals = (1<<cx_Port->mp_SigBit) | SIGBREAKF_CTRL_C | SIGBREAKF_CTRL_E | SIGBREAKF_CTRL_F;
		signals |= (1<<TimerPort->mp_SigBit);
#ifdef GET_TICKS_FILTER
		signals |= (1<<cx_Signal); 
#endif
		if( mywin )
			signals |= mywin->sigmask;

		signals = Wait( signals );
		
		if( signals & (SIGBREAKF_CTRL_C | SIGBREAKF_CTRL_E) )
			break;

		if( signals & 1<<TimerPort->mp_SigBit)
		{
//			RestartTimer( timerio, interval );
			/* Tell Window to update */
			if( mywin )
				Window_Timer( conf, mywin );
//			if( flg > 0 )
//				Window_Sigmask = flg;
		}

#ifdef GET_TICKS_FILTER
		if( signals & (1<<cx_Signal) )
		{
			cx_Handle_Signal(&cust);
		}
#endif
#if 1
		if( mywin )
		{
			if( signals & mywin->sigmask )
			{
				flg = Window_Event(conf, mywin );
				if( flg < 0 ) /* Quit ? */
					break;
			}
		}
#endif
		while( (msg = GetMsg( cx_Port )) )
		{
			/* only returns 0 if a kill event happened */
			if( !cx_Handle_Msg( cx_Broker, (CxMsg*)msg, conf, &mywin ) )
				keeprunning = 0;
		}
	}
	while( keeprunning );
  }
  while(0);

  Window_Destroy(conf, mywin ); /* close and stay closed */

  /* Close Timer */
  if( TimerBase )
  {
	AbortIO((struct IORequest *) timerio);
        CloseDevice((struct IORequest *) timerio);
  }
  if( timerio )
  {
  	DeleteIORequest((struct IORequest *)timerio);
  }
  if( TimerPort )
  {
  	DeleteMsgPort( TimerPort );
  }

  /* Cleanup */
  cx_Dispose( cx_Broker, &cust ); /* checks for NULL */

  /* shut down LED manager */
  ledmanager_exit();

#ifdef GET_TICKS_FILTER
  if( cx_Signal > 0 )
  	FreeSignal( cx_Signal );
#endif
  if( cx_Port )
  {
  	struct Message *msg;

	while( (msg = GetMsg(cx_Port)))
		ReplyMsg(msg);

  	DeleteMsgPort( cx_Port );
  }
  closeLibs();

  return 0;
}


void RestartTimer( struct timerequest *timerio, ULONG secs )
{

 /* this is for forced restart only (i.e. screennotify) */
 if( !CheckIO((struct IORequest *)timerio) )
	AbortIO((struct IORequest *)timerio);
 
 WaitIO((struct IORequest *) timerio);
 timerio->tr_node.io_Command = TR_ADDREQUEST;
 timerio->tr_time.tv_secs = secs;
 timerio->tr_time.tv_micro = 1;
 SendIO((struct IORequest *) timerio);
}

LONG openLibs( void )
{
	LONG res = 0;

	IntuitionBase = NULL;
	CxBase = NULL;
	GfxBase = NULL;
	UtilityBase = NULL;
	GadToolsBase = NULL;
	ColorWheelBase = NULL;
	GradientSliderBase = NULL;

	if( !(IntuitionBase = (struct IntuitionBase*)OpenLibrary( (STRPTR)"intuition.library",39)))
		res = 1;
	if( !(CxBase = OpenLibrary((STRPTR)"commodities.library",37)))
		res = 1;
	if( !(GfxBase = (struct GfxBase*)OpenLibrary((STRPTR)"graphics.library",37)))
		res = 1;
	if( !(UtilityBase = OpenLibrary((STRPTR)"utility.library",37)))
		res = 1;
	if( !(GadToolsBase = OpenLibrary((STRPTR)"gadtools.library",37)))
		res = 1;
	if( !(AslBase = OpenLibrary((STRPTR)"asl.library",37)))
		res = 1;
	pledimage_class = init_pledimage_class();
	capsimage_class = init_capsimage_class();
	pledbutton_class= init_pledbutton_class();
		
	if( !res )
	{
		ColorWheelBase     = OpenLibrary( (STRPTR)"gadgets/colorwheel.gadget",39);
		GradientSliderBase = OpenLibrary( (STRPTR)"gadgets/gradientslider.gadget",39);
		if( !(ColorWheelBase ) )
		{
		        ULONG iflags = 0;
   			const struct EasyStruct libnotfoundES = {
		           sizeof (struct EasyStruct),
	        	   0,
		           (STRPTR)"Error",
			   (STRPTR)"colorwheel.gadget V39 not found. Quitting.",
		           (STRPTR)"OK",
		       };
		       EasyRequest( NULL, &libnotfoundES, &iflags );
		       res = 1;
		}
		else if( !(GradientSliderBase ) )
		{
		        ULONG iflags = 0;
   			const struct EasyStruct libnotfoundES = {
		           sizeof (struct EasyStruct),
	        	   0,
		           (STRPTR)"Error",
			   (STRPTR)"gradientslider.gadget V39 not found. Quitting.",
		           (STRPTR)"OK",
		       };
		       EasyRequest( NULL, &libnotfoundES, &iflags );
		       res = 1;
		}
	}

	return res;
}

LONG closeLibs( void )
{
	if( capsimage_class )
		FreeClass( capsimage_class );
	if( pledimage_class )
		FreeClass( pledimage_class );
	if( pledbutton_class )
		FreeClass( pledbutton_class );

	if( ColorWheelBase )
		CloseLibrary( ColorWheelBase );
	if( GradientSliderBase )
		CloseLibrary( GradientSliderBase );

	if( AslBase )
		CloseLibrary( AslBase );
	if( IntuitionBase )
		CloseLibrary( (struct Library *)IntuitionBase );
	if( CxBase )
		CloseLibrary( CxBase );
	if( GfxBase )
		CloseLibrary( (struct Library *)GfxBase );
	if( UtilityBase )
		CloseLibrary( UtilityBase );
	if( GadToolsBase )
		CloseLibrary( GadToolsBase );
	return 0;
}


LONG   cx_Handle_Msg( CxObj *cx_Broker, CxMsg *msg, struct configvars *conf, struct myWindow **mywin )
{
	LONG ret = 1;
	ULONG id,type;

	id  = CxMsgID(msg);
	type= CxMsgType(msg);
	ReplyMsg( (struct Message*)msg );

#if 1
	/* no window as of yet */
	if( type == CXM_IEVENT )
	{
		switch( id )
		{
			case MY_POPKEY_ID: 
				/* OPEN WINDOW */
				*mywin = Window_Open(conf);
				break;
			default: 
				break;
		}
	}
#endif
	if( type == CXM_COMMAND )
	{
		switch( id )
		{
			case CXCMD_KILL:	ret=0;break;
			case CXCMD_DISABLE:	ActivateCxObj(cx_Broker,0);break;
			case CXCMD_ENABLE:	ActivateCxObj(cx_Broker,1);break;
#ifdef UNIQUE_QUIT
			case CXCMD_UNIQUE:	ret=0;break;
#else
			case CXCMD_UNIQUE:	
				*mywin = Window_Open( conf );
				break;
#endif
#if 1			
			/* Open/Close WINDOW */
			case CXCMD_APPEAR:
				*mywin = Window_Open(conf );
				break;

			case CXCMD_DISAPPEAR:
				Window_Close(conf, *mywin );
				break;
#endif
			default: break;
		}
	}

	return ret;
}


CxObj *cx_Setup( struct MsgPort *cx_port, struct configvars *conf, struct cx_Custom *cust )
{
	struct NewBroker nb;
	CxObj *cx_Broker;

	nb.nb_Version = NB_VERSION;
	nb.nb_Name    = cx_Name;
	nb.nb_Title   = cx_Name;
	nb.nb_Descr   = cx_Desc;
	nb.nb_Unique  = NBU_NOTIFY|NBU_UNIQUE;
	nb.nb_Flags   = COF_SHOW_HIDE; 
	nb.nb_Pri     = (conf->pri) ? *(conf->pri) : 0;
	nb.nb_Port    = cx_port;
	nb.nb_ReservedChannel = 0;

	if( (cx_Broker = CxBroker(&nb,NULL)) )
	{
		LONG err;
#if 1
		if( conf->cx_popkey )
		{
		 /* apply the keyboard shortcut to open the window (if set in prefs) */
	         CxObj *filter = CxFilter(conf->cx_popkey); /* e.g. "ctrl lalt m" */
		 AttachCxObj(filter,CxSender(cx_port,MY_POPKEY_ID)); /* ID we get on messages */
		 AttachCxObj(filter,CxTranslate(NULL));
	         AttachCxObj(cx_Broker,filter);
		}
#endif

#ifdef GET_TICKS_FILTER
		cust->eventport = CreateMsgPort();
		cust->eventreq  = CreateIORequest( cust->eventport,sizeof(struct IOStdReq));
		err = OpenDevice("input.device",NULL,(struct IORequest *)cust->eventreq,NULL);
#else
		err = 0;
#endif
		if( (CxObjError(cx_Broker) == 0) && (err==0) )
		{
			ActivateCxObj(cx_Broker,1);
		}
		else
		{
			DeleteCxObjAll(cx_Broker);
			if( cust->eventreq )
				DeleteIORequest( cust->eventreq );
			if( cust->eventport )
				DeleteMsgPort( cust->eventport );
			cust->eventport = NULL;
			cust->eventreq = NULL;
			cx_Broker = NULL;
		}
	}
	
	return cx_Broker;
}

LONG cx_Dispose( CxObj *cx_Broker, struct cx_Custom *cust )
{
	if( cx_Broker )
	{
		DeleteCxObjAll( cx_Broker );
	}
#ifdef GET_TICKS_FILTER
	if( cust->eventreq )
		DeleteIORequest( cust->eventreq );
	if( cust->eventport )
		DeleteMsgPort( cust->eventport );
	cust->eventport = NULL;
	cust->eventreq = NULL;
#endif
	return 0;
}

#ifdef GET_TICKS_FILTER
/* filter function that signals us for intuition timers */
struct InputXpression my_inputfilter =
{
 IX_VERSION, IECLASS_TIMER,
 0,0,
 (IEQUALIFIER_LEFTBUTTON|IEQUALIFIER_MIDBUTTON|IEQUALIFIER_RBUTTON),
 0
};

LONG cx_AddEventFilter( CxObj *cx_Broker, struct Task *self, ULONG signal )
{
	LONG ret = 1;

	CxObj *filterlist;

	if( (filterlist = CxFilter(NULL)) )
	{
		SetFilterIX( filterlist, &my_inputfilter );
		AttachCxObj( filterlist, CxSignal( self, signal ) );
		ret = 0;
	}

	AttachCxObj(cx_Broker,filterlist); 

	return ret;
}

/* input events */
LONG cx_Handle_Signal( struct cx_Custom *cust )
{
 return 0;
}
#endif


