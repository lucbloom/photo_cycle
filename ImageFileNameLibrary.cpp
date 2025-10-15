
#include "ImageFileNameLibrary.h"
#include "SettingsDialog.h"

#define WIN32_LEAN_AND_MEAN
#include "nlohmann/json.hpp"

#include <exiv2.hpp>
#include <cwctype>
#include <random>
//#include <exiv2/exiv2.hpp>
//#include <iostream>
//#include <iomanip>
//#include <iostream>
//#include <fstream>
//#include <string>
//#include <iostream>
//#include <iomanip>
//#include <sstream>
#include <wininet.h>

#pragma comment(lib, "wininet.lib")
//#pragma comment(lib, "libcurl.lib") 
//#pragma comment(lib, "wldap32.lib") 

struct DateResult {
	bool success = false;
	std::tm date = {};
	double confidence = 0;
	std::string pattern;
	std::string explanation;
};

DateResult ExtractDateFromFilename(std::wstring filename);
std::wstring DescribeLocation(const std::wstring& filePath);

std::wstring FormatDate(std::tm tm, const std::wstring& format = L"dd-mm-yyyy") {
	std::time_t time = std::mktime(&tm);
	struct std::tm* timeInfo = std::localtime(&time);
	std::wstringstream ss;

	if (format == L"dd-mm-yyyy") {
		ss << std::setw(2) << std::setfill(L'0') << timeInfo->tm_mday << L"-"
			<< std::setw(2) << std::setfill(L'0') << (timeInfo->tm_mon + 1) << L"-"
			<< (timeInfo->tm_year + 1900);
	}
	return ss.str();
}

