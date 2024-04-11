#include "postgres_tde/source/filters/network/postgres_tde/mutators/blind_index.h"
#include "source/common/crypto/utility.h"
#include "source/common/common/fmt.h"
#include "absl/strings/escaping.h"
#include "absl/cleanup/cleanup.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace PostgresTDE {

BlindIndexMutator::BlindIndexMutator(DatabaseEncryptionConfig *config) : config_(config) {}

Result BlindIndexMutator::mutateQuery(hsql::SQLParserResult& query) {
  comparison_mutation_candidates_.clear();
  group_by_mutation_candidates_.clear();
  insert_mutation_candidates_.clear();
  update_mutation_candidates_.clear();

  CHECK_RESULT(Visitor::visitQuery(query));
  CHECK_RESULT(mutateComparisons());
  CHECK_RESULT(mutateGroupByExpressions());
  CHECK_RESULT(mutateInsertStatement());
  CHECK_RESULT(mutateUpdateStatement());
  return Result::ok;
}

Result BlindIndexMutator::visitExpression(hsql::Expr* expr) {
  switch (expr->type) {
  case hsql::kExprColumnRef: {
    if (in_select_body_) {
      // SELECT of possibly indexed column is ok
      return Visitor::visitExpression(expr);
    } else if (in_group_by_) {
      // GROUP BY possibly indexed column is ok, but it's necessary to replace it to corresponding BI column
      group_by_mutation_candidates_.push_back(expr);
      return Visitor::visitExpression(expr);
    }

    // Other usages should be prohibited

    if (expr->table == nullptr) {
      return Result::makeError(
          fmt::format("postgres_tde: unable to determine the source of column {}. Please specify an explicit table/alias reference", expr->name));
    }

    const std::string& table_name = getTableNameByAlias(expr->table);
    ColumnConfig *column_config = config_->getColumnConfig(table_name, expr->name);
    if (column_config != nullptr && column_config->hasBlindIndex()) {
      return Result::makeError(
          fmt::format("postgres_tde: invalid use of blind indexed column {}.{}", table_name, expr->name));
    }

    return Visitor::visitExpression(expr);
  }
  default:
    return Visitor::visitExpression(expr);
  }
}


Result BlindIndexMutator::visitOperatorExpression(hsql::Expr* expr) {
  switch (expr->opType) {
  case hsql::kOpEquals:
  case hsql::kOpNotEquals:
    if (expr->expr->isType(hsql::kExprColumnRef) && expr->expr2->isLiteral()) {
      ENVOY_LOG(error, "blind index candidate: {} {}", expr->expr->name, expr->expr2->name);
      comparison_mutation_candidates_.push_back(expr);
      return Result::ok;
    }
    return Visitor::visitOperatorExpression(expr);
  default:
    return Visitor::visitOperatorExpression(expr);
  }
}

Result BlindIndexMutator::mutateComparisons() {
  for (hsql::Expr* expr : comparison_mutation_candidates_) {
    assert(expr->isType(hsql::kExprOperator)
           && (expr->opType == hsql::OperatorType::kOpEquals || expr->opType == hsql::OperatorType::kOpNotEquals)
           && expr->expr->isType(hsql::kExprColumnRef) && expr->expr2->isLiteral());

    hsql::Expr *column = expr->expr;
    hsql::Expr *literal = expr->expr2;

    if (column->table == nullptr) {
      return Result::makeError(
        fmt::format("postgres_tde: unable to determine the source of column '{}'. Please specify an explicit table/alias reference", column->name));
    }

    const std::string& table_name = getTableNameByAlias(column->table);
    ColumnConfig* column_config = config_->getColumnConfig(table_name, column->name);
    if (column_config == nullptr || !column_config->hasBlindIndex()) {
      ENVOY_LOG(error, "blind index is not configured for {}.{}", column->table, column->name);
      continue;
    }

    const std::string& bi_column_name = column_config->BIColumnName();
    free(column->name);
    column->name = Common::Utils::makeOwnedCString(bi_column_name);

    hsql::Expr* bi_literal = createHashLiteral(literal, column_config);
    delete expr->expr2;
    expr->expr2 = bi_literal;
  }

  return Result::ok;
}

Result BlindIndexMutator::visitInsertStatement(hsql::InsertStatement* stmt) {
  switch (stmt->type) {
  case hsql::kInsertValues: {
    if (stmt->columns == nullptr && config_->hasTDEEnabled(stmt->tableName)) {
      return Result::makeError("postgres_tde: columns definition for INSERT is required for TDE-enabled tables");
    }

    insert_mutation_candidates_.push_back(stmt);
    return Visitor::visitInsertStatement(stmt);
  }
  case hsql::kInsertSelect:
    if (config_->hasTDEEnabled(stmt->tableName)) {
      return Result::makeError("postgres_tde: INSERT INTO SELECT is not supported for TDE-enabled tables");
    }

    return Visitor::visitInsertStatement(stmt);
  default:
    assert(false);
  }
}

Result BlindIndexMutator::visitUpdateStatement(hsql::UpdateStatement* stmt) {
  CHECK_RESULT(Visitor::visitUpdateStatement(stmt));
  update_mutation_candidates_.push_back(stmt);
  return Result::ok;
}

