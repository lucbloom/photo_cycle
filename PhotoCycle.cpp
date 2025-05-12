// PhotoCycle.cpp : Defines the entry point for the application.
//

#include "framework.h"
#include "PhotoCycle.h"
#include <string>
#include "TransitionManager.h"
#include "ImageManager.h"

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "Ws2_32.lib")

using namespace Gdiplus;

#define MAX_LOADSTRING 100
#define VK_LALT 0x8000

// Global Variables:
HINSTANCE g_HInstance = NULL; // current instance
HWND g_MainWindow = NULL;
WCHAR g_WindowTitle[MAX_LOADSTRING]; // The title bar text
WCHAR g_WindowClass[MAX_LOADSTRING]; // the main window class name
const DWORD g_WindowStyle = CS_HREDRAW | CS_VREDRAW;
ULONG_PTR g_GDIPlusToken = NULL;
UINT_PTR g_TimerId = 1;
const UINT g_Interval = 5000; // 5 seconds
bool g_IsPreview = false;

TransitionManager g_Renderer;
ImageManager g_ImageLoader; // Change this to your image directory

// Forward declarations of functions included in this code module:
ATOM MyRegisterClass(HINSTANCE hInstance);
BOOL InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK About(HWND, UINT, WPARAM, LPARAM);

void MessageBoxLastError()
{
	DWORD error = GetLastError();
	wchar_t msg[256];
	FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, error, 0, msg, 256, nullptr);
	MessageBoxW(nullptr, msg, L"CreateWindowW Failed", MB_OK);
}

//void SetFullscreen(HWND hWnd, bool fullscreen) {
//	static HMENU savedMenu = nullptr;
//	static RECT savedRect = {};
//
//	if (fullscreen) {
//		// Save current style and window placement
//		savedMenu = GetMenu(hWnd);
//		GetWindowRect(hWnd, &savedRect);
//
//		// Remove borders and title
//		SetWindowLong(hWnd, GWL_STYLE, g_WindowStyle & ~WS_OVERLAPPEDWINDOW);
//
//		// Remove menu bar
//		SetMenu(hWnd, nullptr);
//
//		// Resize to monitor
//		HMONITOR hMonitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
//		MONITORINFO mi = { sizeof(mi) };
//		GetMonitorInfo(hMonitor, &mi);
//		RECT rc = mi.rcMonitor;
//
//		SetWindowPos(hWnd, HWND_TOPMOST,
//			rc.left, rc.top,
//			rc.right - rc.left,
//			rc.bottom - rc.top,
//			SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
//	}
//	else {
//		// Restore style and position
//		SetWindowLong(hWnd, GWL_STYLE, g_WindowStyle);
//		SetWindowPos(hWnd, HWND_NOTOPMOST,
//			savedRect.left, savedRect.top,
//			savedRect.right - savedRect.left,
//			savedRect.bottom - savedRect.top,
//			SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
//
//		// Restore menu if it existed
//		if (savedMenu)
//			SetMenu(hWnd, savedMenu);
//	}
//}

