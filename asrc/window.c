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



#if 0

#include <exec/ports.h>
#include <exec/lists.h>
#include <exec/memory.h>
#include <string.h>
#include <intuition/intuitionbase.h>
#include <intuition/intuition.h>
#include <libraries/gadtools.h>
#include <workbench/workbench.h>
#define __NOLIBBASE__
#include <proto/intuition.h>
#include <proto/utility.h>
#include <proto/graphics.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/asl.h>
#include <proto/i2csensors.h>
#include <proto/gadtools.h>
#include <proto/icon.h>
#include <libraries/i2csensors.h>
#include "window.h"
#include "config.h"
#include "macros.h"
#include "utils.h"
#include "version.h"


#define DEF_GAUGE_WIDTH  64
#define DEF_HSPACE 4 /* 2x HSPACE left/right */
#define DEF_VSPACE 4 /* 2x VSPACE top/bottom */
#define DEF_HEIGHT 30
#define MIN_HEIGHT 20
#define MAX_HEIGHT 100
#define MAXBMWIDTH 128
#define DRAG_HEIGHT 10

/* background types: half transparent, empty, picture, full fake transparent (no borders) */
#define BG_COPY  0
#define BG_EMPTY 1
#define BG_PIC   2
#define BG_TRANS 3
#define FG_TITLES 1
#define FG_NOBORDER 2

struct PenList {
	LONG index;
	UBYTE R,G,B;
};

#define nPENS 9
#define PEN_BG2 7
struct PenList myPens[nPENS] = {
 { -1, 0xf0,  149,  135 }, /* Red */
 { -1, 0xf0,  149,  135 }, /* Red */
 { -1,   90,  190,  120 }, /* Green */
 { -1,   90,  129,  190 }, /* BLUE */
 { -1,   90,  129,  190 }, /* BLUE */
 { -1, 0x90, 0x90, 0x30 }, /* Yellow */
 { -1, 0x90, 0x90, 0x30 }, /* Yellow */
 { -1,  162,  162,  162 }, /* darker grey */
 { -1,   90,  190,  120 }, /* Green */
};
#define PEN_V 0
#define PEN_C 1
#define PEN_T 2
#define PEN_F 3
#define PEN_P 4
#define PEN_POW 5
#define PEN_H 6
#define PEN_M 8

/* types to scan for (sensor sources, menus) */
#define nTypes 8
const LONG senlist[nTypes]    = { I2C_VOLTAGE,I2C_CURRENT,I2C_TEMP,I2C_FAN,I2C_PRESSURE, I2C_POWER, I2C_HUMIDITY, I2C_MISC };
const char*sennames[nTypes]   = { "Voltage",  "Current",  "Temperature","Fan","Pressure","Power",   "Humidity", "Misc" };
const SHORT senheight[nTypes] = { DEF_HEIGHT, DEF_HEIGHT, DEF_HEIGHT, DEF_HEIGHT, DEF_HEIGHT, DEF_HEIGHT, DEF_HEIGHT, DEF_HEIGHT };
/* display scaling bounds (16.16) */
const ULONG senmin[nTypes]       = { 3<<16,      0<<16,      10<<16,  300<<16, 300<<16,      1<<16,      0<<16, 0<<16     };
const ULONG senmax[nTypes]       = {15<<16,      5<<16,      90<<16,  3000<<16,2000<<16,    50<<16,    100<<16, 10000<<16 };
const UBYTE penidx[nTypes]       = { PEN_V,      PEN_C,       PEN_T,     PEN_F,   PEN_P,   PEN_POW,      PEN_H, PEN_M };
/* number of active sensors per type */
LONG sennum[nTypes];
USHORT Wleft = 20;
USHORT Wtop  = 20;
USHORT Wwidth = 0xffff; /* bar width (i.e. without horizontal spaces) */
USHORT Wchg;
USHORT Sdelay = 0xffff;
USHORT bgtype = 0xffff;
USHORT fgflags= 0xffff;
LONG   recessedborder = TRUE;

#define ISB_DISABLED 0
#define ISF_DISABLED 1

#define CMD_HIDE 0x80000001
#define CMD_QUIT 0x80000002
#define CMD_W64  0x80000003
#define CMD_W96  0x80000004
#define CMD_W128 0x80000005
#define CMD_W160 0x80000006
#define CMD_SAVECONF 0x80000007
#define CMD_FONTCONF 0x80000008
#define CMD_D0    0x80000009
#define CMD_D5    0x8000000A
#define CMD_D10   0x8000000B
#define CMD_D15   0x8000000C
#define CMD_D20   0x8000000D
#define CMD_BTRANS 0x8000000E
#define CMD_BNONE  0x8000000F
#define CMD_SHOWTITLE  0x80000010
#define CMD_BFULLTRANS 0x80000011
#define CMD_ABOUT      0x80000012

#define DEF_ITEMS 23
struct NewMenu defmenus[DEF_ITEMS] = {
 {NM_TITLE,  "Project", 0, 0, 0, NULL },
 {NM_ITEM, "About",0 , 0, 0, (APTR)CMD_ABOUT },
 {NM_ITEM, "Hide", "H", 0, 0, (APTR)CMD_HIDE },
 {NM_ITEM, "Quit", "Q", 0, 0, (APTR)CMD_QUIT },
 {NM_TITLE,   "Config", 0, 0, 0, NULL },
 {NM_ITEM,      "Font", 0, 0, 0, (APTR)CMD_FONTCONF },
#define IDX_TITLES 6
 {NM_ITEM,"Show Title", 0, CHECKIT|MENUTOGGLE, 0, (APTR)CMD_SHOWTITLE},
 {NM_ITEM,     "Width", 0, 0, 0, NULL },
#define IDX_WIDTHTOGGLE 8 /* this index +3 = width presets */
#define IDX_WIDTHCOUNT 4
#define IDX_WIDTHADVANCE 32
#define IDX_WIDTHMIN 64
 {NM_SUB,      "64",    0, CHECKED|CHECKIT|MENUTOGGLE,~1,(APTR)CMD_W64  },
 {NM_SUB,      "96",    0, CHECKIT|MENUTOGGLE,~2,(APTR)CMD_W96  },
 {NM_SUB,      "128",   0, CHECKIT|MENUTOGGLE,~4,(APTR)CMD_W128 },
 {NM_SUB,      "160",   0, CHECKIT|MENUTOGGLE,~8,(APTR)CMD_W160 },
 {NM_ITEM,"Start Delay",0, 0, 0, NULL },
#define IDX_DELAYTOGGLE 13
#define IDX_DELAYCOUNT  5
#define IDX_DELAYADVANCE 5
#define IDX_DELAYMIN    0
 {NM_SUB,        "0",    0, CHECKED|CHECKIT|MENUTOGGLE,~1,(APTR)CMD_D0 },
 {NM_SUB,        "5",    0, CHECKIT|MENUTOGGLE,~2,(APTR)CMD_D5 },
 {NM_SUB,        "10",   0, CHECKIT|MENUTOGGLE,~4,(APTR)CMD_D10},
 {NM_SUB,        "15",   0, CHECKIT|MENUTOGGLE,~8,(APTR)CMD_D15},
 {NM_SUB,        "20",   0, CHECKIT|MENUTOGGLE,~16,(APTR)CMD_D20},
 {NM_ITEM,"Background",0, 0, 0, NULL },
#define IDX_BGMODE 19
 {NM_SUB,        "Half transparent", 0, CHECKIT|MENUTOGGLE,~1,(APTR)CMD_BTRANS },
 {NM_SUB,        "Empty",       0, CHECKIT|MENUTOGGLE,~2,(APTR)CMD_BNONE },
 {NM_SUB,        "Full transparent", 0, CHECKIT|MENUTOGGLE,~4,(APTR)CMD_BFULLTRANS },
 {NM_ITEM,"Save Config", 0, 0, 0, (APTR)CMD_SAVECONF },
};

