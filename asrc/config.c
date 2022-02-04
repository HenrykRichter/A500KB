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
STRPTR confstringCLI = "CX_POPUP/K,CX_POPKEY/K,PRIORITY/K/N,WINX/K/N,WINY/K/N,FONTNAME/K,FONTSIZE/K/N";

/* every item here should shadow the position and type in confstringCLI */
struct configttitem confvarsWB[] = {
 { "CX_POPUP",  CTTI_STRING },
 { "CX_POPKEY", CTTI_STRING },
 { "PRIORITY",  CTTI_INT    },
 { "WINX",      CTTI_INT    },
 { "WINY",      CTTI_INT    },
 { "FONTNAME",  CTTI_STRING },
 { "FONTSIZE",  CTTI_INT    },
 { NULL, 0 }
};


