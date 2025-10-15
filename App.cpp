#ifndef UNICODE
#define UNICODE
#endif

#include "App.h"
#include "ScreenSaverWindow.h"

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "Ws2_32.lib")

#ifndef GET_X_LPARAM
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#endif
#ifndef GET_Y_LPARAM
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#endif

//#define HEART L"❤️"
#define THUMB_DOWN L"👎"
#define DOWN_VOTE L"DOWN"
#define LOVE_VOTE L"LOVE"
#define VOTE_BUTTONS_DISPLAY_TIME 3

#define FULLSCREEN_STYLE WS_POPUP

// Custom stream buffer that redirects output to OutputDebugString
class OutputDebugStreamBuf : public std::streambuf {
protected:
	// Override the overflow function to capture characters and send to OutputDebugStringW
	int overflow(int c) override {
		if (c != EOF) {
			char ch = static_cast<char>(c);

			// Convert char to wchar_t using MultiByteToWideChar
			wchar_t wch[2] = { 0 };  // Buffer for single wchar_t character
			MultiByteToWideChar(CP_ACP, 0, &ch, 1, wch, 1);

			OutputDebugString(wch);  // Send the wide character to OutputDebugStringW
		}
		return c;
	}

	// Override the sync function to handle flushing
	int sync() override {
		return 0;
	}
};

// Redirect std::cout to OutputDebugString
class OutputDebugStream : public std::ostream {
public:
	OutputDebugStream() : std::ostream(&buf) {}

private:
	OutputDebugStreamBuf buf;
};

OutputDebugStream out;  // Create a custom output stream

App* App::instance = nullptr;

App::~App()
{
	instance = nullptr;
	for (auto& screenSaver : m_Screensavers) {
		screenSaver.DiscardDeviceResources();
	}

	if (m_MouseHook) {
		UnhookWindowsHookEx(m_MouseHook);
		m_MouseHook = nullptr;
	}
}

//HANDLE hMutex = nullptr;