BYTE fontname[108];     /* storage for fontname */
struct TextAttr Topaz = {       /* KeyToy text attributes */
    NULL,               /* font name */
    8,                  /* font height */
    FS_NORMAL, 0        /* font style, preferences */
};
struct IntuiText myText;
struct IntuiText myTextShadow;

struct InputSource {
	struct MinNode n;
	ULONG type;
	ULONG flags;    /* ISF_DISABLED */
	ULONG w,h;      /* width/height in window */
	ULONG id;       /* sensor ID for use with I2C sensors */
	BYTE  *name;
	BYTE  *unit;

	/* backlog data */
	SHORT *backlog;      /* log buffer (in display units)  */
	USHORT backlog_size; /* size of log buffer */
	USHORT backlog_pos;  /* current position in log buffer */
	ULONG  scale_min;    /* recalculation for backlog: bounds */
	ULONG  scale_max;

	/* drawing into pixbuffer from backlog */
	UBYTE *pixbuffer;    /* graphics output buffer */
	USHORT pixbuffer_w;  /* */
	USHORT pixbuffer_h;  /* */
	UBYTE  pen1;         /* index of pen (in Penlist, not screen pen) */
	UBYTE  pen2;

	/* header logo (datatypes object) (TODO) */
	Object *logo;
	USHORT logo_w;
	USHORT logo_h;
	USHORT logo_x;
	USHORT logo_y;
};


struct BitMap     *myBGMap      = NULL;
struct Screen     *myScreen     = NULL;
struct Window     *myWindow     = NULL;
struct DrawInfo   *myDrawInfo   = NULL;
struct VisualInfo *myVisualInfo = NULL;
struct Menu       *myMenus      = NULL;
struct RastPort   myTmpRP;
ULONG  myTmpBitmapBPR;
UBYTE  *myMask = NULL;
ULONG  myMaskBPR;
//struct Gadget     *myGadgets    = NULL;
struct MsgPort    *myWindowPort = NULL;
ULONG              mySigmask    = 0;
struct Gadget      dragGadget;


/* internal proto */
LONG getSourcesHeight( struct configvars *conf, struct MinList *lst );
LONG getSources( struct configvars *conf, struct MinList *lst );
LONG AddMenus( struct Window *win, struct configvars *conf, struct MinList *lst, LONG ncat, LONG nsensors);
LONG Window_SelectFont( struct configvars *conf );
LONG Window_SaveConfig( struct configvars *conf, struct MinList *myInputList );
void UpdatePens( STRPTR colorspec, LONG penidx );





