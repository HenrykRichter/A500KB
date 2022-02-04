/*
  window.h
*/
#ifndef _INC_WINDOW_H
#define _INC_WINDOW_H

#include <exec/types.h>
#include <exec/lists.h>
#include <intuition/intuition.h>
#include <libraries/gadtools.h>

#ifndef _INC_CONFIG_H
struct configvars;
#endif

#define FLG_WIN_REOPEN 1

/* number of exclusive pens */
#define nPENS 7+2
#define nPENS_GRAD 2
#define PENIDX_GRAD 7

#ifdef _WINDOWPRIVATE_
struct PenList {
        UBYTE R,G,B;
};

/* these default colors are overridden by the actual config of the LEDs */
struct PenList myPens[nPENS] = {
/* Floppy */
 { 0x90, 0x80,  0x00 }, /* Yellow-Orange */
 { 0x90, 0x80,  0x00 }, /* Yellow-Orange */
 { 0x90, 0x80,  0x00 }, /* Yellow-Orange */
/* Power */
 { 0x20, 0xA0,  0x20 }, /* Green */
 { 0x20, 0xA0,  0x20 }, /* Green */
 { 0x20, 0xA0,  0x20 }, /* Green */
/* Caps */
 { 0x20, 0xA0,  0x20 }, /* Green */
/* Slider */
 { 0xFF, 0xFF,  0xFF },
 { 0x7f, 0x7f,  0x7f },
};
#endif /* _WINDOWPRIVATE_ */

/*
  prototype for gadgets
*/
struct myGadProto {
	USHORT x;
	USHORT y;
	USHORT w;
	USHORT h;
	APTR   title;
	USHORT id;
	USHORT type;      /* SLIDER_KIND, CYCLE_KIND, BUTTON_KIND etc */
	USHORT arg1;
	USHORT arg2;
	USHORT arg3; /* min,max,cur for sliders */
	APTR   attribs;  /* STRPTR * for CYCLE_KIND */
};
/* regular gadtools _KIND are enums of which we use a subset; enums below for internal matters */
#define WHEELGRAD_KIND  0x100
#define GRADSLIDER_KIND 0x101
#define PLEDIMAGE_KIND  0x102
#define CAPSIMAGE_KIND  0x103

struct myWindow {
	struct Window *window;
	ULONG  sigmask;
	LONG   IdleTickCount; /* if the user didn't touch the controls for a while, send data to KB */

	struct Screen *screen;
	struct Screen *customscreen;
	struct DrawInfo   *DrawInfo;
	struct VisualInfo *VisualInfo;
	struct TextAttr    Font;	/* used font (either from config or current screen) */
	struct Menu       *Menus;

	SHORT  Pens[nPENS+1];
	SHORT  GradPens[nPENS_GRAD+2];  /* this is a copy for the gradient slider including Black */
	LONG   nPens;			/* allocated pens (consecutive) */

	struct Gadget *glistptr;
	struct Gadget *sliderR;
	struct Gadget *sliderG;
	struct Gadget *sliderB;
	struct Gadget *ButtonSave;

	struct Gadget *CWheel;
	struct Gadget *GradSlider;

	struct Gadget *CycleSrc;       /* Source Cycle: None, Power,Floppy,Caps,In3,In4 */
//	struct Gadget *CycleSecondary; /* Secondary Source Cycle */
	struct Gadget *SrcState;       /* MX State: Off, On, 2nd */

	struct Gadget *PowerLED;
	struct Gadget *FloppyLED;
	struct Gadget *CapsLED;        /* generic Gadget + CapsImage */

	struct Gadget *ActiveText;     /* TEXT_KIND showing active gadget */

	ULONG	active_state;		/* current state 0-2 */
	ULONG	active_led;             /* LED index */

	/* Keyboard comms */
	ULONG	comm_timeouts;
	ULONG	comm_fails;
	ULONG	comm_success;
	ULONG	comm_notified;
	
	ULONG   refreshlist;		/* gadgets that need refreshing */
};


struct myWindow *Window_Open( struct configvars *conf );
LONG Window_Close(struct configvars *conf, struct myWindow *win );
LONG Window_Event(struct configvars *conf, struct myWindow *win );
LONG Window_Timer(struct configvars *conf, struct myWindow *win );
LONG Window_Destroy( struct configvars *conf, struct myWindow *win );


#endif /* _INC_WINDOW_H */
