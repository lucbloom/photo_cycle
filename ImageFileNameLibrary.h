#pragma once

#include "framework.h"
class SettingsDialog;

class ImageInfo {
public:
	int idx = -1;
	bool isCaching = false;
	std::wstring dateTaken;
	int rotation = -1;
	std::wstring filePath;
	std::wstring folderName;
	std::wstring location;
	ImageInfo() = default;
	ImageInfo(const ImageInfo&) = default;
	ImageInfo& operator=(const ImageInfo&) = default;
	ImageInfo(ImageInfo&&) = default;

	void CacheInfo(SettingsDialog& sets);
	std::wstring GetCaption(SettingsDialog& sets);
};

class ImageFileNameLibrary {
public:
	void SetPaths(const std::vector<std::wstring>& include, const std::vector<std::wstring>& exclude);
	ImageInfo* GotoImage(int offset, int nSkip);

private:
	void ShuffleImages();
	void LoadImages(const std::wstring& directory, const std::vector<std::wstring>& exclude);

	std::vector<ImageInfo*> m_ImageList;
	int m_CurrentImageIdx = 0;
	bool m_IsGoingBackward = false;
};