LONG Window_Open( struct configvars *conf, struct MinList *myInputList )
{
	LONG wh,ww,categories=0,nsensors=0;
	

	if( myWindow )			/* Window already open? */
		return (LONG)mySigmask;

	myTmpRP.BitMap = NULL;
	mySigmask = 0;
	Wchg = 0;

	if( Wwidth == 0xffff )
	{
		if( conf->winwidth )
			Wwidth = *conf->winwidth;
		else	Wwidth = DEF_GAUGE_WIDTH;

		/* first start: update pens (if available) */
		UpdatePens( conf->colorV, PEN_V );
		UpdatePens( conf->colorC, PEN_C );
		UpdatePens( conf->colorT, PEN_T );
		UpdatePens( conf->colorF, PEN_F );
		UpdatePens( conf->colorP, PEN_P );
		UpdatePens( conf->colorH, PEN_H );
	}


	{
	 int i;
	 struct InputSource *is;

	 is = (struct InputSource*)GetHead( myInputList );
	 while( is )
	 {
			is=(struct InputSource*)GetSucc(is);
			nsensors++;
	 }

	 /* TODO: this is bad style, sennum should go away... */
	 for( i=0 ; i<nTypes ; i++ )
	 {
	 	if( sennum[i] )
			categories++;
	 }
	}

	wh = getSourcesHeight( conf, myInputList );
	wh+= DRAG_HEIGHT;

	if( !myScreen ) /* keep screen on reopen */
	{
	 int i;
	 myScreen = LockPubScreen( NULL ); /* open on default pubscreen */
	 if( myScreen )
	 {
	  for( i=0 ; i< nPENS ; i++ )
	  {
			myPens[i].index = ObtainBestPenA(myScreen->ViewPort.ColorMap,
			                   ((ULONG)(myPens[i].R))<<24,
			                   ((ULONG)(myPens[i].G))<<24,
			                   ((ULONG)(myPens[i].B))<<24,NULL );
	  }
	  myDrawInfo = GetScreenDrawInfo( myScreen );
	 }
	}
	myVisualInfo = GetVisualInfo(myScreen,TAG_DONE);

	if( !Topaz.ta_Name ) /* override with defaults only on first Window open */
	{
	 if( conf->fontname )
		Topaz.ta_Name = conf->fontname;
	 if( conf->fontsize )
		Topaz.ta_YSize = *(conf->fontsize);
	 else
	  if( myScreen->Font )
	 	Topaz.ta_YSize = myScreen->Font->ta_YSize;
	}

	if( conf->win_x )
		Wleft =	*(conf->win_x);
	if( conf->win_y )
		Wtop  = *(conf->win_y);

	/* clamp to screen */
	if( myScreen->Height < Wtop+wh )
	{
		Wtop = myScreen->BarHeight;
		wh   = myScreen->Height - myScreen->BarHeight;
	}
	if( Wwidth < 64 )
		Wwidth = 64;
	if( Wwidth > 160 )
		Wwidth = 160;
	Wwidth &= ~31; /* n*32 */

	ww = Wwidth+2*DEF_HSPACE;
	if( Wleft+ww > myScreen->Width )
		Wleft = myScreen->Width-ww;

//	APTR    background; /* background type */
	if( bgtype == 0xffff )
	{
		fgflags = 0;
		bgtype  = BG_COPY;
		if( conf->background )
		{
			if( !Strnicmp( conf->background, "None", 4 ) )
				bgtype = BG_EMPTY;
			if( !Strnicmp( conf->background, "Copy", 4 ) )
				bgtype = BG_COPY;
			if( !Strnicmp( conf->background, "Trans",5 ) )
				bgtype = BG_TRANS;
		}
		if( conf->showtitle )
		{
			if( *conf->showtitle > 0 )
				fgflags |= FG_TITLES;
		}
		if( conf->recessed )
		{
			if( *conf->recessed == 2 )
				fgflags |= FG_NOBORDER;
		}
	}


	if( (bgtype == BG_COPY) || (bgtype == BG_TRANS ) )
	{
	 /* get background */
	 myBGMap = AllocBitMap( ww, wh, myScreen->RastPort.BitMap->Depth, BMF_MINPLANES, myScreen->RastPort.BitMap);
	 if( myBGMap )
		BltBitMap(myScreen->RastPort.BitMap, Wleft, Wtop, myBGMap, 0,0, ww,wh, 0xC0, 0xFF, NULL );
	}

	{
	 struct TagItem tgs[20];
	 int idx = 0;
	 //extern STRPTR cx_Name;

	 tgs[idx].ti_Tag    = WA_Left;
	 tgs[idx++].ti_Data = Wleft;
	 tgs[idx].ti_Tag    = WA_Top;
	 tgs[idx++].ti_Data = Wtop;
	 tgs[idx].ti_Tag    = WA_Width;
	 tgs[idx++].ti_Data = ww;
	 tgs[idx].ti_Tag    = WA_Height;
	 tgs[idx++].ti_Data = wh;
	 tgs[idx].ti_Tag    = WA_IDCMP; 
	 tgs[idx++].ti_Data = BUTTONIDCMP | IDCMP_CLOSEWINDOW | IDCMP_REFRESHWINDOW | IDCMP_MENUPICK |IDCMP_CHANGEWINDOW;
	 tgs[idx].ti_Tag    = WA_Flags;
	 tgs[idx++].ti_Data = WFLG_BACKDROP | WFLG_SMART_REFRESH | WFLG_BORDERLESS; /* WFLG_CLOSEGADGET | WFLG_DRAGBAR | */
	 tgs[idx].ti_Tag    = WA_Title;
	 tgs[idx++].ti_Data = (ULONG)cx_Name;
	 if( myScreen )
	 {
	  tgs[idx].ti_Tag    = WA_PubScreen;
	  tgs[idx++].ti_Data = (ULONG)myScreen;
	  tgs[idx].ti_Tag    = WA_PubScreenFallBack;
	  tgs[idx++].ti_Data = TRUE;
	 }
	 tgs[idx].ti_Tag = WA_NewLookMenus;
	 tgs[idx++].ti_Data = TRUE;
	 tgs[idx].ti_Tag = TAG_DONE;

	 myWindow = OpenWindowTagList( NULL, tgs );
	}

	myText.FrontPen  = myDrawInfo->dri_Pens[TEXTPEN];
	myText.BackPen   = 0;
	myText.DrawMode  = JAM1;
	myText.ITextFont = ( Topaz.ta_Name ) ? &Topaz : NULL;
	myText.NextText  = NULL;
	myText.IText     = NULL; /* IText is dynamic here */
	myText.LeftEdge  = 0;
	myText.TopEdge   = 0;
	CopyMem(&myText,&myTextShadow,sizeof(struct IntuiText));
	myTextShadow.FrontPen =	myDrawInfo->dri_Pens[SHINEPEN];
	myTextShadow.LeftEdge  = 1;
	myTextShadow.TopEdge   = 1;

	if( conf->textpen )
		myText.FrontPen  = *conf->textpen;
	if( conf->shadowpen )
		myTextShadow.FrontPen  = *conf->shadowpen;
	if( conf->recessed )
		if( *conf->recessed == 0 )
			recessedborder = FALSE;

	if( myWindow )
	{
		if( myBGMap )
			BltBitMapRastPort( myBGMap, 0,0, myWindow->RPort, 0, 0, myWindow->Width, myWindow->Height, 0xc0 );

		mySigmask |= 1L<<myWindow->UserPort->mp_SigBit;

		CopyMem( myWindow->RPort, &myTmpRP, sizeof( struct RastPort ) );
		myTmpRP.Layer = NULL;
		myTmpRP.BitMap= AllocBitMap( Wwidth+32, 1, 8, 0, NULL );
		myTmpBitmapBPR= myTmpRP.BitMap->BytesPerRow;
		myTmpRP.BitMap->BytesPerRow = ((Wwidth+15)>>4)<<1;

		if( bgtype == BG_TRANS )
		{ /* TODO: in FastMem when using P96 */
			LONG i;

	 		myMask = AllocRaster( myWindow->Width, myWindow->Height+1 ); 
			myMaskBPR =  ((myWindow->Width+15)>>4)<<1;
			for( i=0 ; i < UMult32(myMaskBPR,myWindow->Height) ; i++ )
				myMask[i] = 0xff;
		}

		AddMenus( myWindow, conf, myInputList, categories, nsensors );

		dragGadget.LeftEdge = 0;
		dragGadget.TopEdge = 0;
		dragGadget.Width  = myWindow->Width;
		dragGadget.Height = myWindow->Height;//DRAG_HEIGHT;
		dragGadget.Flags = GFLG_GADGHNONE;
		dragGadget.Activation = 0;
		dragGadget.GadgetType = GTYP_WDRAGGING;
		AddGadget(myWindow, &dragGadget, ~0);

		drawSources( myInputList );
	}
	else
	{
		Window_Close( conf, 0, myInputList );
	}

	return (LONG)mySigmask;		/* return signal mask on success */
}

