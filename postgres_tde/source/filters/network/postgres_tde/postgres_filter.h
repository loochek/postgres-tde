#pragma once

#include "envoy/network/filter.h"
#include "envoy/stats/scope.h"
#include "envoy/stats/stats.h"
#include "envoy/stats/stats_macros.h"

#include "source/common/buffer/buffer_impl.h"
#include "source/common/common/logger.h"

#include "postgres_tde/api/filters/network/postgres_tde/postgres_tde.pb.h"
#include "postgres_tde/source/filters/network/postgres_tde/postgres_decoder.h"
#include "postgres_tde/source/filters/network/postgres_tde/postgres_mutation_manager.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace PostgresTDE {

/**
 * All Postgres proxy stats. @see stats_macros.h
 */
#define ALL_POSTGRES_PROXY_STATS(COUNTER)                                                          \
  COUNTER(errors)                                                                                  \
  COUNTER(errors_error)                                                                            \
  COUNTER(errors_fatal)                                                                            \
  COUNTER(errors_panic)                                                                            \
  COUNTER(errors_unknown)                                                                          \
  COUNTER(messages)                                                                                \
  COUNTER(messages_backend)                                                                        \
  COUNTER(messages_frontend)                                                                       \
  COUNTER(messages_unknown)                                                                        \
  COUNTER(sessions)                                                                                \
  COUNTER(sessions_encrypted)                                                                      \
  COUNTER(sessions_terminated_ssl)                                                                 \
  COUNTER(sessions_unencrypted)                                                                    \
  COUNTER(sessions_upstream_ssl_success)                                                           \
  COUNTER(sessions_upstream_ssl_failed)                                                            \
  COUNTER(statements)                                                                              \
  COUNTER(statements_insert)                                                                       \
  COUNTER(statements_delete)                                                                       \
  COUNTER(statements_update)                                                                       \
  COUNTER(statements_select)                                                                       \
  COUNTER(statements_other)                                                                        \
  COUNTER(transactions)                                                                            \
  COUNTER(transactions_commit)                                                                     \
  COUNTER(transactions_rollback)                                                                   \
  COUNTER(statements_parsed)                                                                       \
  COUNTER(statements_parse_error)                                                                  \
  COUNTER(notices)                                                                                 \
  COUNTER(notices_notice)                                                                          \
  COUNTER(notices_warning)                                                                         \
  COUNTER(notices_debug)                                                                           \
  COUNTER(notices_info)                                                                            \
  COUNTER(notices_log)                                                                             \
  COUNTER(notices_unknown)

/**
 * Struct definition for all Postgres proxy stats. @see stats_macros.h
 */
struct PostgresProxyStats {
  ALL_POSTGRES_PROXY_STATS(GENERATE_COUNTER_STRUCT)
};

/**
 * Configuration for the Postgres proxy filter.
 */
class PostgresFilterConfig {
public:
  struct PostgresFilterConfigOptions {
    std::string stats_prefix_;
    bool enable_sql_parsing_;
    bool terminate_ssl_;
    envoy::extensions::filters::network::postgres_tde::PostgresTDE::SSLMode
        upstream_ssl_;
  };
  PostgresFilterConfig(const PostgresFilterConfigOptions& config_options, Stats::Scope& scope);

  bool enable_sql_parsing_{true};
  bool terminate_ssl_{false};
  envoy::extensions::filters::network::postgres_tde::PostgresTDE::SSLMode
      upstream_ssl_{
          envoy::extensions::filters::network::postgres_tde::PostgresTDE::DISABLE};
  Stats::Scope& scope_;
  PostgresProxyStats stats_;

private:
  PostgresProxyStats generateStats(const std::string& prefix, Stats::Scope& scope) {
    return PostgresProxyStats{ALL_POSTGRES_PROXY_STATS(POOL_COUNTER_PREFIX(scope, prefix))};
  }
};

using PostgresFilterConfigSharedPtr = std::shared_ptr<PostgresFilterConfig>;

class PostgresFilter : public Network::Filter,
                       DecoderCallbacks,
                       Logger::Loggable<Logger::Id::filter> {
public:
  PostgresFilter(PostgresFilterConfigSharedPtr config);
  ~PostgresFilter() override = default;

  // Network::ReadFilter
  Network::FilterStatus onData(Buffer::Instance& data, bool end_stream) override;
  Network::FilterStatus onNewConnection() override;
  void initializeReadFilterCallbacks(Network::ReadFilterCallbacks& callbacks) override;
  void initializeWriteFilterCallbacks(Network::WriteFilterCallbacks& callbacks) override;

  // Network::WriteFilter
  Network::FilterStatus onWrite(Buffer::Instance& data, bool end_stream) override;

  // PostgresProxy::DecoderCallback
  void incErrors(ErrorType) override;
  void incMessagesBackend() override;
  void incMessagesFrontend() override;
  void incMessagesUnknown() override;
  void incNotices(NoticeType) override;
  void incSessionsEncrypted() override;
  void incSessionsUnencrypted() override;
  void incStatements(StatementType) override;
  void incTransactions() override;
  void incTransactionsCommit() override;
  void incTransactionsRollback() override;
  bool processQuery(Buffer::Instance&) override;
  bool processParse(Buffer::Instance&) override;
  void processRowDescription(Buffer::Instance&) override;
  void processDataRow(Buffer::Instance&) override;
  void processCommandComplete(Buffer::Instance&) override;
  void processEmptyQueryResponse() override;
  void processErrorResponse(Buffer::Instance&) override;
  bool onSSLRequest() override;
  bool shouldEncryptUpstream() const override;
  void sendUpstream(Buffer::Instance&) override;
  bool encryptUpstream(bool, Buffer::Instance&) override;

  Decoder::Result doDecode(Buffer::Instance& data, Buffer::Instance& orig_data, bool);
  DecoderPtr createDecoder(DecoderCallbacks* callbacks);
  MutationManagerPtr createMutationManager();
  void setDecoder(std::unique_ptr<Decoder> decoder) { decoder_ = std::move(decoder); }
  Decoder* getDecoder() const { return decoder_.get(); }

  // Routines used during integration and unit tests
  uint32_t getFrontendBufLength() const { return frontend_validation_buffer_.length(); }
  uint32_t getBackendBufLength() const { return backend_validation_buffer_.length(); }
  const PostgresProxyStats& getStats() const { return config_->stats_; }
  Network::Connection& connection() const { return read_callbacks_->connection(); }
  const PostgresFilterConfigSharedPtr& getConfig() const { return config_; }

private:
  Network::ReadFilterCallbacks* read_callbacks_{};
  Network::WriteFilterCallbacks* write_callbacks_{};
  PostgresFilterConfigSharedPtr config_;

  Buffer::OwnedImpl frontend_validation_buffer_;
  Buffer::OwnedImpl frontend_mutation_buffer_;

  Buffer::OwnedImpl backend_validation_buffer_;
  Buffer::OwnedImpl backend_mutation_buffer_;

  std::unique_ptr<Decoder> decoder_;
  std::unique_ptr<MutationManager> mutation_manager_;
};

} // namespace PostgresTDE
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
