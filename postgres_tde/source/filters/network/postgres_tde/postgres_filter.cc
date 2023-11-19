#include "postgres_tde/source/filters/network/postgres_tde/postgres_filter.h"

#include "envoy/buffer/buffer.h"
#include "envoy/network/connection.h"

#include "source/common/common/assert.h"

#include "postgres_tde/source/filters/network/postgres_tde/common.h"
#include "postgres_tde/source/filters/network/postgres_tde/postgres_decoder.h"

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
}

// Network::ReadFilter
Network::FilterStatus PostgresFilter::onData(Buffer::Instance& data, bool) {
  ENVOY_CONN_LOG(trace, "postgres_proxy: got {} bytes", read_callbacks_->connection(),
                 data.length());

  frontend_validation_buffer_.add(data);
  frontend_mutation_buffer_.add(data);
  data.drain(data.length());

  Decoder::Result result = doDecode(frontend_validation_buffer_, frontend_mutation_buffer_, true);
  uint32_t mutated_data_size = frontend_mutation_buffer_.length() - frontend_validation_buffer_.length();

  switch (result) {
  case Decoder::Result::NeedMoreData:
  case Decoder::Result::ReadyForNext:
    // Pass mutated data to the rest of the filter chain and continue
    data.move(frontend_mutation_buffer_, mutated_data_size);
    read_callbacks_->continueReading();
    return Network::FilterStatus::StopIteration;

  case Decoder::Result::Stopped:
    ASSERT(frontend_validation_buffer_.length() == 0);
    frontend_mutation_buffer_.drain(frontend_mutation_buffer_.length());
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
  backend_validation_buffer_.add(data);
  backend_mutation_buffer_.add(data);
  data.drain(data.length());

  Decoder::Result result = doDecode(backend_validation_buffer_, backend_mutation_buffer_, false);
  uint32_t mutated_data_size = backend_mutation_buffer_.length() - backend_validation_buffer_.length();

  switch (result) {
  case Decoder::Result::NeedMoreData:
  case Decoder::Result::ReadyForNext:
    // Pass mutated data to the rest of the filter chain and continue
    data.move(backend_mutation_buffer_, mutated_data_size);
    write_callbacks_->injectWriteDataToFilterChain(data, end_stream);
    return Network::FilterStatus::StopIteration;

  case Decoder::Result::Stopped:
    ASSERT(frontend_validation_buffer_.length() == 0);
    backend_mutation_buffer_.drain(backend_mutation_buffer_.length());
    return Network::FilterStatus::StopIteration;
  }
}

DecoderPtr PostgresFilter::createDecoder(DecoderCallbacks* callbacks) {
  return std::make_unique<DecoderImpl>(callbacks);
}

void PostgresFilter::incMessagesBackend() {
  config_->stats_.messages_.inc();
  config_->stats_.messages_backend_.inc();
}

void PostgresFilter::incMessagesFrontend() {
  config_->stats_.messages_.inc();
  config_->stats_.messages_frontend_.inc();
}

void PostgresFilter::incMessagesUnknown() {
  config_->stats_.messages_.inc();
  config_->stats_.messages_unknown_.inc();
}

void PostgresFilter::incSessionsEncrypted() {
  config_->stats_.sessions_.inc();
  config_->stats_.sessions_encrypted_.inc();
}

void PostgresFilter::incSessionsUnencrypted() {
  config_->stats_.sessions_.inc();
  config_->stats_.sessions_unencrypted_.inc();
}

void PostgresFilter::incTransactions() {
  if (!decoder_->getSession().inTransaction()) {
    config_->stats_.transactions_.inc();
  }
}

void PostgresFilter::incTransactionsCommit() {
  if (!decoder_->getSession().inTransaction()) {
    config_->stats_.transactions_commit_.inc();
  }
}

void PostgresFilter::incTransactionsRollback() {
  if (decoder_->getSession().inTransaction()) {
    config_->stats_.transactions_rollback_.inc();
  }
}

void PostgresFilter::incNotices(NoticeType type) {
  config_->stats_.notices_.inc();
  switch (type) {
  case DecoderCallbacks::NoticeType::Warning:
    config_->stats_.notices_warning_.inc();
    break;
  case DecoderCallbacks::NoticeType::Notice:
    config_->stats_.notices_notice_.inc();
    break;
  case DecoderCallbacks::NoticeType::Debug:
    config_->stats_.notices_debug_.inc();
    break;
  case DecoderCallbacks::NoticeType::Info:
    config_->stats_.notices_info_.inc();
    break;
  case DecoderCallbacks::NoticeType::Log:
    config_->stats_.notices_log_.inc();
    break;
  case DecoderCallbacks::NoticeType::Unknown:
    config_->stats_.notices_unknown_.inc();
    break;
  }
}

void PostgresFilter::incErrors(ErrorType type) {
  config_->stats_.errors_.inc();
  switch (type) {
  case DecoderCallbacks::ErrorType::Error:
    config_->stats_.errors_error_.inc();
    break;
  case DecoderCallbacks::ErrorType::Fatal:
    config_->stats_.errors_fatal_.inc();
    break;
  case DecoderCallbacks::ErrorType::Panic:
    config_->stats_.errors_panic_.inc();
    break;
  case DecoderCallbacks::ErrorType::Unknown:
    config_->stats_.errors_unknown_.inc();
    break;
  }
}

void PostgresFilter::incStatements(StatementType type) {
  config_->stats_.statements_.inc();

  switch (type) {
  case DecoderCallbacks::StatementType::Insert:
    config_->stats_.statements_insert_.inc();
    break;
  case DecoderCallbacks::StatementType::Delete:
    config_->stats_.statements_delete_.inc();
    break;
  case DecoderCallbacks::StatementType::Select:
    config_->stats_.statements_select_.inc();
    break;
  case DecoderCallbacks::StatementType::Update:
    config_->stats_.statements_update_.inc();
    break;
  case DecoderCallbacks::StatementType::Other:
    config_->stats_.statements_other_.inc();
    break;
  case DecoderCallbacks::StatementType::Noop:
    break;
  }
}

void PostgresFilter::processQuery(Buffer::Instance& replace_message, const std::string& sql) {
  ENVOY_CONN_LOG(error, "postgres_proxy: {}", read_callbacks_->connection(), sql.c_str());

  const char* src = u8"Москва";
  const char* tgt = u8"Учалы";
  ssize_t pos = replace_message.search(src, std::strlen(src), 0);
  if (pos != -1) {
    Buffer::OwnedImpl new_orig_data;
    new_orig_data.move(replace_message, pos);
    new_orig_data.add(tgt, std::strlen(tgt));
    replace_message.drain(std::strlen(src));
    new_orig_data.move(replace_message);
    replace_message.move(new_orig_data);
  }

  if (config_->enable_sql_parsing_) {
    ProtobufWkt::Struct metadata;

    auto result = Common::SQLUtils::SQLUtils::setMetadata(sql, decoder_->getAttributes(), metadata);

    if (!result) {
      config_->stats_.statements_parse_error_.inc();
      ENVOY_CONN_LOG(trace, "postgres_proxy: cannot parse SQL: {}", read_callbacks_->connection(),
                     sql.c_str());
      return;
    }

    config_->stats_.statements_parsed_.inc();
    ENVOY_CONN_LOG(trace, "postgres_proxy: query processed {}", read_callbacks_->connection(),
                   sql.c_str());

    // Set dynamic metadata
    read_callbacks_->connection().streamInfo().setDynamicMetadata(POSTGRES_TDE_FILTER_NAME, metadata);
  }
}

void PostgresFilter::processDataRow(Buffer::Instance& replace_message) {
  const char* src = u8"Нижний Новгород";
  const char* tgt = u8"Новгород Нижний";
  ssize_t pos = replace_message.search(src, std::strlen(src), 0);
  if (pos != -1) {
    Buffer::OwnedImpl new_orig_data;
    new_orig_data.move(replace_message, pos);
    new_orig_data.add(tgt, std::strlen(tgt));
    replace_message.drain(std::strlen(src));
    new_orig_data.move(replace_message);
    replace_message.move(new_orig_data);
  }
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
  RELEASE_ASSERT(
      config_->upstream_ssl_ !=
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

Decoder::Result PostgresFilter::doDecode(Buffer::Instance& parse_data, Buffer::Instance& orig_data, bool frontend) {
  // Keep processing data until buffer is empty or decoder says
  // that it cannot process data in the buffer.
  bool first_invocation = true;
  while (0 < parse_data.length()) {
    bool is_first_invocation = first_invocation;
    first_invocation = false;

    switch (decoder_->onData(parse_data, orig_data, frontend, is_first_invocation)) {
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
