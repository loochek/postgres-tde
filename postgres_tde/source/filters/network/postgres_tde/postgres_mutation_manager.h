#pragma once
#include <cstdint>

#include "envoy/common/platform.h"

#include "source/common/buffer/buffer_impl.h"
#include "source/common/common/logger.h"

#include "postgres_tde/source/filters/network/postgres_tde/config/dummy_config.h"
#include "postgres_tde/source/filters/network/postgres_tde/mutators/mutator.h"
#include "postgres_tde/source/common/sqlutils/ast/dump_visitor.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace PostgresTDE {

// Query/result mutation manager
class MutationManager {
public:
  virtual ~MutationManager() = default;

  virtual Result processQuery(std::string&) PURE;

  virtual void processRowDescription(Buffer::Instance&) PURE;
  virtual void processDataRow(Buffer::Instance&) PURE;

  virtual void processCommandComplete(Buffer::Instance&) PURE;
  virtual void processEmptyQueryResponse() PURE;
  virtual void processErrorResponse(Buffer::Instance&) PURE;
};

using MutationManagerPtr = std::unique_ptr<MutationManager>;

class MutationManagerImpl : public MutationManager, Logger::Loggable<Logger::Id::filter> {
public:
  MutationManagerImpl();

  Result processQuery(std::string& query) override;

  void processRowDescription(Buffer::Instance& data) override;
  void processDataRow(Buffer::Instance& data) override;

  void processCommandComplete(Buffer::Instance& data) override;
  void processEmptyQueryResponse() override;
  void processErrorResponse(Buffer::Instance& data) override;

protected:
  std::vector<MutatorPtr> mutator_chain_;
  std::unique_ptr<Envoy::Extensions::Common::SQLUtils::DumpVisitor> dumper_;

  DummyConfig config_;
};

} // namespace PostgresTDE
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
