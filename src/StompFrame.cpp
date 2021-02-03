#include <network-monitor/StompFrame.h>

#include <boost/bimap.hpp>
#include <charconv>

using NetworkMonitor::StompCommand;
using NetworkMonitor::StompHeader;
using NetworkMonitor::StompError;
using NetworkMonitor::StompFrame;

using Headers = StompFrame::Headers;

// https://stackoverflow.com/questions/20288871/how-to-construct-boost-bimap-from-static-list
template <typename L, typename R>
boost::bimap<L, R>
MakeBimap(std::initializer_list<typename boost::bimap<L, R>::value_type> list)
{
    return boost::bimap<L, R>(list.begin(), list.end());
}

static std::string ConstructFrame(
	const StompCommand& command,
    const Headers& 		headers,
    const std::string& 	body
)
{
    using namespace std::string_literals;

    // Construct the frame as a string.
    std::string frame {""};
    frame += NetworkMonitor::ToString(command);
    frame += "\n";
    for (const auto& [header, value]: headers) {
        frame += NetworkMonitor::ToString(header);
        frame += ":";
        frame += value;
        frame += "\n";
    }
    frame += "\n";
    frame += body;
    frame += "\0"s;

	return frame;
}

static bool HasHeader(const Headers& headers, const StompHeader& header)
{
	return headers.find(header) != headers.end();
}

static std::string_view GetHeaderValue(const Headers& headers, const StompHeader stompHeader) 
{
	auto headerIt {headers.find(stompHeader)};
	if (headerIt == headers.end()) {
		return {};
	}

	return headerIt->second;
}

// StompCommand 

static const auto gStompCommands = MakeBimap<StompCommand, std::string_view>(
{
	{StompCommand::kAbort      , "ABORT"      	  },
	{StompCommand::kAck        , "ACK"        	  },
	{StompCommand::kBegin      , "BEGIN"      	  },
	{StompCommand::kCommit     , "COMMIT"     	  },
	{StompCommand::kConnect    , "CONNECT"        },
	{StompCommand::kConnected  , "CONNECTED"      },
	{StompCommand::kDisconnect , "DISCONNECT"     },
	{StompCommand::kError      , "ERROR"      	  },
	{StompCommand::kMessage    , "MESSAGE"    	  },
	{StompCommand::kNack       , "NACK"       	  },
	{StompCommand::kReceipt    , "RECEIPT"    	  },
	{StompCommand::kSend       , "SEND"       	  },
	{StompCommand::kStomp      , "STOMP"      	  },
	{StompCommand::kSubscribe  , "SUBSCRIBE"  	  },
	{StompCommand::kUnsubscribe, "UNSUBSCRIBE"	  },
	{StompCommand::kUnknown	   , "UNKNOWN COMMAND"}
});

std::string NetworkMonitor::ToString(const StompCommand& command) {
	auto commandIt {gStompCommands.left.find(command)};
	if (commandIt == gStompCommands.left.end()) {
		return ToString(StompCommand::kUnknown);
	}

	return std::string(commandIt->second);
}

std::ostream& NetworkMonitor::operator<<(std::ostream& out, const StompCommand& command) {
    return out << ToString(command);
}

// StompHeader

static const auto gStompHeaders = MakeBimap<StompHeader, std::string_view>(
{
	{StompHeader::kAcceptVersion, "accept-version"},
	{StompHeader::kAck          , "ack"           },
	{StompHeader::kContentLength, "content-length"},
	{StompHeader::kContentType  , "content-type"  },
	{StompHeader::kDestination  , "destination"   },
	{StompHeader::kHeartBeat    , "heart-beat"    },
	{StompHeader::kHost         , "host"          },
	{StompHeader::kId           , "id"            },
	{StompHeader::kLogin        , "login"         },
	{StompHeader::kMessage      , "message"       },
	{StompHeader::kMessageId    , "message-id"    },
	{StompHeader::kPasscode     , "passcode"      },
	{StompHeader::kReceipt      , "receipt"       },
	{StompHeader::kReceiptId    , "receipt-id"    },
	{StompHeader::kSession      , "session"       },
	{StompHeader::kSubscription , "subscription"  },
	{StompHeader::kTransaction  , "transaction"   },
	{StompHeader::kServer       , "server"        },
	{StompHeader::kVersion      , "version"       },
	{StompHeader::kUnknown      , "unknown header"}
});

std::string NetworkMonitor::ToString(const StompHeader& header) {
	auto headerIt {gStompHeaders.left.find(header)};
	if (headerIt == gStompHeaders.left.end()) {
		return ToString(StompHeader::kUnknown);
	}

	return std::string(headerIt->second);
}

std::ostream& NetworkMonitor::operator<<(std::ostream& out, const StompHeader& header) {
    return out << ToString(header);
}

