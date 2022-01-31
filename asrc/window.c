/*
   Notes:
   - ledmanager should already be initialized (see cx_main.c)
   - custom classes (pledimage,capsimage,pledbutton) are
     supposed to be initialized outside

  TODO:
   - Custom Screen
   - rainbow cycle mode (slow, medium, fast )
   - save config button

   - Color/Source Manager (including default config)
     - write to Keyboard

*/
#include <exec/memory.h>

#include "config.h"
#define _WINDOWPRIVATE_
#include "window.h"
#define __NOLIBBASE__
#include <proto/intuition.h>
#include <proto/utility.h>
#include <proto/graphics.h>
#include <proto/colorwheel.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/asl.h>
#include <proto/gadtools.h>
#include <gadgets/colorwheel.h>
#include <gadgets/gradientslider.h>
#include <intuition/icclass.h>
#include <intuition/imageclass.h>
#include <intuition/gadgetclass.h>


#include "capsimage.h"
#include "pledimage.h"
#include "pledbutton.h"
#include "ledmanager.h"

/* external lib bases */
#ifdef __SASC
extern struct DosLibrary *DOSBase;
#else
extern struct Library *DOSBase;
#endif
extern struct Library *SysBase;
extern struct IntuitionBase *IntuitionBase;
extern struct GfxBase *GfxBase;
extern struct Library *UtilityBase;
extern struct Library *GadToolsBase;
extern struct Library *AslBase;
extern struct Library *IconBase;
extern struct Library *ColorWheelBase;
extern struct Library *GradientSliderBase;
extern Class *pledimage_class;
extern Class *capsimage_class;
extern Class *pledbutton_class;
extern STRPTR cx_Name;


void DrawFrames( struct myWindow *win );
void DeleteGads( struct myWindow *win );
void RefreshGads( struct myWindow *win, struct Gadget *gad, ULONG flags );
struct Gadget *CreateGads( struct myWindow *win );
LONG SetupScreen( struct configvars *conf, struct myWindow *win );
LONG ShutDownScreen( struct configvars *conf, struct myWindow *win );
void UpdateSliders(struct myWindow *win, ULONG code, struct Gadget *gad, struct TagItem *tlist, LONG color );
void UpdateSrcState( struct myWindow *win, ULONG code, struct Gadget *gad  );
void ScaleXY( struct myWindow *win, USHORT *x, USHORT *y, USHORT *denom );

/* Gadget IDs (also used for refreshlist, so keep <32) */
#define ID_sliderR 1
#define ID_sliderG 2
#define ID_sliderB 3
#define ID_sliderGrad 4
#define ID_Wheel 5
#define ID_SourceCycle 6
#define ID_StateMX 7
#define ID_LEDPower 8
#define ID_LEDFloppy 9
#define ID_LEDCaps 10
#define ID_SaveEEPROM 11



#define WIN_W 247
#define WIN_H 164

#define MX_FORCE_NEW 0x8000

/* strings */
const char levfmt[]= " %2lx "; /* level format for sliders */

/* this order matches LEDB_* in ledmanager.h */
STRPTR sourceStrings[7] = {    /* sources */
 (STRPTR)"No input",
 (STRPTR)"Power",
 (STRPTR)"Floppy",
 (STRPTR)"Auxiliary IN3",
 (STRPTR)"Auxiliary IN4",
 (STRPTR)"Capslock",
 NULL
};
STRPTR MXstateStrings[4] = {   /* MX state */
 (STRPTR)"Off",
 (STRPTR)"On",
 (STRPTR)"2nd",
 NULL
};

struct myGadProto Wheeltmp = {80,16,80,80,"W",ID_Wheel,WHEELGRAD_KIND,0,0,0,NULL};
struct myGadProto sliderGradtmp = {164,16,14,80,"V",ID_sliderGrad,GRADSLIDER_KIND,0,255,128,NULL};
struct myGadProto sliderRtmp = {184,16,14,80,"R",ID_sliderR,SLIDER_KIND,0,255,128,NULL};
struct myGadProto sliderGtmp = {204,16,14,80,"G",ID_sliderG,SLIDER_KIND,0,255,128,NULL};
struct myGadProto sliderBtmp = {224,16,14,80,"B",ID_sliderB,SLIDER_KIND,0,255,128,NULL};
struct myGadProto cycleSource= {74,125,160,14,"Display Source",ID_SourceCycle,CYCLE_KIND,1,0,0,sourceStrings};
struct myGadProto MXstate =   {16+24, 125, 10, 10, "State",ID_StateMX,MX_KIND,1,0,0,MXstateStrings};
struct myGadProto LEDPower  = {13, 24, 48, 6, "Power",  ID_LEDPower,PLEDIMAGE_KIND,3,4,5,NULL};
struct myGadProto LEDFloppy = {13, 48, 48, 6, "Floppy", ID_LEDFloppy,PLEDIMAGE_KIND,0,1,2,NULL};
struct myGadProto LEDCaps   = {33, 72,  8, 8, "Capslock", ID_LEDCaps,CAPSIMAGE_KIND,6,0,0,NULL};
struct myGadProto ButtonSave ={142,144, 92,14, "Save EEPROM",ID_SaveEEPROM,BUTTON_KIND,0,0,0,NULL};

