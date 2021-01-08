#include "network-monitor/WebSocketClient.h"

#include <boost/asio.hpp>
#include <boost/test/unit_test.hpp>

#include <filesystem>
#include <string>

using NetworkMonitor::WebSocketClient;

BOOST_AUTO_TEST_SUITE(network_monitor);

BOOST_AUTO_TEST_CASE(cacert_pem)
{
    BOOST_CHECK(std::filesystem::exists(TESTS_CACERT_PEM));
}

BOOST_AUTO_TEST_CASE(class_WebSocketClient)
{
    // Connection targets
    const std::string url {"echo.websocket.org"};
	const std::string endpoint {"/"};
    const std::string port {"443"};
    const std::string message {"Hello WebSocket"};

    // Always start with an I/O context object.
    boost::asio::io_context ioc {};

	// Contex object for SSL
	boost::asio::ssl::context ctx {boost::asio::ssl::context::tlsv12_client};
	ctx.load_verify_file(TESTS_CACERT_PEM);

    // The class under test
    WebSocketClient client {url, endpoint, port, ioc, ctx};

    // We use these flags to check that the connection, send, receive functions
    // work as expected.
    bool connected {false};
    bool messageSent {false};
    bool messageReceived {false};
    bool messageMatches {false};
    bool disconnected {false};

    // Our own callbacks
    auto onSend {[&messageSent](auto ec) {
        messageSent = !ec;
    }};
    auto onConnect {[&client, &connected, &onSend, &message](auto ec) {
        connected = !ec;
        if (!ec) {
            client.Send(message, onSend);
        }
    }};
    auto onClose {[&disconnected](auto ec) {
        disconnected = !ec;
    }};

	std::string echo {};
    auto onReceive {[&client,
                      &onClose,
                      &messageReceived,
                      &messageMatches,
                      &message,
					  &echo](auto ec, auto received) {
        messageReceived = !ec;
        messageMatches = message == received;
		echo = received;
        client.Close(onClose);
    }};

    // We must call io_context::run for asynchronous callbacks to run.
    client.Connect(onConnect, onReceive);
    ioc.run();

	BOOST_CHECK(connected);
	BOOST_CHECK(messageSent);
	BOOST_CHECK(messageReceived);
	BOOST_CHECK(disconnected);
	BOOST_CHECK_EQUAL(message, echo);
}

bool CheckResponse(const std::string& response)
{
    // We do not parse the whole message. We only check that it contains some
    // expected items.
    bool ok {true};
    ok &= response.find("ERROR") != std::string::npos;
    ok &= response.find("ValidationInvalidAuth") != std::string::npos;
    return ok;
}

BOOST_AUTO_TEST_CASE(STOMP_frame)
{
    // Connection targets
    const std::string url {"ltnm.learncppthroughprojects.com"};
	const std::string endpoint {"/network-events"};
    const std::string port {"443"};

	std::stringstream ss {};
	ss	<< "STOMP" << std::endl
		<< "accept-version:1.2" << std::endl
		<< "host:" << url << std::endl
		<< "login:" << "log1" << std::endl
		<< "passcode:" << "pass1" << std::endl
		<< std::endl
		<< '\0';
    const std::string message {ss.str()};

    // Always start with an I/O context object.
    boost::asio::io_context ioc {};

	// Contex object for SSL
	boost::asio::ssl::context ctx {boost::asio::ssl::context::tlsv12_client};
	ctx.load_verify_file(TESTS_CACERT_PEM);

    // The class under test
    WebSocketClient client {url, endpoint, port, ioc, ctx};

    // We use these flags to check that the connection, send, receive functions
    // work as expected.
    bool connected {false};
    bool messageSent {false};
    bool messageReceived {false};
    bool disconnected {false};

    // Our own callbacks
    auto onSend {[&messageSent](auto ec) {
        messageSent = !ec;
    }};
    auto onConnect {[&client, &connected, &onSend, &message](auto ec) {
        connected = !ec;
        if (!ec) {
            client.Send(message, onSend);
        }
    }};
    auto onClose {[&disconnected](auto ec) {
        disconnected = !ec;
    }};

	std::string responce {};
    auto onReceive {[&client,
                      &onClose,
                      &messageReceived,
                      &message,
					  &responce](auto ec, auto received) {
        messageReceived = !ec;
		responce = received;
        client.Close(onClose);
    }};

    // We must call io_context::run for asynchronous callbacks to run.
    client.Connect(onConnect, onReceive);
    ioc.run();

	BOOST_CHECK(connected);
	BOOST_CHECK(messageSent);
	BOOST_CHECK(messageReceived);
	BOOST_CHECK(disconnected);
	BOOST_CHECK(CheckResponse(responce));
}

BOOST_AUTO_TEST_SUITE_END();