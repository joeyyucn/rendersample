// renderSample.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include <assert.h>
#include "renderSample.h"
#include "render_core.h"
#include "render_backend_api.h"
#include "render_cmds.h"
#include "thread.h"
#include "wnd_proc.h"
#include "com_math.h"

int APIENTRY _tWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPTSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{

	g_WinVar.hInstance = hInstance;

	Sys_InitMainThread();
	Sys_SpawnRenderThread( RB_RenderThread );
	Win_RegisterClass();
	R_Init();

	while(true)
	{
		if ( Sys_QueryWin32QuitEvent() )
		{
			break;
		}
		R_BeginFrame();
		R_EndFrame();
	}

	R_Shutdown();
	return 0;
}

