#pragma once

#include "postgres_tde/source/filters/network/postgres_tde/mutators/mutator.h"
#include "postgres_tde/source/filters/network/postgres_tde/config/database_encryption_config.h"
#include "postgres_tde/source/common/sqlutils/ast/visitor.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace PostgresTDE {

using Common::SQLUtils::Visitor;

class BlindIndexMutator : public Mutator, public Visitor {
public:
  explicit BlindIndexMutator(DatabaseEncryptionConfig *config);
  BlindIndexMutator(const BlindIndexMutator&) = delete;

  Result mutateQuery(hsql::SQLParserResult& query) override;
  void mutateResult() override;

protected:
  Result visitExpression(hsql::Expr* expr) override;
  Result visitOperatorExpression(hsql::Expr* expr) override;

  Result mutateComparisons();
  Result mutateGroupByExpressions();

  void replaceLiteralWithHash(hsql::Expr* expr, ColumnConfig *column_config);
  std::string generateHMACString(absl::string_view data, ColumnConfig *column_config);

  void replaceHyriseName(hsql::Expr* expr, const std::string& str);

protected:
  DatabaseEncryptionConfig *config_;

  std::vector<hsql::Expr*> comparison_mutation_candidates_;
  std::vector<hsql::Expr*> group_by_mutation_candidates_;
};

} // namespace PostgresTDE
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
