#pragma once

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/system/error_code.hpp>

#include <chrono>
#include <functional>
#include <iomanip>
#include <iostream>
#include <string>

namespace NetworkMonitor {

/*! \brief Client to connect to a WebSocket server over TLS
 *
 *  \tparam Resolver        The class to resolve the URL to an IP address. It
 *                          must support the same interface of
 *                          boost::asio::ip::tcp::resolver.
 *  \tparam WebSocketStream The WebSocket stream class. It must support the
 *                          same interface of boost::beast::websocket::stream.
 */
template <
    typename Resolver,
    typename WebSocketStream
>
class WebSocketClient {
public:
    /*! \brief Construct a WebSocket client.
     *
     *  \note This constructor does not initiate a connection.
     *
     *  \param url  The URL of the server.
     *  \param endpoint The endpoint on the server to connect to.
     *                  Example: echo.websocket.org/<endpoint>
     *  \param port The port on the server.
     *  \param ioc  The io_context object. The user takes care of calling
     *              ioc.run().
     *  \param ctx  The TLS context to setup a TLS socket stream.
     */
	WebSocketClient(
		const std::string& url,
		const std::string& endpoint,
		const std::string& port,
		boost::asio::io_context& ioc,
		boost::asio::ssl::context& ctx
	) : m_url {url},
		m_endpoint {endpoint}, 
		m_port {port},
		m_resolver{boost::asio::make_strand(ioc)},
		m_ws{boost::asio::make_strand(ioc), ctx}
	{

	}

    /*! \brief Destructor
     */
    ~WebSocketClient() = default;

    /*! \brief Connect to the server.
     *
     *  \param onConnect     Called when the connection fails or succeeds.
     *  \param onMessage     Called only when a message is successfully
     *                       received. The message is an rvalue reference;
     *                       ownership is passed to the receiver.
     *  \param onDisconnect  Called when the connection is closed by the server
     *                       or due to a connection error.
     */
    void Connect(
        std::function<void (boost::system::error_code)> onConnect = nullptr,
        std::function<void (boost::system::error_code,
                            std::string&&)> onMessage = nullptr,
        std::function<void (boost::system::error_code)> onDisconnect = nullptr
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

    /*! \brief Send a text message to the WebSocket server.
     *
     *  \param message The message to send.
     *  \param onSend  Called when a message is sent successfully or if it
     *                 failed to send.
     */
    void Send(
        const std::string& message,
        std::function<void (boost::system::error_code)> onSend = nullptr
    )
	{
		m_ws.async_write(boost::asio::buffer(std::move(message)), 
			[this, onSend](auto ec, auto) {
				if (onSend) {
					onSend(ec);
				}
		});
	}

    /*! \brief Close the WebSocket connection.
     *
     *  \param onClose Called when the connection is closed, successfully or
     *                 not.
     */
    void Close(
        std::function<void (boost::system::error_code)> onClose = nullptr
    )
	{
		m_ws.async_close(boost::beast::websocket::close_code::none,
			[this, onClose](auto ec) {
				if (onClose) {
					onClose(ec);
				};
		});
	}

private:
	std::string m_url {};
	std::string m_endpoint {};
	std::string m_port {};

	Resolver m_resolver;
	WebSocketStream m_ws;

	boost::beast::flat_buffer m_flatBuffer;

	std::function<void (boost::system::error_code)> clb_OnConnect = nullptr;
	std::function<void (boost::system::error_code,
						std::string&&)> clb_OnMessage = nullptr;
	std::function<void (boost::system::error_code)> clb_OnDisconnect = nullptr;

	static void Log(const std::string& where, boost::system::error_code ec)
	{
		std::cerr << "[" << std::setw(20) << where << "] "
				<< (ec ? "Error: " : "OK")
				<< (ec ? ec.message() : "")
				<< std::endl;
	}

	void OnResolve(
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

	void OnConnect(
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
		m_ws.set_option(boost::beast::websocket::stream_base::timeout::suggested(
			boost::beast::role_type::client
		));

		// Attempt a TLS handshake.
		m_ws.next_layer().async_handshake(boost::asio::ssl::stream_base::client,
			[this](auto ec) {
				OnTlsHandshake(ec);
			}
		);
	}

	void OnTlsHandshake(
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

		m_ws.async_handshake(m_url, m_endpoint,
			[this](auto ec) {
				OnHandshake(ec);
			}
		);
	}

	void OnHandshake(
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

	void ListenToIncomingMessage(
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
		});
	}

	void OnRead(
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

};

using BoostWebSocketClient = WebSocketClient<
    boost::asio::ip::tcp::resolver,
    boost::beast::websocket::stream<
        boost::beast::ssl_stream<boost::beast::tcp_stream>
    >
>;

} // namespace NetworkMonitor