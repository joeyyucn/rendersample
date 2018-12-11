#include <windows.h>
#include "Resource.h"
#include "wnd_proc.h"
#include "platform_defines.h"
#include "thread.h"
WinVars_t g_WinVar;

void Win_RegisterClass()
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= MainWndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= g_WinVar.hInstance;
	wcex.hIcon			= LoadIcon(g_WinVar.hInstance, MAKEINTRESOURCE(IDI_RENDERSAMPLE));
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName	= NULL;
	wcex.lpszClassName	= WINDOW_CLASS_NAME;
	wcex.hIconSm		= LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	if ( !RegisterClassEx(&wcex) )
	{
		exit(0);
	}
}

LONG WINAPI MainWndProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	switch (message)
	{
	case WM_DESTROY:
		g_WinVar.hWnd = NULL;
		break;
	case WM_CLOSE:
		Sys_SetWin32QuitEvent();
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}