#include <windows.h>
#include <stdio.h>
#include <assert.h>
#include <intrin.h>
#include "platform_defines.h"
#include "thread.h"
#include "com_math.h"
#include "com_define.h"

HANDLE g_threadHandle[THREAD_CONTEXT_COUNT] = {NULL};
threadFunction_t g_threadFuncs[THREAD_CONTEXT_COUNT] = {NULL};
DWORD g_threadIds[THREAD_CONTEXT_COUNT] = {0};

THREAD_LOCAL_DEL int s_tlsThreadContext;
THREAD_LOCAL_DEL bool s_tlsThreadFiberInitialized = false;

static PVOID s_threadFiberScheduler[THREAD_CONTEXT_COUNT];
static PVOID s_threadFiber[THREAD_CONTEXT_COUNT];
static volatile_int s_threadFibersAssigned[THREAD_CONTEXT_COUNT] = {0};
static int s_threadFiberNextThread[THREAD_CONTEXT_COUNT];
static HANDLE s_threadFiberAssignEvent[THREAD_CONTEXT_COUNT];
static HANDLE s_d3dOkEvent;
static HANDLE s_winQuitEvent;

static volatile_int s_lockFiberExchangeAssign = 0;

static void* volatile smpData = NULL;
static volatile_int g_renderWaitCount = 0; 

enum
{
	BACKEND_EVENT_GENERIC,
	BACKEND_EVENT_COUNT,
};

static HANDLE backendEvent[BACKEND_EVENT_COUNT];

static const char* s_threadNames[] = 
{
	"Main thread",
	"Back-end thread",
};
static_assert( ARRAY_COUNT( s_threadNames) == THREAD_CONTEXT_COUNT, "ARRAY_COUNT( s_threadNames) == THREAD_CONTEXT_COUNT" );

const DWORD MS_VC_EXCEPTION = 0x406D1388;  
#pragma pack(push,8)  
typedef struct tagTHREADNAME_INFO  
{  
	DWORD dwType; // Must be 0x1000.  
	LPCSTR szName; // Pointer to name (in user addr space).  
	DWORD dwThreadID; // Thread ID (-1=caller thread).  
	DWORD dwFlags; // Reserved for future use, must be zero.  
} THREADNAME_INFO;  

#pragma pack(pop)  
void SetThreadName(DWORD dwThreadID, const char* threadName) {  
	THREADNAME_INFO info;  
	info.dwType = 0x1000;  
	info.szName = threadName;  
	info.dwThreadID = dwThreadID;  
	info.dwFlags = 0;  
#pragma warning(push)  
#pragma warning(disable: 6320 6322)  
	__try{  
		RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR*)&info);  
	}  
	__except (EXCEPTION_EXECUTE_HANDLER){  
	}  
#pragma warning(pop)  
}  


static DWORD WINAPI Sys_ThreadMain( LPVOID lpThreadParam )  
{
	int threadContext = reinterpret_cast<int>(lpThreadParam);
	Sys_InitThread( threadContext );
	SetThreadName( ~0u, s_threadNames[threadContext] );
	g_threadFuncs[threadContext]( threadContext );
	return 0;
}

void Sys_CreateThread( threadFunction_t function , int context )
{
	g_threadFuncs[context] = function;
	g_threadHandle[context] = CreateThread( NULL, 0, Sys_ThreadMain, (void*)context, CREATE_SUSPENDED, (LPDWORD)g_threadIds[context] );
}

void Sys_ResumeThread( int context )
{
	ResumeThread( g_threadHandle[THREAD_CONTEXT_BACKEND] );
}

bool Sys_CheckBackendEvent()
{
	return WaitForSingleObject( backendEvent[BACKEND_EVENT_GENERIC], 0 ) == WAIT_OBJECT_0;
}

void Sys_WaitBackendEvent()
{
	WaitForSingleObject( backendEvent[BACKEND_EVENT_GENERIC], INFINITE );
}

void Sys_WakeUpRender()
{
	SetEvent( backendEvent[BACKEND_EVENT_GENERIC] );
}

bool Sys_SpawnRenderThread( threadFunction_t function )
{
	s_d3dOkEvent = CreateEvent( NULL, true, false, NULL );
	s_winQuitEvent = CreateEvent( NULL, true, false, NULL );
	backendEvent[BACKEND_EVENT_GENERIC] = CreateEvent(NULL, false, false, NULL );
	Sys_CreateThread( function, THREAD_CONTEXT_BACKEND );
	if ( !g_threadHandle[THREAD_CONTEXT_BACKEND] )
	{
		return false;
	}
	Sys_ResumeThread( THREAD_CONTEXT_BACKEND );
	return true;
}

static void AssignFiberToThread( int fiberContext, int threadContext )
{
	Sys_WaitInterlockedCompareExchange( &s_lockFiberExchangeAssign, 1, 0 );
	_InterlockedOr( &s_threadFibersAssigned[threadContext], ( 1 << fiberContext ) );
	SetEvent( s_threadFiberAssignEvent[threadContext] );
	s_lockFiberExchangeAssign = 0;
}

static void RemoveFiberFromeThread( int fiberContext, int threadContext )
{
	Sys_WaitInterlockedCompareExchange( &s_lockFiberExchangeAssign, 1, 0 );
	_InterlockedAnd( &s_threadFibersAssigned[threadContext], ~( 1 << fiberContext ) );
	if ( s_threadFibersAssigned[threadContext] == 0 )
	{
		ResetEvent( s_threadFiberAssignEvent[threadContext] );
	}
	s_lockFiberExchangeAssign = 0;
}

