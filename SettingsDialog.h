#include <string>
#include <windows.h>
#include <dwrite.h>
#include <vector>

class SettingsDialog {
public:
	float FadeDuration = 0.5f;
	float DisplayDuration = 3.0f;
	bool ShowDate = true;
	bool ShowLocation = true;
	bool ShowFolder = false;
	std::wstring TextFontName = L"Segoe UI";
	UINT32 TextColor = 0xffffff;
	UINT32 OutlineColor = 0x000000;
	UINT32 BackgroundColor = 0x00000000;
	float FontSize = 48;
	float OutlineWidth = 5;
	bool SyncChange = false;
	bool SingleScreen = false;
	float PanScanFactor = 1;
	DWRITE_FONT_WEIGHT FontWeight = DWRITE_FONT_WEIGHT_BOLD;
	std::vector<std::wstring> IncludePaths;
	std::vector<std::wstring> ExcludePaths;

	SettingsDialog();
	void Show();
	void ToggleShowDate();
	void ToggleShowLocation();
	void ToggleShowFolder();

private:
	static INT_PTR CALLBACK SettingsDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
};
