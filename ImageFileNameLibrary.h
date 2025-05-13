#pragma once

#include "framework.h"

class ImageInfo {
public:
	std::wstring dateTaken;
	std::wstring filePath;
	std::wstring folderName;
	ImageInfo() = default;
	ImageInfo(const ImageInfo&) = default;
	ImageInfo& operator=(const ImageInfo&) = default;
	ImageInfo(ImageInfo&&) = default;

	void CacheInfo();
};

class ImageFileNameLibrary {
public:
	void SetPaths(const std::vector<std::wstring>& include, const std::vector<std::wstring>& exclude);
	ImageInfo* GotoImage(int offset);

private:
	void ShuffleImages();
	void LoadImages(const std::wstring& directory, const std::vector<std::wstring>& exclude);

	std::vector<ImageInfo*> m_ImageList;
	int m_CurrentImageIdx;
};
