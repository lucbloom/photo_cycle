#include "SettingsDialog.h"

#include "resource.h"
#include <shlobj.h>
#include <gdiplus.h>
#include <memory>
#include <iostream>

#pragma comment(lib, "gdiplus.lib")

using namespace Gdiplus;

ULONG_PTR g_GdiplusToken;

static std::wstring EnsureIniFileExists(bool create);

static void WriteInt(LPCWSTR section, LPCWSTR key, int value);
static void WriteBool(LPCWSTR section, LPCWSTR key, bool value);
static void WriteList(LPCWSTR section, LPCWSTR key, const std::vector<std::wstring>& list, wchar_t delimiter = L',');
void WriteFloat(LPCWSTR section, LPCWSTR key, float value);
void WriteString(LPCWSTR section, LPCWSTR key, const std::wstring& value);
void WriteColor(LPCWSTR section, LPCWSTR key, UINT32 color);

static int ReadInt(LPCWSTR section, LPCWSTR key, int defaultValue);
static UINT32 ReadColor(LPCWSTR section, LPCWSTR key, UINT32 defaultValue);
static float ReadFloat(LPCWSTR section, LPCWSTR key, float defaultValue);
static bool ReadBool(LPCWSTR section, LPCWSTR key, bool defaultValue);
static std::wstring ReadString(LPCWSTR section, LPCWSTR key, LPCWSTR defaultValue);
static std::vector<std::wstring> SplitList(const std::wstring& str, wchar_t delimiter = L',');
static std::vector<std::wstring> ReadList(LPCWSTR section, LPCWSTR key);

static COLORREF ShowColorDialog(HWND hWnd, COLORREF initialColor);
static bool ShowFontDialog(HWND hWnd, std::wstring& fontName, float& fontSize, DWRITE_FONT_WEIGHT& weight);
static bool PickFolder(HWND hWnd, std::wstring& selectedPath);
static bool ListBoxContains(HWND hList, const std::wstring& value);

static const wchar_t* INI_SETTINGS = L"Settings";
static const wchar_t* INI_IMAGES = L"Images";
static const wchar_t* INI_RENDER_TEXT = L"RenderText";

SettingsDialog::SettingsDialog()
{
	FadeDuration = ReadFloat(INI_SETTINGS, L"FadeDuration", FadeDuration);
	DisplayDuration = ReadFloat(INI_SETTINGS, L"DisplayDuration", DisplayDuration);
	FontSize = ReadFloat(INI_SETTINGS, L"FontSize", FontSize);
	OutlineWidth = ReadFloat(INI_SETTINGS, L"Outline", OutlineWidth);
	SyncChange = ReadBool(INI_SETTINGS, L"SyncChange", SyncChange);
	SingleScreen = ReadBool(INI_SETTINGS, L"SingleScreen", SingleScreen);
	PanScanFactor = ReadFloat(INI_SETTINGS, L"PanScanFactor", PanScanFactor);
	RenderText = ReadBool(INI_SETTINGS, INI_RENDER_TEXT, RenderText);
	TextFontName = ReadString(INI_SETTINGS, L"Font", TextFontName.c_str());
	TextColor = ReadColor(INI_SETTINGS, L"FontColor", TextColor);
	BackgroundColor = ReadColor(INI_SETTINGS, L"BackgroundColor", BackgroundColor);

	IncludePaths = ReadList(INI_IMAGES, L"Include");
	ExcludePaths = ReadList(INI_IMAGES, L"Exclude");
	if (IncludePaths.empty())
	{
		IncludePaths.push_back(L"C:\\Users\\lucbl\\Pictures\\Personal");
	}
}

