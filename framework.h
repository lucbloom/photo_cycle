// header.h : include file for standard system include files,
// or project specific include files
//

#pragma once

#include <SDKDDKVer.h>
//#define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers
// Windows Header Files
#include <windows.h>
#undef min
#undef max

// C RunTime Header Files
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
#include <gdiplus.h>
#include <gdiplusimageattributes.h>
#include <string>
#include <list>
#include <filesystem>
#include <algorithm>
#include <random>
#include <locale>
#include <cwctype>
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>

std::string WStringToUtf8(const std::wstring& wstr);
std::wstring Utf8ToWString(const std::string & str);

struct ImageInfo {
	std::wstring dateTaken;
	std::wstring filePath;
	std::wstring folderName;
	ImageInfo() = default;
	ImageInfo(const ImageInfo&) = default;
	ImageInfo& operator=(const ImageInfo&) = default;
	ImageInfo(ImageInfo&&) = default;
};