// StompError

auto gStompErrors = MakeBimap<StompError, std::string_view>(
{
	{StompError::kOk							, "StompError::kOk"},
	{StompError::kErrorCommandInvalid			, "StompError::kErrorCommandInvalid"},
	{StompError::kErrorHeaderMissing			, "StompError::kErrorHeaderMissing"},
	{StompError::kErrorHeaderEmpty				, "StompError::kErrorHeaderEmpty"},
	{StompError::kErrorHeaderMissingNewLine		, "StompError::kErrorHeaderMissingNewLine"},
	{StompError::kErrorHeaderInvalidKey			, "StompError::kErrorHeaderInvalidKey"},
	{StompError::kErrorHeaderEmptyValue			, "StompError::kErrorHeaderEmptyValue"},
	{StompError::kErrorHeaderMissingSemicolon	, "StompError::kErrorHeaderMissingSemicolon"},
	{StompError::kErrorHeaderContentLength		, "StompError::kErrorHeaderContentLength"},
	{StompError::kErrorBodyNoNewLine			, "StompError::kErrorBodyNoNewLine"},
	{StompError::kErrorBodyEmpty				, "StompError::kErrorBodyEmpty"},
	{StompError::kErrorBodyLength				, "StompError::kErrorBodyLength"},
	{StompError::kErrorBodyMissingNull			, "StompError::kErrorBodyMissingNull"},
	{StompError::kErrorWrongSymbolAfterBody		, "StompError::kErrorSymbolAfterBody"},
	{StompError::kErrorUnknown					, "StompError::kErrorUnknown"}
});

std::string NetworkMonitor::ToString(const StompError& error) {
	auto errorIt {gStompErrors.left.find(error)};
	if (errorIt == gStompErrors.left.end()) {
		return ToString(StompError::kErrorUnknown);
	}

	return std::string(errorIt->second);
}

std::ostream& NetworkMonitor::operator<<(std::ostream& out, const StompError& error) {
    return out << ToString(error);
}

// StompFrame 

StompFrame::StompFrame(
	StompError& ec,
	const std::string& frame
)
{
	ec = ParseAndValidateFrame(frame);
}

StompFrame::StompFrame(
	StompError& ec,
	std::string&& frame
)
{
	ec = ParseAndValidateFrame(frame);
}

StompFrame::StompFrame(
    StompError& ec,
    const StompCommand& command,
    const Headers& headers,
    const std::string& body
)
{
    ec = ParseAndValidateFrame(
		ConstructFrame(command, headers, body)
	);
}

StompFrame::StompFrame(const StompFrame& other) = default;

StompFrame::StompFrame(StompFrame&& other) = default;

StompFrame& StompFrame::operator=(const StompFrame& other) = default;

StompFrame& StompFrame::operator=(StompFrame&& other) = default;

StompCommand StompFrame::GetCommand() const {
	return m_command;
}

std::string_view StompFrame::GetHeaderValue(const StompHeader header) const {
	return ::GetHeaderValue(m_headers, header);
}

std::string_view StompFrame::GetBody() const {
	return m_body;
}

std::string StompFrame::ToString() const
{
	return ConstructFrame(
		m_command, 
		m_headers, 
		std::string {m_body}
	);
}

