#ifndef UNICODE
#define UNICODE
#endif

#include "ScreenSaverWindow.h"
#include "App.h"

float EaseInOutQuad(float t) { return t < 0.5f ? 2 * t * t : -1 + (4 - 2 * t) * t; }

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
		m_CurrentSprite->imageInfo = App::instance->m_Library.GotoImage(1, m_AdapterIndex, (int)App::instance->m_Screensavers.size());
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

void ScreenSaverWindow::LoadSprite(Sprite* sprite)
{
	if (!sprite->imageInfo)
	{
		sprite->bitmap.Reset();
		return;
	}

	sprite->imageInfo->CacheInfo(App::instance->settings);

	auto hr = LoadBitmapFromFileWithTransparencyMixedToBlack(sprite);
	if (SUCCEEDED(hr)) {
		sprite->OnLoad();
	}
}

HRESULT ScreenSaverWindow::LoadBitmapFromFileWithTransparencyMixedToBlack(Sprite* sprite) const
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

	if (m_CurrentSprite && m_CurrentSprite->imageInfo)
	{
		auto caption = m_CurrentSprite->imageInfo->GetCaption(App::instance->settings);
		if (!caption.empty())
		{
			//wchar_t buf[16];
			//swprintf_s(buf, 16, L" #%d", m_CurrentSprite->imageInfo->idx);
			//caption += buf;
			auto alpha = (m_NextSprite->bitmap && m_NextSprite->alpha > 0) ? 1 - m_NextSprite->alpha : 1;
			RenderText(caption, alpha, 20, 20, rtSize.width - 20 * 2, rtSize.height - 20);
		}
	}

	if (m_FadeTimer <= 0 && App::instance->m_ShowButtons && m_CurrentSprite && m_CurrentSprite->imageInfo) {
		float size = 60.0f;
		float margin = 20.0f;
		float spacing = 20.0f;
		auto n = 3;
		auto x = rtSize.width - size - margin - (spacing + size) * (n - 1);
		auto y = rtSize.height - margin - size;

		// Rotate button
		D2D1_RECT_F rotRect = D2D1::RectF(x, y, x + size, y + size);
		m_pRenderTarget->FillRectangle(rotRect, m_pTextFillBrush.Get());
		RenderText(L"↻", 1, rotRect.left, rotRect.top - 15, size, size);
		x += spacing + size;

		// Love button
		D2D1_RECT_F loveRect = D2D1::RectF(x, y, x + size, y + size);
		m_pRenderTarget->FillRectangle(loveRect, m_pTextFillBrush.Get());
		RenderText(L"❤️", 1, loveRect.left, loveRect.top - 10, size, size);
		x += spacing + size;

		// Downvote button
		D2D1_RECT_F downRect = D2D1::RectF(x, y, x + size, y + size);
		m_pRenderTarget->FillRectangle(downRect, m_pTextFillBrush.Get());
		RenderText(L"👎", 1, downRect.left, downRect.top - 15, size, size);
		x += spacing + size;

		m_LoveButtonRect = { (LONG)loveRect.left,(LONG)loveRect.top,(LONG)loveRect.right,(LONG)loveRect.bottom };
		m_DownVoteButtonRect = { (LONG)downRect.left,(LONG)downRect.top,(LONG)downRect.right,(LONG)downRect.bottom };
		m_RotateButtonRect = { (LONG)rotRect.left,(LONG)rotRect.top,(LONG)rotRect.right,(LONG)rotRect.bottom };
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

void ScreenSaverWindow::StartSwap(bool animate, int offset, int numScreens)
{
	m_CurrentImageIdx += offset;

	for (int tries = 0; tries < 100; ++tries) {
		auto info = App::instance->m_Library.GotoImage(m_CurrentImageIdx, m_AdapterIndex, numScreens);
		if (!info) return;

		if (App::instance->m_Downvoted.contains(info->filePath)) {
			continue;
		}

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
		if (sprite->bitmap)
		{
			break;
		}

		if (!m_pRenderTarget)
		{
			// Exit because bitmap will be null.
			return;
		}

		offset = offset >= 0 ? 1 : -1;
		m_CurrentImageIdx += offset;
	}
}

void ScreenSaverWindow::EndFade()
{
	m_FadeTimer = 0.0f;
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
			std::swap(m_CurrentSprite, m_NextSprite);
			EndFade();
		}
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

