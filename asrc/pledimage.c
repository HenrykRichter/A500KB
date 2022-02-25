/*
  pledimage.c

  Amiga power LED style BOOPSI image

  This image requires 1-3 pens to be sent.

  If the image is in CAPS-Lock mode, then 1 pen is
  sufficient.


*/

#include "pledimage.h"
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

#define MAXPENS 3

struct pled_data {
	SHORT   Pens[MAXPENS+1]; /* -1 terminated */
	struct  Screen *screen;
	USHORT	w;
	USHORT  h;	/* width, height */
	ULONG  newcolors; /* re-render only if colors have changed */

	/* WPA8 */
	struct RastPort tmpRP;
	struct BitMap *tmpBM;
	ULONG  bpr_ori;
	UBYTE  *rndarr;   /* rendered array */
	SHORT  *fsbuffer; /* Dithering buffer */
};


/*
  Render a w*h pixel horizontal gradient image into 8 Bit Pixel buffer.

  The buffer needs to hold stride*h bytes

  pens are a "-1" terminated array

  fsbuf must be able to hold "w+2"*2 shorts

  This function is not fast. Consequently, the result is cached and re-used in the
  draw method.
*/
void pled_renderimage( UBYTE *buffer, USHORT w, USHORT h, SHORT *pens, SHORT stride, SHORT*fsbuf )
{
	SHORT  j,step,pos,qpos,quant,lastE,curE;
	USHORT np;
	UBYTE  *buf;
	SHORT  *fsb1,*fsb2,*fsbt;

	/* get number of pens */
	np = 0;
	while( pens[np] != -1 )
	{
		np++;
	}

	if( (!np) || (!w) || (!h) )
		return;

	fsb1 = fsbuf;
	for( j=0 ; j < (w+2)*2 ; j++ )
		*fsb1++ = 0;

	step  = UDivMod32( 16384, w); /* step size 16384/w (TODO: Bresenham) */
	quant = UDivMod32( 16384, np);

	fsb1  = fsbuf;
	fsb2  = fsbuf+(w+2);

	/* gradient range is in shorts: short 0...16383 */
	while( h-- )
	{
	 buf=buffer;
	 fsbt=fsb1; fsb1=fsb2; fsb2=fsbt; /* swap Floyd-Steinberg buffer rows */
	 buffer += stride; /* next row */

	 for( pos=0,j=0,lastE=0 ; j < w ; j++, pos+=step )
	 {
		curE = (pos+lastE+fsb1[j+1] + (((h^j)&3)<<5) );
//		qpos = curE / quant;
		qpos = SDivMod32( curE-(quant>>1) , (SHORT)quant );
//		Printf("%2lx ",qpos);
//		Printf("%2lx(%2ld;%2ld) ",fsb1[j+1],pos,quant);

		if( qpos >= np )
			qpos = np-1;
		if( qpos < 0 )
			qpos = 0;

		*buf++ = (UBYTE)pens[qpos];
//		Printf("%2lx ",buf[-1]);

		qpos     *= quant;       /* actual pos after dequantization */
		curE      = curE-qpos;   /* quant error */
//		Printf("%2lx ",curE);

		lastE     = (curE*7)>>4; /* 7 right */
		fsb2[j]   = (curE*3)>>4; /* 3 below left */
		fsb2[j+1] = (curE*5)>>4; /* 5 below */
		fsb2[j+2] = (curE)>>4;   /* 1 below right */
	 }
//	 Printf("\n");
	}
}

void pled_DeleteRenderBuffer( struct pled_data *data )
{
	if( data->tmpBM )
	{
		data->tmpBM->BytesPerRow = data->bpr_ori;
		FreeBitMap( data->tmpRP.BitMap );
		data->tmpBM = NULL;
	}

	if( data->rndarr )
	{
		FreeVec( data->rndarr );
		data->rndarr = NULL;
	}

	if( data->fsbuffer )
	{
		FreeVec( data->fsbuffer );
		data->fsbuffer = NULL;
	}
}


ULONG pled_initRenderBuffer( struct pled_data *data)
{
	ULONG ret = 0;

	if( (data->w <= 0) || (data->h <= 0) )
		return 1;

	pled_DeleteRenderBuffer( data );

	/* Floyd-Steinberg dithered rendering */
	data->rndarr   = (UBYTE*)AllocVec( UMult32((SHORT)((data->w+15)&0xFFF0), data->h), MEMF_PUBLIC );
	data->fsbuffer = (SHORT*)AllocVec( (data->w+2)*2 * sizeof(SHORT),MEMF_PUBLIC);

	/* WPA8 */
	data->tmpBM = AllocBitMap( data->w+32 , 1, 8, 0, NULL );
	data->bpr_ori = data->tmpBM->BytesPerRow;
	data->tmpBM->BytesPerRow = ((data->w+15)>>4)<<1;


	if( !(data->rndarr) || !(data->fsbuffer) || !(data->tmpBM) )
		ret = 1;

	return ret;
}