void ImageFileNameLibrary::SetPaths(const std::vector<std::wstring>& include, const std::vector<std::wstring>& exclude)
{
	for (const auto& dir : include) {
		LoadImages(dir, exclude);
	}
	ShuffleImages();
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

int GetExifRotation(const std::wstring& imagePath) {
	try {
		auto image = Exiv2::ImageFactory::open(WStringToUtf8(imagePath));
		if (!image) return 0;

		image->readMetadata();
		auto& exifData = image->exifData();
		auto it = exifData.findKey(Exiv2::ExifKey("Exif.Image.Orientation"));
		if (it == exifData.end()) return 0;

		int orientation = it->toUint32();
		switch (orientation) {
		case 3: return 180;
		case 6: return 90;
		case 8: return 270;
		default: return 0;
		}
	}
	catch (const Exiv2::Error& e) {
		std::wcerr << L"Error reading EXIF rotation for \"" << imagePath << "\": " << e.what() << std::endl;
		return 0;
	}
}


DateResult ExtractDateTaken(const std::wstring& imagePath) {
	DateResult result;
	try {
		auto fn = WStringToUtf8(imagePath);
		auto image = Exiv2::ImageFactory::open(fn);
		image->readMetadata();

		Exiv2::ExifData& exifData = image->exifData();
		if (exifData.empty()) {
			std::wcerr << L"No EXIF data found" << std::endl;
			return result;
		}

		auto dateTimeTag = exifData.findKey(Exiv2::ExifKey("Exif.Photo.DateTimeOriginal"));
		if (dateTimeTag == exifData.end()) return {};

		auto value = Utf8ToWString(dateTimeTag->value().toString());
		std::wsmatch m;
		std::wregex patterns[] = {
			std::wregex(L"(\\d{4})[:-](\\d{2})[:-](\\d{2})[ T]?(\\d{2}):(\\d{2}):(\\d{2})"),
			std::wregex(L"(\\d{4})[:-](\\d{2})[:-](\\d{2})[ T]?(\\d{2}):(\\d{2})"),
			std::wregex(L"(\\d{4})[:-](\\d{2})[:-](\\d{2})"),
			std::wregex(L"(\\d{4})/(\\d{2})/(\\d{2})")
		};

		for (auto& re : patterns) {
			if (std::regex_match(value, m, re)) {
				result.success = true;
				result.date.tm_year = std::stoi(m[1]) - 1900;
				result.date.tm_mon = std::stoi(m[2]) - 1;
				result.date.tm_mday = std::stoi(m[3]);
				if (m.size() > 4) result.date.tm_hour = std::stoi(m[4]);
				if (m.size() > 5) result.date.tm_min = std::stoi(m[5]);
				if (m.size() > 6) result.date.tm_sec = std::stoi(m[6]);
				break;
			}
		}
	}
	catch (const Exiv2::Error& e) {
		std::wcerr << L"Error reading EXIF data for \"" << imagePath << "\": " << e.what() << std::endl;
	}
	return result;
}

DateResult GetFileCreationDate(const std::wstring& filePath) {
	DateResult result;
	try {
		// Open the file
		HANDLE hFile = CreateFile(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
		if (hFile == INVALID_HANDLE_VALUE) {
			throw std::runtime_error("Unable to open file.");
		}

		// Retrieve the file creation time
		FILETIME creationTime;
		if (GetFileTime(hFile, &creationTime, nullptr, nullptr)) {
			// Convert FILETIME to SYSTEMTIME
			SYSTEMTIME sysTime;
			if (FileTimeToSystemTime(&creationTime, &sysTime))
			{
				// Convert SYSTEMTIME to a time_t (epoch time)
				result.success = true;
				result.date.tm_year = sysTime.wYear - 1900;
				result.date.tm_mon = sysTime.wMonth - 1;
				result.date.tm_mday = sysTime.wDay;
				result.date.tm_hour = sysTime.wHour;
				result.date.tm_min = sysTime.wMinute;
				result.date.tm_sec = sysTime.wSecond;
			}
		}
		CloseHandle(hFile);

	}
	catch (const std::exception& e) {
		std::wcerr << L"Error getting file creation date for \"" << filePath << "\": " << e.what() << std::endl;
	}
	return result;
}

void ImageInfo::CacheInfo(SettingsDialog& sets)
{
	if (sets.ShowDate && dateTaken.empty()) {
		// Extract date taken from EXIF, or use filename or file creation date
		auto dateInfo = ExtractDateTaken(filePath);
		if (!dateInfo.success) {
			// If no DateTaken in EXIF, use the date from the filename or file creation date
			std::filesystem::path fileName = std::filesystem::path(filePath).filename();
			// You could parse the filename for date if the file follows a date-based naming convention
			dateInfo = ExtractDateFromFilename(fileName.stem().wstring());
			if (!dateInfo.success) {
				dateInfo = GetFileCreationDate(filePath);
			}
		}
		if (dateInfo.success) {
			dateTaken = FormatDate(dateInfo.date);
		}
		else
		{
			dateTaken = L" ";
		}
	}

	if (rotation < 0)
	{
		rotation = GetExifRotation(filePath);
	}

	if (sets.ShowLocation && location.empty() && !isCaching)
	{
		isCaching = true;
		std::thread httpThread([this]()
			{
				location = DescribeLocation(filePath);
				if (location.empty())
				{
					location = L" ";
				}
				isCaching = false;
			});
		httpThread.detach();
	}
}

std::wstring ImageInfo::GetCaption(SettingsDialog& sets)
{
	std::wstring caption;
	if (sets.ShowFolder && folderName.length() > 1)
	{
		if (!caption.empty()) { caption += L" "; }
		caption += L" " + folderName;
	}
	if (sets.ShowDate && dateTaken.length() > 1)
	{
		if (!caption.empty()) { caption += L" "; }
		caption += L" " + dateTaken;
	}
	if (sets.ShowLocation && !isCaching && location.length() > 1)
	{
		if (!caption.empty()) { caption += L"\n"; }
		caption += L" " + location;
	}
	return caption;
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

				// Get the folder name
				info->filePath = path;
				info->folderName = entry.path().parent_path().filename().wstring();

				// Add to image list
				m_ImageList.push_back(info);
			}
		}
		else if (entry.is_directory())
		{
			LoadImages(entry.path(), exclude);
		}
	}
}

