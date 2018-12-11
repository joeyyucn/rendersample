#ifndef COM_MATH_H_INCLUDED
#define COM_MATH_H_INCLUDED
#include "com_define.h"

inline uint CountLeadingZeros( int val )
{
	uint	count;
	int		change;

	count = 0;

	// first check first 16 bit
	// if first 16 bit is non-zero, do not change anything.
	// otherwise means the first 16 bit is 0, change will be 16 and later 16 bit should be testified.
	change = ( !( val & 0xffff0000 ) ) << 4;
	count += change;
	val = val << change;

	// test first 8 bit
	// if first 8 bit is non-zero should go through later test
	// otherwise first 8 bit is zero, change is 8, and the val should be left shift 8 bit
	change = ( !( val & 0xff000000 ) ) << 3;
	count += change;
	val = val << change;

	// check first 4 bit
	change = ( !( val & 0xf0000000 ) ) << 2;
	count += change;
	val = val << change;

	// only 4 bit left, the following test would go through 2bit-2bit 1bit-1bit
	change = ( !( val & 0xc0000000 ) ) << 1;
	count += change;
	val = val << change;

	// 1bit-1bit
	change = ( !( val & 0x80000000 ) );
	count += change;
	val = val << change;

	change = ( ! ( val & 0x80000000 ) );
	count += change;

	return count;
}
#endif
