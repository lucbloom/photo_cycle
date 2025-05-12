
#include "TransitionManager.h"

#include <d3d11.h>
#include <dxgi.h>
#include <wincodec.h>
#include <d3dcompiler.h>

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

ID3D11Device* g_pDevice = nullptr;
ID3D11DeviceContext* g_pContext = nullptr;
IDXGISwapChain* g_pSwapChain = nullptr;
ID3D11RenderTargetView* g_pRenderTargetView = nullptr;
ID3D11SamplerState* g_SamplerState = nullptr;

ID3D11VertexShader* g_pVertexShader = nullptr;
ID3D11PixelShader* g_pPixelShader = nullptr;

//struct Vertex
//{
//	DirectX::XMFLOAT3 position; // Position in 3D space (XYZ)
//	DirectX::XMFLOAT2 texCoord; // Texture coordinates (UV)
//};

HRESULT InitD3D(HWND hWnd) {
	DXGI_SWAP_CHAIN_DESC scd = {};
	scd.BufferCount = 1;
	scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	scd.OutputWindow = hWnd;
	scd.SampleDesc.Count = 1;
	scd.Windowed = TRUE;
	scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	HRESULT hr = D3D11CreateDeviceAndSwapChain(
		nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
		nullptr, 0, D3D11_SDK_VERSION,
		&scd, &g_pSwapChain, &g_pDevice, nullptr, &g_pContext);

	if (FAILED(hr)) return hr;

	ID3D11Texture2D* pBackBuffer;
	g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
	g_pDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_pRenderTargetView);
	pBackBuffer->Release();

	g_pContext->OMSetRenderTargets(1, &g_pRenderTargetView, nullptr);

	return S_OK;
}

HRESULT LoadTextureFromFile(const wchar_t* fileName, ID3D11ShaderResourceView** ppSRV) {
	IWICImagingFactory* pIWICFactory = nullptr;
	IWICBitmapDecoder* pDecoder = nullptr;
	IWICBitmapFrameDecode* pFrame = nullptr;
	IWICFormatConverter* pConverter = nullptr;

	// Initialize WIC
	HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pIWICFactory));
	if (FAILED(hr)) return hr;

	// Decode the image file
	hr = pIWICFactory->CreateDecoderFromFilename(fileName, nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &pDecoder);
	if (FAILED(hr)) return hr;

	// Get the first frame of the image
	hr = pDecoder->GetFrame(0, &pFrame);
	if (FAILED(hr)) return hr;

	// Convert to 32bpp format (A8R8G8B8)
	hr = pIWICFactory->CreateFormatConverter(&pConverter);
	if (FAILED(hr)) return hr;

	hr = pConverter->Initialize(pFrame, GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, nullptr, 0.0f, WICBitmapPaletteTypeCustom);
	if (FAILED(hr)) return hr;

	D3D11_TEXTURE2D_DESC desc = {};
	pConverter->GetSize(&desc.Width, &desc.Height);
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	UINT stride = desc.Width * 4;
	UINT imageSize = stride * desc.Height;
	std::vector<BYTE> pixels(imageSize);
	hr = pConverter->CopyPixels(nullptr, stride, imageSize, pixels.data());
	if (FAILED(hr)) return hr;

	D3D11_SUBRESOURCE_DATA initData = {};
	initData.pSysMem = pixels.data();
	initData.SysMemPitch = stride;

	// Create a texture from the WIC bitmap
	ID3D11Texture2D* pTexture = nullptr;
	hr = g_pDevice->CreateTexture2D(&desc, &initData, &pTexture);
	if (FAILED(hr)) return hr;

	// Create the shader resource view
	hr = g_pDevice->CreateShaderResourceView(pTexture, nullptr, ppSRV);
	if (FAILED(hr)) return hr;

	// Cleanup
	pTexture->Release();
	pConverter->Release();
	pFrame->Release();
	pDecoder->Release();
	pIWICFactory->Release();

	return S_OK;
}

HRESULT CompileShader(const WCHAR* shaderFileName, const char* entryPoint, const char* target, ID3DBlob** ppBlob) {
	UINT flags = D3D10_SHADER_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
	flags |= D3D10_SHADER_DEBUG;
#endif

	ID3DBlob* pErrorBlob = nullptr;
	HRESULT hr = D3DCompileFromFile(shaderFileName, nullptr, nullptr, entryPoint, target, flags, 0, ppBlob, &pErrorBlob);

	if (FAILED(hr)) {
		if (pErrorBlob) {
			OutputDebugStringA((char*)pErrorBlob->GetBufferPointer());
			pErrorBlob->Release();
		}
		return hr;
	}

	if (pErrorBlob) pErrorBlob->Release();

	return hr;
}

