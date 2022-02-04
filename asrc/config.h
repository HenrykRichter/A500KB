/*
  config.h

  (C)2020 Henryk Richter <henryk.richter@gmx.net>

  purpose:
   configuration structure and reference to config strings

*/
#ifndef _INC_CONFIG_H
#define _INC_CONFIG_H

#include <exec/types.h>

/* adapt as needed but keep in sync with commandline argument
   list in config.c. Also keep in mind that the options are LONGS
   or LONG pointers.
*/
struct configvars {
        APTR    cx_popup;
	APTR    cx_popkey;
	ULONG	*pri;
	ULONG	*win_x;
	ULONG	*win_y;
	APTR    fontname;
	ULONG   *fontsize;

	/* ----------- safekeeping for CLI args from RDArgs --------- */
	APTR	args;	 /* RDArgs */
	APTR    diskobj; /* icon */
	APTR    intargs; /* integer argument storage */
	STRPTR  progname; /* full program path */
};

/* number of rows to poll (see above: row0-row7 need to stay in one block) */
#define NROWS 8


struct configttitem {
	STRPTR  name;
	ULONG   flags;
};

#define CTTI_SWITCH 1
#define CTTI_STRING 2
#define CTTI_INT    4
#define CTTI_IGNORE 32

extern STRPTR confstringCLI;
extern struct configttitem confvarsWB[];

#endif /* _INC_CONFIG_H */
