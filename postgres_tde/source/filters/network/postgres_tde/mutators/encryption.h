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
  explicit EncryptionMutator(DatabaseEncryptionConfig* config);
  EncryptionMutator(const EncryptionMutator&) = delete;

  Result mutateQuery(hsql::SQLParserResult& query) override;
  void mutateRowDescription(RowDescriptionMessage&) override;
  void mutateDataRow(DataRowMessage&) override;

protected:
  Result visitExpression(hsql::Expr* expr) override;

  Result visitInsertStatement(hsql::InsertStatement* stmt) override;
  Result visitUpdateStatement(hsql::UpdateStatement* stmt) override;

  Result mutateInsertStatement();
  Result mutateUpdateStatement();

  hsql::Expr* createEncryptedLiteral(hsql::Expr* orig_literal, ColumnConfig* column_config);
  std::string generateCryptoString(absl::string_view data, ColumnConfig* column_config);

protected:
  std::vector<hsql::InsertStatement*> insert_mutation_candidates_;
  std::vector<hsql::UpdateStatement*> update_mutation_candidates_;

  std::vector<const ColumnConfig*> data_row_config_;

  DatabaseEncryptionConfig* config_;
};

} // namespace PostgresTDE
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