HRESULT LoadVertexShader() {
	ID3DBlob* pVSBlob = nullptr;
	HRESULT hr = CompileShader(L"vertex_shader.hlsl", "VSMain", "vs_5_0", &pVSBlob);
	if (FAILED(hr)) return hr;

	hr = g_pDevice->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), nullptr, &g_pVertexShader);
	pVSBlob->Release();

	return hr;
}

HRESULT LoadPixelShader() {
	ID3DBlob* pPSBlob = nullptr;
	HRESULT hr = CompileShader(L"pixel_shader.hlsl", "PSMain", "ps_5_0", &pPSBlob);
	if (FAILED(hr)) return hr;

	hr = g_pDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), nullptr, &g_pPixelShader);
	pPSBlob->Release();

	return hr;
}

TransitionManager::TransitionManager()
	: currentImage(nullptr)
	, nextImage(nullptr)
	, alpha(0.0f)
	, transitioning(false)
	, lastUpdateTime(0)
	, m_FadeDuration(4)
{
}

void TransitionManager::Init(HWND hWnd) {
//	HRESULT hr = InitD3D(hWnd);
//	if (FAILED(hr)) {
//		MessageBoxLastError();
//		return;
//	}
//	hr = LoadVertexShader();
//	if (FAILED(hr)) {
//		MessageBoxLastError();
//		return;
//	}
//	hr = LoadPixelShader();
//	if (FAILED(hr)) {
//		MessageBoxLastError();
//		return;
//	}
//
//	D3D11_INPUT_ELEMENT_DESC layout[] =
//	{
//		// Position
//		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
//		// Texture coordinates (UV)
//		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
//	};
//
//	UINT numElements = ARRAYSIZE(layout);
//	ID3D11InputLayout* pInputLayout = nullptr;
//	hr = g_pDevice->CreateInputLayout(layout, numElements,
//		pCompiledShaderBlob->GetBufferPointer(),
//		pCompiledShaderBlob->GetBufferSize(),
//		&pInputLayout);
//
//
//	UINT stride = sizeof(Vertex);
//	UINT offset = 0;
//	g_pContext->IASetVertexBuffers(0, 1, &pVertexBuffer, &stride, &offset);
//	g_pContext->IASetInputLayout(nullptr);
//	g_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
//	g_pContext->PSSetSamplers(0, 1, &g_SamplerState);
//
//	D3D11_VIEWPORT vp = {};
//	vp.Width = (FLOAT)width;
//	vp.Height = (FLOAT)height;
//	vp.MinDepth = 0.0f;
//	vp.MaxDepth = 1.0f;
//	vp.TopLeftX = 0;
//	vp.TopLeftY = 0;
//
//	g_pContext->RSSetViewports(1, &vp);
//
//	g_pContext->OMSetRenderTargets(1, &pRenderTargetView, nullptr);
}

void TransitionManager::StartTransition(const ImageInfo* newImagePath) {
	//if (nextImage) { delete nextImage; nextImage = null; }
	//nextImage = new Gdiplus::Image(newImagePath->filePath.c_str());
	if (nextImage) { nextImage->Release(); nextImage = NULL; }
	LoadTextureFromFile(newImagePath->filePath.c_str(), &nextImage);
	alpha = 0.0f;
	transitioning = true;
	lastUpdateTime = GetTickCount64();
}

void TransitionManager::SetCurrentImage(const ImageInfo* newImagePath) {
	//if (currentImage) { delete currentImage; currentImage = null; }
	//if (nextImage) { delete nextImage; nextImage = null; }
	if (currentImage) { currentImage->Release(); currentImage = NULL; }
	if (nextImage) { nextImage->Release(); nextImage = NULL; }
	//currentImage = new Gdiplus::Image(newImagePath->filePath.c_str());
	LoadTextureFromFile(newImagePath->filePath.c_str(), &currentImage);
	nextImage = nullptr;
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
			//if (currentImage) { delete currentImage; }
			if (currentImage) { currentImage->Release(); }
			currentImage = nextImage;
			nextImage = nullptr;
		}
		lastUpdateTime = currentTime;
		//::InvalidateRect(hWnd, nullptr, true);
	}
}