HRESULT App::Initialize(HINSTANCE hInstance, const std::wstring& cmd) {

	size_t pPos = cmd.find(L"/p");
	if (pPos == std::string::npos) { pPos = cmd.find(L"/P"); }
	m_IsPreview = (pPos != std::string::npos);

	if (m_IsPreview) {
		HANDLE hMutex = CreateMutexA(NULL, FALSE, "MyScreensaverPreviewLock");
		if (GetLastError() == ERROR_ALREADY_EXISTS) {
			return 0; // Another preview already running
		}
	}

	pPos = cmd.find(L"/c");
	if (pPos == std::string::npos) { pPos = cmd.find(L"/C"); }
	m_IsConfigDialogMode = (pPos != std::string::npos);

	//pPos = cmd.find(L"/s");
	//if (pPos == std::string::npos) { pPos = cmd.find(L"/S"); }
	//bool startFullscreen = (pPos != std::string::npos);

	if (m_IsConfigDialogMode)
	{
		settings.Show();
		PostQuitMessage(0);
		return 0;
	}

	m_Library.SetPaths(settings.IncludePaths, settings.ExcludePaths);

	std::wifstream fin(m_VoteFile);
	std::wstring line;
	while (std::getline(fin, line)) {
		if (line.rfind(THUMB_DOWN L" ", 0) == 0 ||
			line.rfind(DOWN_VOTE L" ", 0) == 0) {
			m_Downvoted.insert(line.substr(5));
		}
	}

	HRESULT hr = CreateDeviceIndependentResources();
	if (FAILED(hr)) {
		return hr;
	}

	if (m_IsPreview) {
		auto hwndStr = cmd.c_str() + pPos + 3;  // Skip over "/p "
		m_Screensavers.resize(1);
		m_Screensavers[0].m_hwnd = reinterpret_cast<HWND>(_wtoi64(hwndStr));
		m_Screensavers[0].GetMaximizedRect();
		if (!m_Screensavers[0].m_hwnd) {
			::OutputDebugStringW(L"Failed to create HWND");
			return E_FAIL;
		}
	}
	else
	{
		auto windowClassName = L"Direct2DAppClass";

		// Register window class
		WNDCLASSEX wcex = { sizeof(WNDCLASSEX) };
		wcex.style = CS_HREDRAW | CS_VREDRAW;
		wcex.lpfnWndProc = App::WndProc;
		wcex.cbClsExtra = 0;
		wcex.cbWndExtra = sizeof(LONG_PTR);
		wcex.hInstance = hInstance;
		wcex.hIcon = (HICON)LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_PHOTOCYCLE), IMAGE_ICON, 0, 0, LR_DEFAULTCOLOR);
		wcex.hIconSm = (HICON)LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_PHOTOCYCLE_SMALL), IMAGE_ICON, 0, 0, LR_DEFAULTCOLOR);
		wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
		wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
		wcex.lpszMenuName = nullptr;
		wcex.lpszClassName = windowClassName;
		wcex.hIconSm = LoadIcon(wcex.hInstance, IDI_APPLICATION);

		RegisterClassEx(&wcex);

		m_MainWindow = nullptr;
		DISPLAY_DEVICE dd = {};
		dd.cb = sizeof(dd);
		for (int i = 0; EnumDisplayDevices(NULL, i, &dd, 0); ++i) {
			if (settings.SingleScreen && !(dd.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE)) {
				continue;
			}

			DEVMODE dm = {};
			dm.dmSize = sizeof(dm);
			if (EnumDisplaySettings(dd.DeviceName, ENUM_CURRENT_SETTINGS, &dm)) {
				m_Screensavers.resize(m_Screensavers.size() + 1);
				auto& screen = m_Screensavers.back();
				screen.m_AdapterIndex = i;
				screen.GetMaximizedRect();
				screen.m_hwnd = CreateWindowEx(
					WS_EX_TOOLWINDOW | WS_EX_TOPMOST | (m_MainWindow ? 0 : WS_EX_APPWINDOW),
					wcex.lpszClassName,
					L"Photo Cycle",
					FULLSCREEN_STYLE,// | (isMainWindow ? WS_SYSMENU : 0),
					screen.m_MaximizedRect.left, screen.m_MaximizedRect.top,
					screen.m_MaximizedRect.right, screen.m_MaximizedRect.bottom,
					m_MainWindow,
					nullptr,
					hInstance,
					&screen
				);

				if (!screen.m_hwnd) {
					DWORD err = GetLastError();
					wchar_t buf[512];
					FormatMessageW(
						FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
						nullptr,
						err,
						MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
						buf,
						512,
						nullptr
					);

					wchar_t msg[600];
					swprintf_s(msg, L"Failed to create HWND for device #%d (err=%lu): %s", i, err, buf);
					::OutputDebugStringW(msg);

					return E_FAIL;
				}

				SetWindowLong(screen.m_hwnd, GWL_EXSTYLE,
					GetWindowLong(screen.m_hwnd, GWL_EXSTYLE) & ~WS_EX_NOACTIVATE);

				if (!m_MainWindow) {
					m_MainWindow = screen.m_hwnd; 
				}
			}
		}
	}

	for (auto& screen : m_Screensavers) {
		ShowWindow(screen.m_hwnd, SW_SHOWNORMAL);
		UpdateWindow(screen.m_hwnd);
		screen.LoadSprite(screen.m_CurrentSprite);
		screen.Update(0);
		screen.OnRender();

	}

	//if (!startFullscreen)
	//{
	//	SetFullscreen(false);
	//}

	m_MouseHook = SetWindowsHookExW(WH_MOUSE_LL, LowLevelMouseProc, GetModuleHandleW(nullptr), 0);

	return S_OK;
}

void App::SaveVote(const std::wstring& kind, const std::wstring& path) {
	std::wofstream fout(m_VoteFile, std::ios::app);
	fout << kind << L" " << path << L"\n";
}

