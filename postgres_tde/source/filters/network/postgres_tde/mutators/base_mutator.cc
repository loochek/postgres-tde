#include "postgres_tde/source/filters/network/postgres_tde/mutators/base_mutator.h"
#include "postgres_tde/source/filters/network/postgres_tde/postgres_mutation_manager.h"
#include "source/common/common/assert.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace PostgresTDE {

BaseMutator::BaseMutator(MutationManager *manager) : Mutator(manager) {}

Result BaseMutator::visitInsertStatement(hsql::InsertStatement* stmt) {
  switch (stmt->type) {
  case hsql::kInsertValues: {
    if (stmt->columns == nullptr && mgr_->getEncryptionConfig()->hasTDEEnabled(stmt->tableName)) {
      return Result::makeError("postgres_tde: columns definition for INSERT is required for TDE-enabled tables");
    }

    insert_mutation_candidates_.push_back(stmt);
    return Visitor::visitInsertStatement(stmt);
  }
  case hsql::kInsertSelect:
    if (mgr_->getEncryptionConfig()->hasTDEEnabled(stmt->tableName)) {
      return Result::makeError("postgres_tde: INSERT INTO SELECT is not supported for TDE-enabled tables");
    }

    return Visitor::visitInsertStatement(stmt);
  default:
    ASSERT(false);
  }
}

Result BaseMutator::visitUpdateStatement(hsql::UpdateStatement* stmt) {
  CHECK_RESULT(Visitor::visitUpdateStatement(stmt));
  update_mutation_candidates_.push_back(stmt);
  return Result::ok;
}

} // namespace PostgresTDE
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
