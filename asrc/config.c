/*
  config.c

  (C)2020 Henryk Richter <henryk.richter@gmx.net>

  purpose:
   configuration structure and reference to config strings

*/
#include <exec/types.h>
#include "config.h"

/* Important: apply changes to both confstringCLI and confvarsWB, also don't forget to
   adjust struct configvars accordingly as that struct is the direct result of a call
   to ReadArgs() */
STRPTR confstringCLI = (STRPTR)"CX_POPUP/K,CX_POPKEY/K,PRIORITY/K/N,WINX/K/N,WINY/K/N,FONTNAME/K,FONTSIZE/K/N";

/* every item here should shadow the position and type in confstringCLI */
struct configttitem confvarsWB[] = {
 { (STRPTR)"CX_POPUP",  CTTI_STRING },
 { (STRPTR)"CX_POPKEY", CTTI_STRING },
 { (STRPTR)"PRIORITY",  CTTI_INT    },
 { (STRPTR)"WINX",      CTTI_INT    },
 { (STRPTR)"WINY",      CTTI_INT    },
 { (STRPTR)"FONTNAME",  CTTI_STRING },
 { (STRPTR)"FONTSIZE",  CTTI_INT    },
 { NULL, 0 }
};