// Create resources which are not dependent on the device
HRESULT App::CreateDeviceIndependentResources() {
	HRESULT hr = D2D1CreateFactory(
		D2D1_FACTORY_TYPE_SINGLE_THREADED,
		m_pD2DFactory.GetAddressOf()
	);

	if (SUCCEEDED(hr)) {
		hr = CoCreateInstance(
			CLSID_WICImagingFactory,
			nullptr,
			CLSCTX_INPROC_SERVER,
			IID_PPV_ARGS(m_pWICFactory.GetAddressOf())
		);
	}

	if (SUCCEEDED(hr)) {
		hr = DWriteCreateFactory(
			DWRITE_FACTORY_TYPE_SHARED,
			__uuidof(IDWriteFactory),
			reinterpret_cast<IUnknown**>(m_pDWriteFactory.GetAddressOf())
		);
	}

	if (SUCCEEDED(hr)) {
		hr = m_pDWriteFactory->CreateTextFormat(
			settings.TextFontName.c_str(),
			nullptr,
			settings.FontWeight,
			DWRITE_FONT_STYLE_NORMAL,
			DWRITE_FONT_STRETCH_NORMAL,
			settings.FontSize,
			L"en-us",
			m_pTextFormat.GetAddressOf()
		);
		m_pTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
		m_pTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_FAR);
	}

	return hr;
}

void App::OnRender()
{
	for (auto& screen : m_Screensavers) {
		HRESULT hr = screen.OnRender();
		if (FAILED(hr)) {
			// Handle error
		}
	}
}

