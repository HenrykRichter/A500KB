/*
  capsimage.h

  Capslock boopsi image (circle)

  This image requires 1 pen to be sent.

*/
#ifndef _INC_CAPSIMAGE_H
#define _INC_CAPSIMAGE_H

#ifndef INTUITION_CLASSES_H
#include <intuition/classes.h>
#endif

Class *init_capsimage_class( void );

#define CAPS_Dummy     (IA_Dummy + 2000)
#define CAPS_Screen    (CAPS_Dummy + 0 )
#define CAPS_Pen       (CAPS_Dummy + 1 )
#define CAPS_Window    (CAPS_Dummy + 5 )

/* default size of Capslock image */
#define CAPS_W 8
#define CAPS_H 8


#endif /* _INC_CAPSIMAGE_H */
