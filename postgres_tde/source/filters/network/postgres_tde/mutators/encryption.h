#pragma once

#include "postgres_tde/source/filters/network/postgres_tde/mutators/mutator.h"
#include "postgres_tde/source/filters/network/postgres_tde/config/database_encryption_config.h"
#include "postgres_tde/source/common/sqlutils/ast/visitor.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace PostgresTDE {

using Common::SQLUtils::Visitor;

class EncryptionMutator : public Mutator, public Visitor {
public:
  explicit EncryptionMutator(DatabaseEncryptionConfig *config);
  EncryptionMutator(const EncryptionMutator&) = delete;

  Result mutateQuery(hsql::SQLParserResult& query) override;
  void mutateResult() override;

protected:
  DatabaseEncryptionConfig *config_;
};

} // namespace PostgresTDE
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
