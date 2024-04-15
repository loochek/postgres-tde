#include "postgres_tde/source/filters/network/postgres_tde/mutators/probabilistic_join.h"
#include "postgres_tde/source/filters/network/postgres_tde/postgres_mutation_manager.h"
#include "postgres_tde/source/common/crypto/utility_ext.h"
#include "absl/strings/escaping.h"
#include "absl/cleanup/cleanup.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace PostgresTDE {

ProbabilisticJoinMutator::ProbabilisticJoinMutator(MutationManager* manager)
    : BaseMutator(manager) {}

Result ProbabilisticJoinMutator::mutateQuery(hsql::SQLParserResult& query) {
  join_mutation_candidates_.clear();
  insert_mutation_candidates_.clear();
  update_mutation_candidates_.clear();

  join_comparisons_.clear();
  join_comparisons_indices_.clear();

  CHECK_RESULT(Visitor::visitQuery(query));
  CHECK_RESULT(mutateJoins());
  CHECK_RESULT(mutateInsertStatement());
  CHECK_RESULT(mutateUpdateStatement());
  return Result::ok;
}

Result ProbabilisticJoinMutator::mutateRowDescription(RowDescriptionMessage& message) {
  size_t columns_count = message.column_descriptions().size();

  // Gather info about resulting columns and try to match them with the query
  std::map<ColumnRef, size_t> columns2idx;
  for (size_t i = 0; i < columns_count; i++) {
    auto& column = message.column_descriptions()[i];
    auto column_ref = getSelectColumnByAlias(column->name());
    if (column_ref != nullptr) {
      columns2idx[*column_ref] = i;
      ENVOY_LOG(error, "matched RowDescription column: {} -> ({}, {})", column->name(),
                column_ref->table(), column_ref->column());
    }
  }

  // Determine column indices for each join comparison
  for (auto& [left_column_ref, right_column_ref] : join_comparisons_) {
    if (columns2idx.find(left_column_ref) == columns2idx.end() ||
        columns2idx.find(right_column_ref) == columns2idx.end()) {
      return Result::makeError(
          "postgres_tde: columns present in join condition must be also present in SELECT body");
    }

    size_t left_column_idx = columns2idx.at(left_column_ref);
    size_t right_column_idx = columns2idx.at(right_column_ref);

    ENVOY_LOG(error, "matched join: ({}.{}, {}.{}) -> (column {}, column {})",
              left_column_ref.table(), left_column_ref.column(), right_column_ref.table(),
              right_column_ref.column(), left_column_idx, right_column_idx);
    join_comparisons_indices_.emplace_back(left_column_idx, right_column_idx);
  }

  return Result::ok;
}

Result ProbabilisticJoinMutator::mutateDataRow(std::unique_ptr<DataRowMessage>& message) {
  for (auto [left_column_idx, right_column_idx]: join_comparisons_indices_) {
    auto& left_column_data = message->columns()[left_column_idx];
    auto& right_column_data = message->columns()[right_column_idx];

    if (*left_column_data != *right_column_data) {
      ENVOY_LOG(error, "discarding row because the join condition is not met: {}", message->toString());
      message.reset();
      return Result::ok;
    }
  }

  return Result::ok;
}

Result ProbabilisticJoinMutator::visitExpression(hsql::Expr* expr) {
  switch (expr->type) {
  case hsql::kExprColumnRef: {
    if (in_select_body_) {
      // SELECT of possibly joinable column is ok
      return Visitor::visitExpression(expr);
    }

    // Any other usage must be prohibited

    if (expr->table == nullptr) {
      return Result::makeError(fmt::format("postgres_tde: unable to determine the source of column "
                                           "{}. Please specify an explicit table/alias reference",
                                           expr->name));
    }

    const std::string& table_name = getTableNameByAlias(expr->table);
    auto column_config = mgr_->getEncryptionConfig()->getColumnConfig(table_name, expr->name);
    if (column_config != nullptr && column_config->hasJoin()) {
      return Result::makeError(fmt::format("postgres_tde: invalid use of encrypted column {}.{}",
                                           table_name, expr->name));
    }

    return Visitor::visitExpression(expr);
  }
  default:
    return Visitor::visitExpression(expr);
  }
}

