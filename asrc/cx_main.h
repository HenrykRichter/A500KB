/*
  startup.h

  (C)2020 Henryk Richter <henryk.richter@gmx.net>

  purpose:
   configuration structure and reference to config strings

*/
#ifndef _INC_CX_MAIN_H
#define _INC_CX_MAIN_H

#include <exec/types.h>
#include <dos/dos.h>

#include "config.h" /* config descriptor structure */

#ifndef DOSLIBTYPE
#ifdef __SASC
#define DOSLIBTYPE DosLibrary
#else
#define DOSLIBTYPE Library
#endif
#endif

#define MY_POPKEY_ID 86

LONG cx_main( struct configvars *conf );

#ifndef _INC_EXT_SYS_DOS_ICON
#define _INC_EXT_SYS_DOS_ICON
/* Libraries initialized in Startup */
extern struct DOSLIBTYPE *DOSBase;
extern struct Library   *SysBase;
extern struct Library   *IconBase;
#endif /* _INC_EXT_SYS_DOS_ICON */

#endif /* _INC_CX_MAIN_H */
