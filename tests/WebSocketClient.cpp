#include "network-monitor/WebSocketClient.h"

#include <boost/asio.hpp>
#include <boost/test/unit_test.hpp>

#include <filesystem>
#include <string>

#include "mock/MockResolver.h"

using NetworkMonitor::BoostWebSocketClient;

using NetworkMonitor::MockResolver;
using NetworkMonitor::MockWebSocketClient;

BOOST_AUTO_TEST_SUITE(network_monitor);

static boost::unit_test::timeout gTimeout {3};

BOOST_AUTO_TEST_CASE(cacert_pem)
{
    BOOST_CHECK(std::filesystem::exists(TESTS_CACERT_PEM));
}
 
BOOST_AUTO_TEST_CASE(class_WebSocketClient, *gTimeout)
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
    BoostWebSocketClient client {url, endpoint, port, ioc, ctx};

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

BOOST_AUTO_TEST_CASE(get_error_stomp_frame, *gTimeout)
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
    BoostWebSocketClient client {url, endpoint, port, ioc, ctx};

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

// This fixture is used to re-initialize all mock properties before a test.
struct WebSocketClientTestFixture {
    WebSocketClientTestFixture()
    {
        MockResolver::ec = {};
    }
};

BOOST_FIXTURE_TEST_SUITE(class_MockResolver, WebSocketClientTestFixture);

BOOST_AUTO_TEST_CASE(failed_on_connect, *gTimeout) 
{
	// Connection targets
    const std::string url {"echo.websocket.org"};
	const std::string endpoint {"/"};
    const std::string port {"443"};

    // Always start with an I/O context object.
    boost::asio::io_context ioc {};

	MockResolver::ec = boost::asio::error::host_not_found;

	// Contex object for SSL
	boost::asio::ssl::context ctx {boost::asio::ssl::context::tlsv12_client};
	ctx.load_verify_file(TESTS_CACERT_PEM);

    MockWebSocketClient client {url, endpoint, port, ioc, ctx};
	bool calledOnConnect {false};
    auto onConnect {[&calledOnConnect](auto ec) {
        calledOnConnect = true;
        BOOST_CHECK_EQUAL(ec, boost::asio::error::host_not_found);
    }};
    client.Connect(onConnect);
    ioc.run();

    // When we get here, the io_context::run function has run out of work to do.
    BOOST_CHECK(calledOnConnect);
}

BOOST_AUTO_TEST_SUITE_END(); // MockResolver, WebSocketClientTestFixture

BOOST_AUTO_TEST_SUITE_END(); // network_monitor