/* signal was raised, check msgport */
LONG Window_Event( struct configvars *conf,  struct MinList *myInputList )
{
	struct IntuiMessage* msg;
	ULONG class,menudata,flg;
	ULONG code,chg;
//	struct Gadget *gad;
	struct MenuItem *it;

	if( !myWindow )
		return 0;

	chg = 0; /* changes to sensor configuration ? */
	while(1)
	{
		msg = GT_GetIMsg(myWindow->UserPort);
		if( !msg )
			break;

		class = msg->Class;
		code  = msg->Code;
//		gad   = msg->IAddress;
		GT_ReplyIMsg(msg);

		switch( class )
		{
			case IDCMP_CLOSEWINDOW:
					Window_Close(conf, 0, myInputList);
					return 0;
					break;
			case IDCMP_CHANGEWINDOW:
					if( (myWindow->LeftEdge != Wleft) || (myWindow->TopEdge != Wtop) )
						Wchg = 1;
					break;
			case IDCMP_MENUPICK:
				while( code != MENUNULL )
				{
					it = ItemAddress( myMenus, code );
					menudata = (ULONG)MENU_USERDATA(it);
					switch( menudata )
					{
						case CMD_ABOUT:
						{
						        ULONG iflags = 0;
							struct EasyStruct libnotfoundES = {
						           sizeof (struct EasyStruct),
					        	   0,
							   PROGNAME,
			       				   PROGNAME " " LIBVERSION "." LIBREVISION "\n"
			       				   "(C) 2021 Henryk Richter \n"
			       				   "I2C Sensor display utility.","OK", };
							EasyRequest( NULL, &libnotfoundES, &iflags );
						}
							break;
						case CMD_HIDE:
							Window_Close(conf,0,myInputList);return 0;break;
						case CMD_QUIT:
							Window_Close(conf,0,myInputList);return -1;break;
						case CMD_W64:
							Wwidth = 64;chg=1;break;
						case CMD_W96:
							Wwidth = 96;chg=1;break;
						case CMD_W128:
							Wwidth = 128;chg=1;break;
						case CMD_W160:
							Wwidth = 160;chg=1;break;
						case CMD_D0:  Sdelay = 0;break;
						case CMD_D5:  Sdelay = 5;break;
						case CMD_D10: Sdelay = 10;break;
						case CMD_D15: Sdelay = 15;break;
						case CMD_D20: Sdelay = 20;break;
						case CMD_BTRANS: bgtype=BG_COPY;chg=1;break;
						case CMD_BNONE:  bgtype=BG_EMPTY;chg=1;break;
						case CMD_BFULLTRANS: bgtype=BG_TRANS;chg=1;break;
						case CMD_SHOWTITLE: 
							if( it->Flags & CHECKED )
								 fgflags |= FG_TITLES;
							else fgflags &= ~(FG_TITLES);
							break;
						case CMD_SAVECONF:
							Window_SaveConfig( conf, myInputList );
							break;
						case CMD_FONTCONF:
							if( Window_SelectFont( conf ) )
								chg=1;
							break;
						default: break;
					}
					if( !(menudata & 0x80000000 ) )
					{
						/* source is a sensor pointer */
						struct InputSource *is = (struct InputSource *)menudata;
						flg = is->flags & ~(ISF_DISABLED);
						if( !(it->Flags & CHECKED))
							flg |= ISF_DISABLED;
						is->flags = flg;
						chg = 1;
					}

					code = it->NextSelect;
				}
				break;
		}
	} /* while(1) */
	if( chg )
	{
		Wchg  = 0;
		Wleft = myWindow->LeftEdge;
		Wtop  = myWindow->TopEdge;
		Window_Close(conf, FLG_WIN_REOPEN, myInputList );
		if( (bgtype == BG_COPY) || (bgtype==BG_TRANS) )
			Delay( 50 );
		return Window_Open(conf,myInputList);
	}

	return (LONG)mySigmask;
}


LONG AddMenus( struct Window *win, struct configvars *conf, struct MinList *lst, LONG ncat, LONG nsensors)
{
	struct NewMenu *nmlist,*curnm;
	LONG ncontents,i,curtype,curw;
	struct InputSource *is;

	ncontents = DEF_ITEMS + ncat + nsensors + 1; /* +1 = NM_END */

	nmlist = (struct NewMenu*)AllocVec( sizeof(struct NewMenu) * ncontents, MEMF_ANY|MEMF_CLEAR ); 
	if( !nmlist )
		return 0;

	/* default menus */
	curnm = nmlist;
	CopyMem( defmenus, curnm, DEF_ITEMS*sizeof(struct NewMenu) );

	/* width defaults reflected in menu */
	curnm += IDX_WIDTHTOGGLE;
	curw   = Wwidth-IDX_WIDTHMIN;
	for( i=0 ; i<IDX_WIDTHCOUNT ; i++ )
	{
		if( !curw )
			curnm->nm_Flags = CHECKED|CHECKIT|MENUTOGGLE;
		else	curnm->nm_Flags = CHECKIT|MENUTOGGLE;
		curw -= IDX_WIDTHADVANCE;
		curnm++;
	}

	/* start delay defaults reflected in menu */
	curnm  = nmlist+IDX_DELAYTOGGLE;


	if( Sdelay != 0xffff )
		curw = Sdelay; /* was changed while program ran ? */
	else
		curw   = (conf->startdelay) ? *conf->startdelay : 0;

	for( i=0 ; i < IDX_DELAYCOUNT ; i++ )
	{
		if( !curw )
			curnm->nm_Flags = CHECKED|CHECKIT|MENUTOGGLE;
		else	curnm->nm_Flags = CHECKIT|MENUTOGGLE;
		curw -= IDX_DELAYADVANCE;
		curnm++;
	}

	curnm  = nmlist+IDX_BGMODE;
	if( bgtype == BG_COPY )
		curnm->nm_Flags = CHECKED|CHECKIT|MENUTOGGLE;
	curnm++;
	if( bgtype == BG_EMPTY )
		curnm->nm_Flags = CHECKED|CHECKIT|MENUTOGGLE;
	curnm++;
	if( bgtype == BG_TRANS )
		curnm->nm_Flags = CHECKED|CHECKIT|MENUTOGGLE;


	curnm = nmlist + IDX_TITLES;
	if( fgflags & FG_TITLES )
		curnm->nm_Flags = CHECKED|CHECKIT|MENUTOGGLE;

	curnm = nmlist + DEF_ITEMS;

	/* sensor menus */
	for( i=0 ; i < nTypes ; i++ )
	{
		if( sennum[i] )
		{
			/* set title */
			curnm->nm_Type  = NM_TITLE;
			curnm->nm_Label = (STRPTR)sennames[i];
			curnm++;

			/* add sub-items */
			curtype = senlist[i];
			is = (struct InputSource*)GetHead(lst);
			while( is )
			{
				if( is->type == curtype )
				{
					curnm->nm_Type  = NM_ITEM;
					curnm->nm_Label = is->name;
					curnm->nm_Flags = CHECKIT|MENUTOGGLE;
					if( !(is->flags & ISF_DISABLED) )
						curnm->nm_Flags |= CHECKED;
					curnm->nm_UserData = is;
					curnm++;
				}
				is = (struct InputSource*)GetSucc( is );
			}
		}
	}
	curnm->nm_Type = NM_END;

	myMenus = CreateMenusA( nmlist, NULL );

	FreeVec( nmlist );

	if( myMenus )
	{
	 struct TagItem tgs[2];

	 tgs[0].ti_Tag = GTMN_NewLookMenus;
	 tgs[0].ti_Data= TRUE;
	 tgs[1].ti_Tag = TAG_DONE;
	
	 if( LayoutMenusA( myMenus, myVisualInfo, tgs ) )
	 {
		SetMenuStrip( win , myMenus );
		return 1;
	 }

	 FreeMenus( myMenus );
	 myMenus = NULL;
	}

	return 0;
}