StompError StompFrame::ParseAndValidateFrame(const std::string_view frame) {
	// Frame delimiters
    static const char null {'\0'};
    static const char colon {':'};
    static const char newLine {'\n'};

	// Parse Command
	size_t commandEnd {frame.find(newLine)};
	if (commandEnd == std::string::npos) {
		return StompError::kErrorHeaderEmpty;
	}
	std::string_view command_v {&frame[0], commandEnd};

	auto commandIt {gStompCommands.right.find(command_v)};
	if (commandIt == gStompCommands.right.end()) {
		return StompError::kErrorCommandInvalid;
	}
	auto command = commandIt->second;

	// Parse Headers
	Headers headers {};
	size_t headerStart = command_v.length() + 1;
	while (headerStart < frame.length() && frame.at(headerStart) != newLine) {

		size_t headerEnd {frame.find(newLine, headerStart)};
		if (headerEnd == std::string::npos) {
			return StompError::kErrorHeaderMissingNewLine;
		}
		std::string_view header_v {&frame[headerStart], headerEnd - headerStart};
	
		size_t header_vDelim = header_v.find(colon);
		if (header_vDelim == std::string::npos) {
			return StompError::kErrorHeaderMissingSemicolon;
		}
		if (header_vDelim + 1 == headerEnd - headerStart) {
			return StompError::kErrorHeaderEmptyValue;
		}

		std::string_view headerKey_v {&header_v[0], header_vDelim};
		auto headerIt {gStompHeaders.right.find(headerKey_v)};
		if (headerIt == gStompHeaders.right.end()) {
			return StompError::kErrorHeaderInvalidKey;
		}
		if (headers.find(headerIt->second) == headers.end()) {
			headers.emplace(std::make_pair(
				headerIt->second,
				std::move(std::string{header_v.substr(header_vDelim + 1)}))
			);
		}
		
		headerStart = headerEnd + 1;	
	}
	if (headerStart >= frame.length() || frame.at(headerStart) != newLine) {
		return StompError::kErrorBodyNoNewLine;
	}

	//Parse Body
	size_t bodyStart {headerStart + 1};
	size_t bodyLength {0};
	size_t bodyEnd {0};

	auto contentLengthIt {headers.find(StompHeader::kContentLength)};
	if (contentLengthIt != headers.end()) {

		bodyLength = std::stoi(contentLengthIt->second);
        if (bodyLength == (frame.size() - bodyStart)) {
            return StompError::kErrorBodyMissingNull;
        }
        if (bodyLength > (frame.size() - bodyStart)) {
            return StompError::kErrorBodyLength;
        }
        bodyEnd = bodyStart + bodyLength;
        if (frame.at(bodyEnd) != null) {
            return StompError::kErrorBodyMissingNull;
        }	
	}
	else {
		bodyEnd = frame.find(null, bodyStart);
        if (bodyEnd == std::string::npos) {
            return StompError::kErrorBodyMissingNull;
        }
        bodyLength = bodyEnd - bodyStart;
	}
    for (size_t idx {bodyEnd + 1}; idx < frame.size(); ++idx) {
        if (frame.at(idx) != newLine) {
            return StompError::kErrorWrongSymbolAfterBody;
        }
    }
    std::string body {frame.substr(bodyStart, bodyLength)};

	auto ec {ValidateFrame(command, headers, body)};
	if (ec != StompError::kOk) {
		return ec;
	}

	// Move all data
	m_command = std::move(command);
	m_headers = std::move(headers);
	m_body = std::move(body);

	return StompError::kOk;
}

StompError StompFrame::ValidateFrame(	
	const StompCommand& command,
    const Headers& 		headers, 
    const std::string& 	body
) const
{
    bool hasAllHeaders {true};
    switch (command) {
        case StompCommand::kConnect:
        case StompCommand::kStomp:
            hasAllHeaders &= HasHeader(headers, StompHeader::kAcceptVersion);
            hasAllHeaders &= HasHeader(headers, StompHeader::kHost);
            break;
        case StompCommand::kConnected:
            hasAllHeaders &= HasHeader(headers, StompHeader::kVersion);
            break;
        case StompCommand::kSend:
            hasAllHeaders &= HasHeader(headers, StompHeader::kDestination);
            break;
        case StompCommand::kSubscribe:
            hasAllHeaders &= HasHeader(headers, StompHeader::kDestination);
            hasAllHeaders &= HasHeader(headers, StompHeader::kId);
            break;
        case StompCommand::kUnsubscribe:
            hasAllHeaders &= HasHeader(headers, StompHeader::kId);
            break;
        case StompCommand::kAck:
        case StompCommand::kNack:
            hasAllHeaders &= HasHeader(headers, StompHeader::kId);
            break;
        case StompCommand::kBegin:
        case StompCommand::kCommit:
        case StompCommand::kAbort:
            hasAllHeaders &= HasHeader(headers, StompHeader::kTransaction);
            break;
        case StompCommand::kDisconnect:
            break;
        case StompCommand::kMessage:
            hasAllHeaders &= HasHeader(headers, StompHeader::kDestination);
            hasAllHeaders &= HasHeader(headers, StompHeader::kMessageId);
            hasAllHeaders &= HasHeader(headers, StompHeader::kSubscription);
            break;
        case StompCommand::kReceipt:
            hasAllHeaders &= HasHeader(headers, StompHeader::kReceiptId);
            break;
        case StompCommand::kError:
            break;
        default:
            return StompError::kErrorUnknown;
    }
    if (!hasAllHeaders) {
        return StompError::kErrorHeaderMissing;
    }

    if (HasHeader(headers, StompHeader::kContentLength)) {
		// https://stackoverflow.com/questions/56634507/safely-convert-stdstring-view-to-int-like-stoi-or-atoi
		auto contentLength {::GetHeaderValue(headers, StompHeader::kContentLength)};
		size_t bodyLength {0};
		auto result = std::from_chars(
			contentLength.data(), 
			contentLength.data() + contentLength.size(), 
			bodyLength
		);
    	if (result.ec == std::errc::invalid_argument) {
        	return StompError::kErrorHeaderContentLength;
    	}
        if (bodyLength != body.length()) {
            return StompError::kErrorBodyLength;
        }
	}

    return StompError::kOk;
}