//bool IsFullscreen(HWND hWnd) {
//	DWORD style = GetWindowLong(hWnd, GWL_STYLE);
//
//	// Check if it's borderless (fullscreen should remove WS_OVERLAPPEDWINDOW)
//	if (style & WS_OVERLAPPEDWINDOW) {
//		return false; // If it still has the overlapped window style, it's not fullscreen
//	}
//
//	return true;
//}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
	std::wstring cmd(lpCmdLine);

	if (cmd.find(L"/c") != std::string::npos || cmd.find(L"/C") != std::string::npos) {
		// Launch config dialog or open config.ini in Notepad
		system("notepad.exe config.ini");
		return 0;
	}

	// Perform application initialization:
	g_HInstance = hInstance; // Store instance handle in our global variable

	g_ImageLoader.SetPaths({ L"C:\\Users\\lucbl\\Pictures\\Personal" }, { });

	GdiplusStartupInput gdiplusStartupInput;
	GdiplusStartup(&g_GDIPlusToken, &gdiplusStartupInput, NULL);

	size_t pPos = cmd.find(L"/p");
	if (pPos == std::string::npos) { pPos = cmd.find(L"/P"); }
	g_IsPreview = (pPos != std::string::npos);
	if (g_IsPreview) {
		LPWSTR hwndStr = lpCmdLine + pPos + 3;  // Skip over "/p "
		g_MainWindow = reinterpret_cast<HWND>(_wtoi64(hwndStr)); ;
	}
	else
	{
		// Initialize global strings
		LoadStringW(hInstance, IDS_APP_TITLE, g_WindowTitle, MAX_LOADSTRING);
		LoadStringW(hInstance, IDC_PHOTOCYCLE, g_WindowClass, MAX_LOADSTRING);

		WNDCLASSEXW wcex;
		ZeroMemory(&wcex, sizeof(wcex));
		wcex.cbSize = sizeof(WNDCLASSEX);
		wcex.style = g_WindowStyle;
		wcex.lpfnWndProc = WndProc;
		wcex.cbClsExtra = 0;
		wcex.cbWndExtra = 0;
		wcex.hInstance = hInstance;
		wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_PHOTOCYCLE));
		wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
		wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
		wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_PHOTOCYCLE);
		wcex.lpszClassName = g_WindowClass;
		//wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

		auto r = RegisterClassExW(&wcex);
		if (!r)
		{
			MessageBoxLastError();
			return FALSE;
		}

		g_MainWindow = CreateWindowW(g_WindowClass, g_WindowTitle, WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);
	}

	if (!g_MainWindow)
	{
		MessageBoxLastError();
		return FALSE;
	}

	if (!g_IsPreview)
	{
		ShowWindow(g_MainWindow, nCmdShow);
		UpdateWindow(g_MainWindow);
	}

	const DWORD targetFrameTime = 1000 / 60;  // 60 FPS
	DWORD lastTime = GetTickCount();

	MSG msg;
	while (true) {
		// Peek at the message queue for messages but don't block
		BOOL msgResult = PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE);

		if (msgResult) {
			// If we got a message, process it
			if (msg.message == WM_QUIT) {
				break;  // Exit the loop if we receive a quit message
			}

			::TranslateMessage(&msg);
			::DispatchMessage(&msg);
		}

		// Perform your frame-based updates (e.g., rendering) here
		DWORD currentTime = GetTickCount();
		DWORD frameTime = currentTime - lastTime;

		if (frameTime >= targetFrameTime) {
			g_Renderer.Update(g_MainWindow);  // Frame-based update
			lastTime = currentTime;
		}
		else
		{
			Sleep(targetFrameTime - frameTime - 1);
		}
	}

	return (int)msg.wParam;
}

void Show(HWND hwnd, int offset, bool immediately) {
	auto currentImage = g_ImageLoader.GotoImage(offset);
	if (immediately)
	{
		g_Renderer.SetCurrentImage(currentImage);
	}
	else
	{
		g_Renderer.StartTransition(currentImage);
	}
	InvalidateRect(hwnd, NULL, TRUE);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_CREATE:
		SetTimer(hWnd, g_TimerId, g_Interval, nullptr);
		Show(hWnd, 0, true);
		break;

	case WM_TIMER:
		if (wParam == g_TimerId) {
			Show(hWnd, 1, false);
			InvalidateRect(hWnd, nullptr, TRUE);
		}
		break;

		//case WM_COMMAND:
		//{
		//	int wmId = LOWORD(wParam);
		//	// Parse the menu selections:
		//	switch (wmId)
		//	{
		//	default:
		//		return DefWindowProc(hWnd, message, wParam, lParam);
		//	}
		//}
		//break;

	case WM_KEYDOWN:
		switch (wParam) {
		case VK_LEFT:  // Left arrow key
			Show(hWnd, -1, true);
			// Reset the timer to the start to avoid delay
			SetTimer(hWnd, g_TimerId, g_Interval, nullptr);
			InvalidateRect(hWnd, nullptr, TRUE);
			break;

		case VK_RIGHT:  // Right arrow key
			Show(hWnd, 1, true);
			// Reset the timer to the start to avoid delay
			SetTimer(hWnd, g_TimerId, g_Interval, nullptr);
			InvalidateRect(hWnd, nullptr, TRUE);
			break;
		}
		break;

	case WM_MOUSEMOVE:
		InvalidateRect(hWnd, nullptr, TRUE);
		break;

		//case WM_SYSKEYDOWN:
		//	switch (wParam) {
		//	case VK_RETURN:
		//		if (GetAsyncKeyState(VK_MENU) & VK_LALT) {
		//			// Alt + Enter is pressed
		//			SetFullscreen(hWnd, !IsFullscreen(hWnd));
		//			InvalidateRect(hWnd, nullptr, TRUE);
		//		}
		//		break;
		//	}
		//	break;

		//case WM_SIZE:
		//case WM_WINDOWPOSCHANGED:
		//{
		//	static bool wasFullscreen = false;
		//	bool isFullscreen = IsFullscreen(hWnd);
		//	if (isFullscreen != wasFullscreen) {
		//		wasFullscreen = isFullscreen;
		//		SetFullscreen(hWnd, isFullscreen);
		//	}
		//	break;
		//}

	case WM_PAINT:
		g_Renderer.Draw(hWnd);
		break;

	case WM_DESTROY:
		KillTimer(hWnd, g_TimerId);
		PostQuitMessage(0);
		break;

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}
