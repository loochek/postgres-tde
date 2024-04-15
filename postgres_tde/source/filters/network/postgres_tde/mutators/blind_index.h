#pragma once

#include "postgres_tde/source/filters/network/postgres_tde/mutators/base_mutator.h"
#include "postgres_tde/source/filters/network/postgres_tde/config/database_encryption_config.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace PostgresTDE {

using Common::SQLUtils::Visitor;

class BlindIndexMutator: public BaseMutator {
public:
  explicit BlindIndexMutator(MutationManager *manager);
  BlindIndexMutator(const BlindIndexMutator&) = delete;

  Result mutateQuery(hsql::SQLParserResult& query) override;

protected:
  Result visitExpression(hsql::Expr* expr) override;
  Result visitOperatorExpression(hsql::Expr* expr) override;

  Result mutateComparisons();
  Result mutateGroupByExpressions();
  Result mutateInsertStatement();
  Result mutateUpdateStatement();

  hsql::Expr* createHashLiteral(hsql::Expr* orig_literal, const ColumnConfig *column_config);
  std::string generateHMACString(absl::string_view data, const ColumnConfig *column_config);

protected:
  std::vector<hsql::Expr*> comparison_mutation_candidates_;
  std::vector<hsql::Expr*> group_by_mutation_candidates_;
};

} // namespace PostgresTDE
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