/* destroy window and associated resources */
LONG Window_Close( struct configvars *conf, LONG flags, struct MinList *myInputList )
{
	if( myWindow )
	{
		if( myMask )
			FreeRaster( myMask, myWindow->Width, myWindow->Height+1 );
		myMask = NULL;

		if( myTmpRP.BitMap )
		{
			myTmpRP.BitMap->BytesPerRow = myTmpBitmapBPR;
			FreeBitMap( myTmpRP.BitMap );
		}
		myTmpRP.BitMap = NULL;

		Wleft = myWindow->LeftEdge;
		Wtop  = myWindow->TopEdge;
		if( conf->win_x )
			*(conf->win_x) = myWindow->LeftEdge;
		if( conf->win_y )
			*(conf->win_y) = myWindow->TopEdge;

		ClearMenuStrip( myWindow );
		CloseWindow( myWindow );
		myWindow = NULL;
	}
	FreeMenus( myMenus );
	myMenus = NULL;
	FreeVisualInfo(myVisualInfo);
	myVisualInfo = NULL;

	if( myBGMap )
		FreeBitMap( myBGMap );
	myBGMap = NULL;

	if( !flags & FLG_WIN_REOPEN )
	{
	 if( myInputList )
	 {
	 	/* invalidate pixbuffer */
		struct InputSource *is;
		is = (struct InputSource*)GetHead( myInputList );
		while( is )
		{
			is->pixbuffer_h = 0;
			is = (struct InputSource*)GetSucc( is );
		}
	 }
	 
	 if( myScreen )
	 {
		int i;
		for( i=0 ; i< nPENS ; i++ )
		{
			if( myPens[i].index != -1 )
				ReleasePen( myScreen->ViewPort.ColorMap, myPens[i].index );
			myPens[i].index = -1;
		}

		if( myDrawInfo )
			FreeScreenDrawInfo( myScreen, myDrawInfo );
		myDrawInfo = NULL;

		UnlockPubScreen( NULL, myScreen );
		myScreen = NULL;
	 }
	}

	return 0;
}


LONG Window_Timer(struct configvars *conf, struct MinList *lst )
{
	if(!myWindow)
		return 0;
		
	if( Wchg )
	{
		Wleft = myWindow->LeftEdge;
		Wtop  = myWindow->TopEdge;
		Window_Close(conf, FLG_WIN_REOPEN, lst );
		if( (bgtype == BG_COPY) || (bgtype == BG_TRANS) )
			Delay( 50 );
		Window_Open( conf, lst );
		Wchg = 0;
	}

	drawSources( lst );
	return (LONG)mySigmask;
}

LONG Window_SelectFont( struct configvars *conf )
{
	struct FontRequester *req;
	struct TagItem tgs[4];
	LONG ret=0;
	
	tgs[0].ti_Tag = ASL_Hail;
	tgs[0].ti_Data= (ULONG)"Select Font for Sensei";
	tgs[1].ti_Tag = ASLFO_MaxHeight;
	tgs[1].ti_Data= DEF_HEIGHT-4;
	tgs[2].ti_Tag = (myWindow) ? ASLFO_Window : TAG_IGNORE;
	tgs[2].ti_Data= (ULONG)myWindow;
	tgs[3].ti_Tag = TAG_DONE;

	req = (struct FontRequester *)AllocAslRequest( ASL_FontRequest, tgs );
	if( !req )
		return ret;

	if( AslRequest( (APTR)req, NULL ) )
	{
		CopyMem( &req->fo_Attr, &Topaz, sizeof( struct TextAttr ));
		CopyMem( req->fo_Attr.ta_Name, fontname, 108 );
		Topaz.ta_Name = fontname;

		ret = 1;
	}
	FreeAslRequest( req );

	return ret;
}

void UpdatePens( STRPTR colorspec, LONG penidx )
{
	ULONG val;

	if( !colorspec )
		return;

	/* R<<16 | G<<8 | B */
	val = Hex2LONG( colorspec );

	myPens[penidx].R = (val>>16) & 0xff;
	myPens[penidx].G = (val>>8) & 0xff;
	myPens[penidx].B = val & 0xff;
}


STRPTR ConfDupStr( STRPTR header, STRPTR arg )
{
	LONG len;
	STRPTR ostr,o;

	if( !header )
		return NULL;

	len = StrLen(header) + StrLen(arg);
	ostr = AllocVec( len + 2, MEMF_ANY );
	o = ostr;
	if( !o )
		return NULL;

	o    = StrNCpy( o, header, len );
	if( arg )
	{
		*o++ = '=';
		o    = StrNCpy( o, arg, len );
	}
	*o = 0;

	return ostr;
}

