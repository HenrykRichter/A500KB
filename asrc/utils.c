/* 
  Utils.c

  Collection of convenience functions

*/
#include "utils.h"
#define __NOLIBBASE__
#include <proto/utility.h>

/* UMult32 */
extern struct Library *UtilityBase;

void BZero( APTR p, ULONG bytes )
{
 UBYTE *p1 = (UBYTE*)p;

 while( bytes-- )
  *p1++ = 0;
}


LONG StrLen( STRPTR str )
{
	LONG ret = 0;
	if( str )
	{
		while( *str++ )
			ret++;
	}
	return ret;
}

STRPTR StrNCpy( STRPTR dest, STRPTR src, LONG max )
{
	if( !dest )
		return dest;

	while( (*src) && (max--) )
		*dest++ = *src++;
	*dest = 0;

	return dest;
}

LONG Hex2LONG( STRPTR hex )
{
	LONG ret = 0;
	SHORT i;
	ULONG cur;

	if( !hex )
		return 0;
	
	for( i=0 ; i < 8 ; i++ )
	{
		cur = (*hex++);
		if( cur >= 'a' ) cur &= ~32;

		if( (cur < '0') || (cur > 'F') )
			break;

		cur = ( cur < 'A' ) ? cur-'0' : cur-'A'+10;
		ret = (ret<<4) | cur;
	}

	return ret;
}

/* convert ULONG into 8 hex characters
   caution: no closing \0 in here
*/
STRPTR ULong2Hex( STRPTR o, ULONG hex )
{
 const BYTE hexchars[] = {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};
 SHORT i;
 
	 for( i=0 ; i < 8 ; i++ )
	 {
	 	*o++ = hexchars[(hex>>28)&0xf];
	 	hex<<=4;
	 }

	return o;
}


#define MAXLEN 10
LONG myInt2Str( BYTE *buf, LONG maxlen, LONG val, LONG trailers )
{
        BYTE *b = buf;
        LONG a,c,t;

		if( !val )
		{
			*b = '0';
			return 1;
		}


        a = 0;
        if( maxlen < 0 ) /* length <0 -> leading zeros */
        {
                a=1; /* output leading zeros */
                maxlen = -maxlen;
        }
        if( maxlen == 0 )
                maxlen = MAXLEN;

        if( val < 0 )
        {
                val = -val;
                *b++ = '-';
        }

        c=1000000000;
        while( ((b-buf) < maxlen) && (c>0) )
        {
                t = UDivMod32( val, c );
                if( (t) || (a) )
                {
                        *b++ = t+'0';
                        a=1;
                }
                t = UMult32( t, c );
                val -= t;
                c = UDivMod32( c, 10 );
                if( c < trailers )
                        a=1;
        }

        return (LONG)(b-buf);
}

void mySNprintf1616( BYTE *obuf, LONG chars, LONG val, BYTE *name, BYTE*unit )
{
 LONG l,div;

 chars--; /* closing "0" */

 if( name != NULL )
 {
 	while( (*name) && (chars>0) )
 	{
 	 *obuf++ = *name++;
 	 chars--;
 	}
 	if( chars > 0 )
 	 *obuf++ = 0x20;
 }

 /* sign, if applicable */
 if( val < 0 )
 {
  if( chars>0 )
  {
  	chars--;
	*obuf++ = '-';
  }
  val = -val;
 }

 /* value rounding to 0,1 or 2 decimal places */
 if( val >= 65536*100L )
  val += 32768;
 else
 {
  if( val >= 65536L*10L )
   val += 3276;
  else
   val += 327;
 }

 /* */  
 l = myInt2Str( obuf, chars, val>>16, 10 );
 chars -= l;
 obuf  += l;

 if( val < 65536L*100L )
 {
 	if( chars > 0 )
 	{
 	 *obuf++ = '.';
 	 chars--;
 	}
	if( val < 65536L*10L )
	 div = 100;
	else
	 div = 10;
	val  = UMult32(val&65535,div);
	l = myInt2Str( obuf, chars, val>>16, div );
	chars -= l;
	obuf += l;
 }

 if( chars > 0 )
 {
  *obuf++ = 0x20;
  chars--;
 }

 if( unit != NULL )
 {
  while( (*unit) && (chars>0) )
  {
   *obuf++ = *unit++;
   chars--;
  }
 }

 *obuf = 0;
}

