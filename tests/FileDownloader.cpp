#include <network-monitor/FileDownloader.h>

#include <boost/asio.hpp>
#include <boost/test/unit_test.hpp>

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <string>

using NetworkMonitor::DownloadFile;
using NetworkMonitor::ParseJsonFile;

BOOST_AUTO_TEST_SUITE(network_monitor);

BOOST_AUTO_TEST_CASE(file_downloader)
{
    const std::string fileUrl {
        "https://ltnm.learncppthroughprojects.com/network-layout.json"
    };
    const auto destination {
        std::filesystem::temp_directory_path() / "network-layout.json"
    };

    // Download the file.
    bool downloaded {DownloadFile(fileUrl, destination, TESTS_CACERT_PEM)};
    BOOST_CHECK(downloaded);
    BOOST_CHECK(std::filesystem::exists(destination));

    // Check the content of the file.
    // We cannot check the whole file content as it changes over time, but we
    // can at least check some expected file properties.
    const std::string expectedString {"\"stations\": ["};
    std::ifstream file {destination};
    std::string line {};
    bool foundExpectedString {false};
    while (std::getline(file, line)) {
        if (line.find(expectedString) != std::string::npos) {
            foundExpectedString = true;
            break;
        }
    }
    BOOST_CHECK(foundExpectedString);

    // Clean up.
    std::filesystem::remove(destination);
}

BOOST_AUTO_TEST_CASE(json_parser)
{
	const std::filesystem::path jsonFile{TESTS_NETWORK_LAYOUT_JSON};
	BOOST_CHECK(std::filesystem::exists(jsonFile));

	auto parsed = ParseJsonFile(TESTS_NETWORK_LAYOUT_JSON);
	BOOST_CHECK(parsed != NULL);
	BOOST_CHECK(parsed.is_object());

	// Test json file structure.
	BOOST_CHECK(parsed.contains("lines"));
	BOOST_CHECK(parsed.at("lines").size() > 0);
	BOOST_CHECK(parsed.contains("stations"));
	BOOST_CHECK(parsed.at("stations").size() > 0);
	BOOST_CHECK(parsed.contains("travel_times"));
	BOOST_CHECK(parsed.at("travel_times").size() > 0);
}

BOOST_AUTO_TEST_SUITE_END();