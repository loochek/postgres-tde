#include "postgres_tde/source/filters/network/postgres_tde/postgres_decoder.h"

#include <vector>

#include "absl/strings/str_split.h"
#include "absl/cleanup/cleanup.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace PostgresTDE {

#define BODY_FORMAT(...)                                                                           \
  []() -> std::unique_ptr<Message> { return createMsgBodyReader<__VA_ARGS__>(); }
#define NO_BODY BODY_FORMAT()

#define TYPED_BODY_FORMAT(TYPE)                                                                    \
  []() -> std::unique_ptr<Message> { return std::make_unique<TYPE>(); }

constexpr absl::string_view FRONTEND = "Frontend";
constexpr absl::string_view BACKEND = "Backend";

void DecoderImpl::initialize() {
  // Special handler for first message of the transaction.
  startup_msg_processor_ =
      MessageProcessor{"Startup", BODY_FORMAT(Int32, Repeated<String>), {}};

  // Frontend messages.
  FE_messages_.direction_ = FRONTEND;

  // Setup handlers for known messages.
  absl::flat_hash_map<char, MessageProcessor>& FE_known_msgs = FE_messages_.messages_;

  // Handler for known Frontend messages.
  FE_known_msgs['B'] = MessageProcessor{
      "Bind", BODY_FORMAT(String, String, Array<Int16>, Array<VarByteN>, Array<Int16>), {}};
  FE_known_msgs['C'] = MessageProcessor{"Close", BODY_FORMAT(Byte1, String), {}};
  FE_known_msgs['d'] = MessageProcessor{"CopyData", BODY_FORMAT(ByteN), {}};
  FE_known_msgs['c'] = MessageProcessor{"CopyDone", NO_BODY, {}};
  FE_known_msgs['f'] = MessageProcessor{"CopyFail", BODY_FORMAT(String), {}};
  FE_known_msgs['D'] = MessageProcessor{"Describe", BODY_FORMAT(Byte1, String), {}};
  FE_known_msgs['E'] = MessageProcessor{"Execute", BODY_FORMAT(String, Int32), {}};
  FE_known_msgs['H'] = MessageProcessor{"Flush", NO_BODY, {}};
  FE_known_msgs['F'] = MessageProcessor{
      "FunctionCall", BODY_FORMAT(Int32, Array<Int16>, Array<VarByteN>, Int16), {}};
  FE_known_msgs['p'] =
      MessageProcessor{"PasswordMessage/GSSResponse/SASLInitialResponse/SASLResponse",
                       BODY_FORMAT(Int32, ByteN),
                       {}};
  FE_known_msgs['P'] =
      MessageProcessor{"Parse", TYPED_BODY_FORMAT(ParseMessage), {&DecoderImpl::onParse}};
  FE_known_msgs['Q'] = MessageProcessor{"Query", TYPED_BODY_FORMAT(QueryMessage), {&DecoderImpl::onQuery}};
  FE_known_msgs['S'] = MessageProcessor{"Sync", NO_BODY, {}};
  FE_known_msgs['X'] =
      MessageProcessor{"Terminate", NO_BODY, {}};

  // Handler for unknown Frontend messages.
  FE_messages_.unknown_ =
      MessageProcessor{"Other", BODY_FORMAT(ByteN), {}};

  // Backend messages.
  BE_messages_.direction_ = BACKEND;

  // Setup handlers for known messages.
  absl::flat_hash_map<char, MessageProcessor>& BE_known_msgs = BE_messages_.messages_;

  // Handler for known Backend messages.
  BE_known_msgs['R'] =
      MessageProcessor{"Authentication", BODY_FORMAT(ByteN), {}};
  BE_known_msgs['K'] = MessageProcessor{"BackendKeyData", BODY_FORMAT(Int32, Int32), {}};
  BE_known_msgs['2'] = MessageProcessor{"BindComplete", NO_BODY, {}};
  BE_known_msgs['3'] = MessageProcessor{"CloseComplete", NO_BODY, {}};
  BE_known_msgs['C'] = MessageProcessor{
      "CommandComplete",
      TYPED_BODY_FORMAT(CommandCompleteMessage),
      {&DecoderImpl::onCommandComplete},
  };
  BE_known_msgs['d'] = MessageProcessor{"CopyData", BODY_FORMAT(ByteN), {}};
  BE_known_msgs['c'] = MessageProcessor{"CopyDone", NO_BODY, {}};
  BE_known_msgs['G'] = MessageProcessor{"CopyInResponse", BODY_FORMAT(Int8, Array<Int16>), {}};
  BE_known_msgs['H'] = MessageProcessor{"CopyOutResponse", BODY_FORMAT(Int8, Array<Int16>), {}};
  BE_known_msgs['W'] = MessageProcessor{"CopyBothResponse", BODY_FORMAT(Int8, Array<Int16>), {}};
  BE_known_msgs['D'] = MessageProcessor{
      "DataRow",
      TYPED_BODY_FORMAT(DataRowMessage),
      {&DecoderImpl::onDataRow},
  };
  BE_known_msgs['I'] = MessageProcessor{"EmptyQueryResponse", TYPED_BODY_FORMAT(EmptyQueryResponseMessage), {&DecoderImpl::onEmptyQueryResponse}};
  BE_known_msgs['E'] = MessageProcessor{
      "ErrorResponse",
      TYPED_BODY_FORMAT(ErrorResponseMessage),
      {&DecoderImpl::onErrorResponse},
  };
  BE_known_msgs['V'] = MessageProcessor{"FunctionCallResponse", BODY_FORMAT(VarByteN), {}};
  BE_known_msgs['v'] = MessageProcessor{"NegotiateProtocolVersion", BODY_FORMAT(ByteN), {}};
  BE_known_msgs['n'] = MessageProcessor{"NoData", NO_BODY, {}};
  BE_known_msgs['N'] = MessageProcessor{
      "NoticeResponse", BODY_FORMAT(ByteN), {}};
  BE_known_msgs['A'] =
      MessageProcessor{"NotificationResponse", BODY_FORMAT(Int32, String, String), {}};
  BE_known_msgs['t'] = MessageProcessor{"ParameterDescription", BODY_FORMAT(Array<Int32>), {}};
  BE_known_msgs['S'] = MessageProcessor{"ParameterStatus", BODY_FORMAT(String, String), {}};
  BE_known_msgs['1'] = MessageProcessor{"ParseComplete", NO_BODY, {}};
  BE_known_msgs['s'] = MessageProcessor{"PortalSuspend", NO_BODY, {}};
  BE_known_msgs['Z'] = MessageProcessor{"ReadyForQuery", TYPED_BODY_FORMAT(ReadyForQueryMessage), {}};
  BE_known_msgs['T'] = MessageProcessor{
      "RowDescription",
      TYPED_BODY_FORMAT(RowDescriptionMessage),
      {&DecoderImpl::onRowDescription}
  };

  // Handler for unknown Backend messages.
  BE_messages_.unknown_ =
      MessageProcessor{"Other", BODY_FORMAT(ByteN), {}};
}

