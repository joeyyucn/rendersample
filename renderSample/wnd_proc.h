#ifndef WND_PROC_H_INCLUDED
#define WND_PROC_H_INCLUDED
#include <windows.h>

#define WINDOW_CLASS_NAME L"Rendering Sample"
void Win_RegisterClass();
LONG WINAPI MainWndProc( HWND hwnd, UINT uMSG, WPARAM wParam, LPARAM lParam );

struct WinVars_t
{
	HINSTANCE hInstance;
	HWND hWnd;
};

extern WinVars_t g_WinVar;
#endif