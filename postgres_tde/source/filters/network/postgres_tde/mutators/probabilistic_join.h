#pragma once

#include "postgres_tde/source/filters/network/postgres_tde/mutators/base_mutator.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace PostgresTDE {

using Common::SQLUtils::Visitor;
using Common::SQLUtils::ColumnRef;

class ProbabilisticJoinMutator : public BaseMutator {
public:
  explicit ProbabilisticJoinMutator(MutationManager *manager);
  ProbabilisticJoinMutator(const ProbabilisticJoinMutator&) = delete;

  Result mutateQuery(hsql::SQLParserResult& query) override;
  Result mutateRowDescription(RowDescriptionMessage& message) override;
  Result mutateDataRow(std::unique_ptr<DataRowMessage>& message) override;

protected:
  Result visitExpression(hsql::Expr* expr) override;
  Result visitOperatorExpression(hsql::Expr* expr) override;

  Result mutateJoins();
  Result mutateInsertStatement();
  Result mutateUpdateStatement();

  hsql::Expr* createJoinKeyLiteral(hsql::Expr* orig_literal);
  std::string generateJoinKeyString(absl::string_view data);

protected:
  std::vector<hsql::Expr*> join_mutation_candidates_;

  std::vector<std::pair<ColumnRef, ColumnRef>> join_comparisons_;
  std::vector<std::pair<int, int>> join_comparisons_indices_;
};

} // namespace PostgresTDE
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