#define FRAME_UP     0
#define FRAME_DOWN   1
#define FRAME_DOUBLE 2
#define FRAME_END    3
struct {
 USHORT x;
 USHORT y;
 USHORT w;
 USHORT h;
 USHORT type;
} frames[] = {
 { 2,2,71,107, FRAME_DOWN },
 { 73,2,172,107, FRAME_DOWN },
 { 2,109,243,53, FRAME_DOWN },
 { 0,0,0,0,FRAME_END }
};

struct TextAttr Topaz = {       /* KeyToy text attributes */
    "topaz.font",               /* font name */
    8,                  /* font height */
    FS_NORMAL, FPF_DISKFONT        /* font style, preferences */
};


struct myWindow *Window_Open( struct configvars *conf )
{
	struct myWindow *win;
	struct Gadget *gads;
	ULONG  w,h;
	USHORT scalex,scaley,denom;

	win = (struct myWindow*)AllocVec( sizeof( struct myWindow ), MEMF_PUBLIC|MEMF_CLEAR );
	if( !win )
		return NULL;

	do
	{
		/* get Pubscreen or open custom screen */
		if( SetupScreen( conf, win ) < 0 )
		{
			//Printf("SetupScreen fail\n");
			break;
		}
		/* */
		if( !(gads = CreateGads( win ) ) )
		{
			//Printf("CreateGads fail\n");
			break;
		}

		ScaleXY( win, &scalex, &scaley, &denom );
		w     = UDivMod32( (USHORT)scalex * (USHORT)WIN_W, denom );
		h     = UDivMod32( (USHORT)scaley * (USHORT)WIN_H, denom );

		h    += win->screen->WBorTop + (win->screen->Font->ta_YSize + 1) + win->screen->WBorBottom;
		w    += win->screen->WBorLeft + win->screen->WBorRight;

                if( !(win->window = OpenWindowTags(NULL,WA_Height,h,
                                                 WA_Width,        w,
                                                 WA_CustomScreen, (ULONG)win->screen,
                                                 WA_IDCMP,        SLIDERIDCMP | IDCMP_GADGETUP | IDCMP_GADGETDOWN | IDCMP_CLOSEWINDOW | IDCMP_REFRESHWINDOW | IDCMP_IDCMPUPDATE | IDCMP_INTUITICKS, // IDCMP_MOUSEMOVE | IDCMP_REFRESHWINDOW,
                                                 WA_SizeGadget,   FALSE,
                                                 WA_DragBar,      TRUE,
                                                 WA_CloseGadget,  TRUE,
                                                 WA_Gadgets,      (ULONG)gads,
                                                 WA_Activate,     TRUE,
						 WA_Title,  (ULONG)cx_Name,
                                                 TAG_DONE)))
			break;
		GT_RefreshWindow(win->window,NULL);
		win->IdleTickCount = 0;
		win->sigmask = 1L<<win->window->UserPort->mp_SigBit;
		UpdateSrcState( win, LED_ACTIVE , win->SrcState );
		DrawFrames( win ); 
#if 0
		/* quick test */
		if(1)
		{
		  struct Image *img = NULL;
		  img = NewObject(pledimage_class,NULL,
				                IA_Screen,(ULONG)win->screen,
						IA_Pens, (ULONG)win->GradPens,
						IA_Width, 48,
						IA_Height, 6,
                				TAG_END);
		  DrawImage( win->window->RPort, img, 20, 25 );
		  DisposeObject(img);
		}
#endif

	}
	while(0);

	return win;
}

LONG Window_Destroy( struct configvars *conf, struct myWindow *win )
{
	if( !win )
		return 0;

	Window_Close( conf, win );
	FreeVec( win );

	return 0;
}

LONG Window_Close(struct configvars *conf, struct myWindow *win )
{
	if( !win )
		return 0;

	if( win->window )
		CloseWindow( win->window );
	win->window = NULL;
	win->sigmask = 0;

	DeleteGads(win);
	ShutDownScreen( conf, win );

	return 0;
}


void ScaleXY( struct myWindow *win, USHORT *x, USHORT *y, USHORT *denom )
{
	if( !win )
		return;

	*x = win->Font.ta_YSize;
	*y = win->Font.ta_YSize;
	*denom = 8;
}


