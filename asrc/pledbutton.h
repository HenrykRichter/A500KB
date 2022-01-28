/*
  pledbutton.h

  Amiga power LED style BOOPSI gadget

  This gadget is a button that responds
  to three segments.

*/
#ifndef _INC_PLEDBUTTON_H
#define _INC_PLEDBUTTON_H

#ifndef INTUITION_CLASSES_H
#include <intuition/classes.h>
#endif

Class *init_pledbutton_class( void );

#define PLED_Dummy     (IA_Dummy + 2000)
#define PLED_Screen    (PLED_Dummy + 0 )
#define PLED_Mode      (PLED_Dummy + 4 )
#define PLED_Window    (PLED_Dummy + 5 )

/* 3 LEDs or CapsLock */
#define PLEDMode_3LED	0
#define PLEDMode_Caps   1

/* default size of PLED image */
#define PLED_W 48
#define PLED_H 6


#endif /* _INC_PLEDBUTTON_H */
