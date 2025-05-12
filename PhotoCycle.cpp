#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <d2d1.h>
#include <d2d1helper.h>
#include <dwrite.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <memory>
#include <string>
#include <stdexcept>
#include <algorithm>
#include <ImageFileNameLibrary.h>
#include <shlobj.h>

// Link the Direct2D library
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "windowscodecs.lib")

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


using Microsoft::WRL::ComPtr;

class Sprite
{
public:
	ComPtr<ID2D1Bitmap> bitmap;
	D2D1_SIZE_F originalSize;
	float x;
	float y;
	float scale = 1;
	float alpha = 1;
	const ImageInfo* imageInfo;

	void Clear()
	{
		alpha = 1;
		scale = 1;
		imageInfo = nullptr;
		bitmap.Reset();
	}
};

std::wstring EnsureIniFileExists(bool create) {
	wchar_t path[MAX_PATH];

	// Get AppData directory path
	HRESULT hr = SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, 0, path);
	if (FAILED(hr)) {
		wprintf(L"Error getting AppData path: %08lx\n", hr);
		return L"";
	}

	// Append your app's folder to the AppData path
	wcscat_s(path, MAX_PATH, L"\\PhotoCycle");

	if (create)
	{
		// Ensure the directory exists
		BOOL success = CreateDirectory(path, NULL);
		if (!success) {
			wprintf(L"Error creating directory '%s'\n", path);
			return L"";
		}
	}

	// Append your app's folder to the AppData path
	wcscat_s(path, MAX_PATH, L"\\config.ini");

	if (create)
	{
		DWORD attr = GetFileAttributes(path);
		if (attr == INVALID_FILE_ATTRIBUTES) {
			HANDLE hFile = CreateFile(path, GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
			if (hFile != INVALID_HANDLE_VALUE) CloseHandle(hFile);
		}
	}

	return path;
}

void WriteInt(LPCWSTR section, LPCWSTR key, int value) {
	auto file = EnsureIniFileExists(true);
	wchar_t buffer[32];
	swprintf_s(buffer, L"%d", value);
	WritePrivateProfileString(section, key, buffer, file.c_str());
}

void WriteBool(LPCWSTR section, LPCWSTR key, bool value) {
	auto file = EnsureIniFileExists(true);
	BOOL result = WritePrivateProfileString(section, key, value ? L"1" : L"0", file.c_str());
	if (!result) {
		DWORD error = GetLastError();
		wprintf(L"Write failed with error code: %lu\n", error);
	}
}

void WriteList(LPCWSTR section, LPCWSTR key, const std::vector<std::wstring>& list) {
	auto file = EnsureIniFileExists(true);
	std::wstring joined;
	for (size_t i = 0; i < list.size(); ++i) {
		if (i > 0) joined += L",";
		joined += list[i];
	}
	WritePrivateProfileString(section, key, joined.c_str(), file.c_str());
}

float ReadFloat(LPCWSTR section, LPCWSTR key, float defaultValue) {
	auto file = EnsureIniFileExists(false);
	WCHAR buffer[64];
	GetPrivateProfileString(section, key, nullptr, buffer, 64, file.c_str());
	return buffer[0] ? static_cast<float>(_wtof(buffer)) : defaultValue;
}

bool ReadBool(LPCWSTR section, LPCWSTR key, bool defaultValue) {
	auto file = EnsureIniFileExists(false);
	return GetPrivateProfileInt(section, key, defaultValue ? 1 : 0, file.c_str()) != 0;
}

std::vector<std::wstring> SplitList(const std::wstring& str, wchar_t delimiter = L',') {
	std::vector<std::wstring> result;
	size_t start = 0, end;
	while ((end = str.find(delimiter, start)) != std::wstring::npos) {
		result.push_back(str.substr(start, end - start));
		start = end + 1;
	}
	if (start < str.size())
		result.push_back(str.substr(start));
	return result;
}

std::vector<std::wstring> ReadList(LPCWSTR section, LPCWSTR key) {
	auto file = EnsureIniFileExists(false);
	WCHAR buffer[2048] = {};
	GetPrivateProfileString(section, key, nullptr, buffer, 2048, file.c_str());
	return SplitList(buffer);
}

static const wchar_t* INI_SETTINGS = L"Settings";
static const wchar_t* INI_RENDER_TEXT = L"RenderText";

