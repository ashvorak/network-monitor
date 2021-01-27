#pragma once

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/system/error_code.hpp>

#include <network-monitor/WebSocketClient.h>

namespace NetworkMonitor {

class MockResolver {

public:

	static boost::system::error_code ec;

    template <typename ExecutionContext>
    explicit MockResolver(
        ExecutionContext&& context
    ) : m_context {context}
    {
    }

	template <typename ResolveHandler>
	void async_resolve(
		boost::string_view host,
		boost::string_view service,
		ResolveHandler&& handler
	)
	{
		using resolver = boost::asio::ip::tcp::resolver;
		return boost::asio::async_initiate<
			ResolveHandler,
			void (const boost::system::error_code&, resolver::results_type)
		>(
			[](auto&& handler, auto resolver) {
				if (MockResolver::ec) {
                    boost::asio::post(
                        resolver->m_context,
                        boost::beast::bind_handler(
                            std::move(handler),
                            MockResolver::ec,
                            resolver::results_type {}
                        )
                    );
				}
				else {

				}
			},
			handler,
			this
		);
	}

private:
	boost::asio::strand<boost::asio::io_context::executor_type> m_context;	

};

inline boost::system::error_code MockResolver::ec {};

/*! \brief Type alias for the mocked WebSocketClient.
 *
 *  For now we only mock the DNS resolver.
 */
using MockWebSocketClient = WebSocketClient<
    MockResolver,
    boost::beast::websocket::stream<
        boost::beast::ssl_stream<boost::beast::tcp_stream>
    >
>;

}