/*
  pledbutton.c

  (C) 2022 Henryk Richter

  Amiga power LED style BOOPSI gadget

  This gadget is a button that responds
  to three segments.

  My thanks go to Thomas Rapp for his Boopsi
  example sources.
*/

#include "pledbutton.h"
#include "compiler.h"
#include "utils.h"

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/utility.h>
#include <proto/graphics.h>
#include <proto/intuition.h>

#include <clib/alib_protos.h>
#include <intuition/gadgetclass.h>
#include <intuition/imageclass.h>
#include <intuition/classusr.h>
#include <intuition/intuition.h>

#define MAXPENS 3

struct pled_data {
	ULONG	Mode;

	/* the image: we get it on creation */
	struct Image *LEDImage;

	/* Title */
	STRPTR text;
	ULONG  textchars; /* in characters */
	ULONG  textwidth; /* in pixels */
	struct TextAttr *tattr;

	/* position/size */
	USHORT  x;
	USHORT  y;
	USHORT	w;
	USHORT  h;

	/* active button (if any) */
	SHORT	current_button;
};


ULONG pledb_render( Class *cls, struct Gadget*o, struct gpRender *msg )
{
	struct pled_data *data = INST_DATA(cls,o);
	SHORT  img_x,img_y;
	LONG   state;
	struct Image *img = data->LEDImage;

	if( !img )
		return 0;

	if(1)
	if( data->text )
	{
		struct RastPort *rp = msg->gpr_RPort;
		struct IntuiText myText;
		struct DrawInfo *myDrawInfo = msg->gpr_GInfo->gi_DrInfo;
		LONG x,y;

		myText.IText    =  data->text;
	        myText.FrontPen  = myDrawInfo->dri_Pens[TEXTPEN];
	        myText.BackPen   = 0;
		myText.DrawMode  = JAM2;
	        myText.ITextFont = data->tattr;
        	myText.NextText  = NULL;
	        myText.LeftEdge  = 0;
        	myText.TopEdge   = 0;

		x = data->x + data->w/2 - IntuiTextLength( &myText )/2;
		y = data->y - data->tattr->ta_YSize - 3; /* -3: space for border */

 		SetAPen (rp,1);
		SetAPen (rp,0);
		PrintIText( rp, &myText, x, y );//-10 );
	}

	/* default font in RP: doesn't work as intended in my code when custom font/size 
	   is requested
	*/
	if(0)
	if( data->text )
	{
		struct TextExtent te;
		SHORT x,y;

		TextExtent( msg->gpr_RPort, data->text, data->textchars, &te );
		x = data->x + data->w/2 - te.te_Width/2;
		y = data->y - te.te_Extent.MaxY - 3; /* -3: space for border */

		Move( msg->gpr_RPort, x, y );
		Text( msg->gpr_RPort, data->text, data->textchars );
	}

        /* Center Image */
	img_x = data->x + data->w/2 - img->Width/2;
	img_y = data->y + data->h/2 - img->Height/2;

	if( data->current_button == -1 )
		state = IDS_NORMAL;
	else	state = data->current_button + IDS_INDETERMINATE;

	DrawImageState( msg->gpr_RPort, img, img_x, img_y, state, msg->gpr_GInfo->gi_DrInfo );

	/* Border */
	{
		SHORT x,y;
		ULONG pen1=1,pen2=2;
		struct RastPort *rp = msg->gpr_RPort;

		if( data->current_button >= 0 )
		{
			pen1 = 2;
			pen2 = 1;
		}

		x = data->x;
		y = data->y + data->h;
		Move( rp, x, y );
		SetAPen(rp,pen2);
		y  = data->y;
		Draw( rp, x, y );
		x += data->w;
		Draw( rp, x, y );
		SetAPen(rp,pen1);
		y += data->h;
		Draw( rp, x, y );
		x -= data->w;
		Draw( rp, x, y );
	}

	return 0;
}



