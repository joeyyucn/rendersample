#include <stddef.h>
#include <windows.h>
#include <assert.h>
#include "render_core.h"
#include "system_specfic.h"
#include "wnd_proc.h"
#include "thread.h"
#include "r_d3d.h"
#include "render_cmds.h"

static volatile long s_frontEndSwapFrame;
static volatile int s_gpuSwapFrame;

static volatile int s_gpuSubmitSwapFrame;
static IDirect3DQuery9* g_gpuSwapFence[8];

DXGlobals g_dxGlobals;

void R_RegisterRenderFrame()
{
	for( ;; )
	{
		if ( s_frontEndSwapFrame - s_gpuSwapFrame <= 2 )
		{
			break;
		}
		Sleep( 1 );
	}
	InterlockedIncrement( &s_frontEndSwapFrame );
}

static HMONITOR R_ChooseMonitor()
{
	POINT pt = {0, 0};
	return MonitorFromPoint( pt , MONITOR_DEFAULTTOPRIMARY );
}

static uint R_ChooseAdapter()
{
	uint foundMonitorIndex = D3DADAPTER_DEFAULT;
	uint adapterCount = 0;
	HMONITOR desiredMonitor;
	HMONITOR adapterMonitor;
	IDirect3D9* pD3D = g_dxGlobals.d3d;

	desiredMonitor = R_ChooseMonitor();
	adapterCount = pD3D->GetAdapterCount();
	for ( uint i = 0; i < adapterCount; i++ )
	{
		if ( desiredMonitor )
		{
			adapterMonitor = pD3D->GetAdapterMonitor( i );
			if( adapterMonitor != desiredMonitor )
			{
				continue;
			}
			else
			{
				foundMonitorIndex = i;
				break;
			}
		}
	}

	return foundMonitorIndex;
}

static void R_PreCreateWindow()
{
	HRESULT hr;
	assert( g_dxGlobals.d3d == NULL );
	
	for(;;)
	{
		if ( g_dxGlobals.d3d == NULL )
		{
			g_dxGlobals.useD3d9Ex = true;
			DXCALL_HRESULT(hr, Direct3DCreate9Ex( D3D_SDK_VERSION, (IDirect3D9Ex**)&g_dxGlobals.d3d ) );
			if ( FAILED( hr ) )
			{
				R_FatalError( "Couldn't initialize DirectX" );
			}
		}

		g_dxGlobals.adapterIndex = R_ChooseAdapter();

		break;
	}
}
static bool R_CreateWindow(WindowParam* param)
{
	DWORD exStyle;
	DWORD style;
	HINSTANCE hinst;
	RECT rc;

	assert( param->displayMode == DISPLAY_MODE_WINDOWED );

	hinst = GetModuleHandle( NULL );
	if ( param->displayMode == DISPLAY_MODE_WINDOWED )
	{
		style = WS_OVERLAPPED | WS_BORDER | WS_CAPTION | WS_SYSMENU;
		exStyle = 0;
	}

	rc.left = 0;
	rc.right = param->width;
	rc.top	 = 0;
	rc.bottom = param->height;
	AdjustWindowRectEx( &rc, style, FALSE, exStyle );
	param->hWnd = CreateWindowEx( exStyle, WINDOW_CLASS_NAME, WINDOW_CLASS_NAME, style, param->x, param->y, rc.right- rc.left, rc.bottom - rc.top, NULL, NULL, hinst, NULL);
	return param->hWnd != NULL;
}

