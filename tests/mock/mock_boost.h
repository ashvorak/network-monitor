#pragma once

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/system/error_code.hpp>

#include <network-monitor/WebSocketClient.h>

namespace NetworkMonitor {

/*! \brief Mock the tcp::resolver class from Boost.Asio. 
 */
class MockResolver {

public:

	static boost::system::error_code resolve_ec;

    template <typename ExecutionContext>
    explicit MockResolver(
        ExecutionContext&& context
    ) : m_context {context}
    {
    }

    /*! \brief Mock for resolver::async_resolve
     */
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
			[](auto&& handler, auto resolver, auto host, auto service) {
				if (MockResolver::resolve_ec) {
                    boost::asio::post(
                        resolver->m_context,
                        boost::beast::bind_handler(
                            std::move(handler),
                            MockResolver::resolve_ec,
                            resolver::results_type {}
                        )
                    );
				}
				else {
                    // Successful branch.
                    boost::asio::post(
                        resolver->m_context,
                        boost::beast::bind_handler(
                            std::move(handler),
                            MockResolver::resolve_ec,
                            // Note: The create static method is in the public
                            //       resolver interface but it is not
                            //       documented.
                            resolver::results_type::create(
                                boost::asio::ip::tcp::endpoint {
                                    boost::asio::ip::make_address(
                                        "127.0.0.1"
                                    ),
                                    443
                                },
                                host,
                                service
                            )
                        )
                    );
				}
			},
            handler,
            this,
            host.to_string(),
            service.to_string()
		);
	}

private:
	boost::asio::strand<boost::asio::io_context::executor_type> m_context;	

};

inline boost::system::error_code MockResolver::resolve_ec {};

/*! \brief Mock the TCP socket stream from Boost.Beast.
 */
class MockTcpStream: public boost::beast::tcp_stream {
public:
    /*! \brief Inherit all constructors from the parent class.
     */
    using boost::beast::tcp_stream::tcp_stream;

    static boost::system::error_code connect_ec;

    /*! \brief Mock for tcp_stream::async_connect
     */
    template <typename ConnectHandler>
    void async_connect(
        endpoint_type type,
        ConnectHandler&& handler
    )
    {
        return boost::asio::async_initiate<
            ConnectHandler,
            void (boost::system::error_code)
        >(
            [](auto&& handler, auto stream) {
                // Call the user callback.
                boost::asio::post(
                    stream->get_executor(),
                    boost::beast::bind_handler(
                        std::move(handler),
                        MockTcpStream::connect_ec
                    )
                );
            },
            handler,
            this
        );
    }
};

// Out-of-line static member initialization
inline boost::system::error_code MockTcpStream::connect_ec {};

// This overload is required by Boost.Beast when you define a custom stream.
template <typename TeardownHandler>
void async_teardown(
    boost::beast::role_type role,
    MockTcpStream& socket,
    TeardownHandler&& handler
)
{
    return;
}

/*! \brief Type alias for the mocked WebSocketClient.
 *
 *  For now we only mock the DNS resolver.
 */
using MockWebSocketClient = WebSocketClient<
    MockResolver,
    boost::beast::websocket::stream<
        boost::beast::ssl_stream<MockTcpStream>
    >
>;

} // NetworkMonitor