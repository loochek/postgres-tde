#pragma once

#include "postgres_tde/source/filters/network/postgres_tde/mutators/base_mutator.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace PostgresTDE {

using Common::SQLUtils::Visitor;

class EncryptionMutator : public BaseMutator {
public:
  explicit EncryptionMutator(MutationManager *manager);
  EncryptionMutator(const EncryptionMutator&) = delete;

  Result mutateQuery(hsql::SQLParserResult& query) override;
  Result mutateRowDescription(RowDescriptionMessage&) override;
  Result mutateDataRow(std::unique_ptr<DataRowMessage>&) override;

protected:
  Result visitExpression(hsql::Expr* expr) override;

  Result mutateInsertStatement();
  Result mutateUpdateStatement();

  hsql::Expr* createEncryptedLiteral(hsql::Expr* orig_literal, const ColumnConfig* column_config);
  std::string generateCryptoString(absl::string_view data, const ColumnConfig* column_config);

protected:
  std::vector<const ColumnConfig*> data_row_config_;
};

} // namespace PostgresTDE
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
