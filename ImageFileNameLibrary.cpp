
#include "ImageFileNameLibrary.h"

#define WIN32_LEAN_AND_MEAN
#include <exiv2.hpp>

void ImageFileNameLibrary::SetPaths(const std::vector<std::wstring>& include, const std::vector<std::wstring>& exclude)
{
	for (const auto& dir : include) {
		LoadImages(dir, exclude);
	}
	ShuffleImages();
	m_CurrentImageIdx = 0;
}

static std::wstring NormalizePath(const std::filesystem::path& path) {
	// Convert to weakly_canonical absolute path
	std::wstring canonical = std::filesystem::weakly_canonical(path).wstring();

	// Convert to lowercase for case-insensitive comparison
	std::transform(canonical.begin(), canonical.end(), canonical.begin(),
		[](wchar_t c) { return std::towlower(c); });

	return canonical;
}

std::string WStringToUtf8(const std::wstring& wstr) {
	int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
	std::string strTo(size_needed - 1, 0); // exclude null terminator
	WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &strTo[0], size_needed, nullptr, nullptr);
	return strTo;
}

std::wstring Utf8ToWString(const std::string& str) {
	if (str.empty()) return {};

	int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
	std::wstring wstr(size - 1, 0);
	MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, wstr.data(), size);
	return wstr;
}

std::wstring ExtractDateTaken(const std::wstring& imagePath) {
	try {
		auto image = Exiv2::ImageFactory::open(WStringToUtf8(imagePath));
		image->readMetadata();

		Exiv2::ExifData& exifData = image->exifData();
		if (exifData.empty()) {
			std::wcerr << L"No EXIF data found" << std::endl;
			return L"";
		}

		Exiv2::ExifData::iterator dateTimeTag = exifData.findKey(Exiv2::ExifKey("Exif.Photo.DateTimeOriginal"));
		if (dateTimeTag != exifData.end()) {
			return Utf8ToWString(dateTimeTag->toString());  // Return the DateTimeOriginal value
		}
		else {
			std::wcerr << L"No DateTimeOriginal tag found" << std::endl;
			return L"";
		}
	}
	catch (const Exiv2::Error& e) {
		std::wcerr << L"Error reading EXIF data: " << e.what() << std::endl;
		return L"";
	}
}

std::wstring GetFileCreationDate(const std::wstring& filePath) {
	try {
		// Open the file
		HANDLE hFile = CreateFile(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
		if (hFile == INVALID_HANDLE_VALUE) {
			throw std::runtime_error("Unable to open file.");
		}

		// Retrieve the file creation time
		FILETIME creationTime;
		if (!GetFileTime(hFile, &creationTime, nullptr, nullptr)) {
			CloseHandle(hFile);
			throw std::runtime_error("Unable to get file time.");
		}
		CloseHandle(hFile);

		// Convert FILETIME to SYSTEMTIME
		SYSTEMTIME sysTime;
		FileTimeToSystemTime(&creationTime, &sysTime);

		// Convert SYSTEMTIME to a time_t (epoch time)
		std::tm tm = {};
		tm.tm_year = sysTime.wYear - 1900;
		tm.tm_mon = sysTime.wMonth - 1;
		tm.tm_mday = sysTime.wDay;
		tm.tm_hour = sysTime.wHour;
		tm.tm_min = sysTime.wMinute;
		tm.tm_sec = sysTime.wSecond;
		std::time_t t = std::mktime(&tm);

		// Format as a string
		std::wostringstream oss;
		oss << std::put_time(std::localtime(&t), L"%Y-%m-%d %H:%M:%S");
		return oss.str();
	}
	catch (const std::exception& e) {
		std::wcerr << L"Error getting file creation date: " << e.what() << std::endl;
		return L"";
	}
}

void ImageFileNameLibrary::LoadImages(const std::wstring& directory, const std::vector<std::wstring>& exclude) {
	for (const auto& entry : std::filesystem::directory_iterator(directory)) {
		std::wstring path = entry.path().wstring();

		// Skip if path is in exclude list
		if (std::find(exclude.begin(), exclude.end(), path) != exclude.end()) {
			continue;
		}

		if (entry.is_regular_file()) {
			auto ext = entry.path().extension().wstring();
			std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);

			// Check for supported image formats
			if (ext == L".jpg" || ext == L".jpeg" || ext == L".png" || ext == L".heic") {
				ImageInfo* info = new ImageInfo();

				// Extract date taken from EXIF, or use filename or file creation date
				info->dateTaken = ExtractDateTaken(path);
				if (info->dateTaken.empty()) {
					// If no DateTaken in EXIF, use the date from the filename or file creation date
					std::filesystem::path fileName = entry.path().filename();
					if (fileName.has_extension()) {
						// You could parse the filename for date if the file follows a date-based naming convention
						info->dateTaken = fileName.stem().wstring(); // Just a fallback, customize this as needed
					}
				}

				if (info->dateTaken.empty()) {
					// Fallback to file creation date
					info->dateTaken = GetFileCreationDate(path);
				}

				// Get the folder name
				info->filePath = path;
				info->folderName = entry.path().parent_path().filename().wstring();

				// Add to image list
				m_ImageList.push_back(info);

				// Optional: Print image info
				std::wcout << L"Image: " << path << L", Date Taken: " << info->dateTaken
					<< L", Folder: " << info->folderName << std::endl;
			}
		}
	}
}

void ImageFileNameLibrary::ShuffleImages() {
	std::random_device rd;
	std::mt19937 g(rd());
	std::shuffle(m_ImageList.begin(), m_ImageList.end(), g);
	m_CurrentImageIdx = 0;
}

const ImageInfo* ImageFileNameLibrary::GotoImage(int offset) {
	if (m_ImageList.empty())
	{
		return NULL;
	}
	m_CurrentImageIdx = (m_CurrentImageIdx + offset + m_ImageList.size()) % (int)m_ImageList.size();
	return m_ImageList[(size_t)m_CurrentImageIdx];
}
