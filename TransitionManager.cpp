
#include "TransitionManager.h"
#include <string>

TransitionManager::TransitionManager()
	: currentImage(nullptr)
	, nextImage(nullptr)
	, alpha(0.0f)
	, transitioning(false)
	, lastUpdateTime(0)
	, m_FadeDuration(4)
{
}

void TransitionManager::StartTransition(const ImageInfo* newImagePath) {
	if (nextImage) delete nextImage;
	nextImage = new Gdiplus::Image(newImagePath->filePath.c_str());
	alpha = 0.0f;
	transitioning = true;
	lastUpdateTime = GetTickCount64();
}

void TransitionManager::SetCurrentImage(const ImageInfo* newImagePath) {
	if (nextImage) delete nextImage;
	if (currentImage) delete currentImage;
	nextImage = nullptr;
	currentImage = new Gdiplus::Image(newImagePath->filePath.c_str());
	alpha = 1;
	transitioning = false;
	lastUpdateTime = GetTickCount64();
}

void TransitionManager::Update(HWND hWnd) {
	if (transitioning) {
		ULONGLONG currentTime = GetTickCount64();
		float delta = (currentTime - lastUpdateTime) / (1000.0f * m_FadeDuration);
		alpha += delta;
		if (alpha >= 1.0f) {
			alpha = 1.0f;
			transitioning = false;
			if (currentImage)
			{
				delete currentImage;
			}
			currentImage = nextImage;
			nextImage = nullptr;
		}
		lastUpdateTime = currentTime;
		::InvalidateRect(hWnd, nullptr, true);
	}
}

void DrawImageNoStretch(Gdiplus::Graphics& graphics, Gdiplus::Image* image, RECT rect, const Gdiplus::ImageAttributes* imgAttr) {
	// Get image dimensions
	UINT imgWidth = image->GetWidth();
	UINT imgHeight = image->GetHeight();

	int destWidth = rect.right - rect.left;
	int destHeight = rect.bottom - rect.top;

	// Compute image aspect ratio and rect aspect ratio
	double imgAspect = static_cast<double>(imgWidth) / imgHeight;
	double rectAspect = static_cast<double>(destWidth) / destHeight;

	int drawWidth, drawHeight;

	// Scale image to cover the rect (may crop)
	if (imgAspect > rectAspect) {
		// Image is wider than rect: match height, crop sides
		drawHeight = destHeight;
		drawWidth = static_cast<int>(drawHeight * imgAspect);
	}
	else {
		// Image is taller than rect: match width, crop top/bottom
		drawWidth = destWidth;
		drawHeight = static_cast<int>(drawWidth / imgAspect);
	}

	// Center the image
	int offsetX = (destWidth - drawWidth) / 2;
	int offsetY = (destHeight - drawHeight) / 2;

	// Draw the image to the off-screen buffer
	graphics.DrawImage(image, Gdiplus::Rect(offsetX, offsetY, drawWidth, drawHeight), 0, 0, imgWidth, imgHeight, Gdiplus::UnitPixel, imgAttr);
}

void TransitionManager::Draw(HWND hWnd) {
	if (!currentImage) return;

	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(hWnd, &ps);
	RECT rect;
	GetClientRect(hWnd, &rect);

	// Create an off-screen bitmap to render to
	HDC memDC = CreateCompatibleDC(hdc);
	HBITMAP memBitmap = CreateCompatibleBitmap(hdc, rect.right - rect.left, rect.bottom - rect.top);
	HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);

	// Create a Graphics object for off-screen drawing
	Gdiplus::Graphics offscreenGraphics(memDC);
	offscreenGraphics.SetSmoothingMode(Gdiplus::SmoothingMode::SmoothingModeNone);
	//graphics.SetInterpolationMode(Gdiplus::InterpolationMode::InterpolationModeNearestNeighbor);

	DrawImageNoStretch(offscreenGraphics, currentImage, rect, nullptr);

	if (transitioning && nextImage) {
		Gdiplus::ImageAttributes imgAttr;
		Gdiplus::ColorMatrix colorMatrix = {
			1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 0.0f, alpha, 0.0f,
			0.0f, 0.0f, 0.0f, 0.0f, 1.0f
		};
		imgAttr.SetColorMatrix(&colorMatrix, Gdiplus::ColorMatrixFlagsDefault, Gdiplus::ColorAdjustTypeBitmap);
		DrawImageNoStretch(offscreenGraphics, nextImage, rect, &imgAttr);
	}

	offscreenGraphics.Flush();

	// Copy the off-screen bitmap to the screen
	BitBlt(hdc, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, memDC, 0, 0, SRCCOPY);

	// Cleanup
	SelectObject(memDC, oldBitmap);
	DeleteObject(memBitmap);
	DeleteDC(memDC);
	EndPaint(hWnd, &ps);
}