STRPTR ConfDupInt( STRPTR header, LONG arg )
{
 BYTE intstr[16];
 LONG l;

 l = myInt2Str( intstr, 16, arg, 0 );
 intstr[l] = 0;

 return ConfDupStr( header, intstr );
}


LONG Window_SaveConfig( struct configvars *conf, struct MinList *myInputList )
{
 STRPTR  *savevars,*varsptr=NULL;
 LONG   i;
 struct DiskObject *dobj;

 do
 {
	if( !IconBase )
		break; /* futile */

	/* string array (size is same as input config array) */
	savevars = (STRPTR*)AllocVec( sizeof( struct configvars ), MEMF_ANY|MEMF_CLEAR );
	if( !(varsptr  = savevars) )
		break;

	if( (*savevars = ConfDupStr( "DONOTWAIT", NULL )))
		savevars++;

	if( conf->cx_popup )
		if( (*savevars = ConfDupStr( confvarsWB[0].name, conf->cx_popup )))
			savevars++;
	if( conf->cx_popkey )
		if( (*savevars = ConfDupStr( confvarsWB[1].name, conf->cx_popkey)))
			savevars++;
	if( conf->pri )
		if( (*savevars = ConfDupInt( confvarsWB[2].name, *conf->pri )))
	if( conf->quiet )
		if( (*savevars = ConfDupStr( confvarsWB[3].name, NULL)))
			savevars++;
	if( conf->interval )
		if( (*savevars = ConfDupInt( confvarsWB[4].name, *conf->interval )))
			savevars++;
	if( (*savevars = ConfDupInt( confvarsWB[5].name, Wleft )))
		savevars++;
	if( (*savevars = ConfDupInt( confvarsWB[6].name, Wtop )))
		savevars++;
	if( (*savevars = ConfDupInt( confvarsWB[7].name, Wwidth )))
		savevars++;
	if( Topaz.ta_Name ) /* have a specific font name ? */
	{
	 if( (*savevars = ConfDupStr( confvarsWB[8].name, Topaz.ta_Name)))
	 	savevars++;
	 if( (*savevars = ConfDupInt( confvarsWB[9].name, Topaz.ta_YSize )))
		savevars++;
	}

	/* string over disabled IDs */
	{
		struct InputSource *is;
		UBYTE *ostr,*op;
		LONG disablecount = 0;

		/* get number of disabled IDs */
		is = (struct InputSource*)GetHead( myInputList );
		while( is )
		{
			if( is->flags & ISF_DISABLED)
				disablecount++;
			is = (struct InputSource*)GetSucc( is );
		}
		if( disablecount )
		{
			/* output them as string (1 32 bit hex = 8 chars) */
			ostr = (UBYTE*)AllocVec( disablecount * 8 + 2, MEMF_ANY );
			if( (op = ostr) )
			{
				is = (struct InputSource*)GetHead( myInputList );
				while( is )
				{
					if( is->flags & ISF_DISABLED)
					 op = ULong2Hex( op, is->id );
					 
					is = (struct InputSource*)GetSucc( is );
				}
				*op=0;
				
				if( (*savevars = ConfDupStr( confvarsWB[10].name, ostr)))
	 				savevars++;

				FreeVec( ostr );
			}
		}
	}

	/* start delay */
	{
		LONG bla = 0;
		if( conf->startdelay )
			bla = *conf->startdelay;
		if( Sdelay != 0xffff )
			bla = Sdelay; /* was changed while program ran ? */
		if( (*savevars = ConfDupInt( confvarsWB[11].name, bla)))
			savevars++;
	}

	if( bgtype == BG_COPY )
		if( (*savevars = ConfDupStr( confvarsWB[12].name, "Copy" )))
			savevars++;
	if( bgtype == BG_EMPTY )
		if( (*savevars = ConfDupStr( confvarsWB[12].name, "None" )))
			savevars++;
	if( bgtype == BG_TRANS )
		if( (*savevars = ConfDupStr( confvarsWB[12].name, "Trans" )))
			savevars++;
/*	if( conf->background )
		if( (*savevars = ConfDupStr( confvarsWB[12].name, conf->background )))
			savevars++;*/

	if( (*savevars = ConfDupInt( confvarsWB[13].name, fgflags & FG_TITLES )))
		savevars++;

	/* pens */
	if( conf->textpen )
		if( (*savevars = ConfDupInt( confvarsWB[14].name, *conf->textpen )))
			savevars++;
	if( conf->shadowpen )
		if( (*savevars = ConfDupInt( confvarsWB[15].name, *conf->shadowpen )))
			savevars++;
	if( conf->recessed )
		if( (*savevars = ConfDupInt( confvarsWB[16].name, *conf->recessed )))
			savevars++;
	
	/* colors */
	if( conf->colorT )
		if( (*savevars = ConfDupStr( confvarsWB[17].name, conf->colorT )))
			savevars++;
	if( conf->colorV )
		if( (*savevars = ConfDupStr( confvarsWB[18].name, conf->colorV )))
			savevars++;
	if( conf->colorC )
		if( (*savevars = ConfDupStr( confvarsWB[19].name, conf->colorC )))
			savevars++;
	if( conf->colorF )
		if( (*savevars = ConfDupStr( confvarsWB[20].name, conf->colorF )))
			savevars++;
	if( conf->colorP )
		if( (*savevars = ConfDupStr( confvarsWB[21].name, conf->colorP )))
			savevars++;
	if( conf->colorH )
		if( (*savevars = ConfDupStr( confvarsWB[22].name, conf->colorH )))
			savevars++;

	if( conf->boxh )
		if( (*savevars = ConfDupInt( confvarsWB[23].name, *conf->boxh )))
			savevars++;
	



	/* END OF CONFIG OPTIONS */
	*savevars = NULL; /* just to be sure */


	if( conf->diskobj )	/* started from WB with Icon present ? */
		dobj = (struct DiskObject *)conf->diskobj;
	else
	{
		dobj = GetDiskObject( conf->progname );
		if( !dobj )
			dobj =  GetDefDiskObject(WBTOOL);
	}

	if( dobj ) 
	{
		APTR tt = (APTR)dobj->do_ToolTypes;

		dobj->do_ToolTypes = varsptr;
		PutDiskObject(conf->progname, dobj );
		dobj->do_ToolTypes = tt;

		if( !conf->diskobj )
			FreeDiskObject(dobj);
	}


 }
 while(0);

 if( varsptr )
 {
   for( i=0 ; i < sizeof(struct configvars)/sizeof(STRPTR) ; i++ )
   {
	if( varsptr[i] )
	{
		FreeVec( varsptr[i] );
	}
   }
   FreeVec(varsptr);
 }

 return 0;
}