/* Main handler for incoming messages. Messages are dispatched based on the
   current decoder's state.
*/
Decoder::Result DecoderImpl::onData(Buffer::Instance& parse_data, bool frontend) {
//  ENVOY_LOG(error, "onData ent: {} bytes {}", parse_data.length(), parse_data.toString());
//  absl::Cleanup cleanup = [&]() {
//    ENVOY_LOG(error, "onData ext: {} bytes {}", parse_data.length(), parse_data.toString());
//  };

  switch (state_) {
  case State::InitState:
    return onDataInit(parse_data, frontend);
  case State::OutOfSyncState:
  case State::EncryptedState:
    return onDataIgnore(parse_data, frontend);
  case State::InSyncState:
    return onDataInSync(parse_data, frontend);
  case State::NegotiatingUpstreamSSL:
    return onDataInNegotiating(parse_data, frontend);
  default:
    PANIC("not implemented");
  }
}

/* Handler for messages when decoder is in Init State. There are very few message types which
   are allowed in this state.
   If the initial message has the correct syntax and  indicates that session should be in
   clear-text, the decoder will move to InSyncState. If the initial message has the correct syntax
   and indicates that session should be encrypted, the decoder stays in InitState, because the
   initial message will be received again after transport socket negotiates SSL. If the message
   syntax is incorrect, the decoder will move to OutOfSyncState, in which messages are not parsed.
*/
Decoder::Result DecoderImpl::onDataInit(Buffer::Instance& data, bool) {
  ASSERT(state_ == State::InitState);

  // In Init state the minimum size of the message sufficient for parsing is 4 bytes.
  if (data.length() < 4) {
    // not enough data in the buffer.
    return Decoder::Result::NeedMoreData;
  }

  // Validate the message before processing.
  const MsgBodyReader& message_factory = std::get<1>(startup_msg_processor_);
  replacement_message_ = message_factory();
  // Run the validation.
  uint32_t message_len = data.peekBEInt<uint32_t>(0);
  if (message_len > MAX_STARTUP_PACKET_LENGTH) {
    // Message does not conform to the expected format. Move to out-of-sync state.
    data.drain(data.length());
    state_ = State::OutOfSyncState;
    return Decoder::Result::ReadyForNext;
  }

  Message::ValidationResult validationResult =
      replacement_message_->validate(data, 4, message_len - 4);

  if (validationResult == Message::ValidationNeedMoreData) {
    return Decoder::Result::NeedMoreData;
  }

  if (validationResult == Message::ValidationFailed) {
    // Message does not conform to the expected format. Move to out-of-sync state.
    data.drain(data.length());
    state_ = State::OutOfSyncState;
    return Decoder::Result::ReadyForNext;
  }

  Decoder::Result result = Decoder::Result::ReadyForNext;
  uint32_t code = data.peekBEInt<uint32_t>(4);
  // Startup message with 1234 in the most significant 16 bits
  // indicate request to encrypt.
  if (code >= 0x04d20000) {
    encrypted_ = true;
    // Handler for SSLRequest (Int32(80877103) = 0x04d2162f)
    // See details in https://www.postgresql.org/docs/current/protocol-message-formats.html.
    if (code == 0x04d2162f) {
      // Notify the filter that `SSLRequest` message was decoded.
      // If the filter returns true, it means to pass the message upstream
      // to the server. If it returns false it means, that filter will try
      // to terminate SSL session and SSLRequest should not be passed to the
      // server.
      encrypted_ = callbacks_->onSSLRequest();
    }

    if (encrypted_) {
      ENVOY_LOG(trace, "postgres_proxy: detected encrypted traffic.");
      state_ = State::EncryptedState;
    } else {
      result = Decoder::Result::Stopped;
      // Stay in InitState. After switch to SSL, another init packet will be sent.
    }
  } else {
    ENVOY_LOG(debug, "Detected version {}.{} of Postgres", code >> 16, code & 0x0000FFFF);
    if (callbacks_->shouldEncryptUpstream()) {
      // Copy the received initial request.
      temp_storage_.add(data.linearize(data.length()), data.length());
      // Send SSL request to upstream.
      Buffer::OwnedImpl ssl_request;
      uint32_t len = 8;
      ssl_request.writeBEInt<uint32_t>(len);
      uint32_t ssl_code = 0x04d2162f;
      ssl_request.writeBEInt<uint32_t>(ssl_code);

      callbacks_->sendUpstream(ssl_request);
      result = Decoder::Result::Stopped;
      state_ = State::NegotiatingUpstreamSSL;
    } else {
      state_ = State::InSyncState;
    }
  }

  // Drain message length
  data.drain(4);

  Buffer::OwnedImpl message_data;
  message_data.move(data, message_len - 4);

  processMessageBody(message_data, FRONTEND, startup_msg_processor_);

  ENVOY_LOG(trace, "postgres_proxy: {} bytes remaining in buffer", data.length());
  return result;
}

