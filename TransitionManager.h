
#pragma once
#include "framework.h"

class TransitionManager {
public:
	TransitionManager();
	void StartTransition(const ImageInfo* newImagePath);
	void SetCurrentImage(const ImageInfo* newImagePath);
	void Update(HWND hWnd);
	void Draw(HWND hWnd);

private:
	Gdiplus::Image* currentImage;
	Gdiplus::Image* nextImage;
	float alpha;
	bool transitioning;
	ULONGLONG lastUpdateTime;
	float m_FadeDuration;
};