Result BlindIndexMutator::mutateGroupByExpressions() {
  for (hsql::Expr* column : group_by_mutation_candidates_) {
    assert(column->isType(hsql::kExprColumnRef));

    if (column->table == nullptr) {
      return Result::makeError(
          fmt::format("postgres_tde: unable to determine the source of column {}. Please specify an explicit table/alias reference", column->name));
    }

    const std::string& table_name = getTableNameByAlias(column->table);
    ColumnConfig* column_config = config_->getColumnConfig(table_name, column->name);
    if (column_config == nullptr || !column_config->hasBlindIndex()) {
      ENVOY_LOG(error, "blind index is not configured for the column {}.{}", column->table, column->name);
      continue;
    }

    const std::string& bi_column_name = column_config->BIColumnName();
    free(column->name);
    column->name = Common::Utils::makeOwnedCString(bi_column_name);
  }

  return Result::ok;
}

Result BlindIndexMutator::mutateInsertStatement() {
  for (hsql::InsertStatement* stmt: insert_mutation_candidates_) {
    if (stmt->columns->size() != stmt->values->size()) {
      return Result::makeError("postgres_tde: bad INSERT statement");
    }

    std::vector<char*> bi_columns;
    std::vector<hsql::Expr*> bi_values;
    absl::Cleanup cleanup2 = [&]() {
      for (char* column: bi_columns) {
        free(column);
      }
      for (hsql::Expr* value: bi_values) {
        free(value);
      }
    };

    for (size_t i = 0; i < stmt->columns->size(); i++) {
      char* column = (*stmt->columns)[i];
      hsql::Expr* value = (*stmt->values)[i];

      ColumnConfig * column_config = config_->getColumnConfig(stmt->tableName, column);
      if (column_config == nullptr || !column_config->hasBlindIndex()) {
        continue;
      }

      if (!value->isLiteral()) {
        return Result::makeError("postgres_tde: only literals can be used as INSERT values for blind-indexed columns");
      }

      bi_columns.push_back(Common::Utils::makeOwnedCString(column_config->BIColumnName()));
      bi_values.push_back(createHashLiteral(value, column_config));
    }

    stmt->columns->insert(stmt->columns->end(), bi_columns.begin(), bi_columns.end());
    bi_columns.clear();
    stmt->values->insert(stmt->values->end(), bi_values.begin(), bi_values.end());
    bi_values.clear();
  }

  return Result::ok;
}

Result BlindIndexMutator::mutateUpdateStatement() {
  for (hsql::UpdateStatement* stmt: update_mutation_candidates_) {
    std::vector<hsql::UpdateClause*> bi_updates;
    absl::Cleanup cleanup = [&]() {
      for (hsql::UpdateClause* update : bi_updates) {
        free(update->column);
        delete update->value;
        delete update;
      }
    };

    for (hsql::UpdateClause* update : *stmt->updates) {
      ColumnConfig* column_config = config_->getColumnConfig(stmt->table->name, update->column);
      if (column_config == nullptr || !column_config->hasBlindIndex()) {
        continue;
      }

      if (!update->value->isLiteral()) {
        return Result::makeError(fmt::format("postgres_tde: only literals can be used as UPDATE values for blind-indexed columns"));
      }

      bi_updates.push_back(new hsql::UpdateClause {Common::Utils::makeOwnedCString(column_config->BIColumnName()), createHashLiteral(update->value, column_config)});
    }

    stmt->updates->insert(stmt->updates->end(), bi_updates.begin(), bi_updates.end());
    bi_updates.clear();
  }

  return Result::ok;
}

hsql::Expr* BlindIndexMutator::createHashLiteral(hsql::Expr* orig_literal, ColumnConfig *column_config) {
  assert(orig_literal->isLiteral());

  switch (orig_literal->type) {
  case hsql::kExprLiteralNull:
    // do nothing with null values
    return hsql::Expr::makeNullLiteral();
  case hsql::kExprLiteralString: {
    const std::string& hmac_hex_str = generateHMACString(
        absl::string_view(static_cast<const char*>(orig_literal->name), strlen(orig_literal->name) + 1),
        column_config
    );
    return hsql::Expr::makeLiteral(Common::Utils::makeOwnedCString(hmac_hex_str));
  }
  case hsql::kExprLiteralInt: {
    const std::string& hmac_hex_str = generateHMACString(
        absl::string_view(reinterpret_cast<const char*>(&orig_literal->ival), sizeof(orig_literal->ival)),
        column_config
    );
    return hsql::Expr::makeLiteral(Common::Utils::makeOwnedCString(hmac_hex_str));
  }
  case hsql::kExprLiteralFloat: {
    const std::string& hmac_hex_str = generateHMACString(
        absl::string_view(reinterpret_cast<const char*>(&orig_literal->fval), sizeof(orig_literal->fval)),
        column_config
    );
    return hsql::Expr::makeLiteral(Common::Utils::makeOwnedCString(hmac_hex_str));
  }
  default:
    assert(false);
  }
}

std::string BlindIndexMutator::generateHMACString(absl::string_view data, ColumnConfig *column_config) {
  auto& crypto_util = Envoy::Common::Crypto::UtilitySingleton::get();
  auto hmac = crypto_util.getSha256Hmac(column_config->BIKey(), data);

  std::string hmac_hex_str = std::string("\\x");
  hmac_hex_str.append(absl::BytesToHexString(absl::string_view(reinterpret_cast<const char*>(hmac.data()), hmac.size())));
  return hmac_hex_str;
}

} // namespace PostgresTDE
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
