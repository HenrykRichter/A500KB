/*
  capsimage.c

  Amiga capslock style BOOPSI image

  This image requires 1 pen to be sent.

*/

#include "capsimage.h"
#include "compiler.h"
#include "utils.h"

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/utility.h>
#include <proto/graphics.h>
#include <proto/intuition.h>

#include <clib/alib_protos.h>
#include <intuition/imageclass.h>
#include <intuition/classusr.h>
#include <intuition/intuition.h>

/* V40 include compat kludge */
#ifndef IA_Screen
#define IA_Screen  (IA_Dummy + 0x1f)
#endif
#ifndef GA_TextAttr
#define GA_TextAttr	(GA_Dummy+40)
#endif


#define MAXPENS 3

struct caps_data {
	SHORT   Pens[2]; 
	struct  Screen *screen;
	USHORT	w;
	USHORT  h;	/* width, height */
	ULONG  newcolors; /* re-render only if colors have changed */

	/* WPA8 */
	struct RastPort tmpRP;
};

#define SetAfPt(w,p,n)  {(w)->AreaPtrn = p;(w)->AreaPtSz = n;}

// Integer square root (using binary search)
unsigned int isqrt( unsigned int y )
{
	unsigned int L = 0;
	unsigned int M;
	unsigned int R = y + 1;

    while( L != R - 1 )
    {
        M = (L + R) / 2;

		if( UMult32(M,M) <= y )
			L = M;
		else
			R = M;
    }
    return L;
}


ULONG caps_draw( Class *cls, struct Image *o, struct impDraw *msg )
{
	struct caps_data *data = (struct caps_data*)INST_DATA(cls,o);
	struct RastPort *rp = msg->imp_RPort;
	LONG   x,y,r,dx,yc,j,r2,y2;
	const UWORD ditherData[] = { 0x5555, 0xAAAA };

	x  = msg->imp_Offset.X+o->LeftEdge;
	y  = msg->imp_Offset.Y+o->TopEdge;

	if( msg->imp_State >= IDS_SELECTED )
	{
		SetAfPt(rp, (UWORD*)ditherData, 1);
	}
	else
	{
		SetAfPt(rp, NULL, 0 );
	}
		
	SetDrMd(rp, JAM2);
	SetAPen(rp, data->Pens[0]);
	SetBPen(rp, 2);

	r  = (data->h+1)>>1;
	yc = -r;
	x += r; /* center x */
	for( j=0 ; j < data->h ; j++ )
	{
		r2 = SMult32(r,r);
		y2 = SMult32(yc,yc);
		dx = isqrt( r2-y2 );

		RectFill(rp, x-dx, y, x+dx, y); /* includes highlight pattern */
//		Move(rp,x-dx,y); /* alternative: line draw */
//		Draw(rp,x+dx,y);

		yc++;
		y++;
	}

	SetAfPt(rp, NULL, 0 );

	return 0;
}


ULONG caps_set(Class *cls, struct Image *o,struct opSet *msg, BOOL initial)
{
	ULONG ret = 0;//,nscreen=0;
	struct TagItem *tlist,*ti;
	struct caps_data *data = (struct caps_data*)INST_DATA(cls,o);

	if( !initial )
		ret = DoSuperMethodA( cls, (Object *)o, (Msg)msg );
	
	tlist = msg->ops_AttrList;
	while( (ti = NextTagItem(&tlist)) )
	{
		switch( ti->ti_Tag )
		{
		 case IA_Screen:
		 	data->screen = (struct Screen*)ti->ti_Data;
			//nscreen = 1;
			break;
		 case IA_Pens:	/* 1 pen in Caps mode */
		 	{
			 SHORT *ipens = (SHORT*)ti->ti_Data;
		 	 data->Pens[0] = *ipens++;
			 //nscreen = 1;
			}
		 	break;
		 case IA_Width:
		 	{
				SHORT nw = (SHORT)ti->ti_Data;
				if( (nw < 0) || (nw > 1024) )
					nw = CAPS_W;
				data->w = nw;
				//nscreen = 1;
			}
		 	break;
		 case IA_Height:
		 	{
				SHORT nh = (SHORT)ti->ti_Data;
				if( (nh < 0) || (nh > 1024) )
					nh = CAPS_H;
				data->h = nh;
				//nscreen = 1;
			}
		 	break;
		 default:
			break;
		}
	}

	return ret;
}


ULONG caps_get(Class *cls, struct Image *o,struct opGet *msg )
{
	ULONG ret = 0;
	struct caps_data *data = (struct caps_data*)INST_DATA(cls,o);

	switch( msg->opg_AttrID )
	{
	 case IA_Screen:
	 	*msg->opg_Storage = (ULONG)data->screen;
		break;
	 case IA_Pens:	/* 1 pen in Caps mode */
	 	*msg->opg_Storage = (ULONG)data->Pens;
	 	break;
	 default:
		ret = DoSuperMethodA( cls, (Object *)o, (Msg)msg );
		break;
	}

	return ret;
}


ULONG caps_new(Class *cls, struct Image *o,struct opSet *msg)
{
	struct Image *ret = NULL;

	if( (ret = (struct Image *)DoSuperMethodA( cls, (Object *)o, (Msg)msg ) ))
	{
		struct caps_data *data = (struct caps_data*)INST_DATA(cls,ret);

		data->Pens[0] = 3;
		data->Pens[1] = -1;

		data->w = CAPS_W;
		data->h = CAPS_H;

		caps_set( cls, ret, msg, TRUE );
	}

	return (ULONG)ret;
}


ULONG caps_dispose( Class *cls, Object *o, Msg msg )
{
	return DoSuperMethodA( cls, o, msg );
}


#define DIRECT_HOOK
#ifdef DIRECT_HOOK
ASM ULONG caps_dispatch( ASMR(a0) Class *cls ASMREG(a0),
                         ASMR(a2) Object *o  ASMREG(a2), 
                         ASMR(a1) Msg msg    ASMREG(a1) )
#else
STDARGS ULONG caps_dispatch(Class *cls,Object *o,Msg msg )
#endif
{
	ULONG ret = 0;

	//Printf("dispatch %lx cls %lx o %lx msg %lx\n",msg->MethodID,(ULONG)cls,(ULONG)o,(ULONG)msg );

	switch( msg->MethodID )
	{
	 case IM_DRAW:
		ret = caps_draw(cls, (struct Image *)o, (struct impDraw *)msg);
		break;

	 case OM_NEW:
		ret = caps_new(cls, (struct Image *)o, (struct opSet *)msg);
		break;

	 case OM_SET:
		ret = caps_set(cls, (struct Image *)o, (struct opSet *)msg, FALSE);
		break;
	 case OM_GET:
		ret = caps_get(cls, (struct Image *)o, (struct opGet *)msg);
		break;
	 case OM_DISPOSE:
		ret = caps_dispose( cls, o, msg );
		break;
	 default:
		ret = DoSuperMethodA( cls, o, msg );
	 	break;
	}

	return ret;
}


Class *init_capsimage_class( void )
{
  Class *cls;

  if( (cls = MakeClass( NULL , (STRPTR)"imageclass" , NULL ,sizeof(struct caps_data), 0 ) ))
  {
#ifdef DIRECT_HOOK
	cls->cl_Dispatcher.h_Entry    = (HOOKFUNC)caps_dispatch;
#else	
 	cls->cl_Dispatcher.h_Entry    = (HOOKFUNC)HookEntry;
#endif
	cls->cl_Dispatcher.h_SubEntry = (HOOKFUNC)caps_dispatch;
  }

  return cls;
}
