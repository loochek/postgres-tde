#pragma once
#include <cstdint>

#include "envoy/common/platform.h"

#include "source/common/buffer/buffer_impl.h"
#include "source/common/common/logger.h"

#include "absl/container/flat_hash_map.h"
#include "postgres_tde/source/filters/network/postgres_tde/postgres_message.h"
#include "postgres_tde/source/filters/network/postgres_tde/postgres_session.h"
#include "postgres_tde/source/filters/network/postgres_tde/postgres_protocol.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace PostgresTDE {

// General callbacks for dispatching decoded Postgres messages to a sink.
class DecoderCallbacks {
public:
  virtual ~DecoderCallbacks() = default;

  virtual bool processQuery(QueryMessage& message) PURE;
  virtual bool processParse(ParseMessage& message) PURE;

  virtual void processRowDescription(RowDescriptionMessage& message) PURE;
  virtual void processDataRow(DataRowMessage& message) PURE;

  virtual void processCommandComplete(CommandCompleteMessage& message) PURE;
  virtual void processEmptyQueryResponse() PURE;
  virtual void processErrorResponse(ErrorResponseMessage&) PURE;

  virtual bool onSSLRequest() PURE;
  virtual bool shouldEncryptUpstream() const PURE;
  virtual void sendUpstream(Buffer::Instance&) PURE;
  virtual bool encryptUpstream(bool, Buffer::Instance&) PURE;
};

class MutationManager;

// Postgres message decoder.
class Decoder {
public:
  virtual ~Decoder() = default;

  // The following values are returned by the decoder, when filter
  // passes bytes of data via onData method:
  enum class Result {
    ReadyForNext, // Decoder processed previous message and is ready for the next message.
    NeedMoreData, // Decoder needs more data to reconstruct the message.
    Stopped // Received and processed message disrupts the current flow. Decoder stopped accepting
            // data. This happens when decoder wants filter to perform some action, for example to
            // call starttls transport socket to enable TLS.
  };
  virtual Result onData(Buffer::Instance& parse_data, bool frontend) PURE;
  virtual Buffer::Instance& getReplacementData() PURE;
  virtual PostgresSession& getSession() PURE;
};

using DecoderPtr = std::unique_ptr<Decoder>;

class DecoderImpl : public Decoder, Logger::Loggable<Logger::Id::filter> {
public:
  DecoderImpl(DecoderCallbacks* callbacks) : callbacks_(callbacks) { initialize(); }

  Result onData(Buffer::Instance& parse_data, bool frontend) override;
  PostgresSession& getSession() override { return session_; }
  Buffer::Instance& getReplacementData() override { return replacement_data_; }

  void initialize();

  bool encrypted() const { return encrypted_; }

  enum class State {
    InitState,
    InSyncState,
    OutOfSyncState,
    EncryptedState,
    NegotiatingUpstreamSSL
  };
  State state() const { return state_; }
  void state(State state) { state_ = state; }

protected:
  State state_{State::InitState};

  Result onDataInit(Buffer::Instance& data, bool frontend);
  Result onDataInSync(Buffer::Instance& data, bool frontend);
  Result onDataIgnore(Buffer::Instance& data, bool frontend);
  Result onDataInNegotiating(Buffer::Instance& data, bool frontend);

  // MsgAction defines the Decoder's method which will be invoked
  // when a specific message has been decoded.
  using MsgAction = std::function<void(DecoderImpl*)>;

  // MsgBodyReader is a function which returns a pointer to a Message
  // class which is able to read the Postgres message body.
  // The Postgres message body structure depends on the message type.
  using MsgBodyReader = std::function<std::unique_ptr<Message>()>;

  // MessageProcessor has the following fields:
  // first - string with message description
  // second - function which instantiates a Message object of specific type
  // which is capable of parsing the message's body.
  // third - vector of Decoder's methods which are invoked when the message
  // is processed.
  using MessageProcessor = std::tuple<std::string, MsgBodyReader, std::vector<MsgAction>>;

  // Frontend and Backend messages.
  using MsgGroup = struct {
    // String describing direction (Frontend or Backend).
    absl::string_view direction_;
    // Hash map indexed by messages' 1st byte points to handlers used for processing messages.
    absl::flat_hash_map<char, MessageProcessor> messages_;
    // Handler used for processing messages not found in hash map.
    MessageProcessor unknown_;
  };

  // Hash map binding keyword found in a message to an
  // action to be executed when the keyword is found.
  using KeywordProcessor = absl::flat_hash_map<std::string, MsgAction>;

  // Structure is used for grouping keywords found in a specific message.
  // Known keywords are dispatched via hash map and unknown keywords
  // are handled by unknown_.
  using MsgParserDict = struct {
    // Handler for known keywords.
    KeywordProcessor keywords_;
    // Handler invoked when a keyword is not found in hash map.
    MsgAction unknown_;
  };

  void processMessageBody(Buffer::Instance& message_data, absl::string_view direction,
                          MessageProcessor& processor);
  void onQuery();
  void onRowDescription();
  void onDataRow();
  void onCommandComplete();
  void onEmptyQueryResponse();
  void onErrorResponse();
  void onParse();

  DecoderCallbacks* callbacks_{};
  PostgresSession session_{};

  // The following fields store result of message parsing.
  char command_{'-'};
  Buffer::OwnedImpl replacement_data_;
  std::unique_ptr<Message> replacement_message_;
  bool omit_{false};

  bool encrypted_{false}; // tells if exchange is encrypted

  // Dispatchers for Backend (BE) and Frontend (FE) messages.
  MsgGroup FE_messages_;
  MsgGroup BE_messages_;

  // Handler for startup postgres message.
  // Startup message message which does not start with 1 byte TYPE.
  // It starts with message length and must be therefore handled
  // differently.
  MessageProcessor startup_msg_processor_;

  // hash map for dispatching backend transaction messages
  KeywordProcessor BE_statements_;

  MsgParserDict BE_errors_;
  MsgParserDict BE_notices_;

  // Buffer used to temporarily store a downstream postgres packet
  // while sending other packets. Currently used only when negotiating
  // upstream SSL.
  Buffer::OwnedImpl temp_storage_;

  // MAX_STARTUP_PACKET_LENGTH is defined in Postgres source code
  // as maximum size of initial packet.
  // https://github.com/postgres/postgres/search?q=MAX_STARTUP_PACKET_LENGTH&type=code
  static constexpr uint64_t MAX_STARTUP_PACKET_LENGTH = 10000;
};

} // namespace PostgresTDE
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
