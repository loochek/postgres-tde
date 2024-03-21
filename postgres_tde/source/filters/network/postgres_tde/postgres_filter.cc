#include "postgres_tde/source/filters/network/postgres_tde/postgres_filter.h"

#include "envoy/buffer/buffer.h"
#include "envoy/network/connection.h"

#include "source/common/common/assert.h"

#include "postgres_tde/source/filters/network/postgres_tde/common.h"
#include "postgres_tde/source/filters/network/postgres_tde/postgres_decoder.h"

#include "absl/strings/escaping.h"

#include <cstring>

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace PostgresTDE {

PostgresFilterConfig::PostgresFilterConfig(const PostgresFilterConfigOptions& config_options,
                                           Stats::Scope& scope)
    : enable_sql_parsing_(config_options.enable_sql_parsing_),
      terminate_ssl_(config_options.terminate_ssl_), upstream_ssl_(config_options.upstream_ssl_),
      scope_{scope}, stats_{generateStats(config_options.stats_prefix_, scope)} {}

PostgresFilter::PostgresFilter(PostgresFilterConfigSharedPtr config) : config_{config} {
  if (!decoder_) {
    decoder_ = createDecoder(this);
  }
  if (!mutation_manager_) {
    mutation_manager_ = createMutationManager();
  }
}

// Network::ReadFilter
Network::FilterStatus PostgresFilter::onData(Buffer::Instance& data, bool) {
  ENVOY_CONN_LOG(trace, "postgres_proxy: got {} bytes", read_callbacks_->connection(),
                 data.length());

//  auto kek = data.linearize(data.length());
//  ENVOY_LOG(error, "orig data: {}", absl::BytesToHexString(absl::string_view(reinterpret_cast<const char*>(kek), data.length())));
  frontend_validation_buffer_.add(data);
  data.drain(data.length());

  Decoder::Result result = doDecode(frontend_validation_buffer_, true);
  Buffer::Instance& replacement_data = decoder_->getReplacementData();
  switch (result) {
  case Decoder::Result::NeedMoreData:
  case Decoder::Result::ReadyForNext:
    if (replacement_data.length() > 0) {
      // Pass mutated data to the rest of the filter chain and continue
      data.move(replacement_data, replacement_data.length());
//      auto kek2 = data.linearize(data.length());
//      ENVOY_LOG(error, "mted data: {}", absl::BytesToHexString(absl::string_view(reinterpret_cast<const char*>(kek2), data.length())));
      read_callbacks_->continueReading();
    }
    return Network::FilterStatus::StopIteration;

  case Decoder::Result::Stopped:
    ASSERT(frontend_validation_buffer_.length() == 0);
    replacement_data.drain(replacement_data.length());
    return Network::FilterStatus::StopIteration;
  }
}

Network::FilterStatus PostgresFilter::onNewConnection() { return Network::FilterStatus::Continue; }

void PostgresFilter::initializeReadFilterCallbacks(Network::ReadFilterCallbacks& callbacks) {
  read_callbacks_ = &callbacks;
}

void PostgresFilter::initializeWriteFilterCallbacks(Network::WriteFilterCallbacks& callbacks) {
  write_callbacks_ = &callbacks;
}

// Network::WriteFilter
Network::FilterStatus PostgresFilter::onWrite(Buffer::Instance& data, bool end_stream) {
//  auto kek = data.linearize(data.length());
//  ENVOY_LOG(error, "orig data: {}", absl::BytesToHexString(absl::string_view(reinterpret_cast<const char*>(kek), data.length())));
  backend_validation_buffer_.add(data);
  data.drain(data.length());

  Decoder::Result result = doDecode(backend_validation_buffer_, false);
  Buffer::Instance& replacement_data = decoder_->getReplacementData();
  switch (result) {
  case Decoder::Result::NeedMoreData:
  case Decoder::Result::ReadyForNext:
    if (replacement_data.length() > 0) {
      // Pass mutated data to the rest of the filter chain and continue
      data.move(replacement_data, replacement_data.length());
//      auto kek2 = data.linearize(data.length());
//      ENVOY_LOG(error, "mted data: {}", absl::BytesToHexString(absl::string_view(reinterpret_cast<const char*>(kek2), data.length())));
      write_callbacks_->injectWriteDataToFilterChain(data, end_stream);
    }
    return Network::FilterStatus::StopIteration;

  case Decoder::Result::Stopped:
    ASSERT(backend_validation_buffer_.length() == 0);
    replacement_data.drain(replacement_data.length());
    return Network::FilterStatus::StopIteration;
  }
}

DecoderPtr PostgresFilter::createDecoder(DecoderCallbacks* callbacks) {
  return std::make_unique<DecoderImpl>(callbacks);
}

MutationManagerPtr PostgresFilter::createMutationManager() {
  return std::make_unique<MutationManagerImpl>();
}

bool PostgresFilter::processQuery(QueryMessage& message) {
  Result result = mutation_manager_->processQuery(message);
  if (result.isOk) {
    return true;
  } else {
    Buffer::OwnedImpl injectBuffer;
    createErrorResponseMessage(result.error).write(injectBuffer);
    READY_FOR_QUERY_MESSAGE.write(injectBuffer);
    write_callbacks_->injectWriteDataToFilterChain(injectBuffer, false);
    return false;
  }
}

bool PostgresFilter::processParse(ParseMessage&) {
  Buffer::OwnedImpl injectBuffer;
  createErrorResponseMessage("postgres_tde: prepared statements are not supported").write(injectBuffer);
  write_callbacks_->injectWriteDataToFilterChain(injectBuffer, false);
  return false;
}

void PostgresFilter::processRowDescription(RowDescriptionMessage & replace_message) {
  mutation_manager_->processRowDescription(replace_message);
}

void PostgresFilter::processDataRow(DataRowMessage& replace_message) {
  mutation_manager_->processDataRow(replace_message);
}

void PostgresFilter::processCommandComplete(CommandCompleteMessage& replace_message) {
  mutation_manager_->processCommandComplete(replace_message);
}

void PostgresFilter::processEmptyQueryResponse() { mutation_manager_->processEmptyQueryResponse(); }

void PostgresFilter::processErrorResponse(ErrorResponseMessage& replace_message) {
  mutation_manager_->processErrorResponse(replace_message);
}

bool PostgresFilter::onSSLRequest() {
  if (!config_->terminate_ssl_) {
    // Signal to the decoder to continue.
    return true;
  }
  // Send single bytes 'S' to indicate switch to TLS.
  // Refer to official documentation for protocol details:
  // https://www.postgresql.org/docs/current/protocol-flow.html
  Buffer::OwnedImpl buf;
  buf.add("S");
  // Add callback to be notified when the reply message has been
  // transmitted.
  read_callbacks_->connection().addBytesSentCallback([=](uint64_t bytes) -> bool {
    // Wait until 'S' has been transmitted.
    if (bytes >= 1) {
      if (!read_callbacks_->connection().startSecureTransport()) {
        ENVOY_CONN_LOG(
            info, "postgres_proxy: cannot enable downstream secure transport. Check configuration.",
            read_callbacks_->connection());
        read_callbacks_->connection().close(Network::ConnectionCloseType::NoFlush);
      } else {
        // Unsubscribe the callback.
        config_->stats_.sessions_terminated_ssl_.inc();
        ENVOY_CONN_LOG(trace, "postgres_proxy: enabled SSL termination.",
                       read_callbacks_->connection());
        // Switch to TLS has been completed.
        // Signal to the decoder to stop processing the current message (SSLRequest).
        // Because Envoy terminates SSL, the message was consumed and should not be
        // passed to other filters in the chain.
        return false;
      }
    }
    return true;
  });
  write_callbacks_->injectWriteDataToFilterChain(buf, false);

  return false;
}

bool PostgresFilter::shouldEncryptUpstream() const {
  return (config_->upstream_ssl_ ==
          envoy::extensions::filters::network::postgres_tde::PostgresTDE::REQUIRE);
}

void PostgresFilter::sendUpstream(Buffer::Instance& data) {
  read_callbacks_->injectReadDataToFilterChain(data, false);
}

bool PostgresFilter::encryptUpstream(bool upstream_agreed, Buffer::Instance& data) {
  bool encrypted = false;
  RELEASE_ASSERT(config_->upstream_ssl_ !=
                     envoy::extensions::filters::network::postgres_tde::PostgresTDE::DISABLE,
                 "encryptUpstream should not be called when upstream SSL is disabled.");
  if (!upstream_agreed) {
    ENVOY_CONN_LOG(info,
                   "postgres_proxy: upstream server rejected request to establish SSL connection. "
                   "Terminating.",
                   read_callbacks_->connection());
    read_callbacks_->connection().close(Network::ConnectionCloseType::NoFlush);

    config_->stats_.sessions_upstream_ssl_failed_.inc();
  } else {
    // Try to switch upstream connection to use a secure channel.
    if (read_callbacks_->startUpstreamSecureTransport()) {
      config_->stats_.sessions_upstream_ssl_success_.inc();
      read_callbacks_->injectReadDataToFilterChain(data, false);
      encrypted = true;
      ENVOY_CONN_LOG(trace, "postgres_proxy: upstream SSL enabled.", read_callbacks_->connection());
    } else {
      ENVOY_CONN_LOG(info,
                     "postgres_proxy: cannot enable upstream secure transport. Check "
                     "configuration. Terminating.",
                     read_callbacks_->connection());
      read_callbacks_->connection().close(Network::ConnectionCloseType::NoFlush);
      config_->stats_.sessions_upstream_ssl_failed_.inc();
    }
  }

  return encrypted;
}

Decoder::Result PostgresFilter::doDecode(Buffer::Instance& parse_data,
                                         bool frontend) {
  // Keep processing data until buffer is empty or decoder says
  // that it cannot process data in the buffer.
  while (0 < parse_data.length()) {
    switch (decoder_->onData(parse_data, frontend)) {
    case Decoder::Result::ReadyForNext:
      continue;
    case Decoder::Result::NeedMoreData:
      return Decoder::Result::NeedMoreData;
    case Decoder::Result::Stopped:
      return Decoder::Result::Stopped;
    }
  }
  return Decoder::Result::ReadyForNext;
}

} // namespace PostgresTDE
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
