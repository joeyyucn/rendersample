#ifndef SYSTEM_SPECIFIC_H_INCLUDED
#define SYSTEM_SPECIFIC_H_INCLUDED
int Sys_Milliseconds();

extern "C"
{
	void Com_Printf(const char* fmt, ... );
};

void Com_PrintMessage( const char* message );

#endif