//void DrawImageNoStretch(Gdiplus::Graphics& graphics, Gdiplus::Image* image, RECT rect, const Gdiplus::ImageAttributes* imgAttr) {
//	// Get image dimensions
//	UINT imgWidth = image->GetWidth();
//	UINT imgHeight = image->GetHeight();
//
//	int destWidth = rect.right - rect.left;
//	int destHeight = rect.bottom - rect.top;
//
//	// Compute image aspect ratio and rect aspect ratio
//	double imgAspect = static_cast<double>(imgWidth) / imgHeight;
//	double rectAspect = static_cast<double>(destWidth) / destHeight;
//
//	int drawWidth, drawHeight;
//
//	// Scale image to cover the rect (may crop)
//	if (imgAspect > rectAspect) {
//		// Image is wider than rect: match height, crop sides
//		drawHeight = destHeight;
//		drawWidth = static_cast<int>(drawHeight * imgAspect);
//	}
//	else {
//		// Image is taller than rect: match width, crop top/bottom
//		drawWidth = destWidth;
//		drawHeight = static_cast<int>(drawWidth / imgAspect);
//	}
//
//	// Center the image
//	int offsetX = (destWidth - drawWidth) / 2;
//	int offsetY = (destHeight - drawHeight) / 2;
//
//	// Draw the image to the off-screen buffer
//	graphics.DrawImage(image, Gdiplus::Rect(offsetX, offsetY, drawWidth, drawHeight), 0, 0, imgWidth, imgHeight, Gdiplus::UnitPixel, imgAttr);
//}
//
//void TransitionManager::Draw(HWND hWnd) {
//	if (!currentImage) return;
//
//	PAINTSTRUCT ps;
//	HDC hdc = BeginPaint(hWnd, &ps);
//	RECT rect;
//	GetClientRect(hWnd, &rect);
//
//	// Create an off-screen bitmap to render to
//	HDC memDC = CreateCompatibleDC(hdc);
//	HBITMAP memBitmap = CreateCompatibleBitmap(hdc, rect.right - rect.left, rect.bottom - rect.top);
//	HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);
//
//	// Create a Graphics object for off-screen drawing
//	Gdiplus::Graphics offscreenGraphics(memDC);
//	offscreenGraphics.SetSmoothingMode(Gdiplus::SmoothingMode::SmoothingModeNone);
//	//graphics.SetInterpolationMode(Gdiplus::InterpolationMode::InterpolationModeNearestNeighbor);
//
//	DrawImageNoStretch(offscreenGraphics, currentImage, rect, nullptr);
//
//	if (transitioning && nextImage) {
//		Gdiplus::ImageAttributes imgAttr;
//		Gdiplus::ColorMatrix colorMatrix = {
//			1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
//			0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
//			0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
//			0.0f, 0.0f, 0.0f, alpha, 0.0f,
//			0.0f, 0.0f, 0.0f, 0.0f, 1.0f
//		};
//		imgAttr.SetColorMatrix(&colorMatrix, Gdiplus::ColorMatrixFlagsDefault, Gdiplus::ColorAdjustTypeBitmap);
//		DrawImageNoStretch(offscreenGraphics, nextImage, rect, &imgAttr);
//	}
//
//	offscreenGraphics.Flush();
//
//	// Copy the off-screen bitmap to the screen
//	BitBlt(hdc, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, memDC, 0, 0, SRCCOPY);
//
//	// Cleanup
//	SelectObject(memDC, oldBitmap);
//	DeleteObject(memBitmap);
//	DeleteDC(memDC);
//	EndPaint(hWnd, &ps);
//}


void TransitionManager::Draw(HWND hWnd) {
	const float clearColor[4] = { 0, 0, 0, 1 };
	g_pContext->ClearRenderTargetView(g_pRenderTargetView, clearColor);

	// Set shaders
	g_pContext->VSSetShader(g_pVertexShader, nullptr, 0);
	g_pContext->PSSetShader(g_pPixelShader, nullptr, 0);

	g_pContext->PSSetShaderResources(0, 1, &currentImage);
	g_pContext->Draw(4, 0); // 4 vertices, starting at index 0

	g_pContext->PSSetShaderResources(0, 1, &nextImage);
	g_pContext->Draw(4, 0); // 4 vertices, starting at index 0

	g_pSwapChain->Present(1, 0);  // Present the back buffer
}