LONG getSourcesHeight( struct configvars *conf, struct MinList *lst )
{
	struct InputSource *is;
	LONG ret = 0;

	is = (struct InputSource*)GetHead( lst );
	while( is )
	{
		if( !(is->flags & ISF_DISABLED) )
		{
			ret += is->h;
			ret += 2*DEF_VSPACE;
		}

		is = (struct InputSource*)GetSucc( is );
	}

	return ret;
}

LONG source_checkdisable( STRPTR src, ULONG id )
{
	ULONG decid;
	SHORT i;
	UBYTE  cur;

	if( !src )
		return 0;

	//Printf("%s\n",(ULONG)src);

	while( *src != 0 )
	{
		for( i=0, decid=0 ; i < 8 ; i++ )
		{
			cur = *src++;
			if( (cur < '0') || (cur > 'F') )
				return 0;
			cur = ( cur < 'A' ) ? cur-'0' : cur-'A'+10;
			decid = (decid<<4) + cur;
		}
		//Printf("%lx\n",decid);
		if( decid == id )
			return 1;
	}

	return 0;
}


/* enumerate input sources */
LONG getSources( struct configvars *conf, struct MinList *lst )
{
	LONG count=0,curcount;
	int i,j;
	struct InputSource *is;
	ULONG id;
	BYTE *unit,*name;

	for( i=0 ; i < nTypes ; i++ )
	{
		curcount = 0;
		sennum[i] = i2c_SensorNum( senlist[i] );
		for( j=0 ; j < sennum[i] ; j++ )
		{
			id = i2c_SensorID( senlist[i], j );

			i2c_ReadSensor( senlist[i], j, &unit, &name); /* name,unit == 0 if failed */

			if( name ) /* sensor available ? */
			{
			 is = (struct InputSource *)AllocVec( sizeof( struct InputSource ), MEMF_ANY|MEMF_CLEAR );
			 if( is )
			 {
				curcount++;
				count++;
				ADDTAIL( lst, is );
				is->id     = id;
				//Printf("ID %lx name %s unit %s\n",id,(ULONG)name,(ULONG)unit);
				is->name   = name;
				is->type   = senlist[i];
				is->unit   = unit;
				is->h      = senheight[i];
				if( conf->boxh )
					if( (*conf->boxh > MIN_HEIGHT) && (*conf->boxh < MAX_HEIGHT))
						is->h = *conf->boxh;
				is->w      = Wwidth;
				is->scale_min = senmin[i];
				is->scale_max = senmax[i];
				is->pen1      = penidx[i];
				is->pen2      = penidx[i];
				if( source_checkdisable( conf->disabledsensors, id ) )
				 is->flags = ISF_DISABLED;

//				is->flags |= ...default...
			 }
			}
		}
		sennum[i] = curcount; /* adjust number of sensors by actually available ones */
	}

	return count;
}

LONG deleteSources( struct MinList *lst )
{
	struct InputSource *is;
	while( (is = (struct InputSource *)GetHead( (struct List*)lst ) ) )
	{
		REMOVE( is );
		if( is->pixbuffer )
			FreeVec( is->pixbuffer );
		if( is->backlog )
			FreeVec( is->backlog );
		FreeVec( is );
	}

	return 0;
}




#define ENDVAL 1000
void updateHist( struct InputSource *is, LONG scaleval )
{
	if( is->backlog_size < Wwidth )
	{
		if( is->backlog )
			FreeVec(is->backlog);
		is->backlog = AllocVec( Wwidth * sizeof( SHORT ), MEMF_ANY|MEMF_CLEAR );
		if( is->backlog )
			is->backlog_size = Wwidth;
		is->backlog_pos = 0;
	}
	if( !is->backlog )
		return;

	/* rescale 0...1000 -> 0..(height-1) */
	scaleval = UMult32( scaleval, is->h );
	scaleval = UDivMod32( scaleval, ENDVAL );
	if( scaleval >= is->h )
		scaleval = is->h - 1;

	/* */
	if( is->backlog_pos < is->backlog_size )
	{
		is->backlog[is->backlog_pos] = scaleval;
		is->backlog_pos++;
	}
	else
	{
		CopyMem( &is->backlog[1], is->backlog, is->backlog_size*sizeof(SHORT));
		is->backlog[is->backlog_size-1] = scaleval;
		is->backlog_pos = is->backlog_size;
	}
}

void drawMaskFromHist( struct InputSource *is, LONG xoff, LONG yoff )
{
	UBYTE *p = myMask,*pb;
	UBYTE px = 0x80;
	SHORT i,*bl,k,j;

	if( !myMask )
		return;
	if( !is->pixbuffer )
		return;
	
	p += UMult32( yoff+(is->pixbuffer_h-1), myMaskBPR );
	p += (xoff>>3);
	xoff &= 7;
	px >>= xoff;

	/* draw upwards */
	bl   = is->backlog;
	for( i=0 ; i < is->backlog_pos ; i++ )
	{
		pb = p;
		k = *bl++;
		for( j=0 ; j <= k ; j++ )
		{
			*pb ^= px;
			pb  -= myMaskBPR;
		}

		px >>= 1;
		if( !px )
		{
			px = 0x80;
			p++;
		}
	}
}

