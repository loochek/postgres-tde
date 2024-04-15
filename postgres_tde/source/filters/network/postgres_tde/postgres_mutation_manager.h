#pragma once
#include <cstdint>

#include "envoy/common/platform.h"

#include "source/common/buffer/buffer_impl.h"
#include "source/common/common/logger.h"

#include "postgres_tde/source/filters/network/postgres_tde/config/dummy_config.h"
#include "postgres_tde/source/filters/network/postgres_tde/mutators/mutator.h"
#include "postgres_tde/source/filters/network/postgres_tde/postgres_protocol.h"
#include "postgres_tde/source/common/sqlutils/ast/dump_visitor.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace PostgresTDE {

class PostgresFilterConfig;
using PostgresFilterConfigSharedPtr = std::shared_ptr<PostgresFilterConfig>;

class MutationManagerCallbacks {
public:
  virtual ~MutationManagerCallbacks() = default;

  virtual void emitBackendMessage(MessagePtr) PURE;
};

/**
 * Query/result mutation manager
 *
 * Messages are passed as std::unique_ptr& so they can be just modified
 * as well as consumed in order to handle messages in an arbitrary way
 * (e.g. write them back to the data stream manually using emitBackendMessage/emitFrontendMessage
 * sometime in the future)
 */
class MutationManager {
public:
  virtual ~MutationManager() = default;

  // Frontend messages

  virtual void processQuery(std::unique_ptr<QueryMessage>&) PURE;
  virtual void processParse(std::unique_ptr<ParseMessage>&) PURE;

  // Backend messages

  virtual void processRowDescription(std::unique_ptr<RowDescriptionMessage>&) PURE;
  virtual void processDataRow(std::unique_ptr<DataRowMessage>&) PURE;

  virtual void processCommandComplete(std::unique_ptr<CommandCompleteMessage>&) PURE;
  virtual void processEmptyQueryResponse(std::unique_ptr<EmptyQueryResponseMessage>&) PURE;
  virtual void processErrorResponse(std::unique_ptr<ErrorResponseMessage>&) PURE;

  virtual const PostgresFilterConfig* getConfig() const PURE;
  virtual const DatabaseEncryptionConfig* getEncryptionConfig() const PURE;
};

using MutationManagerPtr = std::unique_ptr<MutationManager>;

class MutationManagerImpl : public MutationManager, Logger::Loggable<Logger::Id::filter> {
public:
  MutationManagerImpl(PostgresFilterConfigSharedPtr config, MutationManagerCallbacks* callbacks);

  void processQuery(std::unique_ptr<QueryMessage>& message) override;
  void processParse(std::unique_ptr<ParseMessage>& message) override;

  void processRowDescription(std::unique_ptr<RowDescriptionMessage>& message) override;
  void processDataRow(std::unique_ptr<DataRowMessage>& message) override;

  void processCommandComplete(std::unique_ptr<CommandCompleteMessage>& cc_message) override;
  void processEmptyQueryResponse(std::unique_ptr<EmptyQueryResponseMessage>& message) override;
  void processErrorResponse(std::unique_ptr<ErrorResponseMessage>& message) override;

  const PostgresFilterConfig* getConfig() const override {
    return config_.get();
  }

  const DatabaseEncryptionConfig* getEncryptionConfig() const override {
    return &encryption_config_;
  }

protected:
  Result processQueryImpl(QueryMessage&);
  void emitErrorResponse(const Result& result);

protected:
  std::vector<MutatorPtr> mutator_chain_;
  std::unique_ptr<Envoy::Extensions::Common::SQLUtils::DumpVisitor> dumper_;

  Result error_state_;

  std::unique_ptr<RowDescriptionMessage> retent_row_description_;
  std::vector<std::unique_ptr<DataRowMessage>> retent_rows_;

  PostgresFilterConfigSharedPtr config_;
  DummyConfig encryption_config_;
  MutationManagerCallbacks* callbacks_;
};

} // namespace PostgresTDE
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
