/*
  utils.h
*/
#ifndef _INC_UTILS_H
#define _INC_UTILS_H

#include <exec/types.h>
#include <exec/lists.h>

LONG myInt2Str( BYTE *buf, LONG maxlen, LONG val, LONG trailers );

/* requires utility.library */
void mySNprintf1616( BYTE *obuf, LONG chars, LONG val, BYTE *name, BYTE*unit );

LONG Hex2LONG( STRPTR hex );
STRPTR ULong2Hex( STRPTR o, ULONG hex );

LONG StrLen( STRPTR str );
STRPTR StrNCpy( STRPTR dest, STRPTR src, LONG max );

void BZero( APTR p, ULONG bytes );

#endif /* _INC_UTILS_H */