void App::SetFullscreen(bool fullscreen)
{
	if (m_IsPreview) {
		return;
	}

	for (int i = 0; i < m_Screensavers.size(); ++i) {
		auto& screen = m_Screensavers[i];
		if (fullscreen) {
			RECT pos = screen.GetMaximizedRect();
			SetWindowLong(screen.m_hwnd, GWL_STYLE, FULLSCREEN_STYLE | WS_VISIBLE);
			SetWindowPos(screen.m_hwnd, HWND_TOP, pos.left, pos.top, pos.right, pos.bottom, SWP_NOZORDER | SWP_FRAMECHANGED);
		}
		else {
			int w = 640, h = 480;
			int x = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
			int y = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;
			int xOff = (int)((i - (m_Screensavers.size() - 1) / 2.f) * w);
			SetWindowLong(screen.m_hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);
			SetWindowPos(screen.m_hwnd, nullptr, x + xOff, y, w, h, SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
		}
	}
}

void App::StartSwap(bool animate, int offset)
{
	if (m_Screensavers.empty()) {
		return;
	}

	if (settings.SyncChange) {
		for (auto& screen : m_Screensavers) {
			screen.StartSwap(animate, offset, (int)m_Screensavers.size());
		}
	}
	else {
		if (offset > 0) { m_CurentScreenIndex += offset; }
		auto n = (int)m_Screensavers.size();
		m_CurentScreenIndex = (m_CurentScreenIndex + n) % n;
		m_Screensavers[(size_t)m_CurentScreenIndex].StartSwap(animate, offset, (int)m_Screensavers.size());
		if (offset < 0) { m_CurentScreenIndex += offset; }
	}

	m_DisplayTimer = settings.DisplayDuration;
}

void App::Update(float deltaTime)
{
	if (std::chrono::steady_clock::now() - m_LastMouseMove > std::chrono::seconds(VOTE_BUTTONS_DISPLAY_TIME)) {
		m_ShowButtons = false;
	}

	if (m_IsPaused)
	{
		return;
	}

	m_DisplayTimer -= deltaTime;
	if (m_DisplayTimer <= 0.0f) {
		StartSwap(true, 1);
	}

	for (auto& screen : m_Screensavers) {
		screen.Update(deltaTime);
	}
}

static void ShowMyCursor(bool show)
{
	if (show)
	{
		while (ShowCursor(TRUE) < 0);
	}
	else
	{
		while (ShowCursor(FALSE) >= 0);
		SetCursor(nullptr);
	}
}

LRESULT CALLBACK App::WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	auto app = App::instance;
	if (message == WM_CREATE) {
		LPCREATESTRUCT pcs = (LPCREATESTRUCT)lParam;
		//ScreenSaverWindow* screen = reinterpret_cast<ScreenSaverWindow*>(pcs->lpCreateParams);
		//SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(screen));
		//screen->m_hwnd = hwnd;
		for (auto& screen : app->m_Screensavers)
		{
			if (screen.m_hwnd == hwnd)
			{
				screen.Update(0);
				screen.OnRender();
			}
		}
		return 1;
	}

	switch (message) {
	case WM_SYSCOMMAND: // Detect maximize button click
		if (wParam == SC_MAXIMIZE) {
			if (app) app->SetFullscreen(true);
		}
		break;

	case WM_SIZE:
		if (app)
		{
			for (auto& screen : app->m_Screensavers) {
				if (screen.m_hwnd == hwnd) {
					screen.OnResize(LOWORD(lParam), HIWORD(lParam));
				}
			}
		}
		break;

	case WM_ACTIVATE:
		if (app)
		{
			if (LOWORD(wParam) != WA_INACTIVE) // this window activated
			{
				for (auto& screen : app->m_Screensavers)
				{
					if (screen.m_hwnd != hwnd)
					{
						// Prevent infinite activation loop by checking if already foreground
						if (GetForegroundWindow() != screen.m_hwnd)
						{
							SetForegroundWindow(screen.m_hwnd);
						}
					}
				}
			}
		}

		ShowMyCursor(LOWORD(wParam) != WA_INACTIVE);
	break;

	case WM_SETFOCUS:
		ShowMyCursor(false);
		break;

	case WM_KILLFOCUS:
		ShowMyCursor(true);
		break;

	case WM_SYSKEYDOWN:
	{
		auto isAltDown = GetKeyState(VK_MENU) & 0x8000;
		if (app && isAltDown && wParam == VK_RETURN) {
			BOOL isFullscreen = !(GetWindowLong(hwnd, GWL_STYLE) & WS_OVERLAPPEDWINDOW);
			app->SetFullscreen(!isFullscreen);
		}
	}
	break;

	case WM_KEYDOWN:
	{
		switch (wParam) {
		case VK_ESCAPE:
			if (app)
			{
				app->m_WantsToQuit = true;
				for (auto& screen : app->m_Screensavers) {
					if (screen.m_hwnd) {
						DestroyWindow(screen.m_hwnd);
					}
				}
			}
			PostQuitMessage(0);
			break;

		case VK_LEFT:
			if (app) app->StartSwap(false, -1);
			break;

		case VK_RIGHT:
			if (app) app->StartSwap(false, 1);
			break;

		case 'F':
			if (app) app->settings.ToggleShowFolder();
			break;

		case 'D':
			if (app) app->settings.ToggleShowDate();
			break;

		case 'L':
			if (app) app->settings.ToggleShowLocation();
			break;

		case 'P':
			if (app) app->TogglePause();
			break;

		case 'C':
			if (app) app->settings.Show();
			break;
		}
	}
	break;

	//case WM_LBUTTONDOWN:
	//	if (app) {
	//		auto screen = reinterpret_cast<ScreenSaverWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
	//		if (screen && screen->m_CurrentSprite && screen->m_CurrentSprite->imageInfo) {
	//			POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
	//			const auto& path = screen->m_CurrentSprite->imageInfo->filePath;
	//			if (PtInRect(&screen->m_LoveButtonRect, pt)) {
	//				app->SaveVote(HEART, path);
	//			}
	//			else if (PtInRect(&screen->m_DownVoteButtonRect, pt)) {
	//				app->SaveVote(THUMB_DOWN, path);
	//				app->m_Downvoted.insert(path);
	//				app->StartSwap(false, 1); // skip immediately
	//			}
	//		}
	//	}
	//break;

	case WM_MOUSEMOVE:
		if (app) {
			POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
			if (pt.x != app->m_LastMouse.x || pt.y != app->m_LastMouse.y) {
				app->m_LastMouse = pt;
				app->m_LastMouseMove = std::chrono::steady_clock::now();
				app->m_ShowButtons = true;
			}
		}
	break;

	//case WM_DISPLAYCHANGE:
	//	InvalidateRect(hwnd, nullptr, FALSE);
	//	break;
	//
	//case WM_PAINT:
	//	ValidateRect(hwnd, nullptr);
	//	break;

	case WM_DESTROY:
		if (app) { app->m_WantsToQuit = true; }
		PostQuitMessage(0);
		break;

	default:
		break;
	}

	return DefWindowProc(hwnd, message, wParam, lParam);
}