void DrawFrames( struct myWindow *win )
{
 SHORT i;
 USHORT scalex,scaley,denom=8; /* scaling in relation to Topaz 8 */
 USHORT top,left,x,y,w,h;

	if( !win->window )
		return;

	ScaleXY( win, &scalex, &scaley, &denom );
	top  = win->screen->WBorTop + (win->screen->Font->ta_YSize + 1);
	left = win->screen->WBorLeft;

	i=0;
	while( frames[i].type != FRAME_END )
	{
		x = left + UDivMod32( frames[i].x * scalex, denom );
		y = top  + UDivMod32( frames[i].y * scaley, denom );
		w =       UDivMod32( frames[i].w * scalex, denom );
		h =       UDivMod32( frames[i].h * scaley, denom );
		//Printf("x %ld y %ld w %ld h %ld\n",x,y,w,h);
		DrawBevelBox( win->window->RPort, 
			      left + UDivMod32( frames[i].x * scalex, denom ),
			      top  + UDivMod32( frames[i].y * scaley, denom ),
			             UDivMod32( frames[i].w * scalex, denom ),
			             UDivMod32( frames[i].h * scaley, denom ),
				GTBB_Recessed, TRUE,
				GTBB_FrameType, BBFT_RIDGE,
				GT_VisualInfo, (ULONG)win->VisualInfo,
				TAG_DONE );
		i++;
	}

}


void UpdateLEDButton( struct myWindow *win, ULONG code, struct Gadget *gad, struct myGadProto *prot )
{
	ULONG ledidx,color24,map;

	/* get LED index */
	if( code == 0 )
		ledidx = prot->arg1;
	else if( code == 1 )
		ledidx = prot->arg2;
	     else
	     	ledidx = prot->arg3;
	
	win->active_led = ledidx;

	 map = ledmanager_getSrc( win->active_led, win->active_state ); /* get source configuration by state (code) */
	 if( map == LEDB_SRC_INACTIVE )
	 	map = 0;
	 else   map++;   /* cycle is sorted by LEDB_ definitions */
	 GT_SetGadgetAttrs( win->CycleSrc, win->window, NULL, GTCY_Active,map,TAG_DONE);

	color24  = ledmanager_getColor( ledidx, win->active_state, 0 )<<16;
	color24 |= ledmanager_getColor( ledidx, win->active_state, 1 )<<8;
	color24 |= ledmanager_getColor( ledidx, win->active_state, 2 );
	UpdateSliders(win,0,NULL,NULL,color24);
}

/*
  source cycle was clicked, remember position
*/
void UpdateCycleSrc( struct myWindow *win, ULONG code, struct Gadget *gad  )
{
	LONG map;

	if( code == 0 )
		map = LEDB_SRC_INACTIVE;
	else	map = code - 1; /* cycle is sorted by LEDB_ definitions */

	ledmanager_setSrc( win->active_led, win->active_state, map );
}


/*
  code = MX index from Window
  code|0x8000 = change state to specified value (MX_FORCE_NEW)
*/
void UpdateSrcState( struct myWindow *win, ULONG code, struct Gadget *gad  )
{
	if( !(gad) || !(win) )
		return;

	if( code & MX_FORCE_NEW )
	{
		code &= ~(MX_FORCE_NEW);
		win->active_state = code;
		GT_SetGadgetAttrs( gad, win->window, NULL, GTMX_Active, code,TAG_DONE );
		return;
	}

	win->active_state = code;

	if( gad == win->SrcState )
	{
		ULONG val = TRUE;
		if( code == 0 ) /* off state */
			GT_SetGadgetAttrs( win->CycleSrc, win->window, NULL, GTCY_Active,0,TAG_DONE);
		else
			val = FALSE;
					
		GT_SetGadgetAttrs( win->CycleSrc, win->window, NULL, GA_Disabled,val,TAG_DONE);
		RefreshGads(win,win->CycleSrc,0);
	}

	/* update colors from config manager */
	{
	 SHORT i;
	 LONG  r,g,b;
	 ULONG rgb24=0;
	 LONG  map;
	 for( i=0; i < N_LED ; i++ )
	 {
		r   = UMult32( ledmanager_getColor( i, code, 0 ), 0x01010101 );
		g   = UMult32( ledmanager_getColor( i, code, 1 ), 0x01010101 );
		b   = UMult32( ledmanager_getColor( i, code, 2 ), 0x01010101 );
		SetRGB32(&win->screen->ViewPort,
		         win->Pens[i],r,g,b);
		if( i == win->active_led )
			rgb24 = ((r>>8)&0xff0000) | ((g>>16)&0xff00) | ((b>>24)&0xff);
	 }

	 UpdateSliders(win,0,NULL,NULL,rgb24);

	 map = ledmanager_getSrc( win->active_led, code ); /* get source configuration by state (code) */
	 if( map == LEDB_SRC_INACTIVE )
	 	map = 0;
	 else   map++;   /* cycle is sorted by LEDB_ definitions */
	 GT_SetGadgetAttrs( win->CycleSrc, win->window, NULL, GTCY_Active,map,TAG_DONE);

	 win->refreshlist |= ( (1<<ID_sliderR)|(1<<ID_sliderG)|(1<<ID_sliderB)|
	                       (1<<ID_sliderGrad)|(1<<ID_Wheel)|
	                       (1<<ID_LEDPower)|(1<<ID_LEDFloppy)|(1<<ID_LEDCaps)
	                     );
	 RefreshGads(win, NULL, 0 ); 
	}


}


