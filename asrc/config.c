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
STRPTR confstringCLI = "CX_POPUP/K,CX_POPKEY/K,PRIORITY/K/N,QUIET/S,INTERVAL/K/N,WINX/K/N,WINY/K/N,WIDTH/K/N,FONTNAME/K,FONTSIZE/K/N,DISABLED/K,STARTDELAY/K/N,BACKGROUND/K,SHOWTITLE/K/N,TEXTPEN/K/N,SHADOWPEN/K/N,RECESSED/K/N,CT/K,CV/K,CC/K,CF/K,CP/K,CH/K,BOXH/K/N";

/* every item here should shadow the position and type in confstringCLI */
struct configttitem confvarsWB[] = {
 { "CX_POPUP",  CTTI_STRING },
 { "CX_POPKEY", CTTI_STRING },
 { "PRIORITY",  CTTI_INT    },
 { "QUIET",     CTTI_SWITCH },
 { "INTERVAL",  CTTI_INT    },
 { "WINX",      CTTI_INT    },
 { "WINY",      CTTI_INT    },
 { "WIDTH",     CTTI_INT    },
 { "FONTNAME",  CTTI_STRING },
 { "FONTSIZE",  CTTI_INT    },
 { "DISABLED",  CTTI_STRING },
 { "STARTDELAY",CTTI_INT    },
 { "BACKGROUND",CTTI_STRING },
 { "SHOWTITLE" ,CTTI_STRING },
 { "TEXTPEN"   ,CTTI_INT    },
 { "SHADOWPEN" ,CTTI_INT    },
 { "RECESSED"  ,CTTI_INT    },
 { "COLORT",CTTI_STRING }, /* Temp */
 { "COLORV",CTTI_STRING }, /* Voltage */
 { "COLORC",CTTI_STRING }, /* Current */
 { "COLORF",CTTI_STRING }, /* Fans */
 { "COLORP",CTTI_STRING }, /* Pressure */
 { "COLORH",CTTI_STRING }, /* Humidity */
 { "BOXH",  CTTI_INT }, /* box height */
 { NULL, 0 }
};


