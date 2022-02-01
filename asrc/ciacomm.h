/*
  ciacomm.h

  Prototypes for CIA-A based communication with
  custom Keyboard

*/
#ifndef _INC_CIACOMM_H
#define _INC_CIACOMM_H

#include "compiler.h"
#include <exec/types.h>

ASM LONG CIAKB_Init( void );

ASM LONG CIAKB_Send( ASMR(a1) UBYTE *sendbuffer ASMREG(a1),
                     ASMR(d0) LONG  nBytes      ASMREG(d0) );

ASM LONG CIAKB_Wait( void ); /* wait until last send is finished */
ASM LONG CIAKB_IsBusy(void); /* check if busy (1/0) */
ASM LONG CIAKB_Stop( void );

ASM LONG CIAKB_Exit( void );

/* keyboard returns ACK/NACK when an incoming sequence was detected
   or does nothing when the start of sequence was missed, also a 
   classic keyboard won't answer at all */
#define KCMD_IDLE	0	// we haven't sent anything
#define KCMD_ACK	(0x80|0x73)	// ACK = OK
#define KCMD_NACK	(0x80|0x77)	// failed reception
#define KCMD_TIMEOUT	0x5A	// no answer

#define KCMD_NOWORK     0x200   // flag: nothing to be done (in ledmanager.c)
#define KCMD_RETRYING   0x100   // flag: retrying (in ledmanager.c)
#define KCMD_BUSY       0x400   // flag: busy (in ledmanager.c)

#endif