static void WINAPI FiberScheduler( LPVOID data )
{
	UINT bit = 0;
	
	const int runThreadContext = reinterpret_cast<int>(data);
	int runFiberContext = runThreadContext;
	volatile_int* threadAssigned = &s_threadFibersAssigned[runThreadContext];
	HANDLE* eventHandler = &s_threadFiberAssignEvent[runThreadContext];
	AssignFiberToThread( runFiberContext, runThreadContext );
	while(true)
	{
		int assigned = *threadAssigned;
		if ( !assigned )
		{
			bit = 0;
			WaitForSingleObject( *eventHandler, INFINITE );
			continue;
		}
		
		assigned &= ( 0xffffffff >> bit );
		if( !assigned )
		{
			assigned = *threadAssigned;
		}
		bit = CountLeadingZeros( assigned );
		runFiberContext = 31 - bit;
		SwitchToFiber( s_threadFiber[ runFiberContext ] );
		assert( s_tlsThreadContext == runThreadContext );
		AssignFiberToThread( runFiberContext, s_threadFiberNextThread[runFiberContext] );
		bit = ( bit + 1 ) & 31;
	}
}

static void Sys_startFiberScheduler()
{
	s_threadFiberNextThread[s_tlsThreadContext] = s_tlsThreadContext;
	SwitchToFiber( s_threadFiberScheduler[s_tlsThreadContext] );
}

void Sys_InitMainThread()
{
	g_threadIds[THREAD_CONTEXT_MAIN] = Sys_GetCurrentThreadId();

	HANDLE pseudoHandle;
	HANDLE process;

	process = GetCurrentProcess();
	pseudoHandle = GetCurrentThread();

	DuplicateHandle( process, pseudoHandle, process, &g_threadHandle[THREAD_CONTEXT_MAIN], 0, false, DUPLICATE_SAME_ACCESS );
	
	s_tlsThreadContext = THREAD_CONTEXT_MAIN;

	s_threadFiberScheduler[THREAD_CONTEXT_MAIN] = CreateFiber( 64*KB, FiberScheduler, (LPVOID)THREAD_CONTEXT_MAIN );
	s_threadFiber[THREAD_CONTEXT_MAIN] = ConvertThreadToFiber( &s_threadFiber[THREAD_CONTEXT_MAIN] );
	s_threadFiberAssignEvent[THREAD_CONTEXT_MAIN] = CreateEvent( NULL, true, false, NULL );
	s_tlsThreadFiberInitialized = true;
	Sys_startFiberScheduler();
}

void Sys_InitThread( int threadContext )
{
	s_tlsThreadContext = threadContext;

	s_threadFiberScheduler[threadContext] = CreateFiber( 64*KB, FiberScheduler, (LPVOID)threadContext );
	s_threadFiber[threadContext] = ConvertThreadToFiber( (LPVOID)&s_threadFiber[threadContext] );
	s_threadFiberAssignEvent[threadContext] = CreateEvent( NULL, true, false, NULL );
	s_tlsThreadFiberInitialized = true;
	Sys_startFiberScheduler();
}

int Sys_GetCurrentThreadId()
{
	DWORD threadId;
	threadId = GetCurrentThreadId();
	assert( threadId );
	return threadId;
}

int Sys_GetFiberContext()
{
	PVOID fiberData = GetFiberData();
	int fiberContext = (PVOID*)fiberData - &s_threadFiber[0];
	return fiberContext;
}

int Sys_RunOnThread( int dstThreadContext, bool (*allowRun)() )
{
	if ( dstThreadContext == s_tlsThreadContext )
	{
		return dstThreadContext;
	}

	int srcFiberContext = Sys_GetFiberContext();
	
	RemoveFiberFromeThread( srcFiberContext, s_tlsThreadContext );
	s_threadFiberNextThread[srcFiberContext] = dstThreadContext;
	int previousThread = s_tlsThreadContext;
	do 
	{
		// Same as Sys_YieldThread(), but this is more effcient
		SwitchToFiber( s_threadFiberScheduler[s_tlsThreadContext] );
	} 
	while ( !allowRun() );
	return previousThread;
}

int Sys_RunOnHomeThread()
{
	return Sys_RunOnThread( Sys_GetFiberContext(), Sys_AllowRunAlways );
}

void Sys_YieldThread()
{
	const int srcFiberContext = Sys_GetFiberContext();
	SwitchToFiber( s_threadFiberScheduler[srcFiberContext] );
}

void Sys_SetWin32QuitEvent()
{
	SetEvent( s_winQuitEvent );
}

bool Sys_QueryWin32QuitEvent()
{
	return WaitForSingleObject( s_winQuitEvent, 0 ) == WAIT_OBJECT_0;
}

void Sys_ClearD3DDeviceOkEvent()
{
	ResetEvent( s_d3dOkEvent );
}

void Sys_SetD3DDeviceOkEvent()
{
	SetEvent( s_d3dOkEvent );
}

bool Sys_QueryD3DDeviceOkEvent()
{
	return WaitForSingleObject( s_d3dOkEvent, 0 ) == WAIT_OBJECT_0;
}

void* Sys_RenderSleep()
{
	void* data;

	data = InterlockedExchangePointer( &smpData, NULL );
	return data;
}

bool Sys_IsRenderReady()
{
	return !smpData;
}

bool Sys_IsRenderReady2()
{
	return g_renderWaitCount <= 1;
}

void Sys_WakeRender( void* data )
{
	InterlockedIncrement( &g_renderWaitCount );
	smpData = data;
	SetEvent( backendEvent[BACKEND_EVENT_GENERIC]);
}

bool Sys_RenderCompleted()
{
	return InterlockedDecrement( &g_renderWaitCount ) == 1;
}

void Sys_NotifyRender()
{
	SetEvent( backendEvent[BACKEND_EVENT_GENERIC] );
}