/*
  - window context
  - gadget or tag list (depending on ICMP class)
  - color24 (-1 for regular updates, else RGB24 forced color)
*/
void UpdateSliders( struct myWindow *win, ULONG code, struct Gadget *gad, struct TagItem *tlist, LONG color24 )
{
 struct ColorWheelHSB  hsb;
 struct ColorWheelRGB  rgb;
 ULONG  sval;

 if( color24 != -1L )
 {
 	rgb.cw_Red   = UMult32( (color24>>16)&0xff, 0x01010101 );
	rgb.cw_Green = UMult32( (color24>>8)&0xff,  0x01010101 );
	rgb.cw_Blue  = UMult32( color24&0xff,       0x01010101 );
 }
 else
 {
    if( !gad )
    {
 	struct TagItem *tag;

	if( !tlist )
		return;

	//Printf("ti_Tag %lx ti_Data %lx\n",tlist->ti_Tag,tlist->ti_Data);

	tag = FindTagItem( GA_ID, tlist );
	if( !tag )
	{	
		//Printf("Tag not found %lx\n",(ULONG)tlist);
		return;
	}
	if( tag->ti_Data == ID_sliderGrad )
		gad = win->GradSlider;
	if( tag->ti_Data == ID_Wheel )
		gad = win->CWheel;
	if( !gad )
	{
		//Printf("unknown source\n");
		return;
	}
    }

    /* get RGB from sliders (possibly overridden, if Wheel or Gradient are active */
    GT_GetGadgetAttrs( win->sliderR, win->window, NULL, GTSL_Level, (ULONG)&sval, TAG_DONE );
    rgb.cw_Red   = UMult32( sval, 0x01010101 );
    GT_GetGadgetAttrs( win->sliderG, win->window, NULL, GTSL_Level, (ULONG)&sval, TAG_DONE );
    rgb.cw_Green = UMult32( sval, 0x01010101 );
    GT_GetGadgetAttrs( win->sliderB, win->window, NULL, GTSL_Level, (ULONG)&sval, TAG_DONE );
    rgb.cw_Blue  = UMult32( sval, 0x01010101 );
 }


 if( gad == win->CWheel )
 {
	GetAttr(WHEEL_RGB,win->CWheel,(ULONG *)&rgb); 
 }

 if( gad==win->GradSlider )
 {
	/* on Slider updates, RGB is returned wrong with colorwheel.gadget V39 */
	GetAttr(WHEEL_HSB,win->CWheel,(ULONG *)&hsb);
	ConvertHSBToRGB(&hsb,&rgb);
	//Printf("H %lx S %lx B %lx\n",hsb.cw_Hue,hsb.cw_Saturation,hsb.cw_Brightness);
	win->refreshlist &= ~(1<<ID_sliderGrad);
 }
#if 0
 else
 {	/* update gradient slider colors */
	USHORT i;
	struct ColorWheelRGB  rgb2;
	/* */

	/* this works immediately on native screens but requires redraw on RTG */
	ConvertRGBToHSB(&rgb,&hsb);
	for( i=0 ; i < nPENS_GRAD ; i++ )
	{
		hsb.cw_Brightness = 0xffffffff - ((0xffffffff / nPENS_GRAD) * i);

		ConvertHSBToRGB(&hsb,&rgb2);
		SetRGB32(&win->screen->ViewPort,
		         win->Pens[i+PENIDX_GRAD],
		         rgb2.cw_Red,rgb2.cw_Green,rgb2.cw_Blue);
	}
 }
#endif

 /* touch Wheel only for RGB sliders */
 if( (gad == win->sliderR)||(gad == win->sliderG)||(gad== win->sliderB)||(!gad) )
 	SetGadgetAttrs( win->CWheel, win->window, NULL, WHEEL_RGB,(ULONG)&rgb,TAG_DONE);

 if( gad != win->sliderR )
 	GT_SetGadgetAttrs( win->sliderR, win->window, NULL, GTSL_Level, rgb.cw_Red>>24,TAG_DONE );
 if( gad != win->sliderG )
 	GT_SetGadgetAttrs( win->sliderG, win->window, NULL, GTSL_Level, rgb.cw_Green>>24,TAG_DONE );
 if( gad != win->sliderB )
 	GT_SetGadgetAttrs( win->sliderB, win->window, NULL, GTSL_Level, rgb.cw_Blue>>24, TAG_DONE );

 /* save to LEDManager */
 ledmanager_setColor( win->active_led , win->active_state, 0, (rgb.cw_Red>>24)&0xff  );
 ledmanager_setColor( win->active_led , win->active_state, 1, (rgb.cw_Green>>24)&0xff);
 ledmanager_setColor( win->active_led , win->active_state, 2, (rgb.cw_Blue>>24)&0xff );

 if( win->active_led < 3 )
 	win->refreshlist |= (1<<ID_LEDFloppy);
 else
 {
 	if( win->active_led == 6 )
	 	win->refreshlist |= (1<<ID_LEDCaps);
	else
	 	win->refreshlist |= (1<<ID_LEDPower);
 }
 
 /* update active LED displays by updating pens */
 {
	ULONG pen = win->Pens[win->active_led];

	SetRGB32(&win->screen->ViewPort,pen,rgb.cw_Red,rgb.cw_Green,rgb.cw_Blue);
 }

 RefreshGads( win, NULL, 0 );

}