static void R_SetupAntiAliasing( const WindowParam* param )
{
	HRESULT hr;
	DWORD qualityLevel;
	int multiSampleCount = max( 1, param->aaSamples );
	while( multiSampleCount > 1 )
	{
		g_dxGlobals.multiSampleType = static_cast<D3DMULTISAMPLE_TYPE>( multiSampleCount );
		DXCALL_HRESULT( hr, g_dxGlobals.d3d->CheckDeviceMultiSampleType(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, R_BACKBUFFER_FORMAT, TRUE, g_dxGlobals.multiSampleType,  &qualityLevel ) );
		if( SUCCEEDED( hr ) )
		{
			g_dxGlobals.multiSampleQuality = qualityLevel - 1;
			return;
		}
		multiSampleCount--;
	}

	g_dxGlobals.multiSampleType = D3DMULTISAMPLE_NONE;
	g_dxGlobals.multiSampleQuality = 0;
}
static void R_SetD3DPresentParameter( D3DPRESENT_PARAMETERS* d3dpp, const WindowParam* param )
{
	R_SetupAntiAliasing( param );

	memset( d3dpp, 0, sizeof( *d3dpp ) );
	d3dpp->BackBufferWidth = param->width;
	d3dpp->BackBufferHeight = param->height;
	d3dpp->BackBufferFormat = R_BACKBUFFER_FORMAT;
	d3dpp->BackBufferCount = 1;
	//TODO: use anti-aliasing config instead
	d3dpp->MultiSampleType = D3DMULTISAMPLE_NONE;
	d3dpp->MultiSampleQuality = 0;
	d3dpp->SwapEffect = D3DSWAPEFFECT_DISCARD;
	d3dpp->EnableAutoDepthStencil = TRUE;
	d3dpp->AutoDepthStencilFormat = g_dxGlobals.depthStencilFormat;
	d3dpp->PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

	d3dpp->hDeviceWindow = param->hWnd;
	d3dpp->Flags = 0;

	if ( param->displayMode == DISPLAY_MODE_WINDOWED )
	{
		d3dpp->Windowed = true;
		d3dpp->FullScreen_RefreshRateInHz = 0;
	}
	else
	{
		R_FatalError( "Full window mode is not supported now " );
	}
}

static void R_SetD3DDisplayModeEx( D3DDISPLAYMODEEX* d3ddmex, const WindowParam* param )
{
	memset( d3ddmex, 0, sizeof( *d3ddmex ) );

	d3ddmex->Size = sizeof( D3DDISPLAYMODEEX );
	d3ddmex->Width = param->width;
	d3ddmex->Height = param->height;
	d3ddmex->RefreshRate = param->hz;
	d3ddmex->Format = R_BACKBUFFER_FORMAT;
	d3ddmex->ScanLineOrdering = D3DSCANLINEORDERING_PROGRESSIVE;
}

static HRESULT R_CreateDeviceInternal( HWND hwnd, DWORD behavior, D3DPRESENT_PARAMETERS* d3dpp, D3DDISPLAYMODEEX* d3ddmex )
{
	HRESULT hr;
	g_dxGlobals.device = NULL;
	IDirect3DDevice9* device;
	if ( g_dxGlobals.useD3d9Ex )
	{
		DXCALL_HRESULT( hr, static_cast<IDirect3D9Ex*>(g_dxGlobals.d3d)->CreateDeviceEx(g_dxGlobals.adapterIndex, D3DDEVTYPE_HAL, hwnd, behavior, d3dpp, d3dpp->Windowed ? NULL : d3ddmex, (IDirect3DDevice9Ex**)&device ) );
	}

	if( SUCCEEDED( hr ) )
	{
		g_dxGlobals.device = device;
		HRESULT getModeSuccessCode;
		D3DDISPLAYMODE displayMode;
		DXCALL_HRESULT( getModeSuccessCode, g_dxGlobals.d3d->GetAdapterDisplayMode( g_dxGlobals.adapterIndex, &displayMode ) );
		if( SUCCEEDED( getModeSuccessCode ) )
		{
			g_dxGlobals.adapterFullscreenWidth = displayMode.Width;
			g_dxGlobals.adapterFullscreenHeight = displayMode.Height;
		}
		else
		{
			g_dxGlobals.adapterFullscreenHeight = d3dpp->BackBufferHeight;
			g_dxGlobals.adapterFullscreenWidth = d3dpp->BackBufferWidth;

		}

		DXCALL_HRESULT( hr, reinterpret_cast<IDirect3DDevice9Ex*>(g_dxGlobals.device)->ResetEx( d3dpp, d3dpp->Windowed ? NULL : d3ddmex ) );
		if ( SUCCEEDED( hr ) )
		{
			Sleep( 34 );
			DXCALL_HRESULT( hr, reinterpret_cast<IDirect3DDevice9Ex*>(g_dxGlobals.device)->ResetEx( d3dpp, d3dpp->Windowed ? NULL : d3ddmex ) );
		}
	}

	return hr;

}

