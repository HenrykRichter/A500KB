/*
  pledimage.h

  Amiga power LED style BOOPSI image

  This image requires 3 pens to be sent.

*/
#ifndef _INC_PLEDIMAGE_H
#define _INC_PLEDIMAGE_H

#ifndef INTUITION_CLASSES_H
#include <intuition/classes.h>
#endif

Class *init_pledimage_class( void );

#define PLED_Dummy     (IA_Dummy + 2000)
#define PLED_Screen    (PLED_Dummy + 0 )
#define PLED_Pen1      (PLED_Dummy + 1 )
#define PLED_Pen2      (PLED_Dummy + 2 )
#define PLED_Pen3      (PLED_Dummy + 3 )
#define PLED_Mode      (PLED_Dummy + 4 )
#define PLED_Window    (PLED_Dummy + 5 )

/* default size of PLED image */
#define PLED_W 48
#define PLED_H 6


#endif /* _INC_PLEDIMAGE_H */
