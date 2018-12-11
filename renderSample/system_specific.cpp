#include <windows.h>
#include <stdio.h>
#include <time.h>
#include "system_specfic.h"


static bool s_sysTimebaseInitialized = false;
static int s_timebase = 0;

static void Sys_InitTimebases()
{
	s_timebase = timeGetTime();
	s_sysTimebaseInitialized = true;
}

int Sys_Milliseconds()
{
	if ( !s_sysTimebaseInitialized )
	{
		Sys_InitTimebases();
	}
	return timeGetTime() - s_timebase;
}

#define MAX_PRINT_MSG	8192
extern "C" void Com_Printf(const char* fmt, ... )
{
	va_list argptr;
	char msg[MAX_PRINT_MSG];
	va_start( argptr, fmt );
	_vsnprintf_s( msg, sizeof( msg ), fmt, argptr );
	msg[sizeof(msg) - 1] = '\0';
	va_end( argptr );
	Com_PrintMessage( msg );
}

void Com_PrintMessage( const char* message )
{
	char timestampStr[128];
	time_t timestamp;
	time( &timestamp );
	tm tmTimestamp;
	localtime_s(&tmTimestamp, &timestamp);
	sprintf_s( timestampStr, "[%02d-%02d-%02d] ", tmTimestamp.tm_hour, tmTimestamp.tm_min, tmTimestamp.tm_sec );

	OutputDebugStringA( timestampStr );
	OutputDebugStringA( message );
}