void SettingsDialog::Show()
{
	GdiplusStartupInput gdiplusStartupInput;
	GdiplusStartup(&g_GdiplusToken, &gdiplusStartupInput, NULL);

	INT_PTR result = DialogBoxParam(
		GetModuleHandle(nullptr),
		MAKEINTRESOURCE(IDD_SETTINGS_DIALOG),
		nullptr,
		SettingsDlgProc,
		(LPARAM)this);

	if (result == IDOK) {
		WriteFloat(INI_SETTINGS, L"FadeDuration", FadeDuration);
		WriteFloat(INI_SETTINGS, L"DisplayDuration", DisplayDuration);
		WriteFloat(INI_SETTINGS, L"FontSize", FontSize);
		WriteFloat(INI_SETTINGS, L"Outline", OutlineWidth);
		WriteBool(INI_SETTINGS, L"SyncChange", SyncChange);
		WriteBool(INI_SETTINGS, L"SingleScreen", SingleScreen);
		WriteFloat(INI_SETTINGS, L"PanScanFactor", PanScanFactor);
		WriteBool(INI_SETTINGS, INI_RENDER_TEXT, RenderText);
		WriteString(INI_SETTINGS, L"Font", TextFontName.c_str());
		WriteColor(INI_SETTINGS, L"FontColor", TextColor);
		WriteColor(INI_SETTINGS, L"BackgroundColor", BackgroundColor);
		WriteList(INI_IMAGES, L"Include", IncludePaths);
		WriteList(INI_IMAGES, L"Exclude", ExcludePaths);
	}

	GdiplusShutdown(g_GdiplusToken);
}

void LoadAndScaleImageToFitDialog(HWND hDlg)
{
	std::unique_ptr<Gdiplus::Bitmap> source;
	{
		HRSRC hRes = FindResource(nullptr, MAKEINTRESOURCE(IDR_MY_IMAGE), RT_RCDATA);
		DWORD dwSize = SizeofResource(nullptr, hRes);
		HGLOBAL hResData = LoadResource(nullptr, hRes);
		void* pData = LockResource(hResData);
		WCHAR tempPath[MAX_PATH];
		GetTempPathW(MAX_PATH, tempPath);
		std::wstring tempFile = std::wstring(tempPath) + L"photo_cycle_logo__temp.jpg";
		FILE* f = nullptr;
		errno_t err = _wfopen_s(&f, tempFile.c_str(), L"wb");
		fwrite(pData, 1, dwSize, f);
		fclose(f);
		source.reset(Gdiplus::Bitmap::FromFile(tempFile.c_str()));
		DeleteFileW(tempFile.c_str());
	}
	
	RECT rect;
	GetClientRect(hDlg, &rect);

	int srcWidth = source->GetWidth();
	int srcHeight = source->GetHeight();
	int maxHeight = 160;
	int destWidth = rect.right;
	int destHeight = maxHeight;

	// Maintain aspect ratio while fitting within maxHeight
	double srcAspect = static_cast<double>(srcWidth) / srcHeight;
	destWidth = static_cast<int>(destHeight * srcAspect);
	if (destWidth > rect.right) {
		destWidth = rect.right;
		destHeight = static_cast<int>(destWidth / srcAspect);
	}

	// Create and scale the image
	Gdiplus::Bitmap target(destWidth, destHeight, PixelFormat32bppARGB);
	Gdiplus::Graphics g(&target);
	g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
	g.DrawImage(source.get(), 0, 0, destWidth, destHeight);

	HBITMAP hBitmap = nullptr;
	target.GetHBITMAP(Gdiplus::Color(0, 0, 0), &hBitmap);

	HWND hControl = GetDlgItem(hDlg, IDC_IMAGE_PICTURE);
	SendMessage(hControl, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hBitmap);
	SetWindowPos(hControl, nullptr, (rect.right - destWidth) / 2, (maxHeight - destHeight) / 2, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
}

void SettingsDialog::ToggleRenderText()
{
	RenderText = !RenderText;
	WriteBool(INI_SETTINGS, INI_RENDER_TEXT, RenderText);
}

static void SetFloat(HWND hDlg, int id, float value) {
	wchar_t buf[32];
	swprintf_s(buf, L"%.2f", value);
	SetDlgItemTextW(hDlg, id, buf);
}

static float GetFloat(HWND hDlg, int id) {
	wchar_t buf[32];
	GetDlgItemTextW(hDlg, id, buf, 32);
	return static_cast<float>(wcstod(buf, nullptr));
}

static void SetHex(HWND hDlg, int id, UINT32 value) {
	wchar_t buf[32];
	swprintf_s(buf, L"%06X", value);
	SetDlgItemTextW(hDlg, id, buf);
}

static UINT32 GetHex(HWND hDlg, int id) {
	wchar_t buf[32];
	GetDlgItemTextW(hDlg, id, buf, 32);
	return wcstoul(buf, nullptr, 16);
}

static void PopulateFontWeightCombo(HWND hDlg, int selectedWeight) {
	HWND hCombo = GetDlgItem(hDlg, IDC_FONT_WEIGHT);
	SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)L"Thin");
	SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)L"Light");
	SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)L"Regular");
	SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)L"Bold");

	int index = 2;
	switch (selectedWeight) {
	case DWRITE_FONT_WEIGHT_THIN: index = 0; break;
	case DWRITE_FONT_WEIGHT_LIGHT: index = 1; break;
	case DWRITE_FONT_WEIGHT_REGULAR: index = 2; break;
	case DWRITE_FONT_WEIGHT_BOLD: index = 3; break;
	}
	SendMessage(hCombo, CB_SETCURSEL, index, 0);
}