/*
  Method invokes actions associated with message type and generate debug logs.
*/
void DecoderImpl::processMessageBody(Buffer::Instance& message_data, absl::string_view direction,
                                     MessageProcessor& processor) {
  replacement_message_->read(message_data, message_data.length());

  ENVOY_LOG(debug, "before processing:");
  ENVOY_LOG(debug, "({}) command = {} ({})", direction, command_, std::get<0>(processor));
  ENVOY_LOG(debug, "({}) length = {}", direction, message_data.length());
  ENVOY_LOG(debug, "({}) message = {}", direction, replacement_message_->toString());

  omit_ = false;
  std::vector<MsgAction>& actions = std::get<2>(processor);
  if (!actions.empty()) {
    // Invoke actions associated with the type of received message.
    for (const auto& action : actions) {
      action(this);
    }
  }

  if (omit_) {
    // Do not write message to the replacement data
    return;
  }

  uint64_t replace_data_old_length = replacement_data_.length();
  if (replacement_message_->isWriteable()) {
    // Write actual message
    replacement_message_->write(replacement_data_);
  } else {
    // Write old message data
    if (command_ != '-') {
      replacement_data_.writeByte(command_);
    }
    replacement_data_.writeBEInt<uint32_t>(message_data.length() + 4);
    replacement_data_.move(message_data);
  }

  ENVOY_LOG(debug, "after processing:");
  ENVOY_LOG(debug, "({}) command = {} ({})", direction, command_, std::get<0>(processor));
  ENVOY_LOG(debug, "({}) length = {}", direction,
            replacement_data_.length() - replace_data_old_length - 1);
  ENVOY_LOG(debug, "({}) message = {}", direction, replacement_message_->toString());
}