class Direct2DApp
{
public:
	static Direct2DApp* instance;

private:
	HWND m_hwnd;
	ComPtr<ID2D1Factory> m_pD2DFactory;
	ComPtr<ID2D1HwndRenderTarget> m_pRenderTarget;
	ComPtr<IWICImagingFactory> m_pWICFactory;
	Sprite* m_CurrentSprite = new Sprite();
	Sprite* m_NextSprite = new Sprite();
	ComPtr<IDWriteFactory> m_pDWriteFactory;
	ComPtr<IDWriteTextFormat> m_pTextFormat;
	ComPtr<ID2D1SolidColorBrush> m_pTextBrush;
	ImageFileNameLibrary m_Library;

	float m_FadeDuration = 1;
	float m_FadeTimer;

	float m_DisplayDuration = 5;
	float m_DisplayTimer;

	bool m_RenderText = true;

	Direct2DApp(const Direct2DApp&) = delete;
	Direct2DApp& operator=(const Direct2DApp&) = delete;

public:
	Direct2DApp() : m_hwnd(nullptr) {}

	~Direct2DApp() {
		DiscardDeviceResources();
	}

	// Register the window class and create the window
	HRESULT Initialize(HINSTANCE hInstance) {
		auto include = ReadList(L"Images", L"Include");
		auto exclude = ReadList(L"Images", L"Exclude");
		if (include.empty())
		{
			include.push_back(L"C:\\Users\\lucbl\\Pictures\\Personal");
		}
		m_Library.SetPaths(include, exclude);

		m_FadeDuration = ReadFloat(INI_SETTINGS, L"FadeDuration", 1.0f);
		m_DisplayDuration = ReadFloat(INI_SETTINGS, L"DisplayDuration", 10.0f);
		m_RenderText = ReadBool(INI_SETTINGS, INI_RENDER_TEXT, true);

		HRESULT hr = CreateDeviceIndependentResources();
		if (FAILED(hr)) {
			return hr;
		}

		// Register window class
		WNDCLASSEX wcex = { sizeof(WNDCLASSEX) };
		wcex.style = CS_HREDRAW | CS_VREDRAW;
		wcex.lpfnWndProc = Direct2DApp::WndProc;
		wcex.cbClsExtra = 0;
		wcex.cbWndExtra = sizeof(LONG_PTR);
		wcex.hInstance = hInstance;
		wcex.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
		wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
		wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
		wcex.lpszMenuName = nullptr;
		wcex.lpszClassName = L"Direct2DAppClass";
		wcex.hIconSm = LoadIcon(wcex.hInstance, IDI_APPLICATION);

		RegisterClassEx(&wcex);

		// Create the window
		m_hwnd = CreateWindow(
			L"Direct2DAppClass",
			L"Direct2D Texture Renderer",
			WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT, CW_USEDEFAULT,
			800, 600,
			nullptr,
			nullptr,
			hInstance,
			this
		);

		if (!m_hwnd) {
			return E_FAIL;
		}

		// Show the window
		ShowWindow(m_hwnd, SW_SHOWNORMAL);
		UpdateWindow(m_hwnd);

		return S_OK;
	}