Result ProbabilisticJoinMutator::visitOperatorExpression(hsql::Expr* expr) {
  switch (expr->opType) {
  case hsql::kOpEquals:
    if (expr->expr->isType(hsql::kExprColumnRef) && expr->expr2->isType(hsql::kExprColumnRef)) {
      ENVOY_LOG(error, "join candidate: {} {}", expr->expr->name, expr->expr2->name);
      join_mutation_candidates_.push_back(expr);
      return Result::ok;
    }
    return Visitor::visitOperatorExpression(expr);
  default:
    return Visitor::visitOperatorExpression(expr);
  }
}

Result ProbabilisticJoinMutator::mutateJoins() {
  for (hsql::Expr* expr : join_mutation_candidates_) {
    ASSERT(expr->isType(hsql::kExprOperator) && (expr->opType == hsql::OperatorType::kOpEquals) &&
           expr->expr->isType(hsql::kExprColumnRef) && expr->expr2->isType(hsql::kExprColumnRef));

    hsql::Expr* left_column = expr->expr;
    hsql::Expr* right_column = expr->expr2;

    for (hsql::Expr* column : {left_column, right_column}) {
      if (column->table == nullptr) {
        return Result::makeError(
            fmt::format("postgres_tde: unable to determine the source of column '{}'. Please "
                        "specify an explicit table/alias reference",
                        column->name));
      }
    }

    const std::string& left_table_name = getTableNameByAlias(left_column->table);
    const std::string& left_column_name = left_column->name;
    auto left_column_config =
        mgr_->getEncryptionConfig()->getColumnConfig(left_table_name, left_column_name);
    auto left_column_ref = ColumnRef(left_table_name, left_column_name);

    const std::string& right_table_name = getTableNameByAlias(right_column->table);
    const std::string& right_column_name = right_column->name;
    auto right_column_config =
        mgr_->getEncryptionConfig()->getColumnConfig(right_table_name, right_column_name);
    auto right_column_ref = ColumnRef(right_table_name, right_column_name);

    if (!isColumnSelected(left_column_ref) || !isColumnSelected(right_column_ref)) {
      return Result::makeError(
          "postgres_tde: columns present in join condition must be also present in SELECT body");
    }

    if (left_column_config == nullptr || !left_column_config->hasJoin()) {
      ENVOY_LOG(error, "join is not configured for {}.{}", left_table_name, left_column_name);
      continue;
    }
    if (right_column_config == nullptr || !right_column_config->hasJoin()) {
      ENVOY_LOG(error, "join is not configured for {}.{}", right_table_name, right_column_name);
      continue;
    }

    const std::string& left_join_key_column_name = left_column_config->joinKeyColumnName();
    free(left_column->name);
    left_column->name = Common::Utils::makeOwnedCString(left_join_key_column_name);

    const std::string& right_join_key_column_name = right_column_config->joinKeyColumnName();
    free(right_column->name);
    right_column->name = Common::Utils::makeOwnedCString(right_join_key_column_name);

    ENVOY_LOG(error, "join: {}.{} == {}.{} ", left_table_name, left_column_name, right_table_name,
              right_column_name);
    join_comparisons_.emplace_back(std::move(left_column_ref), std::move(right_column_ref));
  }

  return Result::ok;
}