/*
  onDataInSync is called when decoder is on-track with decoding messages.
  All previous messages has been decoded properly and decoder is able to find
  message boundaries.
*/
Decoder::Result DecoderImpl::onDataInSync(Buffer::Instance& data, bool frontend) {
  ENVOY_LOG(trace, "postgres_proxy: decoding {} bytes", data.length());

  ENVOY_LOG(trace, "postgres_proxy: parsing message, len {}", data.length());

  // The minimum size of the message sufficient for parsing is 5 bytes.
  if (data.length() < 5) {
    // not enough data in the buffer.
    return Decoder::Result::NeedMoreData;
  }

  data.copyOut(0, 1, &command_);
  ENVOY_LOG(trace, "postgres_proxy: command is {}", command_);

  // The 1 byte message type and message length should be in the buffer
  // Find the message processor and validate the message syntax.

  MsgGroup& msg_processor_group = std::ref(frontend ? FE_messages_ : BE_messages_);

  // Set processing to the handler of unknown messages.
  // If message is found, the processing will be updated.
  std::reference_wrapper<MessageProcessor> msg_processor = msg_processor_group.unknown_;

  auto it = msg_processor_group.messages_.find(command_);
  if (it != msg_processor_group.messages_.end()) {
    msg_processor = std::ref((*it).second);
  }

  // Validate the message before processing.
  const MsgBodyReader& message_factory = std::get<1>(msg_processor.get());
  uint32_t message_len = data.peekBEInt<uint32_t>(1);

  replacement_message_ = message_factory();
  // Run the validation.
  // Because the message validation may return NeedMoreData error, data must stay intact (no
  // draining) until the remaining data arrives and validator will run again. Validator therefore
  // starts at offset 5 (1 byte message type and 4 bytes of length). This is in contrast to
  // processing of the message, which assumes that message has been validated and starts at the
  // beginning of the message.
  Message::ValidationResult validationResult =
      replacement_message_->validate(data, 5, message_len - 4);

  if (validationResult == Message::ValidationNeedMoreData) {
    ENVOY_LOG(trace, "postgres_proxy: cannot parse message. Not enough bytes in the buffer.");
    return Decoder::Result::NeedMoreData;
  }

  if (validationResult == Message::ValidationFailed) {
    // Message does not conform to the expected format. Move to out-of-sync state.
    data.drain(data.length());
    ENVOY_LOG(error, "postgres_proxy: out of sync");
    state_ = State::OutOfSyncState;
    return Decoder::Result::ReadyForNext;
  }

  // Drain message type and length
  data.drain(5);

  Buffer::OwnedImpl message_data;
  message_data.move(data, message_len - 4);

  processMessageBody(message_data, msg_processor_group.direction_, msg_processor);

  ENVOY_LOG(trace, "postgres_proxy: {} bytes remaining in buffer", data.length());
  return Decoder::Result::ReadyForNext;
}

