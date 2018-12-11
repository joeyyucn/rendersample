#include <windows.h>
#include "render_cmds.h"
#include "thread.h"
#include "render_core.h"

#define BACKEND_DATA_COUNT 2
static BackEndData s_backEndData[BACKEND_DATA_COUNT];

BackEndData* g_frontEndDataOut;

static uint s_smpFrame;
static void R_WaitRender()
{
	while( !Sys_IsRenderReady() )
	{
		Sleep(0);
	}
}

static void R_WaitRender2()
{
	while( !Sys_IsRenderReady2() )
	{
		Sleep(0);
	}
}

void R_BeginFrame()
{
	g_frontEndDataOut = NULL;
	R_SyncGPU();
	R_ToggleFrame();
}

void R_EndFrame()
{
	R_RegisterRenderFrame();
	R_HandOffToBackend();
}

void R_ToggleFrame()
{
	R_WaitRender2();
	s_smpFrame = ( s_smpFrame + 1 ) % BACKEND_DATA_COUNT;
	g_frontEndDataOut = &s_backEndData[s_smpFrame];
}

void R_HandOffToBackend()
{
	R_WaitRender();
	Sys_WakeRender( g_frontEndDataOut );
}