Result ProbabilisticJoinMutator::mutateInsertStatement() {
  for (hsql::InsertStatement* stmt : insert_mutation_candidates_) {
    if (stmt->columns->size() != stmt->values->size()) {
      return Result::makeError("postgres_tde: bad INSERT statement");
    }

    std::vector<char*> join_columns;
    std::vector<hsql::Expr*> join_keys;
    absl::Cleanup cleanup2 = [&]() {
      for (char* column : join_columns) {
        free(column);
      }
      for (hsql::Expr* value : join_keys) {
        free(value);
      }
    };

    for (size_t i = 0; i < stmt->columns->size(); i++) {
      char* column = (*stmt->columns)[i];
      hsql::Expr* value = (*stmt->values)[i];

      auto column_config = mgr_->getEncryptionConfig()->getColumnConfig(stmt->tableName, column);
      if (column_config == nullptr || !column_config->hasJoin()) {
        continue;
      }

      if (!value->isLiteral()) {
        return Result::makeError("postgres_tde: only literals can be used as INSERT values for "
                                 "columns with join support");
      }

      join_columns.push_back(Common::Utils::makeOwnedCString(column_config->joinKeyColumnName()));
      join_keys.push_back(createJoinKeyLiteral(value));
    }

    stmt->columns->insert(stmt->columns->end(), join_columns.begin(), join_columns.end());
    join_columns.clear();
    stmt->values->insert(stmt->values->end(), join_keys.begin(), join_keys.end());
    join_keys.clear();
  }

  return Result::ok;
}

Result ProbabilisticJoinMutator::mutateUpdateStatement() {
  for (hsql::UpdateStatement* stmt : update_mutation_candidates_) {
    std::vector<hsql::UpdateClause*> join_key_updates;
    absl::Cleanup cleanup = [&]() {
      for (hsql::UpdateClause* update : join_key_updates) {
        free(update->column);
        delete update->value;
        delete update;
      }
    };

    for (hsql::UpdateClause* update : *stmt->updates) {
      auto column_config =
          mgr_->getEncryptionConfig()->getColumnConfig(stmt->table->name, update->column);
      if (column_config == nullptr || !column_config->hasJoin()) {
        continue;
      }

      if (!update->value->isLiteral()) {
        return Result::makeError(fmt::format("postgres_tde: only literals can be used as UPDATE "
                                             "values for columns with join support"));
      }

      join_key_updates.push_back(new hsql::UpdateClause{
          Common::Utils::makeOwnedCString(column_config->joinKeyColumnName()),
          createJoinKeyLiteral(update->value)});
    }

    stmt->updates->insert(stmt->updates->end(), join_key_updates.begin(), join_key_updates.end());
    join_key_updates.clear();
  }

  return Result::ok;
}

hsql::Expr* ProbabilisticJoinMutator::createJoinKeyLiteral(hsql::Expr* orig_literal) {
  ASSERT(orig_literal->isLiteral());

  switch (orig_literal->type) {
  case hsql::kExprLiteralNull:
    // do nothing with null values
    return hsql::Expr::makeNullLiteral();
  case hsql::kExprLiteralString: {
    const std::string& key_hex_str = generateJoinKeyString(absl::string_view(
        static_cast<const char*>(orig_literal->name), strlen(orig_literal->name) + 1));
    return hsql::Expr::makeLiteral(Common::Utils::makeOwnedCString(key_hex_str));
  }
  case hsql::kExprLiteralInt: {
    const std::string& key_hex_str = generateJoinKeyString(absl::string_view(
        reinterpret_cast<const char*>(&orig_literal->ival), sizeof(orig_literal->ival)));
    return hsql::Expr::makeLiteral(Common::Utils::makeOwnedCString(key_hex_str));
  }
  case hsql::kExprLiteralFloat: {
    const std::string& key_hex_str = generateJoinKeyString(absl::string_view(
        reinterpret_cast<const char*>(&orig_literal->fval), sizeof(orig_literal->fval)));
    return hsql::Expr::makeLiteral(Common::Utils::makeOwnedCString(key_hex_str));
  }
  default:
    ASSERT(false);
  }
}

std::string ProbabilisticJoinMutator::generateJoinKeyString(absl::string_view data) {
  auto& crypto_util_ext = Common::Crypto::UtilityExtSingleton::get();
  auto hash = crypto_util_ext.getSha256Digest(data);

  size_t join_key_size = mgr_->getEncryptionConfig()->join_key_size();
  ASSERT(join_key_size <= hash.size());

  // Join key is the first few bytes of a hash
  std::string key_hex_str = std::string("\\x");
  key_hex_str.append(absl::BytesToHexString(
      absl::string_view(reinterpret_cast<const char*>(hash.data()), join_key_size)));
  return key_hex_str;
}

} // namespace PostgresTDE
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
