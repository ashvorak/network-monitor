#include "network-monitor/FileDownloader.h"

#include <fstream>
#include <curl/curl.h>

bool NetworkMonitor::DownloadFile(
    const std::string& fileUrl,
    const std::filesystem::path& destination,
    const std::filesystem::path& caCertFile
)
{
	// Init curl session
	auto curl {curl_easy_init()};
	if(curl) 
	{
		curl_easy_setopt(curl, CURLOPT_URL, fileUrl.c_str());
		curl_easy_setopt(curl, CURLOPT_CAINFO, caCertFile.c_str());
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
  		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
		// fileUrl is redirected, so we tell libcurl to follow redirection
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

  		auto fp {fopen(destination.c_str(), "wb")};
		if (fp == nullptr) {
			// Clean up in all program paths.
			curl_easy_cleanup(curl);
			return false;
		}
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);

		auto res = curl_easy_perform(curl);
		curl_easy_cleanup(curl);
		fclose(fp);

		if (res == CURLE_OK) {
			return true;
		}
	}
	return false;
}

nlohmann::json NetworkMonitor::ParseJsonFile(
    const std::filesystem::path& source
)
{
	nlohmann::json parsed {};
	if (!std::filesystem::exists(source)) {
		return NULL;
	}

	try {
		std::ifstream ifs(source);
		ifs >> parsed;
	}
	catch (...) {
		// Error during parsing
		return NULL;
	}
	return parsed;
}