ULONG pled_draw( Class *cls, struct Image *o, struct impDraw *msg )
{
	struct pled_data *data = (struct pled_data*)INST_DATA(cls,o);

	if( !(data->rndarr ) )
		return 1;

	if((data->newcolors))  /* re-render internal array only if colors or size have changed */
	{
		data->newcolors = 0;
		pled_renderimage( data->rndarr, data->w, data->h, data->Pens, ((data->w+15)&~15), data->fsbuffer );
	}

	/* set up temporary RastPort for WPA8 */
	CopyMem( msg->imp_RPort, &data->tmpRP, sizeof( struct RastPort ) );
	data->tmpRP.Layer  = NULL;
	data->tmpRP.BitMap = data->tmpBM;

	WritePixelArray8( msg->imp_RPort,
			  msg->imp_Offset.X+o->LeftEdge, msg->imp_Offset.Y+o->TopEdge,
			  data->w-1+msg->imp_Offset.X+o->LeftEdge, data->h-1+msg->imp_Offset.Y+o->TopEdge,
                          data->rndarr, &data->tmpRP );

	/* I'm abusing the states here... */
	if( msg->imp_State >= IDS_INDETERMINATE )
	{
		/* highlight active LED by checkerboard pattern */
		SHORT st = msg->imp_State-IDS_INDETERMINATE;
		SHORT x,y,dw;
		struct RastPort *rp = msg->imp_RPort;
		const UWORD ditherData[] = { 0x5555, 0xAAAA };
#define SetAfPt(w,p,n)  {(w)->AreaPtrn = p;(w)->AreaPtSz = n;}

		if( st > 2 )
			st = 2;

		dw = data->w/3;
		x  = msg->imp_Offset.X+o->LeftEdge;
		y  = msg->imp_Offset.Y+o->TopEdge;
		while( st-- )
			x+=dw;

		SetAfPt(rp, (UWORD*)ditherData, 1);
		SetDrMd(rp, JAM1);
		SetAPen(rp, 2); /* usually white */
		RectFill(rp, x, y, x+dw-1, y+data->h-1);
		SetAfPt(rp, NULL, 0 );
	}

	/* Debug: text relative to bottom/left of string (uses the default Font in Rastport) */
	if( 0 )
	{
		UBYTE textbuf[32];
		struct RastPort *rp = msg->imp_RPort;
		LONG   l;
		struct DrawInfo *myDrawInfo = msg->imp_DrInfo;

		StrNCpy(textbuf,(STRPTR)"State ",6);
		l=myInt2Str( (BYTE*)textbuf+6,10,msg->imp_State,10);
		textbuf[l+6]=0;

		SetAPen(rp,myDrawInfo->dri_Pens[TEXTPEN]);
		Move(rp,msg->imp_Offset.X+o->LeftEdge, msg->imp_Offset.Y+o->TopEdge+8);
		Text(rp,textbuf,l+6);

	}
	/* Debug: text relative to top/left of string (custom Font by convenient textattr) */
	if( 0 )
	{
		UBYTE textbuf[20];
		struct RastPort *rp = msg->imp_RPort;
		struct IntuiText myText;
		struct DrawInfo *myDrawInfo = msg->imp_DrInfo;
		LONG   l;

		StrNCpy(textbuf,(STRPTR)"State ",6);
		l=myInt2Str( (BYTE*)textbuf+6,10,msg->imp_State,10);
		textbuf[l+6]=0;
		
		myText.IText    = textbuf;
	        myText.FrontPen  = myDrawInfo->dri_Pens[TEXTPEN];
	        myText.BackPen   = 0;
		myText.DrawMode  = JAM1;
	        myText.ITextFont = NULL;
        	myText.NextText  = NULL;
	        myText.LeftEdge  = 0;
        	myText.TopEdge   = 0;

		SetAPen (rp,1);
 		PrintIText( rp, &myText, msg->imp_Offset.X, msg->imp_Offset.Y );//-10 );
	}

	return 0;
}


ULONG pled_set(Class *cls, struct Image *o,struct opSet *msg, BOOL initial)
{
	ULONG ret = 0,nscreen=0;
	struct TagItem *tlist,*ti;
	struct pled_data *data = (struct pled_data*)INST_DATA(cls,o);

	if( !initial )
		ret = DoSuperMethodA( cls, (Object *)o, (Msg)msg );
	
	tlist = msg->ops_AttrList;
	while( (ti = NextTagItem(&tlist)) )
	{
		switch( ti->ti_Tag )
		{
		 case IA_Screen:
		 	data->screen = (struct Screen*)ti->ti_Data;
			nscreen = 1;
			break;
		 case IA_Pens:	/* we need 3 pens in regular mode, 1 pen in Caps mode as SHORT array, terminated by ~0 */
		 	{
			 SHORT *ipens = (SHORT*)ti->ti_Data;
			 nscreen = 0;
			 while( nscreen < MAXPENS )
			 {
			 	data->Pens[nscreen] = *ipens++;
				if( data->Pens[nscreen] == ~0 )
					break;
				nscreen++;
			 }
			 nscreen = 1;
			}
		 	break;
		 case IA_Width:
		 	{
				SHORT nw = (SHORT)ti->ti_Data;
				if( (nw < 0) || (nw > 1024) )
					nw = PLED_W;
				data->w = nw;
				nscreen = 1;
			}
		 	break;
		 case IA_Height:
		 	{
				SHORT nh = (SHORT)ti->ti_Data;
				if( (nh < 0) || (nh > 1024) )
					nh = PLED_H;
				data->h = nh;
				nscreen = 1;
			}
		 	break;
//			IA_Left, IA_Top, IA_Width, IA_Height
#if 0
		 case PLED_Window:
			data->win = (struct Window*)ti->ti_Data;
			nwindow = 1;
			break;
#endif
		 case PLED_Pen1:
		 	data->Pens[0] = ti->ti_Data;
			nscreen = 1;
			break;
		 case PLED_Pen2:
		 	data->Pens[1] = ti->ti_Data;
			nscreen = 1;
			break;
		 case PLED_Pen3:
		 	data->Pens[2] = ti->ti_Data;
			nscreen = 1;
			break;
		 default:
			break;
		}
	}

	if( nscreen )
	{
		data->newcolors = 1; /* re-render internal array only if colors have changed */
		pled_initRenderBuffer( data );
	}
	return ret;
}