void ImageFileNameLibrary::ShuffleImages() {
	std::random_device rd;
	std::mt19937 g(rd());
	std::shuffle(m_ImageList.begin(), m_ImageList.end(), g);
	for (int i = 0; i < (int)m_ImageList.size(); i++)
	{
		m_ImageList[i]->idx = i;
	}
}

ImageInfo* ImageFileNameLibrary::GotoImage(int imageIndex, int monitorIndex, int numMonitors) {
	if (m_ImageList.empty())
	{
		return NULL;
	}

	auto n = (int)m_ImageList.size();
	auto offset = (monitorIndex * n) / numMonitors;
	imageIndex = (offset + imageIndex + n) % n;
	auto img = m_ImageList[(size_t)imageIndex];
	return img;
}

DateResult ExtractDateFromFilename(std::wstring nameOnly)
{
	// Ignore file extension
	auto dotIdx = nameOnly.find_last_of('.');
	if (dotIdx != std::string::npos) {
		nameOnly = nameOnly.substr(0, dotIdx);
	}

	DateResult result;

	// Helper function to create a time_t from components
	auto createTime = [&result](int year, int month, int day, int hour = 0, int min = 0, int sec = 0) -> void {
		result.success = true;
		result.date.tm_year = year - 1900;
		result.date.tm_mon = month - 1;
		result.date.tm_mday = day;
		result.date.tm_hour = hour;
		result.date.tm_min = min;
		result.date.tm_sec = sec;
		};

	// iPhone format: IMG_YYYYMMDD_HHMMSS
	std::wregex iPhoneRegex(L"(?:^|[^\\d])IMG_(\\d{4})(\\d{2})(\\d{2})_(\\d{2})(\\d{2})(\\d{2})(?:[^\\d]|$)", std::regex_constants::icase);
	std::wsmatch iPhoneMatches;
	if (std::regex_search(nameOnly, iPhoneMatches, iPhoneRegex)) {
		int year = std::stoi(iPhoneMatches[1]);
		int month = std::stoi(iPhoneMatches[2]);
		int day = std::stoi(iPhoneMatches[3]);
		int hour = std::stoi(iPhoneMatches[4]);
		int minute = std::stoi(iPhoneMatches[5]);
		int second = std::stoi(iPhoneMatches[6]);

		createTime(year, month, day, hour, minute, second);
		result.confidence = 0.9;
		result.pattern = "iPhone";
		result.explanation = "Matched iPhone pattern";
		return result;
	}

	// Digital camera format: DSC_YYYYMMDD_HHMMSS
	std::wregex dscRegex(L"(?:^|[^\\d])DSC_(\\d{4})(\\d{2})(\\d{2})_(\\d{2})(\\d{2})(\\d{2})(?:[^\\d]|$)", std::regex_constants::icase);
	std::wsmatch dscMatches;
	if (std::regex_search(nameOnly, dscMatches, dscRegex)) {
		int year = std::stoi(dscMatches[1]);
		int month = std::stoi(dscMatches[2]);
		int day = std::stoi(dscMatches[3]);
		int hour = std::stoi(dscMatches[4]);
		int minute = std::stoi(dscMatches[5]);
		int second = std::stoi(dscMatches[6]);

		createTime(year, month, day, hour, minute, second);
		result.confidence = 0.9;
		result.pattern = "Digital Camera (DSC)";
		result.explanation = "Matched Digital Camera (DSC) pattern";
		return result;
	}

	// WhatsApp format: IMG-YYYYMMDD-WANHHMSS
	std::wregex whatsappRegex(L"(?:^|[^\\d])IMG-(\\d{4})(\\d{2})(\\d{2})-WA\\d+(?:[^\\d]|$)", std::regex_constants::icase);
	std::wsmatch whatsappMatches;
	if (std::regex_search(nameOnly, whatsappMatches, whatsappRegex)) {
		int year = std::stoi(whatsappMatches[1]);
		int month = std::stoi(whatsappMatches[2]);
		int day = std::stoi(whatsappMatches[3]);

		createTime(year, month, day);
		result.confidence = 0.85;
		result.pattern = "WhatsApp";
		result.explanation = "Matched WhatsApp pattern";
		return result;
	}

	// Screenshot format: Screenshot_YYYYMMDD-HHMMSS
	std::wregex screenshotRegex(L"(?:^|[^\\d])Screenshot_(\\d{4})(\\d{2})(\\d{2})-(\\d{2})(\\d{2})(\\d{2})(?:[^\\d]|$)", std::regex_constants::icase);
	std::wsmatch screenshotMatches;
	if (std::regex_search(nameOnly, screenshotMatches, screenshotRegex)) {
		int year = std::stoi(screenshotMatches[1]);
		int month = std::stoi(screenshotMatches[2]);
		int day = std::stoi(screenshotMatches[3]);
		int hour = std::stoi(screenshotMatches[4]);
		int minute = std::stoi(screenshotMatches[5]);
		int second = std::stoi(screenshotMatches[6]);

		createTime(year, month, day, hour, minute, second);
		result.confidence = 0.9;
		result.pattern = "Android Screenshot";
		result.explanation = "Matched Android Screenshot pattern";
		return result;
	}

	// ISO format: YYYY-MM-DD_HH-MM-SS
	std::wregex isoRegex(L"(?:^|[^\\d])(\\d{4})-(\\d{2})-(\\d{2})_(\\d{2})-(\\d{2})-(\\d{2})(?:[^\\d]|$)");
	std::wsmatch isoMatches;
	if (std::regex_search(nameOnly, isoMatches, isoRegex)) {
		int year = std::stoi(isoMatches[1]);
		int month = std::stoi(isoMatches[2]);
		int day = std::stoi(isoMatches[3]);
		int hour = std::stoi(isoMatches[4]);
		int minute = std::stoi(isoMatches[5]);
		int second = std::stoi(isoMatches[6]);

		createTime(year, month, day, hour, minute, second);
		result.confidence = 0.95;
		result.pattern = "ISO Format";
		result.explanation = "Matched ISO Format pattern";
		return result;
	}

	// Date format YYYYMMDD embedded in filename
	std::wregex embeddedDateRegex(L"(?:^|[^\\d])(\\d{4})(\\d{2})(\\d{2})(?:[^\\d]|$)");
	std::wsmatch embeddedMatches;
	if (std::regex_search(nameOnly, embeddedMatches, embeddedDateRegex)) {
		int year = std::stoi(embeddedMatches[1]);
		int month = std::stoi(embeddedMatches[2]);
		int day = std::stoi(embeddedMatches[3]);

		// Validate date parts
		if (year >= 1990 && year <= 2030 &&
			month >= 1 && month <= 12 &&
			day >= 1 && day <= 31) {
			createTime(year, month, day);
			result.confidence = 0.7;
			result.pattern = "Embedded Date";
			result.explanation = "Matched Embedded Date pattern";
			return result;
		}
	}

	// UNIX timestamp in filename
	std::wregex unixTimestampRegex(L"(?:^|[^\\d])(\\d{10,13})(?:[^\\d]|$)");
	std::wsmatch timestampMatches;
	if (std::regex_search(nameOnly, timestampMatches, unixTimestampRegex)) {
		try {
			long timestamp = std::stol(timestampMatches[1]);
			// Validate timestamp (between 1990 and 2050)
			if (timestamp > 631152000 && timestamp < 2524608000) {
				long timestamp = std::stol(timestampMatches[1]);
				std::time_t time = static_cast<std::time_t>(timestamp);
				result.date = *std::gmtime(&time); // Make a copy if you don't want a pointer
				result.success = true;
				result.confidence = 0.6;
				result.pattern = "UNIX Timestamp";
				result.explanation = "Matched UNIX Timestamp pattern";
				return result;
			}
		}
		catch (const std::out_of_range&) {} // timestamp was te groot
		catch (const std::invalid_argument&) {} // geen geldig getal
	}

	// Canon camera format: IMG_NNNN
	std::wregex canonRegex(L"(?:^|[^\\d])IMG_(\\d{4})(?:[^\\d]|$)", std::regex_constants::icase);
	std::wsmatch canonMatches;
	if (std::regex_search(nameOnly, canonMatches, canonRegex)) {
		// No date information available in this format
		result.success = false;
		result.confidence = 0.0;
		result.pattern = "Canon Sequential";
		result.explanation = "Matched Canon Sequential pattern, but no date information available";
		return result;
	}

	// If no patterns matched
	result.explanation = "No recognizable date pattern found in filename";
	return result;
}

