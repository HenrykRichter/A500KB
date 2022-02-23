/*
  startup.c

  (C)2020 Henryk Richter <henryk.richter@gmx.net>

  purpose:
   Check for start from Workbench or CLI and parse
   ToolTypes or commandline arguments as appropriate.

*/
#include <exec/types.h>
#include <exec/execbase.h>
#include <exec/memory.h>
#include <string.h>
#include <intuition/intuitionbase.h>

#define __NOLIBBASE__
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/icon.h>

#include "startup.h"
#include "cx_main.h"
#include "version.h"
#include "utils.h"

#define FAIL_DOS  10
#define FAIL_ICON 11
#define FAIL_ARGS 2

extern STRPTR mydosname;

LONG Main(void)
{
 struct configvars conf;
 struct Process *self;
 struct WBStartup *WBenchMsg = NULL;
 LONG   res;

 /* some preparations */
 SysBase = *((struct Library**)0x4L);
 self    = (struct Process*)FindTask(0);
 DOSBase = (struct DOSLIBTYPE*)OpenLibrary(mydosname,36);
 BZero( &conf , sizeof(struct configvars));
 IconBase = NULL;

 if( !self->pr_CLI )
	res = Startup_WB( self, &conf, &WBenchMsg );
 else
 	res = Startup_CLI( self, &conf );

 if( !res ) /* startup successful ? */
 {
	res = cx_main( &conf );
 }

 /* Cleanup */
 if( DOSBase )
 {
 	if( conf.args )
		FreeArgs( conf.args );
 	CloseLibrary( (struct Library*)DOSBase );
 }

 if( conf.intargs )
 	FreeVec( conf.intargs );
 if( conf.progname )
 	FreeVec( conf.progname );

 if( IconBase )
 {
	if( conf.diskobj )
		FreeDiskObject( conf.diskobj );
	CloseLibrary( IconBase );
 }

 if( WBenchMsg )
 {
	Forbid();
	ReplyMsg( &WBenchMsg->sm_Message );
 }

 return res;
}

struct Library    *SysBase;
struct DOSLIBTYPE *DOSBase;
struct Library    *IconBase;

STRPTR myversion = (STRPTR)"$VER: " PROGNAME " " LIBVERSION "." LIBREVISION " (" LIBDATE ") (C) Henryk Richter";
STRPTR mydosname = (STRPTR)"dos.library";

#if 0
struct IntuitionBase    *IntuitionBase;
struct Library          *CxBase;
struct GfxBase          *GfxBase;
struct Library          *UtilityBase;
#endif

/* Note: DOSBase might be unavailable in Startup (if attempted on Kick1.3) */
LONG Startup_WB( struct Process *self, struct configvars *conf, struct WBStartup **WBenchMsg )
{
    struct configttitem *ttitems;
    ULONG               *curcf,*cfi;
    struct WBArg        *wbArg;
    struct DiskObject   *dobj;
    STRPTR              ttarg;

    WaitPort(&(self->pr_MsgPort));

    *WBenchMsg = (struct WBStartup *)GetMsg(&(self->pr_MsgPort));

    if( !DOSBase )
    	return FAIL_DOS;

    IconBase = OpenLibrary( (STRPTR)"icon.library", 36 );
    if( !IconBase )
    	return FAIL_ICON;

    wbArg = (*WBenchMsg)->sm_ArgList;

    CurrentDir(wbArg->wa_Lock);
    dobj = GetDiskObject( (STRPTR)wbArg->wa_Name);
    conf->diskobj = dobj;

    if( !dobj )
    	return 0; /* TODO: fail when we don't get the icon */

    conf->progname = AllocVec( 108+256, MEMF_ANY );
    if( conf->progname )
    {
     if( NameFromLock(wbArg->wa_Lock, conf->progname, 108+256 ) )
     {
		AddPart( conf->progname, (STRPTR)wbArg->wa_Name, 108+256 );
     }
    }

    conf->intargs = AllocVec( sizeof( struct configvars ), MEMF_ANY );
    if( !conf->intargs )
    	return 1; /* no mem, bye bye */

    ttitems = confvarsWB;
    curcf   = (ULONG*)conf;
    cfi     = (ULONG*)conf->intargs;
    while( ttitems->flags )
    {
	if( ttitems->name )
	{
		if( (ttarg = FindToolType( (STRPTR*)dobj->do_ToolTypes,(STRPTR)ttitems->name)) )
		{
			if( ttitems->flags & CTTI_SWITCH )
				*curcf = 1;
			else
			{
				if( ttitems->flags & CTTI_INT )
				{
					*curcf = (ULONG)cfi;
					StrToLong(ttarg,(LONG*)(cfi++));
				}
				else
				 if( ttitems->flags & CTTI_STRING )
					*curcf = (ULONG)ttarg;
			}
		}
	}
	curcf++;
	ttitems++;
    }

    return 0;
}



LONG Startup_CLI( struct Process *self, struct configvars *conf )
{
   LONG ret = 0;

   if( !DOSBase )
   	return FAIL_DOS;

   IconBase = OpenLibrary( (STRPTR)"icon.library", 36 ); /* needed for config save */
 
    conf->args = (APTR)ReadArgs( confstringCLI, (LONG*)conf, NULL );
    if( !conf->args )
	ret = FAIL_ARGS;

    conf->progname = AllocVec( 108+256, MEMF_ANY );
    if( conf->progname )
    {
     /* remember program name and path (V36+) */
     BYTE progname[108];
     GetProgramName( (STRPTR)progname, 108 );
     if( NameFromLock(GetProgramDir(), conf->progname, 108+256) )
     {
		AddPart( conf->progname, (STRPTR)progname, 108+256 );
     }
    }

    return ret;
}




