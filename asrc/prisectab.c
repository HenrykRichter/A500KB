/*
  prisectab.c

  (c) 2022 Henryk Richter

  Generate a table that performs the decision about primary/secondary LED
  color for custom Keyboard

*/
#include <stdio.h>

unsigned char cmdstream[64];
typedef unsigned char UBYTE;
typedef int LONG;
typedef unsigned int ULONG;

UBYTE LED_SRCMAP[32*32][3];
UBYTE LED_DSTMAP[256];

#define LEDB_SRC_INACTIVE 0x8
#define LEDF_SRC_SWAP 0x80
#define LED_ACTIVE 1
#define LED_SECONDARY 2
#define NBITS 7

LONG ledmanager_sendcommands( LONG led );

int main( int argc, char **argv )
{
 int i,prib,secb,swp,flg,act;

 for(i=0; i < 256 ; i++ )
 	LED_DSTMAP[i] = 0;

 swp = 0;
 do
 {
  /* run two times: once without swap, once with swap flag */
  for( secb = -1 ; secb < NBITS ; secb++ )
  {
	for( prib = -1 ; prib < NBITS ; prib++ )
	{
		/* current flag combination = table index */
		flg = swp;
		if( secb >= 0 )
			flg |= (1<<secb);
		if( prib >= 0 )
			flg |= (1<<prib);
		act = 0x0; /* idle, no action (yet) */

		/* note: primary and secondary source may overlap: use primary color if swap flag is unset,
	           secondary color if swap flag is set */
		if( prib == secb )
		{
			/* are both active ? -> else both are inactive and we're idle */
			if( prib >= 0 )
				act = ( swp ) ? LED_SECONDARY : LED_ACTIVE;
		}
		else
		{
			if( prib >= 0 ) /* primary active */
			{
				if( secb < 0 ) /* secondary inactive -> primary, regardless of SWP */
					act = LED_ACTIVE;
				else
				{
					/* primary/secondary decision depends on swap bit */
					if( secb < prib )
					{	/* secondary is < primary, hence primary will light first (unless swp is set) */
						act = (swp) ? LED_SECONDARY : LED_ACTIVE;
					}
					else
					{	/* primary < secondary, hence secondary will light first (unless swp is set) */
						act = (swp) ? LED_ACTIVE : LED_SECONDARY;
					}
				}
			}
			else
			{	/* primary inactive */
				if( secb >= 0 )
				{	/* primary inactive, secondary active -> secondary LED */
					act = LED_SECONDARY;
				}
			}
		}

		LED_DSTMAP[flg] = act;
	}
  }
 
  swp += LEDF_SRC_SWAP;
 }
 while( swp < (2*LEDF_SRC_SWAP) );

 printf("unsigned char prisectab[256] = {\n");
 for( i=0 ; i < 256 ; i++ )
 {
	if( (i>0) && !(i&15) )
		printf("\n");

	printf("%d",LED_DSTMAP[i]);
	if( i < 255 )
		printf(", ");
 }
 printf("\n};\n");

 ledmanager_sendcommands( 0 );

 return 0;
}

/*

*/
LONG ledmanager_sendcommands( LONG led )
{
	UBYTE *cmd = cmdstream;
	LONG  ncmd = 0;
	LONG  act,sec,res,i;

	/* source mapping:
	   if LED_ACTIVE < LED_SECONDARY, then send inverse flag
	   alongside activation mask
	   if both are the same source, then use ACTIVE only
	   if( SECONDARY but not ACTIVE), then send inverse flag,too
	*/
	act    = LED_SRCMAP[led][LED_ACTIVE];
	sec    = LED_SRCMAP[led][LED_SECONDARY];
	if( (sec != LEDB_SRC_INACTIVE) &&               /* if secondary is inactive, we won't need to swap */
	    ( (act < sec) || (act==LEDB_SRC_INACTIVE) ) /* if primary is inactive or the secondary has a higher index, then swap */
	  )
	{
		res = (1<<act) | (1<<sec) | LEDF_SRC_SWAP;
	}
	else
	{	/* secondary is < primary, hence primary will light first */
		res = (1<<act) | (1<<sec);
	}

	return res;
}


