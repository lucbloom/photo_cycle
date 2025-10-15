#ifndef UNICODE
#define UNICODE
#endif

#include <string>
#include <d2d1.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;
class ImageInfo;

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
	int m_CurrentImageIdx = 0;
	HWND m_hwnd = nullptr;
	RECT m_MaximizedRect = { CW_USEDEFAULT, CW_USEDEFAULT, 640, 480 };
	RECT m_LoveButtonRect = {}, m_DownVoteButtonRect = {}, m_RotateButtonRect = {};
	ComPtr<ID2D1HwndRenderTarget> m_pRenderTarget = nullptr;
	Sprite* m_CurrentSprite = new Sprite();
	Sprite* m_NextSprite = new Sprite();
	ComPtr<ID2D1SolidColorBrush> m_pTextOutlineBrush;
	ComPtr<ID2D1SolidColorBrush> m_pTextFillBrush;
	ComPtr<IDWriteTextLayout> m_pTextLayout = nullptr;

	float m_FadeTimer = 0;

	HRESULT CreateDeviceResources();
	HRESULT LoadBitmapFromFileWithTransparencyMixedToBlack(Sprite* sprite) const;
	void DiscardDeviceResources();
	void LoadSprite(Sprite* sprite);
	void DrawSprite(Sprite* sprite);
	HRESULT OnRender();
	void RenderText(const std::wstring& caption, float alpha, float x, float y, float w, float h);
	void OnResize(UINT width, UINT height);
	RECT GetMaximizedRect();
	void Update(float deltaTime);
	void StartSwap(bool animate, int offset, int numScreens);
	void EndFade();
	void DrawHackOutline(float x, float y, float xo, float yo);
};
