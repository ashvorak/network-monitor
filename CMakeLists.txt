cmake_minimum_required(VERSION 3.17 FATAL_ERROR)

project(network-monitor)

# Dependencies
# If dependencies are managed with conan, we set the CMake module path (where
# CMake looks for dependencies) to the current build folder.
if(EXISTS ${CMAKE_CURRENT_BINARY_DIR}/conaninfo.txt)
    set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_BINARY_DIR})
endif()
# Add the current CMake modules folder to the CMake search path.
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
find_package(Boost 1.74 REQUIRED COMPONENTS system unit_test_framework)
find_package(Filesystem REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(CURL REQUIRED)
find_package(nlohmann_json REQUIRED)

# Called before any other target is defined.
enable_testing()

# Static library
set(LIB_SOURCES
	"${CMAKE_CURRENT_SOURCE_DIR}/src/FileDownloader.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/TransportNetwork.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/StompFrame.cpp"
)
add_library(network-monitor STATIC ${LIB_SOURCES})
target_compile_features(network-monitor
    PUBLIC
        cxx_std_17
)
target_include_directories(network-monitor
    PUBLIC
        inc
)
target_link_libraries(network-monitor
    PUBLIC
		Boost::Boost
		OpenSSL::OpenSSL
		std::filesystem
		nlohmann_json::nlohmann_json
	PRIVATE	
		CURL::CURL
)
set(SOURCES
	"${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp"
)
add_executable(network-monitor-core ${SOURCES})
target_compile_features(network-monitor-core
    PRIVATE
        cxx_std_17
)
target_link_libraries(network-monitor-core
    PRIVATE
        network-monitor
		Boost::Boost
		OpenSSL::OpenSSL
		std::filesystem
)


# Tests
set(TESTS_SOURCES
	"${CMAKE_CURRENT_SOURCE_DIR}/tests/main.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/tests/WebSocketClient.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/tests/FileDownloader.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/tests/TransportNetwork.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/tests/StompFrame.cpp"
)
set(TESTS_DATA
	"${CMAKE_CURRENT_SOURCE_DIR}/tests/tests_data"
)
add_executable(network-monitor-tests ${TESTS_SOURCES})
target_compile_features(network-monitor-tests
    PRIVATE
        cxx_std_17
)
target_compile_definitions(network-monitor-tests
	PRIVATE
		TESTS_CACERT_PEM="${TESTS_DATA}/cacert.pem"
		TESTS_JSON_FOLDER="${TESTS_DATA}/json"
		TESTS_NETWORK_LAYOUT_JSON="${TESTS_DATA}/json/network-layout.json"
)
target_link_libraries(network-monitor-tests
    PRIVATE
        network-monitor
		Boost::Boost
		OpenSSL::OpenSSL
		std::filesystem
)
add_test(
    NAME network-monitor-tests
    COMMAND $<TARGET_FILE:network-monitor-tests>
)
# This tells CMake to check for a specific output to verify the test outcome.
# When all unit tests pass, Boost.Test prints "No errors detected".
set_tests_properties(network-monitor-tests PROPERTIES
    PASS_REGULAR_EXPRESSION ".*No errors detected"
)