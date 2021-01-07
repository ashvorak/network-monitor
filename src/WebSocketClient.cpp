#include "network-monitor/WebSocketClient.h"

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/system/error_code.hpp>

#include <chrono>
#include <functional>
#include <iomanip>
#include <iostream>
#include <string>

using NetworkMonitor::WebSocketClient;

using tcp = boost::asio::ip::tcp;
namespace websocket = boost::beast::websocket;

static void Log(const std::string& where, boost::system::error_code ec)
{
    std::cerr << "[" << std::setw(20) << where << "] "
              << (ec ? "Error: " : "OK")
              << (ec ? ec.message() : "")
              << std::endl;
}

WebSocketClient::WebSocketClient(
	const std::string& url,
	const std::string& port,
	boost::asio::io_context& ioc,
	boost::asio::ssl::context& ctx
) : m_url{url}, 
	m_port{port},
	m_resolver{boost::asio::make_strand(ioc)},
	m_ws{boost::asio::make_strand(ioc), ctx}
{

}

WebSocketClient::~WebSocketClient() = default;

void WebSocketClient::Connect(
    std::function<void (boost::system::error_code)> onConnect,
	std::function<void (boost::system::error_code,
						std::string&&)> onMessage,
	std::function<void (boost::system::error_code)> onDisconnect
)
{
	clb_OnConnect = onConnect;
	clb_OnMessage = onMessage;
	clb_OnDisconnect = onDisconnect;

	m_resolver.async_resolve(m_url, m_port,
		[this](auto ec, auto endpoint) {
			OnResolve(ec, endpoint);
		}
	);
}

void WebSocketClient::Send(
	const std::string& message,
	std::function<void (boost::system::error_code)> onSend
)
{
    m_ws.async_write(boost::asio::buffer(std::move(message)), 
		[this, onSend](auto ec, auto) {
			if (onSend) {
				onSend(ec);
			}
	});
}

void WebSocketClient::Close(
	std::function<void (boost::system::error_code)> onClose
)
{
    m_ws.async_close(websocket::close_code::none,
		[this, onClose](auto ec) {
			if (onClose) {
				onClose(ec);
			};
	});
}

void WebSocketClient::OnResolve(
	const boost::system::error_code& ec,
	boost::asio::ip::tcp::resolver::iterator endpoint
)
{
	if (ec) {
		Log("OnResolve", ec);
		if (clb_OnConnect)
			clb_OnConnect(ec);
		return;
	}

    // The following timeout only matters for the purpose of connecting to the
    // TCP socket. We will reset the timeout to a sensible default after we are
    // connected.
    boost::beast::get_lowest_layer(m_ws).expires_after(std::chrono::seconds(5));

	boost::beast::get_lowest_layer(m_ws).async_connect(*endpoint,
	    [this](auto ec) {
	        OnConnect(ec);
	    }
	);	
}

void WebSocketClient::OnConnect(
	const boost::system::error_code& ec
)
{
	if (ec) {
		Log("OnConnect", ec);
		if (clb_OnConnect) {
			clb_OnConnect(ec);
		}
		return;
	}

    // Now that the TCP socket is connected, we can reset the timeout to
    // whatever Boost.Beast recommends.
    boost::beast::get_lowest_layer(m_ws).expires_never();
    m_ws.set_option(websocket::stream_base::timeout::suggested(
        boost::beast::role_type::client
    ));

	// Attempt a TLS handshake.
    m_ws.next_layer().async_handshake(boost::asio::ssl::stream_base::client,
        [this](auto ec) {
            OnTlsHandshake(ec);
        }
    );
}

void WebSocketClient::OnTlsHandshake(
	const boost::system::error_code& ec
)
{
	if (ec) {
		Log("OnTlsHandshake", ec);
		if (clb_OnConnect) {
			clb_OnConnect(ec);
		}
		return;
	}

    m_ws.async_handshake(m_url, "/",
        [this](auto ec) {
            OnHandshake(ec);
        }
    );
}

void WebSocketClient::OnHandshake(
	const boost::system::error_code& ec
)
{
	if (ec) {
		Log("OnHandshake", ec);
		if (clb_OnConnect) {
			clb_OnConnect(ec);
		}
		return;
	}

	m_ws.text(true);

	ListenToIncomingMessage(ec);

	if (clb_OnConnect) {
		clb_OnConnect(ec);
	}
}

void WebSocketClient::ListenToIncomingMessage(
	const boost::system::error_code& ec
)
{
    if (ec == boost::asio::error::operation_aborted) {
		if (clb_OnDisconnect) {
			clb_OnDisconnect(ec);
		}
        return;
    }

    m_ws.async_read(m_flatBuffer, 
	[this](auto ec, auto nBytesRead) {
		OnRead(ec, nBytesRead);
		ListenToIncomingMessage(ec);
	}
	);
}

void WebSocketClient::OnRead(
	const boost::system::error_code& ec,
	size_t nBytesRead
)
{
    if (ec) {
        return;
    }
	
	std::string message {boost::beast::buffers_to_string(m_flatBuffer.data())};
	m_flatBuffer.consume(nBytesRead);
	if (clb_OnMessage) {
		clb_OnMessage(ec, std::move(message));
	}
}
