#ifndef _INC_BAXTYPES_H_
#define _INC_BAXTYPES_H_

#include <stdint.h>

typedef uint8_t  UInt8;
typedef int8_t   Int8;
typedef uint16_t UInt16;
typedef int16_t  Int16;
typedef uint32_t UInt32;
typedef int32_t  Int32;

#ifdef __AVR__
#define FLASHMEM PROGMEM
#else
#define PROGMEM
#define FLASHMEM
#define pgm_read_byte(a) *(a)
#endif

#endif
