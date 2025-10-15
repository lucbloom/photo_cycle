#ifndef UNICODE
#define UNICODE
#endif

#include <d2d1.h>
#include <dwrite.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <fstream>
#include <unordered_set>

#include "ImageFileNameLibrary.h"
#include "SettingsDialog.h"

using Microsoft::WRL::ComPtr;
class ScreenSaverWindow;

class App
{
private:
	App(const App&) = delete;
	App& operator=(const App&) = delete;

public:
	static App* instance;

	ComPtr<ID2D1Factory> m_pD2DFactory = nullptr;
	ComPtr<IWICImagingFactory> m_pWICFactory = nullptr;
	ComPtr<IDWriteFactory> m_pDWriteFactory = nullptr;
	ComPtr<IDWriteTextFormat> m_pTextFormat = nullptr;

	std::vector<ScreenSaverWindow> m_Screensavers;
	HWND m_MainWindow = nullptr;
	int m_CurentScreenIndex = 0;

	ImageFileNameLibrary m_Library;

	std::unordered_set<std::wstring> m_Downvoted;
	const std::wstring m_VoteFile = L"votes.txt";

	POINT m_LastMouse = {};
	std::chrono::steady_clock::time_point m_LastMouseMove = std::chrono::steady_clock::now();
	bool m_ShowButtons = false;
	float m_DisplayTimer = 0;
	HHOOK m_MouseHook = nullptr;

	SettingsDialog settings;

	bool m_IsPreview = false;
	bool m_IsConfigDialogMode = false;
	bool m_IsPaused = false;
	bool m_WantsToQuit = false;

	App() { instance = this; }
	~App();
	HRESULT Initialize(HINSTANCE hInstance, const std::wstring& cmd);
	HRESULT CreateDeviceIndependentResources();
	void Update(float deltaTime);
	void OnRender();
	void SetFullscreen(bool fullscreen);
	static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam);
	void StartSwap(bool animate, int offset);
	void TogglePause() { m_IsPaused = !m_IsPaused; }
	void SaveVote(const std::wstring& kind, const std::wstring& path);

	// Load a bitmap from a file
	HRESULT LoadBitmapFromFile(ID2D1RenderTarget* m_pRenderTarget, IWICImagingFactory* pIWICFactory, PCWSTR uri, UINT destinationWidth, UINT destinationHeight, ID2D1Bitmap** ppBitmap);

	void RunMessageLoop();
};