LRESULT CALLBACK App::LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (nCode == HC_ACTION) {
		const MSLLHOOKSTRUCT* ms = reinterpret_cast<const MSLLHOOKSTRUCT*>(lParam);

		// Global idle tracking for 5s auto-hide
		if (wParam == WM_MOUSEMOVE) {
			App::instance->m_LastMouseMove = std::chrono::steady_clock::now();
			App::instance->m_ShowButtons = true;
		}

		if (wParam == WM_LBUTTONDOWN) {
			POINT pt = ms->pt; // screen coords
			HWND hwnd = WindowFromPoint(pt);
			for (auto& screen : App::instance->m_Screensavers)
			{
				if (screen.m_hwnd == hwnd && screen.m_CurrentSprite && screen.m_CurrentSprite->imageInfo)
				{
					// Convert to client coords of that window
					POINT cpt = pt;
					ScreenToClient(hwnd, &cpt);

					// Hit-test buttons
					if (PtInRect(&screen.m_LoveButtonRect, cpt)) {
						App::instance->SaveVote(LOVE_VOTE, screen.m_CurrentSprite->imageInfo->filePath);
						//wchar_t buf[2048] = {};
						//wsprintf(buf, L"LOVE!! %s (%d)\n", screen.m_CurrentSprite->imageInfo->filePath.c_str(), screen.m_AdapterIndex);
						//OutputDebugStringW(buf);
						return 1; // consume click
					}
					else if (PtInRect(&screen.m_DownVoteButtonRect, cpt)) {
						const auto& path = screen.m_CurrentSprite->imageInfo->filePath;
						App::instance->SaveVote(DOWN_VOTE, path);
						App::instance->m_Downvoted.insert(path);
						screen.StartSwap(false, screen.m_AdapterIndex, (int)App::instance->m_Screensavers.size()); // TODO: force reload to a new image
						return 1; // consume click
					}
					else if (PtInRect(&screen.m_RotateButtonRect, cpt)) {
						screen.m_CurrentSprite->imageInfo->RotateImage90();
						screen.StartSwap(false, screen.m_AdapterIndex, (int)App::instance->m_Screensavers.size()); // TODO: force reload rotated
						return 1; // consume click
					}
				}
			}
		}
	}
	return CallNextHookEx(App::instance->m_MouseHook, nCode, wParam, lParam);
}


