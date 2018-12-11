#ifndef THREAD_H_INCLUDED
#define THREAD_H_INCLUDED
#include <intrin.h>
#include "platform_defines.h"

typedef void (*threadFunction_t)(int threadContext );

enum
{
	THREAD_CONTEXT_MAIN,
	THREAD_CONTEXT_BACKEND,
	THREAD_CONTEXT_COUNT
};

void Sys_CreateThread( threadFunction_t  function , int context );
void Sys_ResumeThread( int context );
bool Sys_SpawnRenderThread( threadFunction_t function );
bool Sys_CheckBackendEvent();
void Sys_WaitBackendEvent();

int Sys_GetCurrentThreadId();
void Sys_InitMainThread();
void Sys_InitThread( int threadContext );
int Sys_RunOnThread( int dstThreadContext, bool (*allowRun)() );
int Sys_RunOnHomeThread();
int Sys_GetFiberContext();
void Sys_YieldThread();

void Sys_SetWin32QuitEvent();
bool Sys_QueryWin32QuitEvent();
void Sys_ClearD3DDeviceOkEvent();
void Sys_SetD3DDeviceOkEvent();
bool Sys_QueryD3DDeviceOkEvent();

bool Sys_IsRenderReady();
bool Sys_IsRenderReady2();
void* Sys_RenderSleep();
void Sys_WakeRender( void* data );
bool Sys_RenderCompleted();
void Sys_NotifyRender();

inline bool Sys_AllowRunAlways()
{
	return true;
}

inline void Sys_WaitInterlockedCompareExchange( volatile_int* destination, int value, int comperand )
{
	while( *destination != comperand || InterlockedCompareExchange( destination, value, comperand ) )
	{

	}
}
#endif