#ifndef R_D3D_INCLUDED
#define R_D3D_INCLUDED
#include <d3d9.h>

#define R_BACKBUFFER_FORMAT	D3DFMT_A8R8G8B8
#define R_FRONTBUFFER_FORMAT D3DFMT_X8R8G8B8

#define DXCALL(fn)		(fn)
#define DXCALL_HRESULT( hr, fn ) hr = (fn)
#define DXRELEASE_AND_SET_NULL( var ) var->Release();\
	var = NULL;

struct DXGlobals
{
	IDirect3D9* d3d;
	bool useD3d9Ex;
	IDirect3DDevice9* device;
	D3DPRESENT_PARAMETERS d3dpp;
	D3DDISPLAYMODEEX d3ddmex;
	D3DMULTISAMPLE_TYPE multiSampleType;
	DWORD multiSampleQuality;
	D3DFORMAT depthStencilFormat;

	UINT adapterIndex;
	int adapterFullscreenWidth;
	int adapterFullscreenHeight;

	IDirect3DQuery9* swapFence;
	IDirect3DQuery9* fencePool[32];
	uint nextFence;

	HWND hwnd;
	IDirect3DSwapChain9* swapChain;
	int width;
	int height;


	bool isExiting;
};

extern DXGlobals g_dxGlobals;
#endif