LONG Window_Event(struct configvars *conf, struct myWindow *win )
{
        struct IntuiMessage* msg;
	struct Gadget *gad;
	LONG needsend = 0;
        ULONG class;
        ULONG code;
        ULONG qual;

        if( !win )
                return 0;
	if( !win->window )
		return 0;

        while(1)
        {
                msg = GT_GetIMsg(win->window->UserPort);
                if( !msg )
                        break;

                class = msg->Class;
                code  = msg->Code;
                gad   = msg->IAddress;
                qual  = msg->Qualifier;
                if( qual & IEQUALIFIER_LEFTBUTTON	)
                	needsend = -65536;
                
                GT_ReplyIMsg(msg);
//              if( class != IDCMP_INTUITICKS )
//			Printf("Class %ld Code %ld Gad %lx\n",class,code,(ULONG)gad);
                switch( class )
                {
			case IDCMP_INTUITICKS:
					win->IdleTickCount++;
					if( win->IdleTickCount > 2 )
					{
						LONG res1;
						
						/* most transmissions fail ? */
						res1 = win->comm_timeouts+win->comm_fails;
						if( ( (res1>>4) > win->comm_success) &&
						    ( res1 > 32 )
						  )
						{
							/* TODO: if the sum of 
							         (timeouts and fails)/16 > successful transmissions 
							         bring up a requester 
							*/

						}

						needsend++;
						res1 = KCMD_NOWORK;
						if( res1 & KCMD_NOWORK )
							win->IdleTickCount = 0;
						else
						{
							win->IdleTickCount = 1;
//							Printf("r1 %ld\n",res1);
						}

						switch( res1 & 0xff )
						{
							case KCMD_TIMEOUT:
								win->comm_timeouts++;
								break;
							case KCMD_NACK:
								win->comm_fails++;
								break;
							case KCMD_ACK:
								win->comm_success++;
								break;
							default:
								break;
						}
					}
					break;
                        case IDCMP_CLOSEWINDOW:
                                        Window_Close( conf, win );
                                        return -1;
                                        break;
			case IDCMP_REFRESHWINDOW:
				win->IdleTickCount = 0;
				GT_BeginRefresh(win->window);
				GT_EndRefresh(win->window,TRUE);
					break;
			case IDCMP_MOUSEMOVE:
				win->IdleTickCount = 0;
		                if( qual & IEQUALIFIER_LEFTBUTTON	)
					UpdateSliders( win, code, gad, NULL, -1 );
				break;
			case IDCMP_IDCMPUPDATE:
				win->IdleTickCount = 0;
				UpdateSliders( win, code, NULL, (struct TagItem*)gad, -1 );
					break;
			case IDCMP_GADGETDOWN:
				win->IdleTickCount = 0;
				/* */
				if( gad == win->SrcState )
				{
					UpdateSrcState( win, code, gad );
				}
					break;
			case IDCMP_GADGETUP:
				win->IdleTickCount = 0;
				if( gad == win->CycleSrc )
					UpdateCycleSrc( win, code, gad );
				if( gad == win->PowerLED )
					UpdateLEDButton( win, code, gad, &LEDPower ); 
				if( gad == win->FloppyLED )
					UpdateLEDButton( win, code, gad, &LEDFloppy );
				if( gad == win->CapsLED )
					UpdateLEDButton( win, code, gad, &LEDCaps );
				if( (gad == win->sliderR)||(gad == win->sliderG)||(gad== win->sliderB)||(gad==win->GradSlider))
					UpdateSliders( win, code, gad, NULL, -1 );
				if( gad == win->ButtonSave )
				{
					ledmanager_saveEEPROM();
				}
				break;
		}
	}
	if( needsend > 0 )
	{
		ledmanager_sendConfig(-1);
	}
	return 0;
}

LONG Window_Timer(struct configvars *conf, struct myWindow *win )
{
	return 0;
}


void RefreshGads( struct myWindow *win, struct Gadget *gad, ULONG flags )
{
	if( gad != NULL )
	{
		RefreshGList(gad,win->window,NULL,1);
		return;
	}
	
	if( win->refreshlist & (1<<ID_sliderR) )
		RefreshGList(win->sliderR,win->window,NULL,1);
	if( win->refreshlist & (1<<ID_sliderG) )
		RefreshGList(win->sliderG,win->window,NULL,1);
	if( win->refreshlist & (1<<ID_sliderB) )
		RefreshGList(win->sliderB,win->window,NULL,1);
	if( win->refreshlist & (1<<ID_sliderGrad) )
		RefreshGList(win->GradSlider,win->window,NULL,1);
	if( win->refreshlist & (1<<ID_Wheel) )
		RefreshGList(win->CWheel,win->window,NULL,1);
	if( win->refreshlist & (1<<ID_SourceCycle) )
		RefreshGList(win->CycleSrc,win->window,NULL,1);
	if( win->refreshlist & (1<<ID_StateMX) )
		RefreshGList(win->SrcState,win->window,NULL,1);
		
	if( win->refreshlist & (1<<ID_LEDPower) )
		RefreshGList(win->PowerLED,win->window,NULL,1);
	if( win->refreshlist & (1<<ID_LEDFloppy) )
		RefreshGList(win->FloppyLED,win->window,NULL,1);
	if( win->refreshlist & (1<<ID_LEDCaps) )
		RefreshGList(win->CapsLED,win->window,NULL,1);
		
	win->refreshlist = 0;

	/* TODO: exclude wheel and/or gradslider from refresh */
//	RefreshGList(win->window->FirstGadget,win->window,NULL,-1);
//	RefreshGList(win->PowerLED,win->window,NULL,-1);

}

