#pragma once

#include <string>
#include <map>
#include <unordered_map>
#include <ostream>

namespace NetworkMonitor {

// STOMP 1.2 
// https://stomp.github.io/stomp-specification-1.2.html

/*! \brief Available STOMP commands, from the STOMP protocol v1.2.
 */
enum class StompCommand {
    kAbort,
    kAck,
    kBegin,
    kCommit,
    kConnect,
    kConnected,
    kDisconnect,
    kError,
    kMessage,
    kNack,
    kReceipt,
    kSend,
    kStomp,
    kSubscribe,
    kUnsubscribe,
	kUnknown,
};

std::string ToString(const StompCommand& command);

std::ostream& operator<<(std::ostream& out, const StompCommand& command);

/*! \brief Available STOMP headers, from the STOMP protocol v1.2.
 */
enum class StompHeader {
    kAcceptVersion,
    kAck,
    kContentLength,
    kContentType,
    kDestination,
    kHeartBeat,
    kHost,
    kId,
    kLogin,
    kMessage,
    kMessageId,
    kPasscode,
    kReceipt,
    kReceiptId,
    kSession,
    kSubscription,
    kTransaction,
    kServer,
    kVersion,
	kUnknown,
};

std::string ToString(const StompHeader& header);

std::ostream& operator<<(std::ostream& out, const StompHeader& header);

/*! \brief Error codes for the STOMP protocol
 */
enum class StompError {
    kOk = 0,
	kErrorCommandInvalid,
	kErrorHeaderMissing,
	kErrorHeaderEmpty,
	kErrorHeaderMissingNewLine,
	kErrorHeaderInvalidKey,
	kErrorHeaderEmptyValue,
	kErrorHeaderMissingSemicolon,
	kErrorHeaderContentLength,
	kErrorBodyNoNewLine,
	kErrorBodyEmpty,
	kErrorBodyLength,
	kErrorBodyMissingNull,
	kErrorWrongSymbolAfterBody,
	kErrorUnknown,
};

std::string ToString(const StompError& error);

std::ostream& operator<<(std::ostream& out, const StompError& error);

/* \brief STOMP frame representation, supporting STOMP v1.2.
 */
class StompFrame {
public:

	using Headers = std::unordered_map<StompHeader, std::string>;

    /*! \brief Default constructor. Corresponds to an empty, invalid STOMP
     *         frame.
     */
    StompFrame();

    /*! \brief Construct the STOMP frame from a string. The string is copied.
     *
     *  The result of the operation is stored in the error code.
     */
    StompFrame(
        StompError& ec,
        const std::string& frame
    );

    /*! \brief Construct the STOMP frame from a string. The string is moved into
     *         the object.
     *
     *  The result of the operation is stored in the error code.
     */
    StompFrame(
        StompError& ec,
        std::string&& frame
    );

	/*! \brief Construct the STOMP frame from a StompCommand, StompHeaders and Body.
     *
     *  The result of the operation is stored in the error code.
     */
	StompFrame(
		StompError& ec,
		const StompCommand& command,
		const Headers& headers,
		const std::string& body
	);

    /*! \brief The copy constructor.
     */
    StompFrame(const StompFrame& other);

    /*! \brief Move constructor.
     */
    StompFrame(StompFrame&& other);

    /*! \brief Copy assignment operator.
     */
    StompFrame& operator=(const StompFrame& other);

    /*! \brief Move assignment operator.
     */
    StompFrame& operator=(StompFrame&& other);

    /*! \brief Get STOMP frame command.
     */
	StompCommand		GetCommand() const;

    /*! \brief Get STOMP frame header value.
     */
	std::string_view	GetHeaderValue(const StompHeader header) const;

    /*! \brief Get STOMP frame body.
     */
	std::string_view 	GetBody() const;

    /*! \brief Returns string representation of frame.
     */
	std::string 		ToString() const;

private:

	StompCommand		m_command = StompCommand::kUnknown;
	Headers				m_headers {};
	std::string			m_body {};

	StompError	ParseAndValidateFrame(const std::string_view frame);

	StompError 	ValidateFrame(	
		const StompCommand& command,
		const Headers& 		headers,
		const std::string& 	body
	) const;
	
};

} // namespace NetworkMonitor
