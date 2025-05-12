
#pragma once
#include "framework.h"
#include <d3d11.h>

class TransitionManager {
public:
	TransitionManager();
	void Init(HWND hWnd);
	void StartTransition(const ImageInfo* newImagePath);
	void SetCurrentImage(const ImageInfo* newImagePath);
	void Update(HWND hWnd);
	void Draw(HWND hWnd);

private:
	//Gdiplus::Image* currentImage;
	//Gdiplus::Image* nextImage;
	ID3D11ShaderResourceView* currentImage;
	ID3D11ShaderResourceView* nextImage;

	float alpha;
	bool transitioning;
	ULONGLONG lastUpdateTime;
	float m_FadeDuration;
};