ULONG pled_get(Class *cls, struct Image *o,struct opGet *msg )
{
	ULONG ret = 0;
	struct pled_data *data = (struct pled_data*)INST_DATA(cls,o);

	switch( msg->opg_AttrID )
	{
	 case IA_Screen:
	 	*msg->opg_Storage = (ULONG)data->screen;
		break;
	 case IA_Pens:	/* we need 3 pens in regular mode, 1 pen in Caps mode */
	 	break;
	 
#if 0
	 case PLED_Window:
	 	*msg->opg_Storage = (ULONG)data->win;
		break;
#endif
#if 0
	 case PLED_Pen1:
	 	*msg->opg_Storage = data->Pen1;
		break;
	 case PLED_Pen2:
	 	*msg->opg_Storage = data->Pen2;
		break;
	 case PLED_Pen3:
	 	*msg->opg_Storage = data->Pen3;
		break;
#endif
	 default:
		ret = DoSuperMethodA( cls, (Object *)o, (Msg)msg );
		break;
	}

	return ret;
}


ULONG pled_new(Class *cls, struct Image *o,struct opSet *msg)
{
	struct Image *ret = NULL;

	if( (ret = (struct Image *)DoSuperMethodA( cls, (Object *)o, (Msg)msg ) ))
	{
		struct pled_data *data = (struct pled_data*)INST_DATA(cls,ret);
		SHORT i;

		for( i = 0 ;  i < MAXPENS ; i++ )
			data->Pens[i] = i;  /* placeholder default */
		data->Pens[i] = -1;

		data->w = PLED_W;
		data->h = PLED_H;
		data->newcolors = 1;

		pled_initRenderBuffer( data );

		pled_set( cls, ret, msg, TRUE );
	}

	return (ULONG)ret;
}


ULONG pled_dispose( Class *cls, Object *o, Msg msg )
{
	struct pled_data *data = (struct pled_data*)INST_DATA(cls,o);

	pled_DeleteRenderBuffer( data );

	return DoSuperMethodA( cls, o, msg );
}

#define DIRECT_HOOK
#ifdef DIRECT_HOOK
ASM ULONG pled_dispatch( ASMR(a0) Class *cls ASMREG(a0),
                         ASMR(a2) Object *o  ASMREG(a2), 
                         ASMR(a1) Msg msg    ASMREG(a1) )
#else
STDARGS ULONG pled_dispatch(Class *cls,Object *o,Msg msg )
#endif
{
	ULONG ret = 0;

	//Printf("dispatch %lx cls %lx o %lx msg %lx\n",msg->MethodID,(ULONG)cls,(ULONG)o,(ULONG)msg );

	switch( msg->MethodID )
	{
	 case IM_DRAW:
		ret = pled_draw(cls, (struct Image *)o, (struct impDraw *)msg);
		break;

	 case OM_NEW:
		ret = pled_new(cls, (struct Image *)o, (struct opSet *)msg);
		break;

	 case OM_SET:
		ret = pled_set(cls, (struct Image *)o, (struct opSet *)msg, FALSE);
		break;
	 case OM_GET:
		ret = pled_get(cls, (struct Image *)o, (struct opGet *)msg);
		break;
	 case OM_DISPOSE:
		ret = pled_dispose( cls, o, msg );
		break;
	 default:
		ret = DoSuperMethodA( cls, o, msg );
	 	break;
	}

	return ret;
}


Class *init_pledimage_class( void )
{
  Class *cls;

  if( (cls = MakeClass( NULL , (STRPTR)"imageclass" , NULL ,sizeof(struct pled_data), 0 ) ))
  {
#ifdef DIRECT_HOOK
	cls->cl_Dispatcher.h_Entry    = (HOOKFUNC)pled_dispatch;
#else	
 	cls->cl_Dispatcher.h_Entry    = (HOOKFUNC)HookEntry;
#endif
	cls->cl_Dispatcher.h_SubEntry = (HOOKFUNC)pled_dispatch;//pled_dispatch;
  }

  return cls;
}