static bool R_IsDepthStencilFormatOk( D3DFORMAT targetFormat, D3DFORMAT depthStencilFormat )
{
	HRESULT hr;
	DXCALL_HRESULT( hr, g_dxGlobals.d3d->CheckDeviceFormat( g_dxGlobals.adapterIndex, D3DDEVTYPE_HAL, R_FRONTBUFFER_FORMAT, D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_SURFACE, depthStencilFormat ) );
	if ( FAILED( hr ) )
	{
		return false;
	}

	DXCALL_HRESULT( hr, g_dxGlobals.d3d->CheckDepthStencilMatch( g_dxGlobals.adapterIndex, D3DDEVTYPE_HAL, R_FRONTBUFFER_FORMAT, targetFormat, depthStencilFormat ) );
	if( FAILED( hr ) )
	{
		return false;
	}
	return true;
}

static D3DFORMAT R_GetDepthStencilFormat( D3DFORMAT targetFormat )
{
	if ( R_IsDepthStencilFormatOk( targetFormat, D3DFMT_D24FS8 ) )
	{
		return D3DFMT_D24FS8;
	}
	return D3DFMT_D24S8;
}

static DWORD R_GetD3DDeviceBehavior()
{
	D3DCAPS9 caps;
	HRESULT hr;
	DWORD behavior = 0;
	DXCALL_HRESULT( hr, g_dxGlobals.d3d->GetDeviceCaps( g_dxGlobals.adapterIndex, D3DDEVTYPE_HAL, &caps ) );
	if ( SUCCEEDED( hr ) && caps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT  )
	{
		behavior |= D3DCREATE_HARDWARE_VERTEXPROCESSING;
	}
	else
	{
		behavior |= D3DCREATE_SOFTWARE_VERTEXPROCESSING;
	}

	behavior |= D3DCREATE_FPU_PRESERVE;
	return behavior;
}
static bool R_CreateDevice( const WindowParam* param )
{
	HRESULT hr;
	DWORD behavior;

	g_dxGlobals.depthStencilFormat = R_GetDepthStencilFormat( R_BACKBUFFER_FORMAT );

	R_SetD3DPresentParameter( &g_dxGlobals.d3dpp, param );

	R_SetD3DDisplayModeEx( &g_dxGlobals.d3ddmex, param );

	behavior = R_GetD3DDeviceBehavior();

	hr = R_CreateDeviceInternal( param->hWnd, behavior, &g_dxGlobals.d3dpp, &g_dxGlobals.d3ddmex );
	
	if ( FAILED( hr ) ) 
	{
		R_FatalError( "Could not create a Direct3D device ");
	}
	return true;
}

static void R_FinishAttachingToWindow( const WindowParam* param )
{
	HRESULT hr;
	DXCALL_HRESULT( hr, g_dxGlobals.device->GetSwapChain(0, &g_dxGlobals.swapChain) );
	if( FAILED( hr ) )
	{
		R_FatalError("Failed get device swap chain");
	}
	g_dxGlobals.hwnd = param->hWnd;
	g_dxGlobals.height = param->height;
	g_dxGlobals.width = param->width;
}

static bool R_CreateD3DFencePool()
{
	HRESULT hr;
	for ( uint i = 0; i < ARRAY_COUNT( g_dxGlobals.fencePool ); i++ )
	{
		DXCALL_HRESULT( hr, g_dxGlobals.device->CreateQuery( D3DQUERYTYPE_EVENT, &g_dxGlobals.fencePool[i] ) );
		if ( FAILED( hr ) )
		{
			Com_Printf("Failed create D3D query\n");
			return false;
		}
	}
	return true;
}
static bool R_InitHardware( const WindowParam* param )
{
	
	if ( !R_CreateDevice( param ) )
	{
		return false;
	}

	if ( !R_CreateD3DFencePool() )
	{
		return false;
	}

	R_FinishAttachingToWindow( param );
	return true;
}

static void R_ShutdownDirect3D()
{
	R_FlushSwap();
	if ( IsWindow( g_dxGlobals.hwnd ) )
	{
		DestroyWindow( g_dxGlobals.hwnd );
		g_dxGlobals.hwnd = NULL;
	}

	g_dxGlobals.isExiting = true;
	for ( uint i = 0; i < ARRAY_COUNT( g_dxGlobals.fencePool ); i++ )
	{
		DXRELEASE_AND_SET_NULL( g_dxGlobals.fencePool[i] );
	}
	DXRELEASE_AND_SET_NULL( g_dxGlobals.swapChain )
	DXRELEASE_AND_SET_NULL( g_dxGlobals.device );
	DXRELEASE_AND_SET_NULL( g_dxGlobals.d3d );
}