/*
  onDataIgnore method is called when the decoder does not inspect passing
  messages. This happens when the decoder detected encrypted packets or
  when the decoder could not validate passing messages and lost track of
  messages boundaries. In order not to interpret received values as message
  lengths and not to start buffering large amount of data, the decoder
  enters OutOfSync state and starts ignoring passing messages. Once the
  decoder enters OutOfSyncState it cannot leave that state.
*/
Decoder::Result DecoderImpl::onDataIgnore(Buffer::Instance& data, bool) {
  data.drain(data.length());
  return Decoder::Result::ReadyForNext;
}

Decoder::Result DecoderImpl::onDataInNegotiating(Buffer::Instance& data, bool frontend) {
  if (frontend) {
    // No data from downstream is allowed when negotiating upstream SSL
    // with the server.
    data.drain(data.length());
    state_ = State::OutOfSyncState;
    return Decoder::Result::ReadyForNext;
  }

  // This should be reply from the server indicating if it accepted
  // request to use SSL. It is only one character long packet, where
  // 'S' means use SSL, 'E' means do not use.

  // Indicate to the filter, the response and give the initial
  // packet temporarily buffered to be sent upstream.
  bool upstreamSSL = false;
  state_ = State::InitState;
  if (data.length() == 1) {
    const char c = data.peekInt<char, ByteOrder::Host, 1>(0);
    if (c == 'S') {
      upstreamSSL = true;
    } else {
      if (c != 'E') {
        state_ = State::OutOfSyncState;
      }
    }
  } else {
    state_ = State::OutOfSyncState;
  }

  data.drain(data.length());

  if (callbacks_->encryptUpstream(upstreamSSL, temp_storage_)) {
    state_ = State::InSyncState;
  }

  return Decoder::Result::Stopped;
}

// Method parses Parse message of the following format:
// String: The name of the destination prepared statement (an empty string selects the unnamed
// prepared statement).
//
// String: The query string to be parsed.
//
// Int16: The number of parameter data
// types specified (can be zero). Note that this is not an indication of the number of parameters
// that might appear in the query string, only the number that the frontend wants to pre-specify
// types for. Then, for each parameter, there is the following:
//
// Int32: Specifies the object ID of
// the parameter data type. Placing a zero here is equivalent to leaving the type unspecified.
void DecoderImpl::onParse() {
  omit_ = !callbacks_->processParse(*dynamic_cast<ParseMessage *>(replacement_message_.get()));
}

void DecoderImpl::onQuery() {
  omit_ = !callbacks_->processQuery(*dynamic_cast<QueryMessage*>(replacement_message_.get()));
}

void DecoderImpl::onRowDescription() {
  callbacks_->processRowDescription(*dynamic_cast<RowDescriptionMessage*>(replacement_message_.get()));
}

void DecoderImpl::onDataRow() {
  callbacks_->processDataRow(*dynamic_cast<DataRowMessage *>(replacement_message_.get()));
}

void DecoderImpl::onCommandComplete() {
  callbacks_->processCommandComplete(*dynamic_cast<CommandCompleteMessage *>(replacement_message_.get()));
}

void DecoderImpl::onEmptyQueryResponse() {
  callbacks_->processEmptyQueryResponse();
}

void DecoderImpl::onErrorResponse() {
  callbacks_->processErrorResponse(*dynamic_cast<ErrorResponseMessage *>(replacement_message_.get()));
}

} // namespace PostgresTDE
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