	// Create resources which are not dependent on the device
	HRESULT CreateDeviceIndependentResources() {
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
				L"Segoe UI",
				nullptr,
				DWRITE_FONT_WEIGHT_NORMAL,
				DWRITE_FONT_STYLE_NORMAL,
				DWRITE_FONT_STRETCH_NORMAL,
				16.0f,
				L"en-us",
				m_pTextFormat.GetAddressOf()
			);
		}

		return hr;
	}

	// Create resources which are dependent on the size of the client area
	HRESULT CreateDeviceResources() {
		HRESULT hr = S_OK;

		if (!m_pRenderTarget) {
			RECT rc;
			GetClientRect(m_hwnd, &rc);

			D2D1_SIZE_U size = D2D1::SizeU(
				rc.right - rc.left,
				rc.bottom - rc.top
			);

			// Create a Direct2D render target
			hr = m_pD2DFactory->CreateHwndRenderTarget(
				D2D1::RenderTargetProperties(),
				D2D1::HwndRenderTargetProperties(m_hwnd, size),
				m_pRenderTarget.GetAddressOf()
			);

			if (SUCCEEDED(hr)) {
				// Create a solid color brush for text
				hr = m_pRenderTarget->CreateSolidColorBrush(
					D2D1::ColorF(D2D1::ColorF::Black),
					m_pTextBrush.GetAddressOf()
				);
			}
		}

		if (!m_CurrentSprite->imageInfo)
		{
			LoadCurrentSprite();
		}

		if (!m_CurrentSprite->bitmap.Get())
		{
			LoadSprite(m_CurrentSprite);
		}

		if (!m_NextSprite->bitmap.Get())
		{
			LoadSprite(m_NextSprite);
		}

		return hr;
	}

	void LoadCurrentSprite()
	{
		m_CurrentSprite->imageInfo = m_Library.GotoImage(0);
		LoadSprite(m_CurrentSprite);
	}

	void LoadSprite(Sprite* sprite)
	{
		if (!sprite->imageInfo)
		{
			sprite->bitmap.Reset();
			return;
		}

		auto hr = LoadBitmapFromFileWithTransparencyMixedToBlack(sprite);
		if (SUCCEEDED(hr)) {
			sprite->originalSize = sprite->bitmap->GetSize();
		}
	}

	HRESULT LoadBitmapFromFileWithTransparencyMixedToBlack(Sprite* sprite)
	{
		// Initialize WIC and load the image
		Microsoft::WRL::ComPtr<IWICBitmapDecoder> pDecoder;
		Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> pFrame;
		Microsoft::WRL::ComPtr<IWICFormatConverter> pConverter;

		HRESULT hr = m_pWICFactory->CreateDecoderFromFilename(
			sprite->imageInfo->filePath.c_str(),
			nullptr,
			GENERIC_READ,
			WICDecodeOptions::WICDecodeMetadataCacheOnDemand,
			&pDecoder);
		if (FAILED(hr)) return hr;

		hr = pDecoder->GetFrame(0, &pFrame);
		if (FAILED(hr)) return hr;

		hr = m_pWICFactory->CreateFormatConverter(&pConverter);
		if (FAILED(hr)) return hr;

		hr = pConverter->Initialize(
			pFrame.Get(),
			GUID_WICPixelFormat32bppPBGRA,  // Convert to 32-bit BGRA format (including alpha)
			WICBitmapDitherTypeNone,
			nullptr,
			0.0f,
			WICBitmapPaletteTypeCustom);
		if (FAILED(hr)) return hr;

		UINT width, height;
		hr = pConverter->GetSize(&width, &height);
		if (FAILED(hr)) return hr;

		// Create a D2D bitmap from the converted WIC image
		D2D1_BITMAP_PROPERTIES props;
		props.pixelFormat = D2D1_PIXEL_FORMAT(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED);
		m_pRenderTarget->GetDpi(&props.dpiX, &props.dpiY);

		Microsoft::WRL::ComPtr<ID2D1Bitmap> pBitmap;
		hr = m_pRenderTarget->CreateBitmap(
			D2D1::SizeU(width, height),
			nullptr, 0,
			props,
			&pBitmap);
		if (FAILED(hr)) return hr;

		// Get the pixel data from the WIC converter
		UINT stride = width * 4; // 4 bytes per pixel (BGRA)
		std::vector<BYTE> pixelData(stride * height);

		hr = pConverter->CopyPixels(nullptr, stride, static_cast<UINT>(pixelData.size()), pixelData.data());
		if (FAILED(hr)) return hr;

		const BYTE bgColor[3] = { 0, 0, 0 };

		// Modify the pixel data to blend with black
		for (UINT y = 0; y < height; ++y)
		{
			for (UINT x = 0; x < width; ++x)
			{
				// Get the pixel at (x, y)
				BYTE* pixel = &pixelData[(y * stride) + (x * 4)];
				float a = pixel[3] / 255.0f;
				float a_inv = 1 - a;
				pixel[0] = static_cast<BYTE>(a * pixel[0] + a_inv * bgColor[0]);  // Blue
				pixel[1] = static_cast<BYTE>(a * pixel[1] + a_inv * bgColor[1]);  // Green
				pixel[2] = static_cast<BYTE>(a * pixel[2] + a_inv * bgColor[2]);  // Red
				pixel[3] = 255; // Alpha
			}
		}

		// Create a new bitmap from the modified pixel data
		hr = m_pRenderTarget->CreateBitmap(
			D2D1::SizeU(width, height),
			pixelData.data(),
			stride,
			props,
			sprite->bitmap.GetAddressOf());
		if (FAILED(hr)) return hr;

		return S_OK;
	}

	// Discard device resources
	void DiscardDeviceResources() {
		m_pRenderTarget.Reset();
		m_CurrentSprite->bitmap.Reset();
		m_NextSprite->bitmap.Reset();
	}

	void DrawSprite(Sprite* sprite) {
		if (!sprite || !sprite->imageInfo) {
			return;
		}

		D2D1_POINT_2F upperLeft = D2D1::Point2F(50.0f, 50.0f);

		RECT screenRect;
		GetClientRect(m_hwnd, &screenRect); // Or use GetSystemMetrics(SM_CXSCREEN) and SM_CYSCREEN for full screen size

		// Get bitmap dimensions
		UINT imgWidth = sprite->bitmap->GetSize().width;
		UINT imgHeight = sprite->bitmap->GetSize().height;

		// Calculate the scaling factor based on the screen dimensions
		float scaleFactor = std::max(
			static_cast<float>(screenRect.right - screenRect.left) / imgWidth,
			static_cast<float>(screenRect.bottom - screenRect.top) / imgHeight
		);

		// Calculate new scaled width and height
		float scaledWidth = imgWidth * scaleFactor;
		float scaledHeight = imgHeight * scaleFactor;

		// Center the image on the screen (optional)
		float upperLeftX = (screenRect.right - screenRect.left - scaledWidth) / 2.0f;
		float upperLeftY = (screenRect.bottom - screenRect.top - scaledHeight) / 2.0f;

		// Draw the bitmap, scaled and centered
		m_pRenderTarget->DrawBitmap(
			sprite->bitmap.Get(),
			D2D1::RectF(
				upperLeftX,
				upperLeftY,
				upperLeftX + scaledWidth,
				upperLeftY + scaledHeight
			),
			sprite->alpha, // Transparency
			D2D1_BITMAP_INTERPOLATION_MODE_LINEAR
		);

		if (m_RenderText)
		{
			auto caption = sprite->imageInfo->folderName;
			// Draw label for the first texture
			m_pRenderTarget->DrawText(
				caption.c_str(),
				static_cast<UINT32>(caption.length()),
				m_pTextFormat.Get(),
				D2D1::RectF(
					upperLeft.x,
					upperLeft.y + scaledHeight + 5.0f,
					upperLeft.x + scaledWidth,
					upperLeft.y + scaledHeight + 30.0f
				),
				m_pTextBrush.Get()
			);
		}
	}

	// Draw the content
	HRESULT OnRender() {
		HRESULT hr = CreateDeviceResources();

		if (FAILED(hr)) {
			return hr;
		}

		m_pRenderTarget->BeginDraw();
		m_pRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());
		m_pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::White));

		D2D1_SIZE_F rtSize = m_pRenderTarget->GetSize();

		// Draw the first texture maintaining aspect ratio
		DrawSprite(m_CurrentSprite);
		DrawSprite(m_NextSprite);

		// Draw application instructions
		std::wstring instructionText =
			L"...";

		m_pRenderTarget->DrawText(
			instructionText.c_str(),
			static_cast<UINT32>(instructionText.length()),
			m_pTextFormat.Get(),
			D2D1::RectF(
				50.0f,
				rtSize.height - 100.0f,
				rtSize.width - 50.0f,
				rtSize.height - 10.0f
			),
			m_pTextBrush.Get()
		);

		hr = m_pRenderTarget->EndDraw();

		if (hr == D2DERR_RECREATE_TARGET) {
			hr = S_OK;
			DiscardDeviceResources();
		}

		return hr;
	}

	// Handle window resizing
	void OnResize(UINT width, UINT height)
	{
		if (m_pRenderTarget) {
			m_pRenderTarget->Resize(D2D1::SizeU(width, height));
		}
	}

	void StartSwap(bool animate, int offset)
	{
		auto info = m_Library.GotoImage(offset);
		if (animate) {
			m_FadeTimer = m_FadeDuration;
			m_NextSprite->imageInfo = info;
			LoadSprite(m_NextSprite);
			m_NextSprite->alpha = 0;
		}
		else {
			EndFade();
			m_CurrentSprite->imageInfo = info;
			LoadSprite(m_CurrentSprite);
		}
		InvalidateRect(m_hwnd, nullptr, FALSE);
	}

	void EndFade()
	{
		m_FadeTimer = 0.0f;
		std::swap(m_CurrentSprite, m_NextSprite);
		m_CurrentSprite->alpha = 1;
		m_NextSprite->Clear();
		m_DisplayTimer = m_DisplayDuration;
	}

	void Update(float deltaTime)
	{
		if (m_FadeTimer > 0.0f) {
			m_FadeTimer -= deltaTime;
			m_NextSprite->alpha = 1 - m_FadeTimer / m_FadeDuration;
			if (m_FadeTimer <= 0.0f) {
				EndFade();
			}
			InvalidateRect(m_hwnd, nullptr, FALSE);
		}
		else
		{
			m_DisplayTimer -= deltaTime;
			if (m_DisplayTimer <= 0.0f) {
				StartSwap(true, 1);
			}
		}
	}

	static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
		if (message == WM_CREATE) {
			LPCREATESTRUCT pcs = (LPCREATESTRUCT)lParam;
			Direct2DApp* pApp = (Direct2DApp*)pcs->lpCreateParams;
			SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pApp));
			return 1;
		}

		Direct2DApp* pApp = reinterpret_cast<Direct2DApp*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

		if (!pApp) {
			return DefWindowProc(hwnd, message, wParam, lParam);
		}

		LRESULT result = 0;
		switch (message) {
		case WM_SIZE:
		{
			UINT width = LOWORD(lParam);
			UINT height = HIWORD(lParam);
			pApp->OnResize(width, height);
			result = 0;
		}
		break;

		case WM_KEYDOWN:
		{
			switch (wParam) {
			case VK_LEFT:
				pApp->StartSwap(false, -1);
				InvalidateRect(hwnd, nullptr, FALSE);
				break;

			case VK_RIGHT:
				pApp->StartSwap(false, 1);
				InvalidateRect(hwnd, nullptr, FALSE);
				break;

			case 'T':
				pApp->m_RenderText = !pApp->m_RenderText;
				InvalidateRect(hwnd, nullptr, FALSE);
				WriteBool(INI_SETTINGS, INI_RENDER_TEXT, true);
				break;
			}
		}
		break;

		case WM_DISPLAYCHANGE:
			InvalidateRect(hwnd, nullptr, FALSE);
			result = 0;
			break;

		case WM_PAINT:
		{
			pApp->OnRender();
			ValidateRect(hwnd, nullptr);
			result = 0;
		}
		break;

		case WM_DESTROY:
		{
			PostQuitMessage(0);
			result = 1;
		}
		break;

		default:
			result = DefWindowProc(hwnd, message, wParam, lParam);
			break;
		}

		return result;
	}

	// Load a bitmap from a file
	HRESULT LoadBitmapFromFile(
		ID2D1RenderTarget* pRenderTarget,
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
					hr = pRenderTarget->CreateBitmapFromWicBitmap(
						pConverter.Get(),
						nullptr,
						ppBitmap
					);
				}
				else {
					// Use the scaler when dimensions are specified
					hr = pRenderTarget->CreateBitmapFromWicBitmap(
						pScaler.Get(),
						nullptr,
						ppBitmap
					);
				}
			}
		}

		return hr;
	}

	void RunMessageLoop() {
		// Target frame time for 60 FPS (in milliseconds)
		const DWORD targetFrameTime = 1000 / 60;

		MSG msg;
		DWORD previousFrameStart;
		DWORD frameStart = GetTickCount();
		bool isRunning = true;

		while (isRunning) {
			previousFrameStart = frameStart;
			// Record the start time of this frame
			frameStart = GetTickCount();

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
			auto deltaTime = (frameStart - previousFrameStart) / 1000.f;
			Update(deltaTime);

			// Render frame
			//RenderFrame();

			// Calculate how long this frame took
			auto frameTime = GetTickCount() - frameStart;

			// Sleep if we have time remaining to maintain 60 FPS
			if (frameTime < targetFrameTime) {
				auto sleepTime = targetFrameTime - frameTime;
				Sleep(sleepTime);
			}
		}
	}
};

Direct2DApp* Direct2DApp::instance = nullptr;

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
	HeapSetInformation(nullptr, HeapEnableTerminationOnCorruption, nullptr, 0);

	HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

	if (SUCCEEDED(hr))
	{
		{
			Direct2DApp app;

			if (SUCCEEDED(app.Initialize(hInstance))) {
				app.RunMessageLoop();
			}
		}
		CoUninitialize();
	}

	return 0;
}