/* Call This after removing Gadgets from Window or after Window close */
void DeleteGads( struct myWindow *win )
{
	/* GT Gadgets are behind custom boopsi gadgets in allocation */
	FreeGadgets( win->glistptr );
	win->glistptr = NULL;

	DisposeObject(win->GradSlider);
	DisposeObject(win->CWheel);
	win->GradSlider = NULL;
	win->CWheel = NULL;

	if( win->PowerLED )
	{
		APTR img;
		GetAttr(GA_Image,win->PowerLED,(ULONG *)&img);
		if( img )
			DisposeObject(img);
		DisposeObject( win->PowerLED );
		win->PowerLED = NULL;
	}
	if( win->FloppyLED )
	{
		APTR img;
		GetAttr(GA_Image,win->FloppyLED,(ULONG *)&img);
		if( img )
			DisposeObject(img);
		DisposeObject( win->FloppyLED );
		win->FloppyLED = NULL;
	}
	if( win->CapsLED )
	{
		APTR img;
		GetAttr(GA_Image,win->CapsLED,(ULONG *)&img);
		if( img )
			DisposeObject(img);
		DisposeObject( win->CapsLED );
		win->CapsLED = NULL;
	}
}

/* Create single gadget */
struct Gadget *MakeGad( struct myWindow *win, struct Gadget *glist, struct myGadProto *prot )
{
	struct Gadget *gad = NULL;
	struct NewGadget ng;
	USHORT scalex,scaley,denom=8; /* scaling in relation to Topaz 8 */
	USHORT top,left;

	if( !prot )
		return NULL;

	ScaleXY( win, &scalex, &scaley, &denom );

	top    = win->screen->WBorTop + (win->screen->Font->ta_YSize + 1);
	left   = win->screen->WBorLeft;

	ng.ng_LeftEdge   = left+UDivMod32( prot->x * scalex ,denom);
	ng.ng_TopEdge    = top +UDivMod32( prot->y * scaley ,denom);
	ng.ng_Width      = UDivMod32(prot->w * scalex ,denom);
	ng.ng_Height     = UDivMod32(prot->h * scaley ,denom);
	ng.ng_GadgetText = prot->title;
	ng.ng_TextAttr   = &win->Font;
//	if( prot->type == SLIDER_KIND )
//		ng.ng_TextAttr = &Topaz;
	ng.ng_VisualInfo = win->VisualInfo;
	ng.ng_GadgetID   = prot->id;
	ng.ng_Flags      = PLACETEXT_BELOW;//NG_GRIDLAYOUT;//HIGHLABEL;