double convertToDecimal(const Exiv2::Exifdatum& degreesDatum, const Exiv2::Exifdatum& refDatum) {
	const Exiv2::Value& degrees = degreesDatum.value();
	const Exiv2::Value& ref = refDatum.value();

	if (degrees.count() != 3) return 0.0;

	auto toDouble = [](const Exiv2::Rational& r) {
		return static_cast<double>(r.first) / r.second;
		};

	double d = toDouble(degrees.toRational(0));
	double m = toDouble(degrees.toRational(1));
	double s = toDouble(degrees.toRational(2));
	double decimal = d + m / 60.0 + s / 3600.0;

	std::string direction = ref.toString();
	if (direction == "S" || direction == "W")
		decimal *= -1;

	return decimal;
}

size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* out) {
	out->append((char*)contents, size * nmemb);
	return size * nmemb;
}

std::string MakeHttpRequest(const std::wstring& url)
{
	HINTERNET hInternet, hConnect;
	DWORD bytesRead;
	char buffer[4096] = {};

	// Initialize WinINet
	hInternet = InternetOpen(L"HTTPS Example", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
	if (hInternet == NULL) {
		std::cerr << "InternetOpen failed: " << GetLastError() << std::endl;
		return "";
	}

	// Open an HTTPS connection to a website
	hConnect = InternetOpenUrl(hInternet, url.c_str(), NULL, 0, INTERNET_FLAG_RELOAD, 0);
	if (hConnect == NULL) {
		std::cerr << "InternetOpenUrl failed: " << GetLastError() << std::endl;
		InternetCloseHandle(hInternet);
		return "";
	}

	// Read the data from the connection
	while (InternetReadFile(hConnect, buffer, sizeof(buffer) - 1, &bytesRead) && bytesRead > 0) {
		buffer[bytesRead] = '\0';  // Null-terminate the data
		std::cout << buffer;
	}

	// Clean up
	InternetCloseHandle(hConnect);
	InternetCloseHandle(hInternet);

	return buffer;
}

std::wstring DescribeLocation(const std::wstring& filePath)
{
	std::string location;

	double lat, lon;
	try {
		std::unique_ptr<Exiv2::Image> image = Exiv2::ImageFactory::open(WStringToUtf8(filePath));
		if (!image) {
			std::cerr << "Failed to open image.\n";
			return L"";
		}

		image->readMetadata();
		Exiv2::ExifData& exif = image->exifData();

		lat = convertToDecimal(
			exif["Exif.GPSInfo.GPSLatitude"],
			exif["Exif.GPSInfo.GPSLatitudeRef"]
		);
		lon = convertToDecimal(
			exif["Exif.GPSInfo.GPSLongitude"],
			exif["Exif.GPSInfo.GPSLongitudeRef"]
		);

		std::cout << std::fixed << std::setprecision(7)
			<< "Latitude: " << lat << "\n"
			<< "Longitude: " << lon << "\n";

	}
	catch (const Exiv2::Error& e) {
		std::cerr << "EXIF error: " << e.what() << "\n";
		return L"";
	}


	std::ostringstream url;
	url << "https://nominatim.openstreetmap.org/reverse?format=json"
		<< "&lat=" << std::fixed << std::setprecision(7) << lat
		<< "&lon=" << std::fixed << std::setprecision(7) << lon
		<< "&accept-language=nl";

	//CURL* curl = curl_easy_init();
	//std::string response;
	//
	//if (curl) {
	//	curl_easy_setopt(curl, CURLOPT_URL, url.str().c_str());
	//	curl_easy_setopt(curl, CURLOPT_USERAGENT, "curl/7.88.1");
	//	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
	//	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
	//
	//	CURLcode res = curl_easy_perform(curl);
	//	if (res != CURLE_OK) {
	//		std::cerr << "curl error: " << curl_easy_strerror(res) << "\n";
	//	}
	//
	//	curl_easy_cleanup(curl);
	//}

	auto response = MakeHttpRequest(Utf8ToWString(url.str()));


	nlohmann::json j = nlohmann::json::parse(response);

	const auto& addr = j["address"];

	if (addr.contains("leisure")) {
		location += addr["leisure"].get<std::string>();
	}
	else if (addr.contains("amenity")) {
		location += addr["amenity"].get<std::string>();
	}

	if (addr.contains("village")) {
		location += location.empty() ? "" : " in ";
		location += addr["village"].get<std::string>();
	}
	else if (addr.contains("town")) {
		location += location.empty() ? "" : " in ";
		location += addr["town"].get<std::string>();
	}
	else if (addr.contains("city")) {
		location += location.empty() ? "" : " in ";
		location += addr["city"].get<std::string>();
	}
	else if (addr.contains("state")) {
		location += location.empty() ? "" : " in ";
		location += addr["state"].get<std::string>();
	}
	else if (addr.contains("county")) {
		location += location.empty() ? "" : " in ";
		location += addr["county"].get<std::string>();
	}

	if (addr.contains("country")) {
		location += location.empty() ? "" : ", ";
		location += addr["country"].get<std::string>();
	}

	return Utf8ToWString(location);
}

bool ImageInfo::RotateImage90()
{
	try {
		auto img = Exiv2::ImageFactory::open(WStringToUtf8(filePath));
		if (!img) return false;

		img->readMetadata();
		auto& exifData = img->exifData();

		int newOrientation = 1; // default "normal"
		auto it = exifData.findKey(Exiv2::ExifKey("Exif.Image.Orientation"));
		if (it != exifData.end()) {
			int orientation = it->toUint32();
			// Map orientation → angle
			int currentDeg = 0;
			switch (orientation) {
			case 3: currentDeg = 180; break;
			case 6: currentDeg = 90; break;
			case 8: currentDeg = 270; break;
			}
			// Add 90°
			int newDeg = (currentDeg + 90) % 360;
			switch (newDeg) {
			case 0: newOrientation = 1; break;
			case 90: newOrientation = 6; break;
			case 180: newOrientation = 3; break;
			case 270: newOrientation = 8; break;
			}
		}
		else {
			// If no orientation tag, start at 90°
			newOrientation = 6;
		}

		exifData["Exif.Image.Orientation"] = newOrientation;
		img->setExifData(exifData);
		img->writeMetadata();

		switch (newOrientation) {
		case 1: rotation = 0; break;
		case 6: rotation = 90; break;
		case 3: rotation = 180; break;
		case 8: rotation = 270; break;
		}
		return true;
	}
	catch (const Exiv2::Error& e) {
		std::wcerr << L"Rotate EXIF error: " << e.what() << std::endl;
		return false;
	}
}
