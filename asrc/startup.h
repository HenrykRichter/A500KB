/*
  startup.h

  (C)2020 Henryk Richter <henryk.richter@gmx.net>

  purpose:
   configuration structure and reference to config strings

*/
#ifndef _INC_STARTUP_H
#define _INC_STARTUP_H

#include <exec/types.h>
#include <dos/dos.h>
#include <workbench/startup.h>

#ifdef __SASC
#define DOSLIBTYPE DosLibrary
#else
#define DOSLIBTYPE Library
#endif

#include "config.h" /* config descriptor structure */

LONG Startup_WB( struct Process *self, struct configvars *conf, struct WBStartup **WBenchMsg );
LONG Startup_CLI( struct Process *self, struct configvars *conf );

#ifndef _INC_EXT_SYS_DOS_ICON
#define _INC_EXT_SYS_DOS_ICON
/* Libraries initialized in Startup */
extern struct DOSLIBTYPE *DOSBase;
extern struct Library    *SysBase;
extern struct Library    *IconBase;
#endif /* _INC_EXT_SYS_DOS_ICON */


#endif /* _INC_STARTUP_H */
