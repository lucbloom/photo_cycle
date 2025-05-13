#ifndef UNICODE
#define UNICODE
#endif

#include <sstream>
#include <d2d1.h>
#include <dwrite.h>
#include <wincodec.h>
#include <wrl/client.h>

#include "ImageFileNameLibrary.h"
#include "SettingsDialog.h"

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "Ws2_32.lib")

std::string WStringToUtf8(const std::wstring& wstr);
std::wstring Utf8ToWString(const std::string& str);

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

float EaseInOutQuad(float t) { return t < 0.5f ? 2 * t * t : -1 + (4 - 2 * t) * t; }

class Sprite
{
public:
	ComPtr<ID2D1Bitmap> bitmap;
	D2D1_SIZE_F originalSize;
	ImageInfo* imageInfo;

	float x;
	float y;
	float scale = 1;
	float alpha = 1;

	float ZoomStart = 0;
	float ZoomEnd = 0;
	float PanXStart = 0;
	float PanXEnd = 0;
	float PanYStart = 0;
	float PanYEnd = 0;
	float PanScanProgress = 0;

	void Clear();
	void OnLoad();
	void Update(float deltaTime);
};

class ScreenSaverWindow
{
public:
	int m_AdapterIndex = 0;
	HWND m_hwnd = nullptr;
	RECT m_MaximizedRect = { CW_USEDEFAULT, CW_USEDEFAULT, 640, 480 };
	ComPtr<ID2D1HwndRenderTarget> m_pRenderTarget = nullptr;
	Sprite* m_CurrentSprite = new Sprite();
	Sprite* m_NextSprite = new Sprite();
	ComPtr<ID2D1SolidColorBrush> m_pTextOutlineBrush;
	ComPtr<ID2D1SolidColorBrush> m_pTextFillBrush;
	ComPtr<IDWriteTextLayout> m_pTextLayout = nullptr;

	float m_FadeTimer = 0;

	HRESULT CreateDeviceResources();
	void LoadCurrentSprite();
	HRESULT LoadBitmapFromFileWithTransparencyMixedToBlack(Sprite* sprite);
	void DiscardDeviceResources();
	void LoadSprite(Sprite* sprite);
	void DrawSprite(Sprite* sprite);
	HRESULT OnRender();
	void RenderText(const std::wstring& caption, float alpha, float x, float y, float w, float h);
	void OnResize(UINT width, UINT height);
	RECT GetMaximizedRect();
	void Update(float deltaTime);
	void StartSwap(bool animate, int offset);
	void EndFade();
	void DrawHackOutline(float x, float y, float xo, float yo);
};

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
	int m_CurentScreenIndex = 0;

	ImageFileNameLibrary m_Library;

	float m_DisplayTimer = 0;

	SettingsDialog settings;

	bool m_IsPreview = false;
	bool m_IsConfigDialogMode = false;

	App() { instance = this; }
	~App();
	HRESULT Initialize(HINSTANCE hInstance, const std::wstring& cmd);
	HRESULT CreateDeviceIndependentResources();
	void Update(float deltaTime);
	void OnRender();
	void SetFullscreen(bool fullscreen);
	static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
	void StartSwap(bool animate, int offset);
	void EndFade();

	// Load a bitmap from a file
	HRESULT LoadBitmapFromFile(ID2D1RenderTarget* m_pRenderTarget, IWICImagingFactory* pIWICFactory, PCWSTR uri, UINT destinationWidth, UINT destinationHeight, ID2D1Bitmap** ppBitmap);

	void RunMessageLoop();
};

App* App::instance = nullptr;

App::~App()
{
	for (auto& screenSaver : m_Screensavers) {
		screenSaver.DiscardDeviceResources();
	}
}

void Sprite::Clear()
{
	alpha = 1;
	scale = 1;
	imageInfo = nullptr;
	bitmap.Reset();
}