static DWRITE_FONT_WEIGHT GetSelectedFontWeight(HWND hDlg) {
	HWND hCombo = GetDlgItem(hDlg, IDC_FONT_WEIGHT);
	int index = (int)SendMessage(hCombo, CB_GETCURSEL, 0, 0);
	switch (index) {
	case 0: return DWRITE_FONT_WEIGHT_THIN;
	case 1: return DWRITE_FONT_WEIGHT_LIGHT;
	case 2: return DWRITE_FONT_WEIGHT_REGULAR;
	case 3: return DWRITE_FONT_WEIGHT_BOLD;
	default: return DWRITE_FONT_WEIGHT_REGULAR;
	}
}

INT_PTR CALLBACK SettingsDialog::SettingsDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	static SettingsDialog* pSettings = nullptr;

	switch (message) {
	case WM_INITDIALOG:
	{
		// Center the dialog on screen
		RECT rcDlg;
		GetWindowRect(hDlg, &rcDlg);

		int dlgWidth = rcDlg.right - rcDlg.left;
		int dlgHeight = rcDlg.bottom - rcDlg.top;

		// Get screen size
		int screenWidth = GetSystemMetrics(SM_CXSCREEN);
		int screenHeight = GetSystemMetrics(SM_CYSCREEN);

		int x = (screenWidth - dlgWidth) / 2;
		int y = (screenHeight - dlgHeight) / 2;

		SetWindowPos(hDlg, nullptr, x, y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);

		pSettings = (SettingsDialog*)lParam;

		SetFloat(hDlg, IDC_FADE_DURATION, pSettings->FadeDuration);
		SetFloat(hDlg, IDC_DISPLAY_DURATION, pSettings->DisplayDuration);
		CheckDlgButton(hDlg, IDC_RENDER_TEXT, pSettings->RenderText ? BST_CHECKED : BST_UNCHECKED);
		SetDlgItemTextW(hDlg, IDC_FONT_NAME, pSettings->TextFontName.c_str());
		SetHex(hDlg, IDC_TEXT_COLOR, pSettings->TextColor);
		SetHex(hDlg, IDC_BACKGROUND_COLOR, pSettings->BackgroundColor);
		SetFloat(hDlg, IDC_FONT_SIZE, pSettings->FontSize);
		SetFloat(hDlg, IDC_OUTLINE_WIDTH, pSettings->OutlineWidth);
		CheckDlgButton(hDlg, IDC_SYNC_CHANGE, pSettings->SyncChange ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hDlg, IDC_SINGLE_SCREEN, pSettings->SingleScreen ? BST_CHECKED : BST_UNCHECKED);
		SetFloat(hDlg, IDC_PANSCAN_FACTOR, pSettings->PanScanFactor);
		PopulateFontWeightCombo(hDlg, pSettings->FontWeight);
		for (const auto& path : pSettings->IncludePaths) {
			SendDlgItemMessageW(hDlg, IDC_INCLUDE_LIST, LB_ADDSTRING, 0, (LPARAM)path.c_str());
		}

		for (const auto& path : pSettings->ExcludePaths) {
			SendDlgItemMessageW(hDlg, IDC_EXCLUDE_LIST, LB_ADDSTRING, 0, (LPARAM)path.c_str());
		}

		LoadAndScaleImageToFitDialog(hDlg);

		//HICON hIcon = (HICON)LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_PHOTOCYCLE), IMAGE_ICON, 0, 0, LR_DEFAULTCOLOR);
		//SendMessage(hDlg, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
		//SendMessage(hDlg, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);

		return TRUE;
	}

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDOK:
		{
			pSettings->FadeDuration = GetFloat(hDlg, IDC_FADE_DURATION);
			pSettings->DisplayDuration = GetFloat(hDlg, IDC_DISPLAY_DURATION);
			pSettings->RenderText = IsDlgButtonChecked(hDlg, IDC_RENDER_TEXT) == BST_CHECKED;
			{
				wchar_t buf[256];
				GetDlgItemTextW(hDlg, IDC_FONT_NAME, buf, 256);
				pSettings->TextFontName = buf;
			}
			pSettings->TextColor = GetHex(hDlg, IDC_TEXT_COLOR);
			pSettings->BackgroundColor = GetHex(hDlg, IDC_BACKGROUND_COLOR);
			pSettings->FontSize = GetFloat(hDlg, IDC_FONT_SIZE);
			pSettings->OutlineWidth = GetFloat(hDlg, IDC_OUTLINE_WIDTH);
			pSettings->SyncChange = IsDlgButtonChecked(hDlg, IDC_SYNC_CHANGE) == BST_CHECKED;
			pSettings->SingleScreen = IsDlgButtonChecked(hDlg, IDC_SINGLE_SCREEN) == BST_CHECKED;
			pSettings->PanScanFactor = GetFloat(hDlg, IDC_PANSCAN_FACTOR);
			pSettings->FontWeight = GetSelectedFontWeight(hDlg);
			// Clear old paths
			pSettings->IncludePaths.clear();
			pSettings->ExcludePaths.clear();

			// Extract IncludePaths
			int count = (int)SendDlgItemMessageW(hDlg, IDC_INCLUDE_LIST, LB_GETCOUNT, 0, 0);
			for (int i = 0; i < count; ++i) {
				wchar_t buf[MAX_PATH];
				SendDlgItemMessageW(hDlg, IDC_INCLUDE_LIST, LB_GETTEXT, i, (LPARAM)buf);
				pSettings->IncludePaths.emplace_back(buf);
			}

			// Extract ExcludePaths
			count = (int)SendDlgItemMessageW(hDlg, IDC_EXCLUDE_LIST, LB_GETCOUNT, 0, 0);
			for (int i = 0; i < count; ++i) {
				wchar_t buf[MAX_PATH];
				SendDlgItemMessageW(hDlg, IDC_EXCLUDE_LIST, LB_GETTEXT, i, (LPARAM)buf);
				pSettings->ExcludePaths.emplace_back(buf);
			}

			EndDialog(hDlg, IDOK);
			return TRUE;
		}

		case IDCANCEL:
			EndDialog(hDlg, IDCANCEL);
			return TRUE;

		case IDC_TEXT_COLOR_BTN:
			pSettings->TextColor = ShowColorDialog(hDlg, pSettings->TextColor);
			return TRUE;

		case IDC_BACKGROUND_COLOR_BTN:
			pSettings->BackgroundColor = ShowColorDialog(hDlg, pSettings->BackgroundColor);
			return TRUE;

		case IDC_FONT_SELECT_BTN:
			if (ShowFontDialog(hDlg, pSettings->TextFontName, pSettings->FontSize, pSettings->FontWeight)) {
				SetDlgItemTextW(hDlg, IDC_FONT_NAME, pSettings->TextFontName.c_str());
				SetFloat(hDlg, IDC_FONT_SIZE, pSettings->FontSize);
				PopulateFontWeightCombo(hDlg, pSettings->FontWeight);
			}
			return TRUE;

		case IDC_INCLUDE_ADD: {
			std::wstring folder;
			if (PickFolder(hDlg, folder)) {
				if (!ListBoxContains(GetDlgItem(hDlg, IDC_INCLUDE_LIST), folder))
					SendDlgItemMessageW(hDlg, IDC_INCLUDE_LIST, LB_ADDSTRING, 0, (LPARAM)folder.c_str());
			}
			return TRUE;
		}

		case IDC_EXCLUDE_ADD: {
			std::wstring folder;
			if (PickFolder(hDlg, folder)) {
				if (!ListBoxContains(GetDlgItem(hDlg, IDC_EXCLUDE_LIST), folder))
					SendDlgItemMessageW(hDlg, IDC_EXCLUDE_LIST, LB_ADDSTRING, 0, (LPARAM)folder.c_str());
			}
			return TRUE;
		}

		}

		break;
	}
	return FALSE;
}

