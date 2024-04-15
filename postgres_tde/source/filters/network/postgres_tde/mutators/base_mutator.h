#pragma once

#include "postgres_tde/source/filters/network/postgres_tde/mutators/mutator.h"
#include "postgres_tde/source/filters/network/postgres_tde/config/database_encryption_config.h"
#include "postgres_tde/source/common/sqlutils/ast/visitor.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace PostgresTDE {

using Common::SQLUtils::Visitor;

class BaseMutator : public Mutator, public Visitor {
public:
  BaseMutator(const BaseMutator&) = delete;

protected:
  explicit BaseMutator(MutationManager *manager);

  Result visitInsertStatement(hsql::InsertStatement* stmt) override;
  Result visitUpdateStatement(hsql::UpdateStatement* stmt) override;

protected:
  std::vector<hsql::InsertStatement*> insert_mutation_candidates_;
  std::vector<hsql::UpdateStatement*> update_mutation_candidates_;
};

} // namespace PostgresTDE
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
