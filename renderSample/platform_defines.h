#ifndef PLATFORM_DEFINES_H_INCLUDED
#define PLATFORM_DEFINES_H_INCLUDED
#include "com_define.h"

#if defined( WIN32 ) && !defined( WIN64 )
#define SIZEOF_POINTER 4
#endif

#if defined( WIN64 )
#define SIZEOF_POINTER 8
#endif 

#define ARRAY_COUNT( array ) (sizeof( array ) / ( sizeof( array[0] ) * ( sizeof( array ) != SIZEOF_POINTER || sizeof( array[0] ) <= SIZEOF_POINTER ) ) )

#define PROJECT_NAME "RenderingSample"

#define KB	(1024)
#define MB	(1024*1024)

#define THREAD_LOCAL_DEL	__declspec(thread)

#endif