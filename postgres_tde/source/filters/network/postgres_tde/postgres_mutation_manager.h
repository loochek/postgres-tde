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

// Query/result mutation manager
class MutationManager {
public:
  virtual ~MutationManager() = default;

  virtual Result processQuery(QueryMessage&) PURE;

  virtual void processRowDescription(RowDescriptionMessage&) PURE;
  virtual void processDataRow(DataRowMessage &) PURE;

  virtual void processCommandComplete(CommandCompleteMessage &) PURE;
  virtual void processEmptyQueryResponse() PURE;
  virtual void processErrorResponse(ErrorResponseMessage&) PURE;
};

using MutationManagerPtr = std::unique_ptr<MutationManager>;

class MutationManagerImpl : public MutationManager, Logger::Loggable<Logger::Id::filter> {
public:
  MutationManagerImpl();

  Result processQuery(QueryMessage& message) override;

  void processRowDescription(RowDescriptionMessage& message) override;
  void processDataRow(DataRowMessage& message) override;

  void processCommandComplete(CommandCompleteMessage& message) override;
  void processEmptyQueryResponse() override;
  void processErrorResponse(ErrorResponseMessage& message) override;

protected:
  std::vector<MutatorPtr> mutator_chain_;
  std::unique_ptr<Envoy::Extensions::Common::SQLUtils::DumpVisitor> dumper_;

  DummyConfig config_;
};

} // namespace PostgresTDE
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