// Load a bitmap from a file
HRESULT App::LoadBitmapFromFile(
	ID2D1RenderTarget* m_pRenderTarget,
	IWICImagingFactory* pIWICFactory,
	PCWSTR uri,
	UINT destinationWidth,
	UINT destinationHeight,
	ID2D1Bitmap** ppBitmap
) {
	ComPtr<IWICBitmapDecoder> pDecoder;
	ComPtr<IWICBitmapFrameDecode> pSource;
	ComPtr<IWICStream> pStream;
	ComPtr<IWICFormatConverter> pConverter;
	ComPtr<IWICBitmapScaler> pScaler;

	HRESULT hr = pIWICFactory->CreateDecoderFromFilename(
		uri,
		nullptr,
		GENERIC_READ,
		WICDecodeMetadataCacheOnLoad,
		pDecoder.GetAddressOf()
	);

	if (SUCCEEDED(hr)) {
		hr = pDecoder->GetFrame(0, pSource.GetAddressOf());
	}

	if (SUCCEEDED(hr)) {
		hr = pIWICFactory->CreateFormatConverter(pConverter.GetAddressOf());
	}

	if (SUCCEEDED(hr)) {
		hr = pConverter->Initialize(
			pSource.Get(),
			GUID_WICPixelFormat32bppPBGRA,
			WICBitmapDitherTypeNone,
			nullptr,
			0.f,
			WICBitmapPaletteTypeMedianCut
		);
	}

	if (SUCCEEDED(hr)) {
		if (destinationWidth != 0 || destinationHeight != 0) {
			UINT originalWidth, originalHeight;
			hr = pSource->GetSize(&originalWidth, &originalHeight);
			if (SUCCEEDED(hr)) {
				if (destinationWidth == 0) {
					FLOAT scalar = static_cast<FLOAT>(destinationHeight) / static_cast<FLOAT>(originalHeight);
					destinationWidth = static_cast<UINT>(scalar * static_cast<FLOAT>(originalWidth));
				}
				else if (destinationHeight == 0) {
					FLOAT scalar = static_cast<FLOAT>(destinationWidth) / static_cast<FLOAT>(originalWidth);
					destinationHeight = static_cast<UINT>(scalar * static_cast<FLOAT>(originalHeight));
				}

				hr = pIWICFactory->CreateBitmapScaler(pScaler.GetAddressOf());
				if (SUCCEEDED(hr)) {
					hr = pScaler->Initialize(
						pConverter.Get(),
						destinationWidth,
						destinationHeight,
						WICBitmapInterpolationModeCubic
					);
				}
			}
		}

		if (SUCCEEDED(hr)) {
			// Choose the appropriate source based on whether we're scaling
			if (destinationWidth == 0 || destinationHeight == 0) {
				// Use the converter directly if no scaling is needed
				hr = m_pRenderTarget->CreateBitmapFromWicBitmap(
					pConverter.Get(),
					nullptr,
					ppBitmap
				);
			}
			else {
				// Use the scaler when dimensions are specified
				hr = m_pRenderTarget->CreateBitmapFromWicBitmap(
					pScaler.Get(),
					nullptr,
					ppBitmap
				);
			}
		}
	}

	return hr;
}

void App::RunMessageLoop()
{
	const auto targetFrameTime = std::chrono::milliseconds(1000 / 60);

	auto frameStart = std::chrono::steady_clock::now();
	auto previousFrameStart = frameStart;
	bool isRunning = true;

	MSG msg;
	while (isRunning && !m_WantsToQuit) {
		previousFrameStart = frameStart;
		// Record the start time of this frame
		frameStart = std::chrono::steady_clock::now();

		// Process all pending Windows messages
		while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
			// Check if we should quit
			if (msg.message == WM_QUIT) {
				isRunning = false;
				break;
			}

			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		if (!isRunning) {
			break;
		}

		// Update game state
		auto deltaTime = std::chrono::duration_cast<std::chrono::milliseconds>(frameStart - previousFrameStart).count() / 1000.f;
		Update(deltaTime);

		OnRender();

		// Calculate how long this frame took
		auto frameTime = std::chrono::steady_clock::now() - frameStart;

		// Sleep if we have time remaining to maintain 60 FPS
		if (frameTime < targetFrameTime) {
			auto sleepTime = targetFrameTime - frameTime;
			Sleep((DWORD)std::chrono::duration_cast<std::chrono::milliseconds>(sleepTime).count());
		}
	}
}

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
	HeapSetInformation(nullptr, HeapEnableTerminationOnCorruption, nullptr, 0);

	HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

	if (SUCCEEDED(hr))
	{
		try
		{
			App app;

			if (SUCCEEDED(app.Initialize(hInstance, lpCmdLine))) {
				app.RunMessageLoop();
			}
		}
		catch (...)
		{
		}
		CoUninitialize();
	}

	//ReleaseMutex(hMutex);
	//CloseHandle(hMutex);

	return 0;
}