	if( prot->type == SLIDER_KIND )
	{
		gad = CreateGadget(SLIDER_KIND,glist,&ng,
				GTSL_LevelFormat, (ULONG)levfmt, /* "%lx", */
				GTSL_MaxLevelLen, (ULONG)3,
				GTSL_Min,(ULONG)prot->arg1,
				GTSL_Max,(ULONG)prot->arg2,
				GTSL_Level,(ULONG)prot->arg3,
				GA_RelVerify,     TRUE,
				GA_Immediate, TRUE,
//				GTSL_MaxPixelLen, ng.ng_Width+8,
				GTSL_LevelPlace, (ULONG)PLACETEXT_ABOVE,
//				GTSL_Justification, GTJ_CENTER,
				PGA_Freedom,   LORIENT_VERT,
				ICA_TARGET,    ICTARGET_IDCMP,
				GT_Underscore, '_',
				TAG_DONE);
	}
	if( prot->type == BUTTON_KIND )
	{
		ng.ng_Flags = PLACETEXT_IN;
		gad = CreateGadget(BUTTON_KIND,glist,&ng,
				GA_RelVerify,     TRUE,
//				GA_Immediate, TRUE,
//				GTSL_MaxPixelLen, ng.ng_Width+8,
//				GTSL_LevelPlace, (ULONG)PLACETEXT_ABOVE,
//				GTSL_Justification, GTJ_CENTER,
//				PGA_Freedom,   LORIENT_VERT,
//				ICA_TARGET,    ICTARGET_IDCMP,
				GT_Underscore, '_',
				TAG_DONE);
	}
	if( prot->type == CYCLE_KIND )
	{
		ng.ng_Flags      = PLACETEXT_ABOVE;//|NG_HIGHLABEL;
		gad = CreateGadget(CYCLE_KIND,glist,&ng,
				GTCY_Labels, (ULONG)prot->attribs,
				GTCY_Active, (ULONG)prot->arg1,
				//GA_Immediate, TRUE,
				//GT_Underscore, '_',
				TAG_END );
	}
	if( prot->type == MX_KIND )
	{
		ng.ng_Flags      = PLACETEXT_LEFT;
		gad = CreateGadget(MX_KIND,glist,&ng,
				GTMX_TitlePlace, PLACETEXT_ABOVE,
				GTMX_Labels, (ULONG)prot->attribs,
				GTMX_Spacing, 4,//ng.ng_TextAttr->ta_YSize + 1,
				GTMX_Scaled, TRUE,
				GTMX_Active, (ULONG)prot->arg1,
				//GA_Immediate, TRUE,
				//GT_Underscore, '_',
				TAG_END );
	}
	if( prot->type == PLEDIMAGE_KIND )
	{
		struct Image *img = NULL;
		SHORT pens[4]; /* 3 LED setup */

		pens[0] = win->Pens[prot->arg1];
		pens[1] = win->Pens[prot->arg2];
		pens[2] = win->Pens[prot->arg3];
		pens[3] = ~0;

		img = NewObject(pledimage_class,NULL,
				                IA_Screen,(ULONG)win->screen,
						IA_Pens, (ULONG)pens,//win->GradPens,
						IA_Width, ng.ng_Width,
						IA_Height, ng.ng_Height,
                				TAG_END);
		if( img )
			gad = (struct Gadget*)NewObject(pledbutton_class,NULL,
						GA_Left,   ng.ng_LeftEdge,
						GA_Top,    ng.ng_TopEdge,
						GA_Width,  ng.ng_Width,
						GA_Height, ng.ng_Height,
						GA_Image,  (ULONG)img,
						GA_Text,   (ULONG)prot->title,
						GA_TextAttr, (ULONG)&win->Font,
						GA_ID,     prot->id,
						//GA_Immediate, TRUE,
						GA_RelVerify,TRUE,
						(glist) ? GA_Previous:TAG_IGNORE,(ULONG)glist,
						TAG_DONE);
		if( !gad )
		  	DisposeObject(img);
	}
	if( prot->type == CAPSIMAGE_KIND )
	{
		struct Image *img = NULL;
		SHORT pens[2]; /* 1 LED setup */

		pens[0] = win->Pens[prot->arg1];
		pens[1] = ~0;

		img = NewObject(capsimage_class,NULL,
				                IA_Screen,(ULONG)win->screen,
						IA_Pens, (ULONG)pens,//win->GradPens,
						IA_Width, ng.ng_Width,
						IA_Height, ng.ng_Height,
                				TAG_END);
		if( img )
		{
			gad = (struct Gadget*)NewObject(pledbutton_class,NULL,
			//gad = (struct Gadget*)NewObject(NULL,"buttongclass", /* generic button, doesn't give outline or text with images */
						GA_Left,   ng.ng_LeftEdge,
						GA_Top,    ng.ng_TopEdge,
						GA_Width,  ng.ng_Width,
						GA_Height, ng.ng_Height,
						GA_Image,  (ULONG)img,
						GA_Text,   (ULONG)prot->title,
						GA_TextAttr, (ULONG)&win->Font,
						GA_ID,     prot->id,
						//GA_Immediate, TRUE,
						GA_RelVerify,TRUE,
						PLED_Mode, PLEDMode_Caps, 
						(glist) ? GA_Previous:TAG_IGNORE,(ULONG)glist,
						TAG_DONE);
			if( !gad )
		  		DisposeObject(img);
		}
	}
	if( prot->type == GRADSLIDER_KIND )
	{
           gad = (struct Gadget *)NewObject(NULL,"gradientslider.gadget",
                                                GA_Top,        ng.ng_TopEdge,
                                                GA_Left,       ng.ng_LeftEdge,
                                                GA_Width,      ng.ng_Width,
                                                GA_Height,     ng.ng_Height,
                                                GRAD_PenArray, (ULONG)win->GradPens,//&win->Pens[PENIDX_GRAD],
                                                PGA_Freedom,   LORIENT_VERT,
						ICA_TARGET,     ICTARGET_IDCMP,
						GA_ID,         prot->id,
						(glist) ? GA_Previous:TAG_IGNORE,(ULONG)glist,
                                                TAG_END);
	}
	if( prot->type == WHEELGRAD_KIND )
	{
            gad = (struct Gadget *)NewObject(NULL, "colorwheel.gadget",
                                                GA_Top,        ng.ng_TopEdge,
                                                GA_Left,       ng.ng_LeftEdge,
                                                GA_Width,      ng.ng_Width,
                                                GA_Height,     ng.ng_Height,
						GA_ID,         prot->id,
						WHEEL_Screen,  (ULONG)win->screen,
						/* TODO: put this in to prot->attribs */
						(glist) ? WHEEL_GradientSlider:TAG_IGNORE,(ULONG)glist,
                                                GA_FollowMouse,       TRUE,
                                                (glist) ? GA_Previous:TAG_IGNORE,         (ULONG)glist,
						ICA_TARGET,     ICTARGET_IDCMP,
                                                TAG_END);
	}
	return gad;
}
/*
  Create Gadgets, store their pointers in myWindow and return GList
*/
struct Gadget *CreateGads( struct myWindow *win )
{
	struct Gadget *glist = NULL;
	struct Gadget *gtprev,*first = NULL;
	LONG success = 0;