static COLORREF ShowColorDialog(HWND hWnd, COLORREF initialColor) {
	CHOOSECOLOR cc = { sizeof(cc) };
	static COLORREF customColors[16] = {};
	cc.hwndOwner = hWnd;
	cc.lpCustColors = customColors;
	cc.rgbResult = initialColor;
	cc.Flags = CC_RGBINIT | CC_FULLOPEN;

	if (ChooseColor(&cc))
		return cc.rgbResult;

	return initialColor;
}

static bool ShowFontDialog(HWND hWnd, std::wstring& fontName, float& fontSize, DWRITE_FONT_WEIGHT& weight) {
	LOGFONT lf = {};
	wcscpy_s(lf.lfFaceName, fontName.c_str());
	lf.lfHeight = -MulDiv((int)fontSize, GetDeviceCaps(GetDC(hWnd), LOGPIXELSY), 72);
	lf.lfWeight = weight;

	CHOOSEFONT cf = { sizeof(cf) };
	cf.hwndOwner = hWnd;
	cf.lpLogFont = &lf;
	cf.Flags = CF_INITTOLOGFONTSTRUCT | CF_SCREENFONTS | CF_EFFECTS;

	if (ChooseFont(&cf)) {
		fontName = lf.lfFaceName;
		fontSize = static_cast<float>(-lf.lfHeight * 72 / GetDeviceCaps(GetDC(hWnd), LOGPIXELSY));
		weight = static_cast<DWRITE_FONT_WEIGHT>(lf.lfWeight);
		return true;
	}
	return false;
}

