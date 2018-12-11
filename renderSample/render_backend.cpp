#include <windows.h>
#include "render_backend_api.h"
#include "thread.h"
#include "system_specfic.h"
#include "wnd_proc.h"
#include "r_d3d.h"
#include "render_core.h"

const BackEndData* g_backendData;

static void* smpData;

void RB_RenderThread( int context )
{
	while( true )
	{
		RB_RenderThreadCircle();
	}
}

static int g_lastGetMessageTime = 0;
void RB_PumpMessageQueue()
{
	MSG msg;

	while( PeekMessage( &msg, g_WinVar.hWnd, 0, 0, PM_NOREMOVE ) )
	{
		g_lastGetMessageTime = Sys_Milliseconds();
		if( GetMessage( &msg, NULL, 0, 0 ) )
		{
			TranslateMessage( &msg );
			DispatchMessage( &msg );
		}
		else
		{
			Sys_SetWin32QuitEvent();
		}
	}
}

static void RB_BeginFrame( const void* data )
{
	g_dxGlobals.device->Clear(0, NULL, D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER, D3DCOLOR_ARGB(255, 0, 0, 255 ), 1.0f, 0 );
	g_dxGlobals.device->BeginScene();
}

static void RB_SwapBuffers()
{
	HRESULT hr;
	DXCALL_HRESULT( hr, g_dxGlobals.swapChain->Present( NULL, NULL, NULL, NULL, 0 ) );
}

static void RB_EndFrame()
{
	g_dxGlobals.device->EndScene();
	HRESULT hr;
	LARGE_INTEGER start, end, cost, frequency;
	QueryPerformanceCounter( &start );
	DXCALL_HRESULT( hr, g_dxGlobals.swapChain->Present( NULL, NULL, NULL, NULL, 0 ) );
	QueryPerformanceCounter( &end );
	cost.QuadPart = end.QuadPart - start.QuadPart;

	QueryPerformanceFrequency( &frequency );
	int timecost = ( cost.QuadPart * 1000000 ) / frequency.QuadPart;
	Com_Printf( "cost %d ticks %d us ", cost.QuadPart, timecost );
	Com_Printf( " %d us\n", timecost );

	R_HW_InsertFence( &g_dxGlobals.swapFence );
	R_AddSwapFence( g_dxGlobals.swapFence );
}

static void RB_Draw3D()
{

}

static void RB_RenderFrame( const void* data )
{
	RB_BeginFrame( data );
	RB_Draw3D();
	RB_EndFrame();
	Sys_RenderCompleted();
}

static bool RB_IsRenderExiting()
{
	return g_dxGlobals.isExiting;
}
void RB_RenderThreadCircle()
{
	do 
	{
		R_UpdateSwapFrame();
		RB_PumpMessageQueue();
		Sys_YieldThread();
		if( RB_IsRenderExiting() )
		{
			return;
		}
		
	} while ( !Sys_CheckBackendEvent() );

	smpData = Sys_RenderSleep();
	if ( smpData )
	{
		if ( Sys_QueryD3DDeviceOkEvent() )
		{
			RB_RenderFrame( smpData );
		}
		else
		{
			Sys_RenderCompleted();
			R_AddSwapFence( g_dxGlobals.swapFence );
		}
	}
}