ULONG  pledb_set(Class *cls, struct Gadget *o,struct opSet *msg, BOOL initial)
{
	ULONG ret = 0;
	struct TagItem *tlist,*ti;
	struct pled_data *data = (struct pled_data*)INST_DATA(cls,o);

	if( !initial )
		ret = DoSuperMethodA( cls, (Object *)o, (Msg)msg );
	
	tlist = msg->ops_AttrList;
	while( (ti = NextTagItem(&tlist)) )
	{
		switch( ti->ti_Tag )
		{
		 case GA_Image:
		 	data->LEDImage = (struct Image*)ti->ti_Data;
			break;
//	case GA_SelectRender:
		 case GA_Left:
		 	data->x = ti->ti_Data;
		 	break;
		 case GA_Top:
		 	data->y = ti->ti_Data;
		 	break;
		 case GA_Width:
		 	data->w = ti->ti_Data;
		 	break;
		 case GA_Height:
		 	data->h = ti->ti_Data;
		 	break;
		 case GA_Text:
			data->text = (STRPTR)ti->ti_Data;
			if( data->text )
				data->textchars = StrLen(data->text);
			break;
		 case GA_TextAttr:
			data->tattr = (struct TextAttr *)ti->ti_Data;
			break;
		 case PLED_Mode:
		 	data->Mode = ti->ti_Data;
		 default:
			break;
		}
	}

	return ret;
}

ULONG  pledb_get(Class *cls, struct Gadget *o, struct opGet *msg )
{
	ULONG ret = 0;
	struct pled_data *data = (struct pled_data*)INST_DATA(cls,o);

	switch( msg->opg_AttrID )
	{
	 case GA_Image:
	 	*msg->opg_Storage = (ULONG)data->LEDImage;
		break;
	 case PLED_Mode:
	 	*msg->opg_Storage = (ULONG)data->Mode;
//	case GA_SelectRender:
	 default:
	 	ret = DoSuperMethodA( cls, (Object *)o, (Msg)msg );
	 	break;
	}

	return ret;
}

ULONG  pledb_new(Class *cls, Object *o,struct opSet *msg)
{
	struct Gadget *ret = NULL;

	if( (ret = (struct Gadget *)DoSuperMethodA( cls, (Object *)o, (Msg)msg ) ))
	{
		struct pled_data *data = (struct pled_data*)INST_DATA(cls,ret);

		data->current_button = -1;
		data->Mode = PLEDMode_3LED;

		pledb_set( cls, ret, msg, TRUE );
	}

	return (ULONG)ret;
}




ULONG pledb_layout( Class *cls, struct Gadget *o, struct gpLayout *msg )
{
	struct pled_data *data = INST_DATA(cls,o);
	struct Window *win = msg->gpl_GInfo->gi_Window;

	data->x = o->LeftEdge;
	if( o->Flags & GFLG_RELRIGHT )
		data->x += win->Width;

	data->y = o->TopEdge;
	if( o->Flags & GFLG_RELBOTTOM )
		data->y += win->Height;

	data->w = o->Width;
	if( o->Flags & GFLG_RELWIDTH )
		data->w += win->Width;

	data->h = o->Height;
	if( o->Flags & GFLG_RELHEIGHT )
		data->h += win->Height;

	return 0;
}

/*
  mousex - mouse position relative to left edge of gadget
  w      - width of gadget
*/
LONG pledb_checkbuttonsegment( LONG mousex, LONG w )
{
	LONG xhit,xcmp,res;

	/* just check horizontal position for <1/3, <2/3 */
	xhit = (mousex<<1)+mousex; /* 3*x_pos relative to u/l corner of gadget */
	xcmp = w; /* 1/3 */

	res=0;
	if( xhit > xcmp )
	{
		xcmp += w; /* 2/3 */
		res   = 1;
		if( xhit > xcmp )
		{
			res = 2;
#if 0
			xcmp += w; /* 3/3 */
			if( xhit > xcmp )
				res = -1;
#endif
		}
	}

	return res;
}

ULONG pledb_input( Class *cls, struct Gadget*o, struct gpInput *msg )
{
	struct pled_data *data = INST_DATA(cls,o);
	ULONG  ret = GMR_NOREUSE;
        struct InputEvent *event = msg->gpi_IEvent;
        SHORT  button;
        
	if( !event )
		return ret;

	switch (event->ie_Class)
	{
		case IECLASS_TIMER:
			ret = GMR_MEACTIVE;
			break;

		case IECLASS_RAWMOUSE:
			if( event->ie_Code == IECODE_NOBUTTON )
			{
				struct Window *win = msg->gpi_GInfo->gi_Window;
				if( data->Mode == PLEDMode_3LED )
				{
					button = pledb_checkbuttonsegment( win->MouseX - data->x, data->w );
					if( button != data->current_button )
					{
						data->current_button = button;
						RefreshGList (o,win,NULL,1);
					}
				}
				else
					data->current_button = 0;

				ret = GMR_MEACTIVE;
			}
			else
			{
				if( data->current_button >= 0 )
				{
					*msg->gpi_Termination = data->current_button;
					ret = GMR_NOREUSE|GMR_VERIFY;
				}
			}
			break;

		default:
			break;
	}

	return ret;
}