static bool ListBoxContains(HWND hList, const std::wstring& value) {
	int count = (int)SendMessage(hList, LB_GETCOUNT, 0, 0);
	for (int i = 0; i < count; ++i) {
		wchar_t buf[MAX_PATH];
		SendMessage(hList, LB_GETTEXT, i, (LPARAM)buf);
		if (_wcsicmp(buf, value.c_str()) == 0)
			return true;
	}
	return false;
}


static bool PickFolder(HWND hWnd, std::wstring& selectedPath) {
	BROWSEINFOW bi = { 0 };
	wchar_t path[MAX_PATH];
	bi.hwndOwner = hWnd;
	bi.lpszTitle = L"Select Folder";
	bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
	LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
	if (pidl && SHGetPathFromIDListW(pidl, path)) {
		selectedPath = path;
		CoTaskMemFree(pidl);
		return true;
	}
	return false;
}

void Report(BOOL result, const std::wstring& path)
{
	if (!result) {
		DWORD error = GetLastError();
		wchar_t buf[256];
		swprintf_s(buf, L"Write failed with error code: %lu '%s'", error, path.c_str());
		wprintf(buf);
		MessageBoxW(nullptr, buf, L"Write Error", MB_ICONERROR | MB_OK);
	}
}

void WriteInt(LPCWSTR section, LPCWSTR key, int value) {
	auto file = EnsureIniFileExists(true);
	wchar_t buffer[32];
	swprintf_s(buffer, L"%d", value);
	Report(WritePrivateProfileString(section, key, buffer, file.c_str()), file);
}