	win->refreshlist = 0;		/* gadgets that need refreshing */

	gtprev = CreateContext(&win->glistptr);

	do
	{
		/* keep these together */
		if( !(win->GradSlider = glist = MakeGad( win, glist, &sliderGradtmp )) )
			break;
		first = glist;
		if( !(win->CWheel     = glist = MakeGad( win, glist, &Wheeltmp )) )
			break;
		/* */
		if( !(win->PowerLED   = glist = MakeGad( win, glist, &LEDPower )) )
			break;
		if( !(win->FloppyLED  = glist = MakeGad( win, glist, &LEDFloppy)) )
			break;
		if( !(win->CapsLED    = glist = MakeGad( win, glist, &LEDCaps)) )
			break;

		/* gadtools gadgets need the context */
		glist->NextGadget = gtprev;
		if( !(win->sliderB = glist = MakeGad( win, gtprev, &sliderBtmp )) )
			break;
		if( !(win->sliderG = glist = MakeGad( win, glist, &sliderGtmp )) )
			break;
		if( !(win->sliderR = glist = MakeGad( win, glist, &sliderRtmp )) )
			break;
		if( !(win->CycleSrc = glist = MakeGad( win, glist, &cycleSource )) )
			break;
		if( !(win->SrcState = glist = MakeGad( win, glist, &MXstate )) )
			break;
		if( !(win->ButtonSave =glist= MakeGad( win, glist, &ButtonSave)) )
			break;


		success = 1;
	}
	while(0);

	return first; /* will be NULL if any gadget cannot be created */
}



LONG ShutDownScreen( struct configvars *conf, struct myWindow *win )
{
	LONG i;
	struct Screen *scr = win->screen;

	if( !scr )
		return 0;
	
	for( i=0 ; i< win->nPens ; i++ )
	{
		if( win->Pens[i] == -1 )
			continue;
		ReleasePen( scr->ViewPort.ColorMap,win->Pens[i]);
		win->Pens[i] = -1;
	}

	if( win->VisualInfo )
		FreeVisualInfo( win->VisualInfo );
	win->VisualInfo = NULL;

	if( !win->customscreen )
		UnlockPubScreen( NULL, scr );
	else
	{
		/* TODO: shutdown custom screen */
	}
	win->customscreen = NULL;
	win->screen = NULL;

	return 0;
}



LONG SetupScreen(struct configvars *conf, struct myWindow *win )
{
	struct Screen *scr;
	LONG i;

	/* TODO: Customscreen in config */
	/* TODO: retry on custom screen */
	win->screen = LockPubScreen( NULL );
	scr = win->screen;

	if( scr )
	{
	  win->nPens = nPENS;
	  for( i=0 ; i< nPENS ; i++ )
	  {
		win->Pens[i] = ObtainPen(scr->ViewPort.ColorMap,-1,
			                   ((ULONG)(myPens[i].R))<<24,
			                   ((ULONG)(myPens[i].G))<<24,
			                   ((ULONG)(myPens[i].B))<<24,
					   PEN_EXCLUSIVE );
		if( win->Pens[i] == -1 )
		{
			win->nPens = i; /* this failed, previous worked */
			break;
		}
			
	  }
	  win->Pens[nPENS] = ~0; /* TODO: remove */

	  for( i=0 ; i < nPENS_GRAD ; i++ )
	  {
		win->GradPens[i] = win->Pens[PENIDX_GRAD+i];
	  }
	  win->GradPens[i] = 1;
	  win->GradPens[i+1] = ~0;

	  win->DrawInfo = GetScreenDrawInfo( scr );

	  if(1)//	  if( win->nPens == nPENS )
		win->VisualInfo = GetVisualInfo( scr ,TAG_DONE );
	  else
	  {
		ShutDownScreen( conf, win );
		scr = NULL;
	  }

	  if( !win->Font.ta_Name ) 
	  {
		 win->Font.ta_YSize = 8;
		 win->Font.ta_Name  = "topaz.font";

		 if( conf->fontname )
			win->Font.ta_Name = conf->fontname;
		 if( conf->fontsize )
			win->Font.ta_YSize = *(conf->fontsize);
		 else
		 	if( scr->Font )
			{
				win->Font.ta_Name  = scr->Font->ta_Name;
				win->Font.ta_YSize = scr->Font->ta_YSize;
			}
	  }
	}
		
	if( !scr )
		return -1;
	else	return 0;
}