static bool R_CreateGameWindow()
{
	WindowParam param;
	param.x	 = 0;
	param.y = 0;
	param.width = 600;
	param.height = 480;
	param.displayMode = DISPLAY_MODE_WINDOWED;
	param.aaSamples = 4;
	param.hz = 60;

	if ( !R_CreateWindow( &param ) )
	{
		return false;
	}

	if ( !R_InitHardware( &param ) )
	{
		return false;
	}
	g_WinVar.hWnd = param.hWnd;
	ShowWindow( param.hWnd, SW_SHOW );
	return true;
}

static bool R_InitGraphicsAPI()
{
	g_dxGlobals.d3d = NULL;
	R_PreCreateWindow();
	return R_CreateGameWindow();
}

void R_FatalError( const char* msg )
{
	MessageBoxA( GetActiveWindow(), msg, "FATAL", MB_OK | MB_ICONERROR );
	exit(0);
}

void R_Init()
{
	int previousContext = Sys_RunOnThread( THREAD_CONTEXT_BACKEND, Sys_AllowRunAlways);
	if ( !R_InitGraphicsAPI() )
	{
		R_FatalError( "Failed init Graphics API\n" );
	}
	Sys_SetD3DDeviceOkEvent();
	Sys_RunOnThread( previousContext, Sys_AllowRunAlways );
}

void R_Shutdown()
{
	int previousContext = Sys_RunOnThread( THREAD_CONTEXT_BACKEND, Sys_AllowRunAlways);
	Sys_ClearD3DDeviceOkEvent();
	R_ShutdownDirect3D();
	Sys_RunOnThread( previousContext, Sys_AllowRunAlways );
}

void R_HW_InsertFence( IDirect3DQuery9** ppFence )
{
	*ppFence = g_dxGlobals.fencePool[g_dxGlobals.nextFence];
	g_dxGlobals.nextFence = ( g_dxGlobals.nextFence + 1 ) % ARRAY_COUNT( g_dxGlobals.fencePool );
	DXCALL( (*ppFence)->Issue( D3DISSUE_END ) );
}

bool R_HW_IsFencePending( IDirect3DQuery9* swapFence )
{
	HRESULT hr;
	DWORD data;

	if ( swapFence == NULL )
	{
		return false;
	}
	DXCALL_HRESULT( hr, swapFence->GetData( &data, sizeof(data), D3DGETDATA_FLUSH ) );
	
	if( hr == D3DERR_DEVICELOST )
	{
		return false;
	}

	return (hr == S_FALSE);
}
void R_AddSwapFence( IDirect3DQuery9* swapFence )
{
	g_gpuSwapFence[s_gpuSubmitSwapFrame & 7] = swapFence;
	s_gpuSubmitSwapFrame++;
}

void R_UpdateSwapFrame()
{
	int gpuSwapFrame;
	for(;;)
	{
		gpuSwapFrame = s_gpuSwapFrame;
		if( gpuSwapFrame == s_gpuSubmitSwapFrame )
		{
			break;
		}

		assert( ( s_gpuSubmitSwapFrame - s_gpuSwapFrame ) < ARRAY_COUNT( g_gpuSwapFence ) );
		if( R_HW_IsFencePending( g_gpuSwapFence[gpuSwapFrame&7] ) )
		{
			if ( s_gpuSwapFrame != gpuSwapFrame )
			{
				continue;
			}
			break;
		}

		InterlockedCompareExchange( (volatile_int*)&s_gpuSwapFrame, gpuSwapFrame+1, gpuSwapFrame );
	}
}

void R_FlushSwap()
{
	R_UpdateSwapFrame();

	while( s_gpuSwapFrame != s_gpuSubmitSwapFrame )
	{
		Sleep(1);
		R_UpdateSwapFrame();
	}

	s_gpuSubmitSwapFrame = s_frontEndSwapFrame;
	s_gpuSwapFrame = s_gpuSubmitSwapFrame;
}

bool R_FinishGpuSync()
{
	return Sys_IsRenderReady2() && ( s_frontEndSwapFrame - s_gpuSwapFrame ) <= 1;
}

void R_SyncGPU()
{
	while( !R_FinishGpuSync() )
	{

	}
}