void WriteBool(LPCWSTR section, LPCWSTR key, bool value) {
	auto file = EnsureIniFileExists(true);
	Report(WritePrivateProfileString(section, key, value ? L"1" : L"0", file.c_str()), file);
}

void WriteList(LPCWSTR section, LPCWSTR key, const std::vector<std::wstring>& list, wchar_t delimiter) {
	auto file = EnsureIniFileExists(true);
	std::wstring joined;
	for (size_t i = 0; i < list.size(); ++i) {
		if (i > 0) joined += delimiter;
		joined += list[i];
	}
	Report(WritePrivateProfileString(section, key, joined.c_str(), file.c_str()), file);
}

void WriteFloat(LPCWSTR section, LPCWSTR key, float value) {
	auto file = EnsureIniFileExists(true);
	wchar_t buf[64];
	swprintf_s(buf, L"%.6f", value); // Format with fixed 6 decimals

	// Trim trailing zeroes and possibly the decimal dot
	wchar_t* p = buf + wcslen(buf) - 1;
	while (p > buf && *p == L'0') --p;
	if (*p == L'.') --p;
	*(p + 1) = L'\0';

	Report(WritePrivateProfileString(section, key, buf, file.c_str()), file);
}

void WriteString(LPCWSTR section, LPCWSTR key, const std::wstring& value) {
	auto file = EnsureIniFileExists(true);
	Report(WritePrivateProfileString(section, key, value.c_str(), file.c_str()), file);
}

void WriteColor(LPCWSTR section, LPCWSTR key, UINT32 color) {
	auto file = EnsureIniFileExists(true);
	wchar_t buf[16];
	swprintf_s(buf, L"%08X", color); // Use %06X for RGB only
	Report(WritePrivateProfileString(section, key, buf, file.c_str()), file);
}

int ReadInt(LPCWSTR section, LPCWSTR key, int defaultValue) {
	auto file = EnsureIniFileExists(false);
	return GetPrivateProfileInt(section, key, defaultValue, file.c_str());
}

UINT32 ReadColor(LPCWSTR section, LPCWSTR key, UINT32 defaultValue) {
	auto file = EnsureIniFileExists(false);
	WCHAR buffer[64];
	GetPrivateProfileString(section, key, nullptr, buffer, 64, file.c_str());
	return buffer[0] ? wcstoul(buffer, nullptr, 16) : defaultValue;
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

std::wstring ReadString(LPCWSTR section, LPCWSTR key, LPCWSTR defaultValue) {
	auto file = EnsureIniFileExists(false);
	WCHAR buffer[2048] = {};
	GetPrivateProfileString(section, key, defaultValue, buffer, 2048, file.c_str());
	return buffer;
}

std::vector<std::wstring> SplitList(const std::wstring& str, wchar_t delimiter) {
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
	return SplitList(ReadString(section, key, nullptr));
}

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
			DWORD err = GetLastError();
			if (err != ERROR_ALREADY_EXISTS) {
				wprintf(L"Error creating directory '%s' (error code: %lu)\n", path, err);
				return L"";
			}
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