void Sprite::OnLoad()
{
	originalSize = bitmap->GetSize();

	int panScanMod = (int)(1000 * App::instance->settings.PanScanFactor);
	if (panScanMod <= 0)
	{
		ZoomStart = ZoomEnd = 1;
		PanXStart = PanXEnd = PanYStart = PanYEnd = 0;
	}
	else
	{
		ZoomStart = 1, ZoomEnd = 1.1f + ((rand() % panScanMod) / 10000.0f); // 1.000 to 1.50
		if (rand() % 2) { std::swap(ZoomStart, ZoomEnd); }

		float panRangeStart = (ZoomStart - 1.0f) / ZoomStart;
		float panRangeEnd = (ZoomEnd - 1.0f) / ZoomEnd;

		PanXStart = ((rand() % 1000) / 1000.0f - 0.5f) * 2 * panRangeStart;
		PanXEnd = ((rand() % 1000) / 1000.0f - 0.5f) * 2 * panRangeEnd;
		PanYStart = ((rand() % 1000) / 1000.0f - 0.5f) * 2 * panRangeStart;
		PanYEnd = ((rand() % 1000) / 1000.0f - 0.5f) * 2 * panRangeEnd;
	}

	PanScanProgress = 0.0f;
}

void Sprite::Update(float deltaTime)
{
	PanScanProgress += deltaTime;
}

