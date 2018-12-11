#ifndef RENDER_CORE_H_INCLUDED
#define RENDER_CORE_H_INCLUDED
#include <d3d9.h>
enum
{
	DISPLAY_MODE_WINDOWED,
};
struct WindowParam
{
	int x;
	int y;
	int width;
	int height;
	int displayMode;

	int aaSamples;
	int hz;

	HWND hWnd;
};

void R_Init();
void R_RegisterRenderFrame();
void R_Shutdown();

void R_HW_InsertFence( IDirect3DQuery9** ppFence );
void R_AddSwapFence( IDirect3DQuery9* swapFence );
void R_UpdateSwapFrame();
void R_FlushSwap();
void R_SyncGPU();

void R_FatalError( const char* msg );
#endif