ULONG pledb_hittest( Class *cls, struct Gadget *o, struct gpHitTest *msg )
{
	struct pled_data *data = INST_DATA(cls,o);
	
	if( data->Mode != PLEDMode_3LED )
	{
		data->current_button = 0;
		return GMR_GADGETHIT;
	}

	data->current_button = pledb_checkbuttonsegment( msg->gpht_Mouse.X, data->w ); 

	return (data->current_button != -1) ? GMR_GADGETHIT : 0;
}


ULONG pledb_goactive( Class *cls, struct Gadget *o, struct gpInput *msg)
{
	struct pled_data *data = (struct pled_data*)INST_DATA(cls,o);

	RefreshGList( o, msg->gpi_GInfo->gi_Window, NULL, 1 );
	
	if( data->Mode == PLEDMode_3LED )
		data->current_button = pledb_checkbuttonsegment( msg->gpi_Mouse.X, data->w );
	else	data->current_button = 0;

	return GMR_MEACTIVE;
}


ULONG pledb_goinactive( Class *cls, struct Gadget *o, struct gpGoInactive * msg )
{
	struct pled_data *data = (struct pled_data*)INST_DATA(cls,o);

	data->current_button = -1;

	RefreshGList( o, msg->gpgi_GInfo->gi_Window, NULL, 1 );
	return 0;
}


#if 0
ULONG  pledb_dispose( Class *cls, Object *o, Msg msg )
{
	return DoSuperMethodA( cls, o, msg );
}
#endif


#define DIRECT_HOOK
#ifdef DIRECT_HOOK
ASM ULONG pledb_dispatch( ASMR(a0) Class *cls ASMREG(a0),
                         ASMR(a2) Object *o  ASMREG(a2), 
                         ASMR(a1) Msg msg    ASMREG(a1) )
#else
STDARGS ULONG pledb_dispatch(Class *cls,Object *o,Msg msg )
#endif
{
	ULONG ret = 0;

//	Printf("dispatch %lx cls %lx o %lx msg %lx\n",msg->MethodID,(ULONG)cls,(ULONG)o,(ULONG)msg );

	switch( msg->MethodID )
	{
	 case OM_NEW:
		ret = pledb_new( cls, o, (struct opSet *)msg );
		break;
	 case GM_LAYOUT:
	 	ret = pledb_layout( cls, (struct Gadget*)o, (struct gpLayout *)msg );
		break;
	case GM_HANDLEINPUT:
		ret = pledb_input( cls, (struct Gadget*)o, (struct gpInput *)msg );
		break;
	 case GM_RENDER:
		ret = pledb_render( cls, (struct Gadget*)o, (struct gpRender *)msg);
		break;

	case GM_HITTEST:
		ret = pledb_hittest( cls, (struct Gadget*)o, (struct gpHitTest *)msg );
		break;
	case GM_GOACTIVE:
		ret = pledb_goactive( cls, (struct Gadget*)o, (struct gpInput *)msg );
		break;
	case GM_GOINACTIVE:
		ret = pledb_goinactive( cls, (struct Gadget*)o, (struct gpGoInactive *)msg );
		break;

	 case OM_UPDATE:
	 case OM_SET:
		ret = pledb_set(cls, (struct Gadget *)o, (struct opSet *)msg, FALSE);
		break;
	 case OM_GET:
		ret = pledb_get(cls, (struct Gadget *)o, (struct opGet *)msg);
		break;
	 case OM_DISPOSE:
//		ret = pledb_dispose( cls, o, msg );
	 default:
		ret = DoSuperMethodA( cls, o, msg );
	 	break;
	}

	return ret;
}


Class *init_pledbutton_class( void )
{
  Class *cls;

  if( (cls = MakeClass( NULL , "gadgetclass" , NULL ,sizeof(struct pled_data), 0 ) ))
  {
#ifdef DIRECT_HOOK
	cls->cl_Dispatcher.h_Entry    = (HOOKFUNC)pledb_dispatch;
#else	
 	cls->cl_Dispatcher.h_Entry    = (HOOKFUNC)HookEntry;
#endif
	cls->cl_Dispatcher.h_SubEntry = (HOOKFUNC)pledb_dispatch;//pled_dispatch;
  }

  return cls;
}