void drawHist( struct InputSource *is, LONG pen1, LONG pen2 )
{
	SHORT i,j,*bl,k;
	UBYTE *pb;
	LONG boff,pendiff;

	pendiff = pen1^pen2;
	pen2    = pen1;

	/* check buffers */
	if( (is->pixbuffer_w < Wwidth) || (is->pixbuffer_h < is->h) )
	{
		if( is->pixbuffer )
			FreeVec(is->pixbuffer);
		j = UMult32( (((Wwidth+15)>>4)<<4) ,is->h);
		is->pixbuffer = AllocVec( j , MEMF_ANY );
		if( is->pixbuffer )
		{
			ULONG pd2,px;

			is->pixbuffer_w = (((Wwidth+15)>>4)<<4);
			is->pixbuffer_h = is->h;

			/* this draws horizontal bars */
			pd2  = myPens[PEN_BG2].index;
			pb = is->pixbuffer;
			px   = pd2;
			for( k=0 ; k < is->pixbuffer_h; k++ )
			{
				for( i=0 ; i < is->pixbuffer_w ; i++ )
				{
					*pb++ = px;
				}
				px ^= pd2;
			}
		}
	}
	if( !is->pixbuffer )
		return;
	
	boff = UMult32(is->pixbuffer_w,(is->pixbuffer_h-1));
	bl   = is->backlog;
	for( i=0 ; i < is->backlog_pos ; i++ )
	{
		pb = is->pixbuffer + boff;
		boff++; /* next pixel */

		k=*bl++;
		for( j=0 ; j <= k ; j++ )
		{
			*pb = (UBYTE)pen1;
			pb -= is->pixbuffer_w;
			pen1 ^= pendiff; /* horizontal line */
		}
		pen1 = pen2; /* start with same color at bottom */
	}
}

/* rescale 16.16 to 0...1000 based on min/max bounds */
LONG rescale( LONG val, LONG scale_min, LONG scale_max )
{

 /* linear redistribution
    scaleval = (val-min) * endval / (max-min)

    -> keep 16.16 in mind, hence (val-min)/((max-min)/endval)
 */
 val -= scale_min;
 if( val < 0 )
 	val = 0;
 
 scale_max -= scale_min;
 scale_max = UDivMod32( scale_max, ENDVAL );
 val = UDivMod32( val, scale_max );

 if( val > ENDVAL )
 	val = ENDVAL;

 return val;
}

/* constrain IText to "Wwidth" */
/* note: "textbuffer" is overwritten */
void Constrained_PrintIText( BYTE *textbuffer, LONG tblen, LONG y, struct IntuiText *Itxt, struct RastPort *rp )
{
	LONG len,textwidth;
	
	Itxt->IText    = textbuffer;
	
	len=0;while( textbuffer[len] != 0 ) len++;

	textwidth = Wwidth+1;
	while( (textwidth > Wwidth) && (len>0) ) 
	{
	  textwidth = IntuiTextLength( Itxt );

	  textbuffer[len] = 0;
	  len--;
	}
	PrintIText( rp, Itxt, DEF_HSPACE, y+DEF_VSPACE );
}


LONG drawSources( struct MinList *lst )
{
	struct InputSource *is;
	LONG ypos=DRAG_HEIGHT,val,scaleval,y2;
	BYTE *unit,*name;
	BYTE textbuffer[64];
	struct TagItem tgs[3];

	tgs[0].ti_Tag = GT_VisualInfo;
	tgs[0].ti_Data= (ULONG)myVisualInfo;

	tgs[1].ti_Tag = TAG_IGNORE;
	if( recessedborder == TRUE )
	{
		tgs[1].ti_Tag = GTBB_Recessed;
		tgs[1].ti_Data= TRUE;
	}
	tgs[2].ti_Tag = TAG_DONE;

	if( !myWindow )
		return 0;

	is = (struct InputSource*)GetHead( lst );
	while( is )
	{
		if( !(is->flags & ISF_DISABLED) )
		{
			val = i2c_ReadSensor( I2C_ID, is->id, &unit, &name );

			scaleval = rescale( val, is->scale_min, is->scale_max );

			updateHist( is, scaleval );
			drawHist( is, myPens[(ULONG)is->pen1].index, myPens[(ULONG)is->pen1].index );

			if( bgtype == BG_TRANS )
				drawMaskFromHist( is, DEF_HSPACE, ypos+DEF_VSPACE );
			
			if( is->pixbuffer )
			{
			 WritePixelArray8( myWindow->RPort, 
			                   DEF_HSPACE, ypos+DEF_VSPACE, 
			                   DEF_HSPACE+is->pixbuffer_w-1, ypos+DEF_VSPACE+is->pixbuffer_h-1,
			                   is->pixbuffer, &myTmpRP );
			 if( bgtype == BG_TRANS )
			 {
				/* draw masked from background map */
				BltMaskBitMapRastPort( myBGMap, DEF_HSPACE, ypos+DEF_VSPACE, myWindow->RPort,
				                       DEF_HSPACE, ypos+DEF_VSPACE, is->pixbuffer_w, is->pixbuffer_h,
						       (ABC|ABNC|ANBC), myMask );
				drawMaskFromHist( is, DEF_HSPACE, ypos+DEF_VSPACE ); /* restore mask */
			 }
			}
			drawHist( is, 0, myPens[PEN_BG2].index ); /* clear again for next call */

			y2 = ypos;
			if( (fgflags & FG_TITLES) && (Topaz.ta_YSize<=(is->h>>1)) )
			{
				StrNCpy( textbuffer, is->name, 63 );
				
				if( bgtype == BG_TRANS )
				{
//				 Constrained_PrintIText( textbuffer, 64, y2-2, &myTextShadow, myWindow->RPort );
				 Constrained_PrintIText( textbuffer, 64, y2,   &myTextShadow, myWindow->RPort );
				}

				Constrained_PrintIText( textbuffer, 64, y2, &myText, myWindow->RPort );
				y2 += Topaz.ta_YSize;
			}
			
			mySNprintf1616( textbuffer, 64, val, NULL /* name */, unit );
			textbuffer[63] = 0;
			
			if( bgtype == BG_TRANS )
			{
//				myTextShadow.LeftEdge-=2;
//				Constrained_PrintIText( textbuffer, 64, y2-2, &myTextShadow, myWindow->RPort );
//				myTextShadow.LeftEdge+=2;
				Constrained_PrintIText( textbuffer, 64, y2, &myTextShadow, myWindow->RPort );
			}
			Constrained_PrintIText( textbuffer, 64, y2, &myText, myWindow->RPort );

			if( !(fgflags & FG_NOBORDER ) )
				DrawBevelBoxA( myWindow->RPort, DEF_HSPACE/2, ypos+DEF_VSPACE/2, 
			                       myWindow->Width-DEF_HSPACE, is->h+DEF_VSPACE/2, tgs);

			ypos += 2*DEF_VSPACE;
			ypos += is->h;
		}
		is=GetSucc(is);

		if( ypos+(DEF_VSPACE*2) >= myWindow->Height )
			break;
	}

	return 1;
}

#endif