HRESULT ScreenSaverWindow::CreateDeviceResources() {
	HRESULT hr = S_OK;

	if (!m_pRenderTarget) {
		RECT rc;
		GetClientRect(m_hwnd, &rc);

		D2D1_SIZE_U size = D2D1::SizeU(
			rc.right - rc.left,
			rc.bottom - rc.top
		);

		// Create a Direct2D render target
		hr = App::instance->m_pD2DFactory->CreateHwndRenderTarget(
			D2D1::RenderTargetProperties(),
			D2D1::HwndRenderTargetProperties(m_hwnd, size),
			m_pRenderTarget.GetAddressOf()
		);

		if (SUCCEEDED(hr)) {
			// Create a solid color brush for text
			hr = m_pRenderTarget->CreateSolidColorBrush(
				D2D1::ColorF(App::instance->settings.OutlineColor),
				m_pTextOutlineBrush.GetAddressOf()
			);

			// Create a solid color brush for text
			hr = m_pRenderTarget->CreateSolidColorBrush(
				D2D1::ColorF(App::instance->settings.TextColor),
				m_pTextFillBrush.GetAddressOf()
			);
		}
		else
		{
			std::cout << "Error (" << hr << ") creating Render target for window " << m_hwnd;
		}
	}

	if (!m_CurrentSprite->imageInfo)
	{
		m_CurrentSprite->imageInfo = App::instance->m_Library.GotoImage(0);
		LoadSprite(m_CurrentSprite);
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

#define FULLSCREEN_STYLE WS_POPUP

HRESULT App::Initialize(HINSTANCE hInstance, const std::wstring& cmd) {

	size_t pPos = cmd.find(L"/p");
	if (pPos == std::string::npos) { pPos = cmd.find(L"/P"); }
	m_IsPreview = (pPos != std::string::npos);

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

	HRESULT hr = CreateDeviceIndependentResources();
	if (FAILED(hr)) {
		return hr;
	}

	if (m_IsPreview) {
		auto hwndStr = cmd.c_str() + pPos + 3;  // Skip over "/p "
		m_Screensavers.resize(1);
		m_Screensavers[0].m_hwnd = reinterpret_cast<HWND>(_wtoi64(hwndStr));
		m_Screensavers[0].GetMaximizedRect();
	}
	else
	{
		//DISPLAY_DEVICE dd;
		//dd.cb = sizeof(dd);
		//int i = 0;
		//
		//while (EnumDisplayDevices(NULL, i, &dd, 0)) {
		//	DEVMODE dm;
		//	dm.dmSize = sizeof(dm);
		//
		//	if (EnumDisplaySettings(dd.DeviceName, ENUM_CURRENT_SETTINGS, &dm)) {
		//		WNDCLASS wc = { 0 };
		//		wc.lpfnWndProc = WndProc;
		//		wc.hInstance = hInstance;
		//		wc.lpszClassName = L"ScreenSaverClass";
		//		RegisterClass(&wc);
		//
		//		m_hwnd = CreateWindowEx(
		//			WS_EX_TOPMOST,
		//			L"ScreenSaverClass",
		//			L"ScreenSaver",
		//			WS_POPUP,
		//			dm.dmPosition.x, dm.dmPosition.y, dm.dmPelsWidth, dm.dmPelsHeight,
		//			nullptr, nullptr, hInstance, this
		//		);
		//
		//		ShowWindow(m_hwnd, SW_SHOW);
		//	}
		//
		//	i++;
		//}

		auto windowClassName = L"Direct2DAppClass";

		// Register window class
		WNDCLASSEX wcex = { sizeof(WNDCLASSEX) };
		wcex.style = CS_HREDRAW | CS_VREDRAW;
		wcex.lpfnWndProc = App::WndProc;
		wcex.cbClsExtra = 0;
		wcex.cbWndExtra = sizeof(LONG_PTR);
		wcex.hInstance = hInstance;
		wcex.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
		wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
		wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
		wcex.lpszMenuName = nullptr;
		wcex.lpszClassName = windowClassName;
		wcex.hIconSm = LoadIcon(wcex.hInstance, IDI_APPLICATION);

		RegisterClassEx(&wcex);

		DISPLAY_DEVICE dd;
		dd.cb = sizeof(dd);
		for (int i = 0; EnumDisplayDevices(NULL, i, &dd, 0); ++i) {
			if (settings.SingleScreen && !(dd.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE)) {
				continue;
			}

			DEVMODE dm;
			dm.dmSize = sizeof(dm);
			if (EnumDisplaySettings(dd.DeviceName, ENUM_CURRENT_SETTINGS, &dm)) {
				m_Screensavers.resize(m_Screensavers.size() + 1);
				auto& screen = m_Screensavers.back();
				screen.m_AdapterIndex = i;
				screen.GetMaximizedRect();
				screen.m_hwnd = CreateWindow(
					windowClassName,
					L"Photo Cycle",
					FULLSCREEN_STYLE,
					screen.m_MaximizedRect.left, screen.m_MaximizedRect.top,
					screen.m_MaximizedRect.right, screen.m_MaximizedRect.bottom,
					nullptr,
					nullptr,
					hInstance,
					&screen
				);
			}
		}
	}

	for (auto& screen : m_Screensavers) {
		if (!screen.m_hwnd) {
			return E_FAIL;
		}

		// Show the window
		ShowWindow(screen.m_hwnd, SW_SHOWNORMAL);
		UpdateWindow(screen.m_hwnd);
		screen.StartSwap(true, 0);
		screen.Update(0);
		screen.OnRender();
	}

	//if (!startFullscreen)
	//{
	//	SetFullscreen(false);
	//}

	return S_OK;
}

RECT ScreenSaverWindow::GetMaximizedRect()
{
	DISPLAY_DEVICE dd;
	dd.cb = sizeof(dd);

	if (EnumDisplayDevices(NULL, m_AdapterIndex, &dd, 0)) {
		DEVMODE dm;
		dm.dmSize = sizeof(dm);
		if (EnumDisplaySettings(dd.DeviceName, ENUM_CURRENT_SETTINGS, &dm)) {
			m_MaximizedRect.left = dm.dmPosition.x;
			m_MaximizedRect.top = dm.dmPosition.y;
			m_MaximizedRect.right = dm.dmPelsWidth;
			m_MaximizedRect.bottom = dm.dmPelsHeight;
		}
	}
	return m_MaximizedRect;
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

void ScreenSaverWindow::LoadSprite(Sprite* sprite)
{
	if (!sprite->imageInfo)
	{
		sprite->bitmap.Reset();
		return;
	}

	sprite->imageInfo->CacheInfo();

	auto hr = LoadBitmapFromFileWithTransparencyMixedToBlack(sprite);
	if (SUCCEEDED(hr)) {
		sprite->OnLoad();
	}
}

HRESULT ScreenSaverWindow::LoadBitmapFromFileWithTransparencyMixedToBlack(Sprite* sprite)
{
	if (!m_pRenderTarget)
	{
		return -1;
	}

	ComPtr<IWICBitmapDecoder> pDecoder;
	ComPtr<IWICBitmapFrameDecode> pFrame;
	ComPtr<IWICFormatConverter> pConverter;
	ComPtr<IWICBitmapFlipRotator> pRotator;

	// Create decoder
	HRESULT hr = App::instance->m_pWICFactory->CreateDecoderFromFilename(
		sprite->imageInfo->filePath.c_str(),
		nullptr,
		GENERIC_READ,
		WICDecodeMetadataCacheOnDemand,
		&pDecoder);
	if (FAILED(hr)) return hr;

	// Get frame
	hr = pDecoder->GetFrame(0, &pFrame);
	if (FAILED(hr)) return hr;

	// Convert to 32bppPBGRA
	hr = App::instance->m_pWICFactory->CreateFormatConverter(&pConverter);
	if (FAILED(hr)) return hr;

	hr = pConverter->Initialize(
		pFrame.Get(),
		GUID_WICPixelFormat32bppPBGRA,
		WICBitmapDitherTypeNone,
		nullptr,
		0.0f,
		WICBitmapPaletteTypeCustom);
	if (FAILED(hr)) return hr;

	// Apply rotation
	WICBitmapTransformOptions transform = WICBitmapTransformRotate0;
	switch (sprite->imageInfo->rotation) {
	case 90:  transform = WICBitmapTransformRotate90; break;
	case 180: transform = WICBitmapTransformRotate180; break;
	case 270: transform = WICBitmapTransformRotate270; break;
	default: transform = WICBitmapTransformRotate0; break;
	}

	ComPtr<IWICBitmapSource> pSource = pConverter;

	if (transform != WICBitmapTransformRotate0) {
		hr = App::instance->m_pWICFactory->CreateBitmapFlipRotator(&pRotator);
		if (FAILED(hr)) return hr;

		hr = pRotator->Initialize(pConverter.Get(), transform);
		if (FAILED(hr)) return hr;

		pSource = pRotator;
	}

	// Get final size
	UINT width, height;
	hr = pSource->GetSize(&width, &height);
	if (FAILED(hr)) return hr;

	// Create D2D bitmap
	D2D1_BITMAP_PROPERTIES props;
	props.pixelFormat = D2D1_PIXEL_FORMAT(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED);
	m_pRenderTarget->GetDpi(&props.dpiX, &props.dpiY);

	Microsoft::WRL::ComPtr<ID2D1Bitmap> pBitmap;
	m_pRenderTarget->CreateBitmap(
		D2D1::SizeU(width, height),
		nullptr, 0,
		props,
		&pBitmap);

	// Get the pixel data from the WIC converter
	UINT stride = width * 4; // 4 bytes per pixel (BGRA)
	std::vector<BYTE> pixelData(stride * height);

	hr = pConverter->CopyPixels(nullptr, stride, static_cast<UINT>(pixelData.size()), pixelData.data());
	if (FAILED(hr)) return hr;

	auto bgc = App::instance->settings.BackgroundColor;
	const BYTE bgColor[3] = { (BYTE)bgc, (BYTE)(bgc >> 8), (BYTE)(bgc >> 16) };

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
void ScreenSaverWindow::DiscardDeviceResources() {
	m_pRenderTarget.Reset();
	m_CurrentSprite->bitmap.Reset();
	m_NextSprite->bitmap.Reset();
}

void ScreenSaverWindow::DrawSprite(Sprite* sprite) {
	if (!sprite || !sprite->imageInfo || !sprite->bitmap) {
		return;
	}

	RECT screenRect;
	GetClientRect(m_hwnd, &screenRect); // Or use GetSystemMetrics(SM_CXSCREEN) and SM_CYSCREEN for full screen size
	auto screenWidth = screenRect.right - screenRect.left;

	// Get bitmap dimensions
	float imgWidth = sprite->bitmap->GetSize().width;
	float imgHeight = sprite->bitmap->GetSize().height;

	// Calculate the scaling factor based on the screen dimensions
	float scaleFactor = std::max(
		screenWidth / imgWidth,
		(screenRect.bottom - screenRect.top) / imgHeight
	);

	float totalDislayTime = App::instance->settings.DisplayDuration;
	if (!App::instance->settings.SyncChange)
	{
		totalDislayTime *= App::instance->m_Screensavers.size();
	}
	float progress = std::clamp(sprite->PanScanProgress / totalDislayTime, 0.f, 1.f);

	float zoom = sprite->ZoomStart + (sprite->ZoomEnd - sprite->ZoomStart) * progress;
	float panX = sprite->PanXStart + (sprite->PanXEnd - sprite->PanXStart) * progress;
	float panY = sprite->PanYStart + (sprite->PanYEnd - sprite->PanYStart) * progress;

	float scaledWidth = imgWidth * scaleFactor * zoom;
	float scaledHeight = imgHeight * scaleFactor * zoom;

	float offsetX = (screenWidth - scaledWidth) * 0.5f + panX * (screenWidth - scaledWidth);
	float offsetY = (screenRect.bottom - screenRect.top - scaledHeight) * 0.5f + panY * (screenRect.bottom - screenRect.top - scaledHeight);

	m_pRenderTarget->DrawBitmap(
		sprite->bitmap.Get(),
		D2D1::RectF(
			offsetX,
			offsetY,
			offsetX + scaledWidth,
			offsetY + scaledHeight
		),
		sprite->alpha,
		D2D1_BITMAP_INTERPOLATION_MODE_LINEAR
	);

	//if (App::instance->settings.RenderText)
	//{
	//	RenderText(sprite->imageInfo->folderName, sprite->alpha, 100, 100, (float)screenWidth, 200);
	//}
}

void ScreenSaverWindow::DrawHackOutline(float x, float y, float xo, float yo)
{
	m_pRenderTarget->DrawTextLayout(
		D2D1::Point2F(x + xo, y + yo),
		m_pTextLayout.Get(),
		m_pTextOutlineBrush.Get(),
		D2D1_DRAW_TEXT_OPTIONS_NO_SNAP
	);
}

void SetBrushColor(ID2D1SolidColorBrush* brush, UINT32 color, float alpha)
{
	float red = ((color >> 16) & 0xFF) / 255.0f;
	float green = ((color >> 8) & 0xFF) / 255.0f;
	float blue = (color & 0xFF) / 255.0f;
	brush->SetColor(D2D1::ColorF(red, green, blue, alpha));
}

void ScreenSaverWindow::RenderText(const std::wstring& caption, float alpha, float x, float y, float w, float h)
{
	if (w <= 0 || h <= 0 || alpha <= 0)
	{
		return;
	}

	// OUTLINE (WIP) ComPtr<IDWriteFontCollection> pFontCollection;
	// OUTLINE (WIP) ComPtr<IDWriteFont> pFont;
	// OUTLINE (WIP) ComPtr<IDWriteFontFace> pFontFace;
	// OUTLINE (WIP) ComPtr<IDWriteFontFamily> pFontFamily;
	// OUTLINE (WIP) if (SUCCEEDED(App::instance->m_pDWriteFactory->GetSystemFontCollection(&pFontCollection))) {
	// OUTLINE (WIP) 	if (SUCCEEDED(pFontCollection->GetFontFamily(0, &pFontFamily))) {
	// OUTLINE (WIP) 		if (SUCCEEDED(pFontFamily->GetFont(0, &pFont))) {
	// OUTLINE (WIP) 			pFont->CreateFontFace(&pFontFace);
	// OUTLINE (WIP) 		}
	// OUTLINE (WIP) 	}
	// OUTLINE (WIP) }
	// OUTLINE (WIP) pFont->CreateFontFace(&pFontFace);
	// OUTLINE (WIP) 
	// OUTLINE (WIP) // Convert Unicode characters to glyph indices
	// OUTLINE (WIP) std::vector<UINT32> unicodeCodePoints(caption.length());
	// OUTLINE (WIP) for (int i = 0; i < caption.length(); ++i) {
	// OUTLINE (WIP) 	unicodeCodePoints.push_back(caption[i]);
	// OUTLINE (WIP) }
	// OUTLINE (WIP) std::vector<UINT16> glyphIndices(unicodeCodePoints.size());
	// OUTLINE (WIP) pFontFace->GetGlyphIndices(unicodeCodePoints.data(), (UINT32)unicodeCodePoints.size(), glyphIndices.data());
	// OUTLINE (WIP) FLOAT dpiX, dpiY;
	// OUTLINE (WIP) m_pRenderTarget->GetDpi(&dpiX, &dpiY);
	// OUTLINE (WIP) float pixelsPerDip = dpiX / 96.0f; // Convert DPI to pixels per DIP
	// OUTLINE (WIP) 
	// OUTLINE (WIP) DWRITE_FONT_METRICS fontMetrics;
	// OUTLINE (WIP) pFontFace->GetGdiCompatibleMetrics(App::instance->settings.FontSize, pixelsPerDip, nullptr, &fontMetrics);
	// OUTLINE (WIP) 
	// OUTLINE (WIP) std::vector<DWRITE_GLYPH_METRICS> glyphMetrics(glyphIndices.size());
	// OUTLINE (WIP) HRESULT hr = pFontFace->GetGdiCompatibleGlyphMetrics(App::instance->settings.FontSize, pixelsPerDip, nullptr, FALSE, glyphIndices.data(), (UINT32)glyphIndices.size(), glyphMetrics.data());
	// OUTLINE (WIP) 
	// OUTLINE (WIP) // Convert advance widths
	// OUTLINE (WIP) std::vector<FLOAT> glyphAdvances(glyphIndices.size());
	// OUTLINE (WIP) for (size_t i = 0; i < glyphIndices.size(); ++i) {
	// OUTLINE (WIP) 	glyphAdvances[i] = static_cast<FLOAT>(glyphMetrics[i].advanceWidth) / fontMetrics.designUnitsPerEm * App::instance->settings.FontSize - App::instance->settings.OutlineWidth;
	// OUTLINE (WIP) }
	// OUTLINE (WIP) 
	// OUTLINE (WIP) float advW = 0;
	// OUTLINE (WIP) for (float adv : glyphAdvances) { advW += adv; }
	// OUTLINE (WIP) 
	// OUTLINE (WIP) // Generate outline from glyph run
	// OUTLINE (WIP) ComPtr<ID2D1PathGeometry> pPathGeometry;
	// OUTLINE (WIP) ComPtr<ID2D1GeometrySink> pSink;
	// OUTLINE (WIP) App::instance->m_pD2DFactory->CreatePathGeometry(&pPathGeometry);
	// OUTLINE (WIP) pPathGeometry->Open(&pSink);
	// OUTLINE (WIP) 
	// OUTLINE (WIP) std::vector<DWRITE_GLYPH_OFFSET> glyphOffsets(glyphIndices.size());
	// OUTLINE (WIP) pFontFace->GetGlyphRunOutline(App::instance->settings.FontSize, glyphIndices.data(), glyphAdvances.data(), glyphOffsets.data(), (UINT32)glyphIndices.size(), FALSE, FALSE, pSink.Get());
	// OUTLINE (WIP) 
	// OUTLINE (WIP) pSink->Close();
	// OUTLINE (WIP) 
	// OUTLINE (WIP) RECT screenRect;
	// OUTLINE (WIP) GetClientRect(m_hwnd, &screenRect);
	// OUTLINE (WIP) auto screenWidth = screenRect.right - screenRect.left;
	// OUTLINE (WIP) auto screenHeight = screenRect.bottom - screenRect.top;
	// OUTLINE (WIP) 
	// OUTLINE (WIP) D2D1_MATRIX_3X2_F transform = D2D1::Matrix3x2F::Translation(-advW / 2, y + h);
	// OUTLINE (WIP) ComPtr<ID2D1TransformedGeometry> pTransformedGeometry;
	// OUTLINE (WIP) App::instance->m_pD2DFactory->CreateTransformedGeometry(pPathGeometry.Get(), &transform, &pTransformedGeometry);
	// OUTLINE (WIP) m_pRenderTarget->DrawGeometry(pTransformedGeometry.Get(), m_pTextOutlineBrush.Get(), App::instance->settings.OutlineWidth);

	SetBrushColor(m_pTextFillBrush.Get(), App::instance->settings.TextColor, alpha);
	SetBrushColor(m_pTextOutlineBrush.Get(), App::instance->settings.OutlineColor, alpha);

	App::instance->m_pDWriteFactory->CreateTextLayout(
		caption.c_str(),
		(UINT32)caption.length(),
		App::instance->m_pTextFormat.Get(),
		w, h,
		m_pTextLayout.GetAddressOf()
	);

	// HACK outline
	{
		float o = 2;// App::instance->settings.OutlineWidth;
		float oo = o * 1.1f;
		DrawHackOutline(x, y, -o, -o);
		DrawHackOutline(x, y, o, -o);
		DrawHackOutline(x, y, -o, o);
		DrawHackOutline(x, y, o, o);
		DrawHackOutline(x, y, -oo, 0);
		DrawHackOutline(x, y, oo, 0);
		DrawHackOutline(x, y, 0, -oo);
		DrawHackOutline(x, y, 0, oo);
	}

	m_pRenderTarget->DrawTextLayout(
		D2D1::Point2F(x, y),
		m_pTextLayout.Get(),
		m_pTextFillBrush.Get(),
		D2D1_DRAW_TEXT_OPTIONS_NO_SNAP
	);
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

HRESULT ScreenSaverWindow::OnRender()
{
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

	if (App::instance->settings.RenderText)
	{
		if (m_CurrentSprite && m_CurrentSprite->imageInfo)
		{
			auto caption = m_CurrentSprite->imageInfo->folderName;
			if (m_CurrentSprite->imageInfo->dateTaken.length() > 1)
			{
				caption += L" " + m_CurrentSprite->imageInfo->dateTaken;
			}
			auto alpha = m_NextSprite->bitmap ? 1 - m_NextSprite->alpha : 1;
			RenderText(caption, alpha, 20, 20, rtSize.width - 20 * 2, rtSize.height - 20);
		}
	}

	hr = m_pRenderTarget->EndDraw();

	if (hr == D2DERR_RECREATE_TARGET) {
		hr = S_OK;
		DiscardDeviceResources();
	}

	return hr;
}

// Handle window resizing
void ScreenSaverWindow::OnResize(UINT width, UINT height)
{
	if (m_pRenderTarget) {
		m_pRenderTarget->Resize(D2D1::SizeU(width, height));
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
			screen.StartSwap(animate, offset);
		}
	}
	else {
		if (offset > 0) {
			// Going forward: advance first, then animate new screen
			m_CurentScreenIndex = (m_CurentScreenIndex + 1) % (int)m_Screensavers.size();
			m_Screensavers[m_CurentScreenIndex].StartSwap(animate, offset);
		}
		else if (offset < 0) {
			// Going back: animate current screen, then move index backward
			m_Screensavers[m_CurentScreenIndex].StartSwap(animate, offset);
			m_CurentScreenIndex = (m_CurentScreenIndex - 1 + (int)m_Screensavers.size()) % (int)m_Screensavers.size();
		}
	}

	m_DisplayTimer = settings.DisplayDuration;
}

void App::Update(float deltaTime)
{
	m_DisplayTimer -= deltaTime;
	if (m_DisplayTimer <= 0.0f) {
		StartSwap(true, 1);
	}

	for (auto& screen : m_Screensavers) {
		screen.Update(deltaTime);
	}
}

void ScreenSaverWindow::StartSwap(bool animate, int offset)
{
	for (bool success = false; !success;) {
		auto info = App::instance->m_Library.GotoImage(offset);
		Sprite* sprite = m_CurrentSprite;
		if (animate)
		{
			m_FadeTimer = App::instance->settings.FadeDuration;
			m_NextSprite->alpha = 0;
			sprite = m_NextSprite;
		}
		else
		{
			EndFade();
		}

		sprite->imageInfo = info;
		LoadSprite(sprite);
		success = sprite->bitmap;

		if (!m_pRenderTarget)
		{
			// Exit because bitmap will be null.
			return;
		}
	}
}

void ScreenSaverWindow::EndFade()
{
	m_FadeTimer = 0.0f;
	std::swap(m_CurrentSprite, m_NextSprite);
	m_CurrentSprite->alpha = 1;
	m_NextSprite->Clear();
}

void ScreenSaverWindow::Update(float deltaTime)
{
	m_CurrentSprite->Update(deltaTime);
	m_NextSprite->Update(deltaTime);

	if (m_FadeTimer > 0.0f) {
		m_FadeTimer -= deltaTime;
		m_NextSprite->alpha = EaseInOutQuad(1 - m_FadeTimer / App::instance->settings.FadeDuration);
		if (m_FadeTimer <= 0.0f) {
			EndFade();
		}
	}
}

LRESULT CALLBACK App::WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	auto app = App::instance;
	if (message == WM_CREATE) {
		LPCREATESTRUCT pcs = (LPCREATESTRUCT)lParam;
		ScreenSaverWindow* screen = reinterpret_cast<ScreenSaverWindow*>(pcs->lpCreateParams);
		SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(screen));
		screen->m_hwnd = hwnd;
		screen->Update(0);
		screen->OnRender();
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
				for (auto& screen : app->m_Screensavers) {
					DestroyWindow(screen.m_hwnd);
				}
			}
			break;

		case VK_LEFT:
			if (app) app->StartSwap(false, -1);
			break;

		case VK_RIGHT:
			if (app) app->StartSwap(false, 1);
			break;

		case 'T':
			if (app) app->settings.ToggleRenderText();
			break;
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
		if (app)
		{
			for (auto& screen : app->m_Screensavers) {
				DestroyWindow(screen.m_hwnd);
			}
		}
		PostQuitMessage(0);
		break;

	default:
		break;
	}

	return DefWindowProc(hwnd, message, wParam, lParam);
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

void App::RunMessageLoop() {
	const auto targetFrameTime = std::chrono::milliseconds(1000 / 60);

	auto frameStart = std::chrono::steady_clock::now();
	auto previousFrameStart = frameStart;
	bool isRunning = true;

	MSG msg;
	while (isRunning) {
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

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
	HeapSetInformation(nullptr, HeapEnableTerminationOnCorruption, nullptr, 0);

	HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

	if (SUCCEEDED(hr))
	{
		{
			App app;

			if (SUCCEEDED(app.Initialize(hInstance, lpCmdLine))) {
				app.RunMessageLoop();
			}
		}
		CoUninitialize();
	}

	return 0;
}

