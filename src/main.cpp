#include <boost/asio.hpp>
#include <boost/system/error_code.hpp>
#include <boost/beast/websocket/stream.hpp>
#include <boost/beast/core/tcp_stream.hpp>

#include <iostream>

using tcp = boost::asio::ip::tcp;

constexpr auto host = "echo.websocket.org";
constexpr auto port = "80";

const std::string message = "ECHO";

void Log(boost::system::error_code ec)
{
    std::cerr << (ec ? "Error: " : "OK")
              << (ec ? ec.message() : "")
              << std::endl;
}

int main()
{
    // Always start with an I/O context object.
    boost::asio::io_context ioc {};

    // Create an I/O object. Every Boost.Asio I/O object API needs an io_context
    // as the first parameter.
    tcp::socket socket {ioc};

    // Under the hood, socket.connect uses I/O context to talk to the socket
    // and get a response back. The response is saved in ec.
    boost::system::error_code ec {};
    tcp::resolver resolver {ioc};
    auto endpoint {resolver.resolve(host, port, ec)};
    if (ec) {
        Log(ec);
        return -1;
    }
    socket.connect(*endpoint, ec);
    if (ec) {
        Log(ec);
        return -2;
    }
    Log(ec);

   	boost::beast::websocket::stream<boost::beast::tcp_stream> ws {std::move(socket)};
	ws.handshake(host, "/", ec);
	if (ec) {
	    Log(ec);
	    return -3;
	}

	// Tell the WebSocket object to exchange messages in text format.
	ws.text(true);

	// Send a message to the connected WebSocket server.
	boost::asio::const_buffer wbuffer {message.c_str(), message.size()};
	ws.write(wbuffer, ec);
	if (ec) {
	    Log(ec);
	    return -4;
	}

	// Read the echoed message back.
	boost::beast::flat_buffer rbuffer {};
	ws.read(rbuffer, ec);
	if (ec) {
	    Log(ec);
	    return -5;
	}

	// Print the echoed message.
	std::cout << "ECHO: "
	          << boost::beast::make_printable(rbuffer.data())
	          << std::endl;

    return 0;
}