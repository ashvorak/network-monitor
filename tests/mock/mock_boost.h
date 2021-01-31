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

/*! \brief Mock the SSL socket stream from Boost.Beast.
 */
template<class NextLayer>
class MockSslStream : public boost::beast::ssl_stream<NextLayer> {
public:
	/*! \brief Inherit all constructors from the parent class.
     */
    using boost::beast::ssl_stream<NextLayer>::ssl_stream;

	static boost::system::error_code handshake_ec;

    template <typename HandshakeHandler>
    void async_handshake(
        boost::asio::ssl::stream_base::handshake_type type,
        HandshakeHandler&& handler
    )
    {
        return boost::asio::async_initiate<
            HandshakeHandler,
            void (boost::system::error_code)
        >(
            [](auto&& handler, auto stream) {
                boost::asio::post(
                    stream->get_executor(),
                    boost::beast::bind_handler(
                        std::move(handler),
                        MockSslStream::handshake_ec
                    )
                );
            },
            handler,
            this
        );
    }
};

template<typename NextLayer>
inline boost::system::error_code MockSslStream<NextLayer>::handshake_ec {};

// This overload is required by Boost.Beast when you define a custom stream.
template <typename TeardownHandler>
void async_teardown(
    boost::beast::role_type role,
    MockSslStream<MockTcpStream>& socket,
    TeardownHandler&& handler
)
{
	return;
}

/*! \brief Mock the websocket::stream from Boost.Beast.
 */
template<class NextLayer>
class MockWebSocketStream : public boost::beast::websocket::stream<NextLayer> {
public:	
	/*! \brief Inherit all constructors from the parent class.
     */
    using boost::beast::websocket::stream<NextLayer>::stream;

	static boost::system::error_code handshake_ec;
	static boost::system::error_code write_ec;
	static boost::system::error_code read_ec;
	static boost::system::error_code close_ec;

	static std::string sReadBuffer;

	template<class HandshakeHandler>
	void async_handshake(
		boost::string_view host,
		boost::string_view target,
		HandshakeHandler&& handler)
    {
        return boost::asio::async_initiate<
            HandshakeHandler,
            void (boost::system::error_code)
        >(
            [](auto&& handler, auto stream, auto host, auto targer) {
				stream->m_closed = false;
                boost::asio::post(
                    stream->get_executor(),
                    boost::beast::bind_handler(
                        std::move(handler),
                        MockWebSocketStream::handshake_ec
                    )
                );
            },
            handler,
            this,
			host.to_string(),
			target.to_string()
        );
    }

	template<class ConstBufferSequence, class WriteHandler>
	void async_write(
		ConstBufferSequence const& buffers,
		WriteHandler&& handler)
    {
        return boost::asio::async_initiate<
            WriteHandler,
            void (boost::system::error_code, size_t)
        >(
            [](auto&& handler, auto stream, auto& buffers) {
				if (stream->m_closed) {
					boost::asio::post(
                    stream->get_executor(),
                    boost::beast::bind_handler(
                        std::move(handler),
                        boost::asio::error::connection_aborted,
						0
                    ));
				}
				else {
					boost::asio::post(
						stream->get_executor(),
						boost::beast::bind_handler(
							std::move(handler),
							MockWebSocketStream::write_ec,
							MockWebSocketStream::write_ec ? 0 : buffers.size()
						)
					);
				}
            },
            handler,
            this,
			buffers
        );
    }

	template<class DynamicBuffer, class ReadHandler>
	void async_read(
		DynamicBuffer& buffer,
    	ReadHandler&& handler)
    {
        return boost::asio::async_initiate<
            ReadHandler,
            void (boost::system::error_code, size_t)
        >(
            [this](auto&& handler, auto stream, auto& buffer) {
				RecursiveRead(handler, buffer);
            },
            handler,
            this,
			buffer
        );
    }

	template<class CloseHandler>
	void async_close(
		boost::beast::websocket::close_reason const& cr,
		CloseHandler&& handler)
    {
        return boost::asio::async_initiate<
            CloseHandler,
            void (boost::system::error_code)
        >(
            [](auto&& handler, auto stream) {
                if (stream->m_closed) {
					boost::asio::post(
                    stream->get_executor(),
                    boost::beast::bind_handler(
                        std::move(handler),
                        boost::asio::error::connection_aborted
                    ));
				}
				else {
					if (!MockWebSocketStream::close_ec) {
						stream->m_closed = true;
					}

					boost::asio::post(
                    stream->get_executor(),
                    boost::beast::bind_handler(
                        std::move(handler),
                        MockWebSocketStream::close_ec
                    ));
				}
            },
            handler,
            this
        );
    }


private:
	bool m_closed {true};

		// This function mimicks a socket reading messages. It's the function we
	// call from async_read.
	template <typename DynamicBuffer, typename ReadHandler>
	void RecursiveRead(
		ReadHandler&& handler,
		DynamicBuffer& buffer)
	{
		if (m_closed) {
			// If the connection has been closed, the read operation aborts.
			boost::asio::post(
				this->get_executor(),
				boost::beast::bind_handler(
					std::move(handler),
					boost::asio::error::operation_aborted,
					0
				)
			);
		} else {
			// Read the buffer. This may be empty — For testing purposes, we
			// interpret this as "no new message".
			size_t nRead;
			nRead = MockWebSocketStream::sReadBuffer.size();
			nRead = boost::asio::buffer_copy(
				buffer.prepare(nRead),
				boost::asio::buffer(MockWebSocketStream::sReadBuffer)
			);
			buffer.commit(nRead);

			// We clear the mock buffer for the next read.
			MockWebSocketStream::sReadBuffer = "";

			if (nRead == 0) {
				// If there was nothing to read, we recursively go and wait for
				// a new message.
				// Note: We can't just loop on RecursiveRead, we have to use
				//       post, otherwise this handler would be holding the
				//       io_context hostage.
				boost::asio::post(
					this->get_executor(),
					[this, handler = std::move(handler), &buffer]() {
						RecursiveRead(handler, buffer);
					}
				);
			} else {
				// On a legitimate message, just call the async_read original
				// handler.
				boost::asio::post(
					this->get_executor(),
					boost::beast::bind_handler(
						std::move(handler),
						MockWebSocketStream::read_ec,
						nRead
					)
				);
			}
		}
	}

};

template<typename NextLayer>
boost::system::error_code MockWebSocketStream<NextLayer>::handshake_ec {};

template<typename NextLayer>
boost::system::error_code MockWebSocketStream<NextLayer>::write_ec {};

template<typename NextLayer>
boost::system::error_code MockWebSocketStream<NextLayer>::read_ec {};

template<typename NextLayer>
boost::system::error_code MockWebSocketStream<NextLayer>::close_ec {};

template <typename NextLayer>
std::string MockWebSocketStream<NextLayer>::sReadBuffer = "";

using MockTlsStream = MockSslStream<MockTcpStream>;

using MockTlsWebSocketStream = MockWebSocketStream<MockTlsStream>;

/*! \brief Type alias for the mocked WebSocketClient.
 *
 *  For now we only mock the DNS resolver.
 */
using MockWebSocketClient = WebSocketClient<MockResolver, MockTlsWebSocketStream>;

} // NetworkMonitor